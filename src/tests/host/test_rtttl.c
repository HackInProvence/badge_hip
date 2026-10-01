/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Host tests of the RTTTL parser (rtttl_parse.c): defaults, durations, dots, sharps, octaves, pauses, spaces, case,
 * errors and their position, garbage (no crash), and the tunes in the firmware. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rtttl_parse.h"
#include "test.h"

/* Opens \p text and reads its notes in \p notes, returns the result of the last call */
static rtttl_err_t parse(const char *text, rtttl_t *t, rtttl_note_t *notes, int max, int *count) {
    *count = 0;
    rtttl_err_t e = rtttl_open(t, text, strlen(text));
    if (e != RTTTL_OK)
        return e;
    rtttl_note_t n;
    while ((e = rtttl_next(t, &n)) == RTTTL_OK)
        if (*count < max)
            notes[(*count)++] = n;
    return e;
}

/* The error of \p text and its position */
static void check_error(const char *text, rtttl_err_t expected, size_t pos, int line) {
    rtttl_t t;
    rtttl_err_t e = rtttl_open(&t, text, strlen(text));
    if (e == RTTTL_OK)
        e = rtttl_check(&t, NULL, NULL);
    if (e != expected || t.err_pos != pos) {
        ++test_failures;
        printf("FAIL line %d: \"%s\": error %d at %u (expected %d at %u)\n", line, text, e, (unsigned)t.err_pos,
               expected, (unsigned)pos);
    } else {
        ++test_passes;
    }
}
#define ERROR(text, e, pos) check_error(text, e, pos, __LINE__)

static void test_basic(void) {
    rtttl_t t;
    rtttl_note_t n[16];
    int count;
    CHECK_EQ(parse("Test:d=4,o=5,b=60:c,8d#6,p,2a.", &t, n, 16, &count), RTTTL_END);
    CHECK_STR(t.name, "Test");
    CHECK_EQ(t.duration, 4);
    CHECK_EQ(t.octave, 5);
    CHECK_EQ(t.bpm, 60);
    CHECK_EQ(count, 4);
    CHECK_EQ(n[0].freq_mhz, 523251);  /* C5 */
    CHECK_EQ(n[0].ms, 1000);
    CHECK_EQ(n[0].pos, 18);
    CHECK_EQ(n[1].freq_mhz, 1244508);  /* D#6 */
    CHECK_EQ(n[1].semitone, 3);
    CHECK_EQ(n[1].octave, 6);
    CHECK_EQ(n[1].ms, 500);
    CHECK(n[2].rest);
    CHECK_EQ(n[2].freq_mhz, 0);
    CHECK_EQ(n[2].ms, 1000);
    CHECK_EQ(n[3].freq_mhz, 880000);  /* A5 */
    CHECK_EQ(n[3].ms, 3000);  /* Dotted half */

    /* rtttl_check(): count, total, rewound */
    uint32_t notes, ms;
    rtttl_open(&t, "Test:d=4,o=5,b=60:c,8d#6,p,2a.", 100);
    CHECK_EQ(rtttl_check(&t, &notes, &ms), RTTTL_OK);
    CHECK_EQ(notes, 4);
    CHECK_EQ(ms, 5500);
    rtttl_note_t first;
    CHECK_EQ(rtttl_next(&t, &first), RTTTL_OK);
    CHECK_EQ(first.freq_mhz, 523251);
    rtttl_rewind(&t);
    CHECK_EQ(rtttl_next(&t, &first), RTTTL_OK);
    CHECK_EQ(first.pos, 18);
}

static void test_dots(void) {
    rtttl_t t;
    rtttl_note_t n[8];
    int count;
    /* The dot before or after the octave, twice, on a pause */
    CHECK_EQ(parse("x:b=60:8c.6,8c6.,4c..,4c.6.,p.", &t, n, 8, &count), RTTTL_END);
    CHECK_EQ(count, 5);
    CHECK_EQ(n[0].ms, 750);
    CHECK_EQ(n[0].octave, 6);
    CHECK_EQ(n[1].ms, 750);
    CHECK_EQ(n[2].ms, 1750);
    CHECK_EQ(n[3].ms, 1750);
    CHECK_EQ(n[4].ms, 1500);
    CHECK(n[4].rest);
    ERROR("x::c...", RTTTL_ERR_SEPARATOR, 6);
}

