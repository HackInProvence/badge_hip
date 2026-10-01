/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Name tag: the name of the cicada in big and the type of the badge (set in the admin menu),
 * made to stay on the screen (the e-Paper keeps it without power) */

#include <stdio.h>
#include <string.h>

#include "app.h"
#include "skills.h"
#include "social.h"
#include "store.h"

static const char *TYPES[] = {"PARTICIPANT", "ORATEUR", "STAFF"};

const char *nametag_type(void) {
    uint8_t t = store_get()->badge_type;
    return t < sizeof(TYPES) / sizeof(TYPES[0]) ? TYPES[t] : TYPES[0];
}

/* The rows really inked by \p text at y = 0 (the fonts have room above and below the letters) */
static void ink_rows(const gfx_font_t *font, const char *text, int *top, int *bottom) {
    static uint8_t scratch[GFX_FB_SIZE];
    gfx_clear(scratch, GFX_WHITE);
    gfx_text(scratch, GFX_WIDTH/2, 0, font, text, GFX_BLACK, GFX_ALIGN_CENTER);
    *top = -1;
    *bottom = font->height - 1;
    for (int y = 0; y < font->height && y < GFX_HEIGHT; ++y) {
        bool ink = false;
        for (int i = 0; i < GFX_WIDTH / 8 && ! ink; ++i)
            ink = scratch[y * (GFX_WIDTH / 8) + i] != 0xFF;
        if (ink) {
            if (*top < 0)
                *top = y;
            *bottom = y;
        }
    }
    if (*top < 0)
        *top = 0;
}

static void nametag_render(uint8_t *fb);

static void nametag_start(absolute_time_t now) {
    (void)now;
    printf("nametag: %s, %s\n", social_name(), nametag_type());
    /* Shown like the screensaver: full waveform of the screen (no ghost), stays without power, any button leaves */
    app_show_still(nametag_render);
}

static bool nametag_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    return ! b->pressed;
}

static void nametag_page(uint8_t *fb, absolute_time_t now) {
    (void)now;
    nametag_render(fb);
}

static void nametag_render(uint8_t *fb) {
    /* The event at the top, the name in the biggest font that fits, the type in a black band */
    gfx_text(fb, GFX_WIDTH/2, 8, &gfx_font_medium, "SecSea 2026", GFX_BLACK, GFX_ALIGN_CENTER);
    gfx_text(fb, GFX_WIDTH/2, 30, &gfx_font_small, "Hack In Provence - La Ciotat", GFX_BLACK, GFX_ALIGN_CENTER);
    gfx_fill_rect(fb, 10, 52, GFX_WIDTH - 20, 2, GFX_BLACK);
    const char *name = social_name();
    const gfx_font_t *font = gfx_text_width(&gfx_font_large, name) <= GFX_WIDTH - 8 ? &gfx_font_large : &gfx_font_medium;
    char fitted[32];
    ui_fit(font, fitted, sizeof(fitted), name, GFX_WIDTH - 8);
    gfx_text(fb, GFX_WIDTH/2, 78, font, fitted, GFX_BLACK, GFX_ALIGN_CENTER);
    /* The type centered (its ink, not its font box) in the band */
    const int band_y = 132, band_h = 40;
    gfx_fill_rect(fb, 0, band_y, GFX_WIDTH, band_h, GFX_BLACK);
    int top, bottom;
    ink_rows(&gfx_font_large, nametag_type(), &top, &bottom);
    int y = band_y + (band_h - (bottom - top + 1)) / 2 - top;
    gfx_text(fb, GFX_WIDTH/2, y, &gfx_font_large, nametag_type(), GFX_WHITE, GFX_ALIGN_CENTER);
    /* The pictograms of the skills (Social > Compétences) under the band */
    skills_draw_row(fb, GFX_WIDTH/2, band_y + band_h + 6, store_get()->skills, 10, GFX_BLACK);
}

const app_t app_nametag = {
    .name = "Badge nominatif",
    .start = nametag_start,
    .buttons = nametag_buttons,
    .render = nametag_page,
    .no_saver = true,  /* It is made to stay */
};
