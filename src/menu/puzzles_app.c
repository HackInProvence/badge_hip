/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The puzzles (puzzles.c) as applications of the menus: one application per puzzle */

#include "app.h"
#include "puzzles.h"

static bool pz_buttons(const app_buttons_t *b, absolute_time_t now) {
    /* The puzzles want the short presses on release (a held button is a long press, reported alone) */
    return puzzles_buttons(b->released_short, b->long_pressed, now);
}

static bool pz_task(absolute_time_t now) {
    return puzzles_task(now);
}

static void pz_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    puzzles_render(fb);
}

static bool pz_calm(void) {
    return puzzles_calm();
}

#define PUZZLE_APP(id, var, title) \
    static void var##_start(absolute_time_t now) { puzzles_start(id, now); } \
    const app_t var = {.name = title, .start = var##_start, .buttons = pz_buttons, \
                       .task = pz_task, .render = pz_render, .calm = pz_calm};

PUZZLE_APP(PUZZLE_MINES, app_mines, "Démineur")
PUZZLE_APP(PUZZLE_2048, app_2048, "2048")
PUZZLE_APP(PUZZLE_TAQUIN, app_taquin, "Taquin")
PUZZLE_APP(PUZZLE_SOKOBAN, app_sokoban, "Sokoban")
PUZZLE_APP(PUZZLE_MASTERMIND, app_mastermind, "Mastermind")
PUZZLE_APP(PUZZLE_PENDU, app_pendu, "Pendu")