static void test_defaults(void) {
    rtttl_t t;
    rtttl_note_t n[8];
    int count;
    /* Missing defaults: d=4, o=6, b=63 */
    CHECK_EQ(parse("x::c", &t, n, 8, &count), RTTTL_END);
    CHECK_EQ(count, 1);
    CHECK_EQ(n[0].freq_mhz, 1046502);  /* C6 */
    CHECK_EQ(n[0].ms, 952);
    /* Spaces, case, order, a part only, empty items, unknown key */
    CHECK_EQ(parse("  My Song : B = 120 ,, D=8 , l=15 : C , 4E5 , P , ", &t, n, 8, &count), RTTTL_END);
    CHECK_STR(t.name, "My Song");
    CHECK_EQ(t.bpm, 120);
    CHECK_EQ(t.duration, 8);
    CHECK_EQ(t.octave, 6);
    CHECK_EQ(count, 3);
    CHECK_EQ(n[0].ms, 250);
    CHECK_EQ(n[0].octave, 6);
    CHECK_EQ(n[1].ms, 500);
    CHECK_EQ(n[1].freq_mhz, 659255);  /* E5 */
    CHECK(n[2].rest);
    CHECK_EQ(n[2].ms, 250);
    /* Durations 1 to 64, every one */
    CHECK_EQ(parse("x:b=60:1c,2c,4c,8c,16c,32c,64c", &t, n, 8, &count), RTTTL_END);
    CHECK_EQ(count, 7);
    CHECK_EQ(n[0].ms, 4000);
    CHECK_EQ(n[5].ms, 125);
    CHECK_EQ(n[6].ms, 63);
    /* Fastest: never 0 ms */
    CHECK_EQ(parse("x:b=999:64c", &t, n, 8, &count), RTTTL_END);
    CHECK_EQ(n[0].ms, 4);
    /* Empty name */
    CHECK_EQ(parse(":d=4:c", &t, n, 8, &count), RTTTL_END);
    CHECK_STR(t.name, "");
    /* CR LF at the end, UTF-8 byte order mark */
    CHECK_EQ(parse("\xEF\xBB\xBFName:d=4:c,d\r\n", &t, n, 8, &count), RTTTL_END);
    CHECK_STR(t.name, "Name");
    CHECK_EQ(count, 2);
}

static void test_notes(void) {
    rtttl_t t;
    rtttl_note_t n[16];
    int count;
    /* All the letters, h = b, sharps, b# and e# */
    CHECK_EQ(parse("x:o=5:c,c#,d,d#,e,f,f#,g,g#,a,a#,b,h,b#,e#", &t, n, 16, &count), RTTTL_END);
    CHECK_EQ(count, 15);
    for (int i = 0; i < 12; ++i) {
        CHECK_EQ(n[i].semitone, i);
        CHECK_EQ(n[i].octave, 5);
    }
    CHECK_EQ(n[9].freq_mhz, 880000);
    CHECK_EQ(n[11].freq_mhz, 987766);  /* B5 */
    CHECK_EQ(n[12].freq_mhz, 987766);  /* H5 */
    CHECK_EQ(n[13].semitone, 0);  /* B#5 = C6 */
    CHECK_EQ(n[13].octave, 6);
    CHECK_EQ(n[13].freq_mhz, 1046502);
    CHECK_EQ(n[14].semitone, 5);  /* E# = F */
    /* Octaves 3 to 8 */
    CHECK_EQ(parse("x::a3,a4,A7,a8,B#8", &t, n, 16, &count), RTTTL_END);
    CHECK_EQ(n[0].freq_mhz, 220000);
    CHECK_EQ(n[1].freq_mhz, 440000);
    CHECK_EQ(n[2].freq_mhz, 3520000);
    CHECK_EQ(n[3].freq_mhz, 7040000);
    CHECK_EQ(n[4].octave, 9);
    CHECK_EQ(n[4].freq_mhz, 8372018);
    /* Upper case, spaces inside a note */
    CHECK_EQ(parse("x:D=4,O=5,B=60: 8 C # 6 . , 2 P", &t, n, 16, &count), RTTTL_END);
    CHECK_EQ(count, 2);
    CHECK_EQ(n[0].semitone, 1);
    CHECK_EQ(n[0].octave, 6);
    CHECK_EQ(n[0].ms, 750);
    CHECK(n[1].rest);
    CHECK_EQ(n[1].ms, 2000);
    /* Text cut by its length */
    rtttl_open(&t, "x::c,d,e", 4);
    uint32_t c;
    CHECK_EQ(rtttl_check(&t, &c, NULL), RTTTL_OK);
    CHECK_EQ(c, 1);
}

