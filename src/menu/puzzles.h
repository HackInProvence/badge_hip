/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file puzzles.h
 *
 * \brief Turn based puzzle games, played with the 4 buttons on the slow e-Paper screen:
 * - Démineur (minesweeper): 9x9, 12 mines, the first cell opened is never a mine,
 * - 2048: slide the tiles, two equal tiles merge,
 * - Taquin (15-puzzle): put the tiles back in order,
 * - Sokoban: push the boxes on the targets (6 levels, undo),
 * - Mastermind: find the code of 4 symbols in 10 tries,
 * - Pendu (hangman): find the French word letter by letter.
 *
 * The badge seen from the front has its flanks at the top and its wings at the bottom. In every game the short presses
 * move: left flank = left, right flank = right, left wing = up, right wing = down. The long presses act:
 * right wing = main action (open, validate...), left flank = secondary action (flag, undo), right flank = the help page,
 * left wing = quit. Each game starts on its help page (rules, controls and record).
 *
 * Like games.h, the games only draw in a frame buffer and use the hooks of games.h for the sound, the LEDs and the
 * random numbers, so that they can be tested on a PC (tests/host/test_puzzles.c). Nothing blocks.
 * */

#ifndef _PUZZLES_H
#define _PUZZLES_H

#include <stdbool.h>
#include <stdint.h>

#include "pico/time.h"

#include "games.h"

typedef enum {
    PUZZLE_MINES,
    PUZZLE_2048,
    PUZZLE_TAQUIN,
    PUZZLE_SOKOBAN,
    PUZZLE_MASTERMIND,
    PUZZLE_PENDU,
    PUZZLE_COUNT,
} puzzle_t;

/* Persistent records (index: puzzle_t), GAMES_NO_RECORD when none:
 * - Démineur: best time (s), Taquin: fewest moves, Mastermind: fewest tries (lower is better),
 * - 2048: best score (capped at 0xFFFE), Sokoban: number of levels solved, Pendu: most words found in a row. */

/** \brief Set the hooks and the records (PUZZLE_COUNT values, kept up to date by the games, 0 is read as no record). */
void puzzles_init(const games_hooks_t *hooks, uint16_t *records);

/** \brief Name of the game, for the menu. */
const char *puzzles_name(puzzle_t p);

/** \brief Record of the game as a text (e.g. "Record : 83 s"). */
void puzzles_record_text(puzzle_t p, char *buf, int len);

/** \brief Start a new game, on its help page. */
void puzzles_start(puzzle_t p, absolute_time_t now);

/** \brief Buttons since the last call: short presses and long presses (>= 0.8 s, not reported as short).
 * Returns false when the player leaves the game. */
bool puzzles_buttons(uint8_t pressed, uint8_t long_pressed, absolute_time_t now);

/** \brief Run the game, to call in the main loop. Returns true when the screen must be redrawn (e.g. the timer). */
bool puzzles_task(absolute_time_t now);

/** \brief Draw the whole page in the frame buffer (GFX_WIDTH x GFX_HEIGHT). */
void puzzles_render(uint8_t *fb);

/** \brief Whether the game is waiting for the player: the screen can do a slow full refresh. */
bool puzzles_calm(void);

#endif /* _PUZZLES_H */
