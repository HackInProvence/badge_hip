/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Name tag: the name of the cicada in big and the type of the badge (set in the admin menu),
 * made to stay on the screen (the e-Paper keeps it without power) */

#include <stdio.h>
#include <string.h>

#include "app.h"
#include "social.h"
#include "store.h"

static const char *TYPES[] = {"PARTICIPANT", "ORATEUR", "STAFF"};

const char *nametag_type(void) {
    uint8_t t = store_get()->badge_type;
    return t < sizeof(TYPES) / sizeof(TYPES[0]) ? TYPES[t] : TYPES[0];
}

static void nametag_start(absolute_time_t now) {
    (void)now;
    printf("nametag: %s, %s\n", social_name(), nametag_type());
}

static bool nametag_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    return ! (b->pressed & UI_BTN_A);
}

static void nametag_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    /* The event at the top, the name in the biggest font that fits, the type in a black band */
    gfx_text(fb, GFX_WIDTH/2, 8, &gfx_font_medium, "SecSea 2026", GFX_BLACK, GFX_ALIGN_CENTER);
    gfx_text(fb, GFX_WIDTH/2, 30, &gfx_font_small, "Hack In Provence - La Ciotat", GFX_BLACK, GFX_ALIGN_CENTER);
    gfx_fill_rect(fb, 10, 52, GFX_WIDTH - 20, 2, GFX_BLACK);
    const char *name = social_name();
    const gfx_font_t *font = gfx_text_width(&gfx_font_large, name) <= GFX_WIDTH - 8 ? &gfx_font_large : &gfx_font_medium;
    char fitted[32];
    ui_fit(font, fitted, sizeof(fitted), name, GFX_WIDTH - 8);
    gfx_text(fb, GFX_WIDTH/2, 78, font, fitted, GFX_BLACK, GFX_ALIGN_CENTER);
    gfx_fill_rect(fb, 0, 128, GFX_WIDTH, 34, GFX_BLACK);
    gfx_text(fb, GFX_WIDTH/2, 132, &gfx_font_large, nametag_type(), GFX_WHITE, GFX_ALIGN_CENTER);
    gfx_text(fb, GFX_WIDTH/2, 176, &gfx_font_small, "G : retour", GFX_BLACK, GFX_ALIGN_CENTER);
}

const app_t app_nametag = {
    .name = "Badge nominatif",
    .start = nametag_start,
    .buttons = nametag_buttons,
    .render = nametag_render,
    .no_saver = true,  /* It is made to stay */
};