static void test_errors(void) {
    ERROR("no colon at all", RTTTL_ERR_NAME, 15);
    ERROR("", RTTTL_ERR_NAME, 0);
    ERROR("name:d=4,o=5", RTTTL_ERR_SECTION, 12);
    ERROR("x:d=3:c", RTTTL_ERR_DEFAULT, 4);
    ERROR("x:o=9:c", RTTTL_ERR_DEFAULT, 4);
    ERROR("x:o=2:c", RTTTL_ERR_DEFAULT, 4);
    ERROR("x:b=0:c", RTTTL_ERR_DEFAULT, 4);
    ERROR("x:b=99999:c", RTTTL_ERR_DEFAULT, 4);
    ERROR("x:d:c", RTTTL_ERR_DEFAULT, 3);
    ERROR("x:d=:c", RTTTL_ERR_DEFAULT, 4);
    ERROR("x:d=4 o=5:c", RTTTL_ERR_DEFAULT, 6);
    ERROR("x:=4:c", RTTTL_ERR_DEFAULT, 2);
    ERROR("x::3c", RTTTL_ERR_DURATION, 3);
    ERROR("x::c,d,48e", RTTTL_ERR_DURATION, 7);
    ERROR("x::c,x", RTTTL_ERR_NOTE, 5);
    ERROR("x::c,8", RTTTL_ERR_NOTE, 6);
    ERROR("x::p#", RTTTL_ERR_NOTE, 4);
    ERROR("x::c9", RTTTL_ERR_OCTAVE, 4);
    ERROR("x::c2", RTTTL_ERR_OCTAVE, 4);
    ERROR("x::c,c66", RTTTL_ERR_OCTAVE, 6);
    ERROR("x::c6x", RTTTL_ERR_SEPARATOR, 5);
    ERROR("x::c d", RTTTL_ERR_SEPARATOR, 5);
    ERROR("x::c##", RTTTL_ERR_SEPARATOR, 5);
    ERROR("x::c:d", RTTTL_ERR_SEPARATOR, 4);
    ERROR("x:d=4:", RTTTL_ERR_EMPTY, 6);
    ERROR("x::  , ,, ", RTTTL_ERR_EMPTY, 3);
    /* The error texts exist */
    for (int e = RTTTL_OK; e <= RTTTL_ERR_TOO_LONG; ++e)
        CHECK(strlen(rtttl_error_text((rtttl_err_t)e)) > 0);
}

static void test_name(void) {
    char text[200];
    rtttl_t t;
    /* A long name of 2 byte characters is cut between the characters */
    text[0] = 0;
    for (int i = 0; i < 30; ++i)
        strcat(text, "\xC3\xA9");  /* é */
    strcat(text, ":d=4:c");
    CHECK_EQ(rtttl_open(&t, text, strlen(text)), RTTTL_OK);
    CHECK(strlen(t.name) < RTTTL_NAME_MAX);
    CHECK_EQ(strlen(t.name) % 2, 0);
    CHECK(strlen(t.name) >= RTTTL_NAME_MAX - 2);
}

static void test_labels(void) {
    rtttl_t t;
    rtttl_note_t n[8];
    int count;
    char buf[16];
    parse("x:o=5:a#,b#,d4,p,g7", &t, n, 8, &count);
    rtttl_note_label(&n[0], buf, sizeof(buf));
    CHECK_STR(buf, "La#5");
    rtttl_note_label(&n[1], buf, sizeof(buf));
    CHECK_STR(buf, "Do6");
    rtttl_note_label(&n[2], buf, sizeof(buf));
    CHECK_STR(buf, "Ré4");
    rtttl_note_label(&n[3], buf, sizeof(buf));
    CHECK_STR(buf, "Silence");
    rtttl_note_label(&n[4], buf, sizeof(buf));
    CHECK_STR(buf, "Sol7");
}

