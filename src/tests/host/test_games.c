/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Host tests of the mini games (menu/games.c): rules, the players of the cicada, records */

#include "pico_host.h"
#include "../../menu/games.c"
#include "test.h"

uint64_t host_time_us = 0;
bool host_gpio[32];
uint8_t host_spi_log[65536];
size_t host_spi_len = 0;

static uint32_t next_random = 0;
static uint32_t fake_random(void) { return next_random; }
static int tones = 0, saves = 0;
static uint8_t led_g = 0;
static void fake_tone(uint16_t hz, uint16_t ms) { (void)ms; tones += hz != 0; }
static void fake_leds(uint8_t r, uint8_t g, uint8_t b) { (void)r; (void)b; led_g = g; }
static void fake_saved(void) { ++saves; }
static const char *fake_name(void) { return "Cig 33EC"; }
static uint32_t fake_id(void) { return 0x1A2B3C4D; }
static const games_hooks_t HOOKS = {fake_tone, fake_leds, fake_random, fake_saved, fake_name, fake_id};
static uint16_t recs[GAME_COUNT] = {GAMES_NO_RECORD, GAMES_NO_RECORD, 0xFFFF, 0, 0xFFFF};

/* Runs the game for ms milliseconds, 1 ms per call like a busy main loop */
static void run_ms(int ms) {
    for (int i = 0; i < ms; ++i) {
        host_time_us += 1000;
        games_task(host_time_us);
    }
}

static void press(uint8_t b) {
    games_buttons(b, host_time_us);
}

/* The perfect tic tac toe player never loses, whatever the player does: all the games are tried */
static int ttt_losses = 0, ttt_games = 0;
static void ttt_explore(uint8_t *b) {
    for (int i = 0; i < 9; ++i) {
        if (b[i])
            continue;
        b[i] = 1;
        int w = ttt_winner(b, NULL);
        if (w) {
            ++ttt_games;
            ttt_losses += w == 1;
        } else {
            int m = ttt_best_move(b, 2);
            b[m] = 2;
            w = ttt_winner(b, NULL);
            if (w) {
                ++ttt_games;
                ttt_losses += w == 1;
            } else {
                ttt_explore(b);
            }
            b[m] = 0;
        }
        b[i] = 0;
    }
}

