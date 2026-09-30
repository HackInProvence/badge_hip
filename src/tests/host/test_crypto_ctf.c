/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Host tests of the crypto challenges (menu/crypto_ctf.c): answers, normalization, pieces and final flag,
 * texts that fit the screen */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../../gfx/gfx.h"
#include "../../menu/crypto_ctf.h"
#include "test.h"

/* SPOILERS: the answers and the flag, as in tools/crypto_ctf_make.py --answers */
static const char *ANSWERS[] = {
    "PATCH", "SOLEIL", "LUMIERE", "CHANTIER NAVAL", "BOULE", "CALANQUE",
    "EDEN THEATRE", "MISTRAL", "TRAIN", "SARDINE", "PIEDS TANQUES", "GABIAN",
    "DAUPHIN",
};
#define N_ANSWERS ((int)(sizeof(ANSWERS) / sizeof(ANSWERS[0])))
static const char FLAG[] = "SECSEA{L4_C1G4L3_CH1FFR3_4_L4_C10T4T}";

/* Whether the font has a glyph for each character (gfx_text() silently skips the others) */
static bool all_glyphs(const gfx_font_t *font, const char *s) {
    const uint8_t *p = (const uint8_t *)s;
    while (*p) {
        uint32_t cp = *p++;
        if (cp >= 0xC0) {  /* 2 bytes UTF-8 (the accents) */
            cp = ((cp & 0x1F) << 6) | (*p++ & 0x3F);
        }
        bool found = false;
        for (int i = 0; i < font->n_glyphs && ! found; ++i)
            found = font->glyphs[i].codepoint == cp;
        if (! found) {
            printf("No glyph U+%04X in \"%s\"\n", (unsigned)cp, s);
            return false;
        }
    }
    return true;
}

/* Checks each line of \p text: width, glyphs; returns the number of lines */
static int check_lines(const char *text, const char *what, int i) {
    char line[128];
    int n = 0;
    while (*text) {
        const char *end = strchr(text, '\n');
        size_t len = end ? (size_t)(end - text) : strlen(text);
        CHECK(len < sizeof(line));
        if (len >= sizeof(line))
            len = sizeof(line) - 1;
        memcpy(line, text, len);
        line[len] = 0;
        int w = gfx_text_width(&gfx_font_small, line);
        if (w > 196)
            printf("%s %d too wide (%d px): \"%s\"\n", what, i, w, line);
        CHECK(w <= 196);
        CHECK(all_glyphs(&gfx_font_small, line));
        ++n;
        text += len;
        if (*text == '\n')
            ++text;
    }
    return n;
}

int main(void) {
    int n = crypto_ctf_count();
    CHECK_EQ(n, N_ANSWERS);
    CHECK(n >= 8 && n <= 16);
    uint32_t all = (1u << n) - 1;

    /* Texts: they fit the screen */
    for (int i = 0; i < n; ++i) {
        const char *title = crypto_ctf_title(i);
        CHECK(title[0] != 0);
        CHECK(gfx_text_width(&gfx_font_medium, title) <= 196);
        CHECK(all_glyphs(&gfx_font_medium, title));
        int lines = check_lines(crypto_ctf_text(i), "Text", i);
        CHECK(lines >= 1 && lines <= 7);
        CHECK(check_lines(crypto_ctf_hint(i), "Hint", i) <= 3);
        const char *m = crypto_ctf_morse(i);
        if (m)
            CHECK(strspn(m, ".-/ ") == strlen(m) && (crypto_ctf_ultrasound(i) || strstr(crypto_ctf_text(i), m)));
        CHECK(! crypto_ctf_ultrasound(i) || m);  /* The ultrasound is Morse (hidden from the text) */
    }
    int n_morse = 0, n_ultrasound = 0;
    for (int i = 0; i < n; ++i) {
        n_morse += crypto_ctf_morse(i) != NULL;
        n_ultrasound += crypto_ctf_ultrasound(i);
    }
    CHECK_EQ(n_morse, 2);
    CHECK_EQ(n_ultrasound, 1);
    CHECK(! crypto_ctf_ultrasound(n));
    /* Out of range */
    CHECK_STR(crypto_ctf_title(-1), "");
    CHECK_STR(crypto_ctf_text(n), "");
    CHECK_STR(crypto_ctf_hint(n), "");
    CHECK(crypto_ctf_morse(n) == NULL);
    CHECK(! crypto_ctf_check(n, "PATCH"));
    CHECK(! crypto_ctf_check(-1, "PATCH"));

    /* Answers: each one only for its challenge */
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            CHECK(crypto_ctf_check(i, ANSWERS[j]) == (i == j));
    /* Normalization: case, spaces at the ends, repeated spaces */
    CHECK(crypto_ctf_check(0, "patch"));
    CHECK(crypto_ctf_check(0, "  Patch  "));
    CHECK(crypto_ctf_check(3, "chantier   naval"));
    CHECK(crypto_ctf_check(3, "\tChantier Naval "));
    CHECK(crypto_ctf_check(10, " pieds tanques"));
    /* Wrong answers */
    CHECK(! crypto_ctf_check(0, ""));
    CHECK(! crypto_ctf_check(0, "   "));
    CHECK(! crypto_ctf_check(0, NULL));
    CHECK(! crypto_ctf_check(0, "PATC"));
    CHECK(! crypto_ctf_check(0, "PATCHE"));
    CHECK(! crypto_ctf_check(0, "P ATCH"));
    CHECK(! crypto_ctf_check(3, "CHANTIERNAVAL"));
    CHECK(! crypto_ctf_check(6, "EDEN THÉATRE"));
    CHECK(! crypto_ctf_check(1, "SOLEIL!"));
    CHECK(! crypto_ctf_check(2, "LUMIERELUMIERELUMIERELUMIERELUMIERE"));  /* Too long */

    /* Pieces: only when solved, together they make the flag */
    char buf[64], piece[8], inner[CRYPTO_CTF_FLAG_MAX] = "";
    CHECK(! crypto_ctf_piece(0, 0, piece, sizeof(piece)));
    CHECK(! crypto_ctf_piece(0, 1, piece, 1));  /* Too small */
    CHECK(! crypto_ctf_piece(n, 0xFFFFFFFF, piece, sizeof(piece)));
    for (int i = 0; i < n; ++i) {
        CHECK(crypto_ctf_piece(i, 1u << i, piece, sizeof(piece)));
        CHECK(strlen(piece) >= 1);
        strcat(inner, piece);
    }
    snprintf(buf, sizeof(buf), "SECSEA{%s}", inner);
    CHECK_STR(buf, FLAG);

    /* Final flag: only with all the challenges solved */
    CHECK(! crypto_ctf_final_flag(0, buf, sizeof(buf)));
    CHECK_STR(buf, "");
    for (int i = 0; i < n; ++i) {
        CHECK(! crypto_ctf_final_flag(all & ~(1u << i), buf, sizeof(buf)));
        CHECK_STR(buf, "");
    }
    CHECK(! crypto_ctf_final_flag(all, buf, 10));  /* Too small */
    CHECK(crypto_ctf_final_flag(all, buf, sizeof(buf)));
    CHECK_STR(buf, FLAG);
    CHECK(crypto_ctf_final_flag(0xFFFFFFFF, buf, sizeof(buf)));  /* Other bits ignored */
    CHECK_STR(buf, FLAG);

    TEST_END();
}
