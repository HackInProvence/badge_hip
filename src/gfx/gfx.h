/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file gfx.h
 *
 * \brief Graphics API: draw text and rectangles in a 1 bit frame buffer that can be pushed to the screen.
 *
 * The frame buffer has the same format as the images of image2epaper.py (SCREEN_WIDTH*SCREEN_HEIGHT/8 bytes,
 * 8 pixels per byte, bit 7 is the leftmost pixel, 1 = white), so it can be given to screen_show_image_bw()
 * or screen_push_rams().
 *
 * Text is UTF-8, only ASCII and the Latin-1 letters used in French are available (see gen_fonts.py).
 * */

#ifndef _GFX_H
#define _GFX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GFX_WIDTH 200
#define GFX_HEIGHT 200
#define GFX_FB_SIZE ((GFX_WIDTH*GFX_HEIGHT)/8)

typedef enum {
    GFX_BLACK = 0,
    GFX_WHITE = 1,
    GFX_INVERT = 2,
} gfx_color_t;

typedef enum {
    GFX_ALIGN_LEFT,
    GFX_ALIGN_CENTER,
    GFX_ALIGN_RIGHT,
} gfx_align_t;

typedef struct {
    uint16_t codepoint;
    uint16_t advance;  /* Horizontal distance to the next glyph, in 1/16 pixels (keeps the spacing regular) */
    uint8_t width;  /* Width of the bitmap, rows are packed on whole bytes */
    uint16_t offset;  /* In the bitmap of the font */
} gfx_glyph_t;

typedef struct {
    uint8_t height;  /* Line height, all glyph bitmaps have this height */
    uint16_t n_glyphs;
    const gfx_glyph_t *glyphs;
    const uint8_t *bitmap;
} gfx_font_t;

extern const gfx_font_t gfx_font_small;  /* 14px */
extern const gfx_font_t gfx_font_medium;  /* 18px */
extern const gfx_font_t gfx_font_large;  /* 26px */

/** \brief Size of the next frame buffers drawn (default GFX_WIDTH x GFX_HEIGHT, the e-Paper screen),
 * e.g. 128x64 for the OLED screen. The width must be a multiple of 8. */
void gfx_set_size(int width, int height);

/** \brief Fill the whole frame buffer. */
void gfx_clear(uint8_t *fb, gfx_color_t color);

/** \brief Set a pixel, silently ignores pixels out of the screen. */
void gfx_pixel(uint8_t *fb, int x, int y, gfx_color_t color);

/** \brief Fill a rectangle. */
void gfx_fill_rect(uint8_t *fb, int x, int y, int w, int h, gfx_color_t color);

/** \brief Draw the outline of a rectangle. */
void gfx_rect(uint8_t *fb, int x, int y, int w, int h, gfx_color_t color);

/** \brief Width of the text in pixels. */
int gfx_text_width(const gfx_font_t *font, const char *utf8);

/** \brief Draw text, (x, y) is the top of the line (at the left, center or right of the text depending on \p align).
 *
 * Only the pixels of the glyphs are drawn (the background is kept).
 * \return the x coordinate after the text */
int gfx_text(uint8_t *fb, int x, int y, const gfx_font_t *font, const char *utf8, gfx_color_t color, gfx_align_t align);

#endif /* _GFX_H */
