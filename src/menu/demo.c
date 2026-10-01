/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Admin > Mode démo: explains, then starts the demo of main.c (the features one after the other, in a loop). */

#include "app.h"

void demo_start(void);  /* main.c */

static void demo_page_start(absolute_time_t now) {
    (void)now;
}

static bool demo_page_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (b->pressed & UI_BTN_B) {
        demo_start();
        return false;  /* The demo takes the screen */
    }
    return ! (b->pressed & UI_BTN_A);
}

static void demo_page_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    ui_title(fb, "Mode démo");
    ui_lines(fb, UI_TITLE_H + 14, &gfx_font_small,
             "Le badge présente ses\nfonctions en boucle :\nbadge nominatif, images,\nvidéo, musique, jeux...\n\n"
             "Pour un stand.\nUn bouton l'arrête.");
    ui_footer(fb, "D : lancer  G : retour");
}

const app_t app_demo = {
    .name = "Mode démo",
    .start = demo_page_start,
    .buttons = demo_page_buttons,
    .render = demo_page_render,
};
