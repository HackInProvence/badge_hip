/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Host tests of the signed score QR codes (menu/score_code.c): SipHash, text, signature, QR code */

#include <string.h>

#include "../../menu/score_code.c"  /* Included to check the signature with the masked key */
#include "test.h"

static int count_black(const uint8_t *fb) {
    int n = 0;
    for (int i = 0; i < GFX_FB_SIZE; ++i)
        for (int b = 0; b < 8; ++b)
            n += ! (fb[i] & (1 << b));
    return n;
}

int main(void) {
    /* Reference vector of SipHash-2-4: key 00..0f, message 00..0e */
    uint8_t k[16], m[15];
    for (int i = 0; i < 16; ++i)
        k[i] = i;
    for (int i = 0; i < 15; ++i)
        m[i] = i;
    CHECK(score_code_siphash(k, m, 15) == 0xa129ca6149be45e5ULL);
    CHECK(score_code_siphash(k, m, 0) == 0x726fdb47dd0e0e31ULL);

    /* Text and signature */
    char text[SCORE_CODE_MAX];
    score_code_text(text, sizeof(text), "SNAKE", "42", 0x00C0FFEE, "Miaou");
    CHECK(! strncmp(text, "HIP26:SNAKE:42:00C0FFEE:Miaou:", 30));
    CHECK_EQ(strlen(text), 30 + 16);
    uint8_t key[16];
    unmask(key);
    char expected[24];
    uint64_t sig = score_code_siphash(key, (const uint8_t *)text, 29);
    snprintf(expected, sizeof(expected), "%08lX%08lX", (unsigned long)(sig >> 32), (unsigned long)(sig & 0xFFFFFFFF));
    CHECK_STR(text + 30, expected);
    /* Another score, another signature */
    char other[SCORE_CODE_MAX];
    score_code_text(other, sizeof(other), "SNAKE", "43", 0x00C0FFEE, "Miaou");
    CHECK(strcmp(text + 30, other + 30) != 0);
    /* Too long for the buffer: empty */
    char small[20];
    score_code_text(small, sizeof(small), "SNAKE", "42", 1, "Miaou");
    CHECK_STR(small, "");

    /* QR code: 4 pixels per module fit, black modules drawn */
    static uint8_t fb[GFX_FB_SIZE];
    gfx_clear(fb, GFX_WHITE);
    int px = score_code_draw(NULL, text, 0, 4);
    CHECK(px >= 21 * 4 && px <= 37 * 4);
    CHECK_EQ(score_code_draw(fb, text, 6, 4), px);
    CHECK(count_black(fb) > px * px / 4);
    CHECK_EQ(fb[0], 0xFF);  /* Quiet zone at the top left */

    TEST_END();
}