int main(void) {
    games_init(&HOOKS, recs);
    CHECK_EQ(recs[GAME_REFLEX], GAMES_NO_RECORD);  /* 0 (new store) is not a record */
    CHECK_STR(games_name(GAME_CONNECT4), "Puissance 4");

    /* ---- Morpion ---- */
    uint8_t b[9] = {0};
    ttt_explore(b);
    CHECK(ttt_games > 100);
    CHECK_EQ(ttt_losses, 0);
    /* The cicada starts too (any opening) */
    for (next_random = 0; next_random < 5; ++next_random) {
        memset(b, 0, 9);
        b[ttt_best_move(b, 2)] = 2;
        ttt_explore(b);
    }
    CHECK_EQ(ttt_losses, 0);
    /* Takes a win, blocks a line */
    uint8_t win[9] = {2, 2, 0, 1, 1, 0, 0, 0, 0};
    CHECK_EQ(ttt_best_move(win, 2), 2);
    uint8_t block[9] = {1, 1, 0, 0, 2, 0, 0, 0, 0};
    CHECK_EQ(ttt_best_move(block, 2), 2);
    /* A round through the buttons: the player plays, the cicada answers after a while */
    next_random = 1;  /* No mistake */
    games_start(GAME_TICTACTOE, host_time_us);
    CHECK_EQ(ttt_cursor, 4);
    press(GAMES_BTN_B);
    CHECK_EQ(ttt[4], 1);
    CHECK(ttt_badge_turn);
    press(GAMES_BTN_B);  /* Ignored while the cicada thinks */
    run_ms(TTT_THINK_MS + 5);
    CHECK(! ttt_badge_turn);
    int circles = 0;
    for (int i = 0; i < 9; ++i)
        circles += ttt[i] == 2;
    CHECK_EQ(circles, 1);
    CHECK(ttt[ttt_cursor] == 0);  /* The cursor is on a free cell */
    CHECK(games_buttons(GAMES_BTN_A, host_time_us) == false);  /* Left wing: quit */

    /* ---- Puissance 4 ---- */
    games_start(GAME_CONNECT4, host_time_us);
    /* The cicada wins when it can: 3 rings in column 0 */
    memset(c4, 0, sizeof(c4));
    c4[0][0] = c4[1][0] = c4[2][0] = 2;
    c4[0][1] = c4[0][2] = c4[0][3] = 1;  /* ...rather than blocking */
    for (int c = 0; c < C4_COLS; ++c)
        c4_think_column(c);
    CHECK_EQ(c4_choose(), 0);
    /* It blocks the player: 3 discs in a row on the bottom line, only one free end */
    memset(c4, 0, sizeof(c4));
    c4[0][0] = c4[0][1] = c4[0][2] = 1;
    c4[1][1] = 2;
    for (int c = 0; c < C4_COLS; ++c)
        c4_think_column(c);
    CHECK_EQ(c4_choose(), 3);
    /* It sees a double threat coming: open two with both ends free */
    memset(c4, 0, sizeof(c4));
    c4[0][2] = c4[0][3] = 1;
    c4[1][3] = 2;
    for (int c = 0; c < C4_COLS; ++c)
        c4_think_column(c);
    int prevent = c4_choose();
    CHECK(prevent == 1 || prevent == 4);
    /* Diagonal detection */
    memset(c4, 0, sizeof(c4));
    c4[0][0] = c4[1][1] = c4[2][2] = c4[3][3] = 1;
    CHECK(c4_wins_at(2, 2, true));
    CHECK_EQ(c4_win[0][0], 0);
    CHECK_EQ(c4_win[3][1], 3);
    /* Through the buttons: drop, then the cicada answers one column per call */
    games_start(GAME_CONNECT4, host_time_us);
    press(GAMES_BTN_X);
    press(GAMES_BTN_B);
    CHECK_EQ(c4[0][4], 1);
    CHECK(c4_badge_turn);
    run_ms(400 + C4_COLS + 2);
    CHECK(! c4_badge_turn);
    int rings = 0;
    for (int r = 0; r < C4_ROWS; ++r)
        for (int c = 0; c < C4_COLS; ++c)
            rings += c4[r][c] == 2;
    CHECK_EQ(rings, 1);

    /* ---- Simon ---- */
    next_random = 1;  /* Always the pad 1 (right flank, top right like on the badge) */
    games_start(GAME_SIMON, host_time_us);
    CHECK(! games_calm());
    run_ms(1500 + 300 + 600 + 250);
    CHECK_EQ(simon_phase, SIMON_INPUT);
    CHECK_EQ(simon_len, 1);
    press(GAMES_BTN_X);
    CHECK_EQ(simon_phase, SIMON_SHOW);
    CHECK_EQ(simon_len, 2);
    run_ms(1000 + 2 * (600 + 200) + 100);
    CHECK_EQ(simon_phase, SIMON_INPUT);
    press(GAMES_BTN_X);
    CHECK(games_buttons(GAMES_BTN_A, host_time_us));  /* Wrong pad (the left wing is a pad): game over, not quit */
    CHECK_EQ(simon_phase, SIMON_OVER);
    CHECK_EQ(recs[GAME_SIMON], 1);
    CHECK(games_calm());
    /* End of the game: a flank shows the signed score, any button closes it */
    press(GAMES_BTN_Y);
    CHECK(qr_shown);
    CHECK(! strncmp(qr_text, "HIP26:SIMON:1:1A2B3C4D:Cig 33EC:", 32));
    CHECK_EQ(strlen(qr_text), 32 + 16);
    static uint8_t qr_fb[GFX_FB_SIZE];
    games_render(qr_fb);
    CHECK(games_buttons(GAMES_BTN_A, host_time_us));  /* Closes the QR code, doesn't quit */
    CHECK(! qr_shown);
    CHECK(games_buttons(GAMES_BTN_A, host_time_us) == false);
    /* Too slow */
    games_start(GAME_SIMON, host_time_us);
    run_ms(1500 + 300 + 600 + 250 + SIMON_INPUT_TIMEOUT_MS + 10);
    CHECK_EQ(simon_phase, SIMON_OVER);
    CHECK_EQ(recs[GAME_SIMON], 1);  /* 0 is not better */

    /* ---- Réflexes ---- */
    next_random = 0;
    games_start(GAME_REFLEX, host_time_us);
    press(GAMES_BTN_B);
    CHECK_EQ(reflex_phase, REFLEX_WAIT);
    press(GAMES_BTN_X);  /* Too early */
    CHECK_EQ(reflex_phase, REFLEX_MISSED);
    run_ms(1500 + 1);
    for (int round = 0; round < REFLEX_ROUNDS; ++round) {
        run_ms(1500 + 1);
        CHECK_EQ(reflex_phase, REFLEX_GO);
        CHECK_EQ(led_g, 255);
        run_ms(200);
        press(GAMES_BTN_B);
        run_ms(1500 + 1);
    }
    CHECK_EQ(reflex_phase, REFLEX_DONE);
    CHECK(reflex_average >= 178 && reflex_average <= 182);  /* 200 ms minus the debounce */
    CHECK_EQ(recs[GAME_REFLEX], reflex_average);
    press(GAMES_BTN_X);
    CHECK(qr_shown && strstr(qr_text, ":REFLEX:") && strstr(qr_text, "ms:"));
    press(GAMES_BTN_B);  /* Closes */
    CHECK_EQ(reflex_phase, REFLEX_DONE);
    press(GAMES_BTN_B);  /* Restarts */
    CHECK_EQ(reflex_phase, REFLEX_WAIT);

    /* ---- Snake ---- */
    games_start(GAME_SNAKE, host_time_us);
    CHECK_EQ(snake_len, 3);
    CHECK(snake_food >= 0 && ! snake_on(snake_food, snake_len));
    snake_food = (SNAKE_ROWS/2) * SNAKE_COLS + 6;  /* Just in front of the head */
    press(GAMES_BTN_B);
    run_ms(SNAKE_START_MS + 1);
    CHECK_EQ(snake_len, 4);
    CHECK_EQ(snake_score, 1);
    press(GAMES_BTN_Y);  /* Turn left: up */
    run_ms(snake_step_ms() + 1);
    CHECK_EQ(snake_dir, 3);
    CHECK_EQ(snake_cell(0), (SNAKE_ROWS/2 - 1) * SNAKE_COLS + 6);
    /* Straight into the top wall */
    run_ms(SNAKE_START_MS * SNAKE_ROWS);
    CHECK_EQ(snake_phase, SNAKE_OVER);
    CHECK_EQ(recs[GAME_SNAKE], 1);
    /* Biting its tail: 3 turns right in a row with a long snake */
    games_start(GAME_SNAKE, host_time_us);
    snake_len = 5;
    snake_head = 4;
    for (int i = 0; i < 5; ++i)
        snake_body[i] = 7 * SNAKE_COLS + 3 + i;
    snake_phase = SNAKE_RUN;
    for (int t = 0; t < 3; ++t) {
        snake_turns[0] = 1;
        snake_n_turns = 1;
        snake_step();
    }
    CHECK_EQ(snake_phase, SNAKE_OVER);

    /* The pages can be drawn in every state */
    static uint8_t fb[GFX_FB_SIZE];
    for (int g = 0; g < GAME_COUNT; ++g) {
        games_start(g, host_time_us);
        games_render(fb);
    }
    CHECK(tones > 0);
    CHECK(saves >= 3);

    TEST_END();
}
