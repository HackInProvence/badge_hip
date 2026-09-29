/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Host tests of the gfx library: frame buffer format, text, sizes */

#include <string.h>

#include "gfx.h"
#include "test.h"

static int count_black(const uint8_t *fb, int size) {
    int n = 0;
    for (int i = 0; i < size; ++i)
        for (int b = 0; b < 8; ++b)
            n += ! (fb[i] & (1 << b));
    return n;
}

int main(void) {
    static uint8_t fb[GFX_FB_SIZE];

    /* Format: row major, bit 7 = leftmost pixel, 1 = white */
    gfx_clear(fb, GFX_WHITE);
    CHECK_EQ(fb[0], 0xFF);
    gfx_pixel(fb, 0, 0, GFX_BLACK);
    CHECK_EQ(fb[0], 0x7F);
    gfx_pixel(fb, 9, 1, GFX_BLACK);
    CHECK_EQ(fb[25 + 1], 0xBF);
    gfx_pixel(fb, -1, 0, GFX_BLACK);  /* Outside: ignored */
    gfx_pixel(fb, 200, 0, GFX_BLACK);
    CHECK_EQ(count_black(fb, GFX_FB_SIZE), 2);
    gfx_pixel(fb, 0, 0, GFX_INVERT);
    CHECK_EQ(fb[0], 0xFF);

    /* Rectangles are clipped */
    gfx_clear(fb, GFX_WHITE);
    gfx_fill_rect(fb, 190, 190, 20, 20, GFX_BLACK);
    CHECK_EQ(count_black(fb, GFX_FB_SIZE), 100);

    /* Text: accents are glyphs of their own (not '?'), unknown characters are '?' */
    int e = gfx_text_width(&gfx_font_small, "e"), e_acute = gfx_text_width(&gfx_font_small, "é");
    int question = gfx_text_width(&gfx_font_small, "?");
    CHECK(e > 0);
    CHECK(e_acute > 0);
    CHECK_EQ(gfx_text_width(&gfx_font_small, "\xe6\xbc\xa2"), question);  /* A CJK character */
    /* Accented letters missing in the font are drawn without their accent */
    CHECK_EQ(gfx_text_width(&gfx_font_small, "\xc5\xb7"), gfx_text_width(&gfx_font_small, "y"));  /* U+0177 */
    CHECK_EQ(gfx_text_width(&gfx_font_small, "\xc5\x92"), gfx_text_width(&gfx_font_small, "O"));  /* U+0152 */
    CHECK_EQ(gfx_text_width(&gfx_font_small, "\xc5\xa6"), gfx_text_width(&gfx_font_small, "T"));  /* U+0166 */
    CHECK_EQ(gfx_text_width(&gfx_font_small, "\xc5\xbf"), gfx_text_width(&gfx_font_small, "s"));  /* U+017F, last */
    CHECK(gfx_text_width(&gfx_font_small, "Badge SecSea") < gfx_text_width(&gfx_font_large, "Badge SecSea"));
    CHECK_EQ(gfx_text_width(&gfx_font_small, ""), 0);

    /* Text is drawn, and only its pixels (the background is kept) */
    gfx_clear(fb, GFX_WHITE);
    int end = gfx_text(fb, 10, 10, &gfx_font_medium, "Cigale", GFX_BLACK, GFX_ALIGN_LEFT);
    CHECK_EQ(end, 10 + gfx_text_width(&gfx_font_medium, "Cigale"));
    int ink = count_black(fb, GFX_FB_SIZE);
    CHECK(ink > 50);
    gfx_text(fb, 10, 10, &gfx_font_medium, "Cigale", GFX_WHITE, GFX_ALIGN_LEFT);
    CHECK_EQ(count_black(fb, GFX_FB_SIZE), 0);

    /* Centered and right aligned */
    int w = gfx_text_width(&gfx_font_small, "OK");
    CHECK_EQ(gfx_text(fb, 100, 0, &gfx_font_small, "OK", GFX_BLACK, GFX_ALIGN_CENTER), 100 - w / 2 + w);
    CHECK_EQ(gfx_text(fb, 100, 0, &gfx_font_small, "OK", GFX_BLACK, GFX_ALIGN_RIGHT), 100);

    /* Other sizes (OLED 128x64) */
    static uint8_t oled[128 * 64 / 8];
    gfx_set_size(128, 64);
    gfx_clear(oled, GFX_BLACK);
    gfx_pixel(oled, 127, 63, GFX_WHITE);
    CHECK_EQ(oled[sizeof(oled) - 1], 0x01);
    gfx_pixel(oled, 128, 0, GFX_WHITE);  /* Outside of 128x64 */
    CHECK_EQ(oled[0], 0x00);
    gfx_set_size(GFX_WIDTH, GFX_HEIGHT);

    TEST_END();
}