/* Garbage: never a crash nor an endless loop, the errors are inside the text */
static void test_garbage(void) {
    static const char ALPHABET[] = "abcdefghpABCP0123456789#.,: =dob\r\n\t\xC3\xA9\x01\xFF";
    char text[64];
    srand(1234);
    int ok = 0, errors = 0, bad = 0;
    for (int k = 0; k < 200000; ++k) {
        int len = rand() % (int)sizeof(text);
        for (int i = 0; i < len; ++i)
            text[i] = ALPHABET[rand() % (sizeof(ALPHABET) - 1)];
        /* Often a valid header and notes like characters, to test the notes deeper */
        if (len > 8 && rand() % 2) {
            static const char NOTES[] = "abcdefghpC#.,,,4568 ";
            memcpy(text, "x:d=4:", 6);
            for (int i = 6; i < len; ++i)
                text[i] = NOTES[rand() % (sizeof(NOTES) - 1)];
        }
        rtttl_t t;
        rtttl_err_t e = rtttl_open(&t, text, len);  /* Not 0 terminated: the length is the limit */
        uint32_t notes = 0;
        if (e == RTTTL_OK)
            e = rtttl_check(&t, &notes, NULL);
        if (e == RTTTL_OK)
            ++ok;
        else
            ++errors;
        if (t.err_pos > (size_t)len || notes > (uint32_t)len || (e != RTTTL_OK && e < RTTTL_ERR_NAME))
            ++bad;
    }
    printf("garbage: %d valid, %d errors\n", ok, errors);
    CHECK_EQ(bad, 0);
    CHECK(ok > 0);
}

/* The tunes of the firmware are valid, in the range of the buzzer, not too long */
static void test_builtin(void) {
    CHECK(RTTTL_N_BUILTIN >= 10);
    for (int i = 0; i < RTTTL_N_BUILTIN; ++i) {
        rtttl_t t;
        uint32_t notes = 0, ms = 0;
        rtttl_err_t e = rtttl_open(&t, RTTTL_BUILTIN[i], strlen(RTTTL_BUILTIN[i]));
        if (e == RTTTL_OK)
            e = rtttl_check(&t, &notes, &ms);
        if (e != RTTTL_OK)
            printf("built-in %d: error %d (%s) at %u: \"%.20s\"\n", i, e, rtttl_error_text(e), (unsigned)t.err_pos,
                   RTTTL_BUILTIN[i] + t.err_pos);
        CHECK_EQ(e, RTTTL_OK);
        CHECK(t.name[0]);
        CHECK(ms >= 5000 && ms <= 120000);
        printf("built-in: %-26s %3u notes, %5.1f s\n", t.name, (unsigned)notes, ms / 1000.0);
        rtttl_note_t n;
        while (rtttl_next(&t, &n) == RTTTL_OK)
            CHECK(n.rest || (n.freq_mhz >= 200000 && n.freq_mhz <= 5000000));
    }
}

/* The example files of the SD card (docs/sd/SONNERIES, copied by run_tests.py): valid, but the volunteer error */
static void test_sd_files(void) {
    static const char *const FILES[] = {"classique.txt", "exemples.rtttl"};
    int tunes = 0;
    for (size_t f = 0; f < sizeof(FILES) / sizeof(FILES[0]); ++f) {
        FILE *fp = fopen(FILES[f], "rb");
        CHECK(fp != NULL);
        if (! fp)
            continue;
        char text[2048];
        while (fgets(text, sizeof(text), fp)) {
            text[strcspn(text, "\r\n")] = 0;
            const char *p = text + strspn(text, " \t");
            if (! *p || *p == '#')
                continue;
            rtttl_t t;
            uint32_t notes = 0, ms = 0;
            rtttl_err_t e = rtttl_open(&t, text, strlen(text));
            if (e == RTTTL_OK)
                e = rtttl_check(&t, &notes, &ms);
            printf("%s: %-22s error %d at %u, %u notes, %.1f s\n", FILES[f], t.name, e, (unsigned)t.err_pos,
                   (unsigned)notes, ms / 1000.0);
            if (! strncmp(t.name, "Erreur", 6)) {
                CHECK_EQ(e, RTTTL_ERR_NOTE);
                CHECK_EQ(text[t.err_pos], 'x');
            } else {
                CHECK_EQ(e, RTTTL_OK);
            }
            ++tunes;
        }
        fclose(fp);
    }
    CHECK_EQ(tunes, 6);
}

int main(void) {
    test_basic();
    test_dots();
    test_defaults();
    test_notes();
    test_errors();
    test_name();
    test_labels();
    test_garbage();
    test_builtin();
    test_sd_files();
    TEST_END();
}
