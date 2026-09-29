/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <string.h>

#include "gfx.h"


/* Size of the frame buffers, see gfx_set_size() */
static int width = GFX_WIDTH;
static int height = GFX_HEIGHT;


void gfx_set_size(int w, int h) {
    width = w;
    height = h;
}


void gfx_clear(uint8_t *fb, gfx_color_t color) {
    size_t size = (size_t)(width/8) * height;
    if (color == GFX_INVERT) {
        for (size_t i = 0; i < size; ++i)
            fb[i] = ~fb[i];
    } else {
        memset(fb, color == GFX_WHITE ? 0xFF : 0x00, size);
    }
}


void gfx_pixel(uint8_t *fb, int x, int y, gfx_color_t color) {
    if (x < 0 || y < 0 || x >= width || y >= height)
        return;
    uint8_t *byte = &fb[y*(width/8) + x/8];
    uint8_t mask = 0x80 >> (x % 8);
    if (color == GFX_WHITE)
        *byte |= mask;
    else if (color == GFX_BLACK)
        *byte &= ~mask;
    else
        *byte ^= mask;
}


void gfx_fill_rect(uint8_t *fb, int x, int y, int w, int h, gfx_color_t color) {
    for (int j = y; j < y+h; ++j)
        for (int i = x; i < x+w; ++i)
            gfx_pixel(fb, i, j, color);
}


void gfx_rect(uint8_t *fb, int x, int y, int w, int h, gfx_color_t color) {
    gfx_fill_rect(fb, x, y, w, 1, color);
    gfx_fill_rect(fb, x, y+h-1, w, 1, color);
    gfx_fill_rect(fb, x, y+1, 1, h-2, color);
    gfx_fill_rect(fb, x+w-1, y+1, 1, h-2, color);
}


/* Decodes the next UTF-8 code point (up to 3 bytes, enough for our fonts), advances *s */
static uint16_t next_codepoint(const char **s) {
    const uint8_t *p = (const uint8_t *)*s;
    uint16_t cp;
    if (p[0] < 0x80) {
        cp = p[0];
        *s += 1;
    } else if ((p[0] & 0xE0) == 0xC0 && p[1]) {
        cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
        *s += 2;
    } else if ((p[0] & 0xF0) == 0xE0 && p[1] && p[2]) {
        cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        *s += 3;
    } else {
        cp = '?';
        *s += 1;
    }
    return cp;
}

/* Letter without its accent of U+00C0..U+017F (Latin-1 Supplement and Latin Extended-A), for the letters missing
 * in the fonts (generated with unicodedata: first letter of the NFD decomposition) */
static const char UNACCENTED[] =
    "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuytyAaAaAaCcCcCcCcDdDdEeEeEeEeEeGgGgGgGgHhHhIiIiIiIiIiIiJjKkk"
    "LlLlLlLlLlNnNnNnnNnOoOoOoOoRrRrRrSsSsSsSsTtTtTtUuUuUuUuUuUuWwYyYZzZzZzs";

static const gfx_glyph_t *find_glyph(const gfx_font_t *font, uint16_t cp) {
    for (uint16_t i = 0; i < font->n_glyphs; ++i)
        if (font->glyphs[i].codepoint == cp)
            return &font->glyphs[i];
    /* Accented letter not in the font: the letter without its accent */
    if (cp >= 0xC0 && cp < 0xC0 + sizeof(UNACCENTED) - 1)
        return find_glyph(font, (uint8_t)UNACCENTED[cp - 0xC0]);
    /* Unknown character: use '?' */
    return cp == '?' ? NULL : find_glyph(font, '?');
}


int gfx_text_width(const gfx_font_t *font, const char *utf8) {
    int w16 = 0;  /* In 1/16 pixels */
    while (*utf8) {
        const gfx_glyph_t *g = find_glyph(font, next_codepoint(&utf8));
        if (g)
            w16 += g->advance;
    }
    return (w16 + 8) / 16;
}


int gfx_text(uint8_t *fb, int x, int y, const gfx_font_t *font, const char *utf8, gfx_color_t color, gfx_align_t align) {
    if (align == GFX_ALIGN_CENTER)
        x -= gfx_text_width(font, utf8) / 2;
    else if (align == GFX_ALIGN_RIGHT)
        x -= gfx_text_width(font, utf8);

    int x16 = x * 16;  /* Pen position in 1/16 pixels */
    while (*utf8) {
        const gfx_glyph_t *g = find_glyph(font, next_codepoint(&utf8));
        if (! g)
            continue;
        const uint8_t *row = font->bitmap + g->offset;
        int w8 = (g->width + 7) / 8;
        int gx = (x16 + 8) / 16;
        for (int j = 0; j < font->height; ++j, row += w8)
            for (int i = 0; i < g->width; ++i)
                if (row[i/8] & (0x80 >> (i % 8)))
                    gfx_pixel(fb, gx+i, y+j, color);
        x16 += g->advance;
    }
    return (x16 + 8) / 16;
}
