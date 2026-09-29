/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Host tests of the fast reading (rsvp.c): words, French typography, encodings, timings, long words, seek.
 * run_tests.py copies rsvp.c next to the stand-ins of stubs/rsvp (store.h, screen.h, ff.h, sd.h). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rsvp.c"
#include "test.h"

store_t host_store;
uint64_t host_time_us = 0;
bool host_gpio[32];
uint8_t host_spi_log[65536];
size_t host_spi_len = 0;
const uint8_t screen_ws_10fps[1], screen_ws_20fps[1];

static const char *write_file(const char *name, const char *content, size_t len) {
    FILE *f = fopen(name, "wb");
    fwrite(content, 1, len, f);
    fclose(f);
    return name;
}

static void open_text(const char *name) {
    if (file_open)
        f_close(&file);
    f_open(&file, name, FA_READ);
    file_open = true;
    file_size = f_size(&file);
    detect_encoding();
    have_word = false;
    goto_offset(0, true);
    first_word = false;
    wpm = 250;
}

/* The next displayed chunk */
static const char *next(void) {
    static char text[WORD_MAX];
    if (! have_word)
        return "<end>";
    chunk_text(text);
    advance();
    return text;
}

int main(void) {
    /* UTF-8 with a byte order mark, French typography (narrow no-break spaces, guillemets, apostrophes) */
    static const char utf8[] =
        "\xef\xbb\xbf" "Le Horla\n\n"
        "8 mai. \xe2\x80\x94 Quelle journ\xc3\xa9" "e admirable\xe2\x80\xaf! J\xe2\x80\x99" "ai pass\xc3\xa9, la maison.\n\n"
        "Il dit\xc2\xa0: \xc2\xab\xc2\xa0" "Anticonstitutionnellement\xc2\xa0\xc2\xbb, puis\xe2\x80\xa6 Fin.\n";
    open_text(write_file("rsvp_utf8.txt", utf8, sizeof(utf8) - 1));
    CHECK(! latin1);
    CHECK_STR(next(), "Le");  /* The byte order mark is skipped */
    CHECK_STR(next(), "Horla");
    CHECK_STR(next(), "8");
    CHECK_STR(next(), "mai.");
    CHECK_STR(next(), "- Quelle");  /* The dialogue dash goes with the next word */
    CHECK_STR(next(), "journée");
    CHECK_STR(next(), "admirable !");  /* The punctuation after a space goes with the previous word */
    CHECK_STR(next(), "J'ai");
    CHECK_STR(next(), "passé,");
    CHECK_STR(next(), "la");
    CHECK_STR(next(), "maison.");
    CHECK_STR(next(), "Il");
    CHECK_STR(next(), "dit :");
    /* Long words are split in chunks of the same size */
    CHECK_STR(next(), "\" Anticons-");
    CHECK_STR(next(), "titutionne-");
    CHECK_STR(next(), "llement \",");
    CHECK_STR(next(), "puis...");
    CHECK_STR(next(), "Fin.");
    CHECK_STR(next(), "<end>");

    /* Timings at 250 words per minute: 240ms, x1.2 long word, x1.3 comma, x1.6 end of sentence, x2 end of paragraph */
    open_text("rsvp_utf8.txt");
    CHECK_EQ(word_ms(), 240);   /* Le */
    advance();
    CHECK_EQ(word_ms(), 480);   /* Horla (end of paragraph) */
    advance(); advance();
    CHECK_EQ(word_ms(), 384);   /* mai. */
    advance(); advance(); advance();
    CHECK_EQ(word_ms(), 384);   /* admirable ! */
    advance(); advance();
    CHECK_EQ(word_ms(), 312);   /* passé, */
    advance(); advance();
    CHECK_EQ(word_ms(), 480);   /* maison. (end of paragraph) */
    wpm = 900;
    CHECK_EQ(word_ms(), 132);   /* 66ms x2 */
    advance();
    CHECK_EQ(word_ms(), MIN_WORD_MS);  /* The screen can't be faster */

    /* Optimal recognition point: by number of letters */
    CHECK_EQ(orp_index(1), 0);
    CHECK_EQ(orp_index(4), 1);
    CHECK_EQ(orp_index(7), 2);
    CHECK_EQ(orp_index(12), 3);
    CHECK_EQ(orp_index(20), 4);
    CHECK(is_letter_at("J'ai", 0));
    CHECK(! is_letter_at("J'ai", 1));
    CHECK(is_letter_at("journée", 5));  /* é */

    /* Windows-1252 (Latin-1) texts are detected and converted */
    static const char latin[] = "D\xe9j\xe0 l\x92\xe9t\xe9, dit-il ; c\x92" "est \x93super\x94 \x96 vraiment.\n";
    open_text(write_file("rsvp_latin1.txt", latin, sizeof(latin) - 1));
    CHECK(latin1);
    CHECK_STR(next(), "Déjà");
    CHECK_STR(next(), "l'été,");
    CHECK_STR(next(), "dit-il ;");
    CHECK_STR(next(), "c'est");
    CHECK_STR(next(), "\"super\"");
    CHECK_STR(next(), "- vraiment.");

    /* Seek: forward exact, backward from the average length of the words */
    char text[4096] = "";
    for (int i = 0; i < 300; ++i) {
        char w[16];
        snprintf(w, sizeof(w), "mot%03d ", i);
        strcat(text, w);
    }
    open_text(write_file("rsvp_seek.txt", text, strlen(text)));
    do_seek(1);  /* 250 wpm x 10s = 41 words */
    CHECK_STR(word.text, "mot041");
    for (int i = 0; i < 100; ++i)
        advance();
    CHECK_STR(word.text, "mot141");
    do_seek(-1);
    int back = atoi(word.text + 3);
    CHECK(back >= 95 && back <= 105);  /* ~41 words back from 141 */

    f_close(&file);
    remove("rsvp_utf8.txt");
    remove("rsvp_latin1.txt");
    remove("rsvp_seek.txt");
    TEST_END();
}
