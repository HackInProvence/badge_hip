/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Assassin: to be written. */

#include "app.h"

static void stub_start(absolute_time_t now) {
    (void)now;
}

static bool stub_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    return ! (b->pressed & UI_BTN_A);
}

static void stub_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    ui_title(fb, "Assassin");
    ui_lines(fb, 80, &gfx_font_small, "Bientôt");
    ui_footer(fb, "G : retour");
}

const app_t app_assassin = {
    .name = "Assassin",
    .start = stub_start,
    .buttons = stub_buttons,
    .render = stub_render,
};
