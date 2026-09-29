/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file games.h
 *
 * \brief Mini games of the "Jeux" menu, played with the 4 buttons on the e-Paper screen:
 * - Morpion (tic tac toe) and Puissance 4 (connect four) against the cicada,
 * - Simon: repeat the sequence of sounds and lights, one pad per button,
 * - Réflexes: press as soon as the LEDs light up (the screen is too slow for that, the LEDs and the sound are instant),
 * - Snake: the flanks turn left or right.
 * At the end of a game, the flanks show the score as a signed QR code (score_code.h).
 *
 * The games only draw in a frame buffer (gfx.h): the sound, the LEDs and the random numbers are given by the caller
 * (hooks), so that they can be tested on a PC (tests/host/test_games.c).
 * Like the rest of the menu application, nothing blocks: the connect four player thinks one column per call.
 * */

#ifndef _GAMES_H
#define _GAMES_H

#include <stdbool.h>
#include <stdint.h>

#include "pico/time.h"

/* Buttons, same bits as btns_get_state() */
#define GAMES_BTN_A 0x01  /* Left wing: quit (an input in Simon) */
#define GAMES_BTN_B 0x02  /* Right wing: play, validate */
#define GAMES_BTN_X 0x04  /* Right flank: next, turn right */
#define GAMES_BTN_Y 0x08  /* Left flank: previous, turn left */

/* The buttons are seen after their debounce: removed from the reaction times */
#define GAMES_INPUT_LAG_MS 20

typedef enum {
    GAME_TICTACTOE,
    GAME_CONNECT4,
    GAME_SIMON,
    GAME_REFLEX,
    GAME_SNAKE,
    GAME_COUNT,
} game_t;

/* Persistent records (index: game_t): best Simon and Snake scores, best average reaction time (ms).
 * 0xFFFF = no record yet. */
#define GAMES_NO_RECORD 0xFFFF

typedef struct {
    void (*tone)(uint16_t hz, uint16_t ms);  /* Plays a tone (replaces the previous one), 0 Hz stops */
    void (*leds)(uint8_t r, uint8_t g, uint8_t b);  /* All the LEDs of this color, black = off */
    uint32_t (*random)(void);
    void (*records_changed)(void);  /* To save them */
    const char *(*player_name)(void);  /* Name of the cicada, in the score QR codes */
    uint32_t (*badge_id)(void);  /* Id of the badge, in the score QR codes */
} games_hooks_t;

/** \brief Set the hooks and the records (GAME_COUNT values, kept up to date by the games). */
void games_init(const games_hooks_t *hooks, uint16_t *records);

/** \brief Name of the game, for the menu. */
const char *games_name(game_t game);

/** \brief Record of the game as a text (e.g. "record 12"), empty when the game has none. */
void games_record_text(game_t game, char *buf, int len);

/** \brief Start a new game (the scores of the previous rounds are reset). */
void games_start(game_t game, absolute_time_t now);

/** \brief Buttons pressed since the last call. Returns false when the player leaves the game. */
bool games_buttons(uint8_t pressed, absolute_time_t now);

/** \brief Run the game, to call in the main loop. Returns true when the screen must be redrawn. */
bool games_task(absolute_time_t now);

/** \brief Draw the game in the frame buffer (GFX_WIDTH x GFX_HEIGHT). */
void games_render(uint8_t *fb);

/** \brief Whether the game is waiting (between rounds): the screen can do a slow full refresh without disturbing it. */
bool games_calm(void);

#endif /* _GAMES_H */
