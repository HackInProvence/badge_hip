/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdio.h>
#include <string.h>

#include "games.h"
#include "gfx.h"

#define TITLE_H 28
#define FOOTER_Y 180

static const games_hooks_t *hooks = NULL;
static uint16_t *records = NULL;
static game_t game = GAME_TICTACTOE;
static bool changed = false;  /* The screen must be redrawn */

static const char *NAMES[GAME_COUNT] = {"Morpion", "Puissance 4", "Simon", "Réflexes", "Snake"};


/* ------ Common ------ */

static uint32_t rnd(uint32_t n) {
    return n ? hooks->random() % n : 0;
}

static void tone(uint16_t hz, uint16_t ms) {
    if (hooks->tone)
        hooks->tone(hz, ms);
}

static void leds(uint8_t r, uint8_t g, uint8_t b) {
    if (hooks->leds)
        hooks->leds(r, g, b);
}

static bool reached(absolute_time_t t, absolute_time_t now) {
    return absolute_time_diff_us(t, now) >= 0;
}

/* Updates the record of the game, returns whether it is a new one */
static bool new_record(game_t g, uint16_t value, bool lower_is_better) {
    uint16_t *r = &records[g];
    if (*r != GAMES_NO_RECORD && (lower_is_better ? value >= *r : value <= *r))
        return false;
    *r = value;
    if (hooks->records_changed)
        hooks->records_changed();
    return true;
}

static void draw_title(uint8_t *fb, const char *title) {
    gfx_fill_rect(fb, 0, 0, GFX_WIDTH, TITLE_H, GFX_BLACK);
    gfx_text(fb, GFX_WIDTH/2, (TITLE_H - gfx_font_medium.height)/2, &gfx_font_medium, title, GFX_WHITE, GFX_ALIGN_CENTER);
}

static void draw_footer(uint8_t *fb, const char *text) {
    gfx_fill_rect(fb, 0, FOOTER_Y - 2, GFX_WIDTH, 1, GFX_BLACK);
    gfx_text(fb, GFX_WIDTH/2, FOOTER_Y, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
}

/* Centered lines separated by '\n', returns the y after the last one */
static int draw_lines(uint8_t *fb, int y, const gfx_font_t *font, const char *text, gfx_color_t color) {
    char line[48];
    while (*text) {
        const char *end = strchr(text, '\n');
        size_t n = end ? (size_t)(end - text) : strlen(text);
        if (n >= sizeof(line))
            n = sizeof(line) - 1;
        memcpy(line, text, n);
        line[n] = 0;
        gfx_text(fb, GFX_WIDTH/2, y, font, line, color, GFX_ALIGN_CENTER);
        y += font->height + 3;
        text += end ? (size_t)(end - text) + 1 : n;
    }
    return y;
}

static void frame(uint8_t *fb, int x, int y, int w, int h, int t, gfx_color_t color) {
    gfx_fill_rect(fb, x, y, w, t, color);
    gfx_fill_rect(fb, x, y + h - t, w, t, color);
    gfx_fill_rect(fb, x, y, t, h, color);
    gfx_fill_rect(fb, x + w - t, y, t, h, color);
}

/* A message box over the game */
static void draw_box(uint8_t *fb, const char *text) {
    int lines = 1;
    for (const char *c = text; *c; ++c)
        lines += *c == '\n';
    int h = lines * (gfx_font_medium.height + 3) + 16;
    int y = TITLE_H + (FOOTER_Y - 2 - TITLE_H - h) / 2;
    gfx_fill_rect(fb, 16, y, GFX_WIDTH - 32, h, GFX_WHITE);
    frame(fb, 16, y, GFX_WIDTH - 32, h, 3, GFX_BLACK);
    draw_lines(fb, y + 8, &gfx_font_medium, text, GFX_BLACK);
}

static void fill_circle(uint8_t *fb, int cx, int cy, int r, gfx_color_t color) {
    for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r; dx <= r; ++dx)
            if (dx*dx + dy*dy <= r*r + r)
                gfx_pixel(fb, cx + dx, cy + dy, color);
}

static void ring(uint8_t *fb, int cx, int cy, int r, int thickness) {
    fill_circle(fb, cx, cy, r, GFX_BLACK);
    fill_circle(fb, cx, cy, r - thickness, GFX_WHITE);
}

/* A line drawn with a square brush of w pixels */
static void thick_line(uint8_t *fb, int x0, int y0, int x1, int y1, int w) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    while (true) {
        gfx_fill_rect(fb, x0 - w/2, y0 - w/2, w, w, GFX_BLACK);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static const char *result_text(uint8_t result) {
    return result == 1 ? "Gagné !  D : rejouer" : result == 2 ? "Perdu...  D : rejouer" : "Match nul  D : rejouer";
}

static void end_sound(uint8_t result) {
    if (result == 1) {
        tone(1319, 400);
        leds(0, 255, 0);
    } else if (result == 2) {
        tone(330, 500);
        leds(255, 0, 0);
    } else {
        tone(659, 200);
    }
}


/* ------ Morpion (tic tac toe): the player has the crosses (1), the cicada the circles (2) ------ */

#define TTT_CELL 46
#define TTT_X0 31
#define TTT_Y0 31
#define TTT_THINK_MS 500
#define TTT_MISTAKES 5  /* The cicada plays a random cell 1 time in TTT_MISTAKES, otherwise it can't lose */

static uint8_t ttt[9];
static int ttt_cursor = 4;
static uint8_t ttt_result = 0;  /* 0: playing, 1: the player won, 2: the cicada won, 3: draw */
static int ttt_line = -1;
static bool ttt_player_starts = true;
static bool ttt_badge_turn = false;
static absolute_time_t ttt_badge_ts = 0;
static uint16_t ttt_score[3];  /* Player, cicada, draws */

static const uint8_t TTT_LINES[8][3] = {{0,1,2}, {3,4,5}, {6,7,8}, {0,3,6}, {1,4,7}, {2,5,8}, {0,4,8}, {2,4,6}};

/* 0: not finished, 1 or 2: the winner (and its line), 3: draw */
static int ttt_winner(const uint8_t *b, int *line) {
    for (int i = 0; i < 8; ++i) {
        const uint8_t *l = TTT_LINES[i];
        if (b[l[0]] && b[l[0]] == b[l[1]] && b[l[1]] == b[l[2]]) {
            if (line)
                *line = i;
            return b[l[0]];
        }
    }
    for (int i = 0; i < 9; ++i)
        if (! b[i])
            return 0;
    return 3;
}

/* Score for who is to move (> 0: who wins, the sooner the better), alpha-beta */
static int ttt_negamax(uint8_t *b, int who, int depth, int alpha, int beta) {
    for (int i = 0; i < 9 && alpha < beta; ++i) {
        if (b[i])
            continue;
        b[i] = who;
        int w = ttt_winner(b, NULL);
        int s = w == who ? 10 - depth : w == 3 ? 0 : -ttt_negamax(b, 3 - who, depth + 1, -beta, -alpha);
        b[i] = 0;
        if (s > alpha)
            alpha = s;
    }
    return alpha;
}

/* The best cell for who (a random one among the best), -1 when the board is full */
static int ttt_best_move(uint8_t *b, int who) {
    int best = -1000, moves[9], n = 0, empty = 0;
    for (int i = 0; i < 9; ++i)
        empty += ! b[i];
    if (empty == 9) {
        static const uint8_t OPENINGS[5] = {0, 2, 4, 6, 8};  /* Corners and center, as good and faster */
        return OPENINGS[rnd(5)];
    }
    for (int i = 0; i < 9; ++i) {
        if (b[i])
            continue;
        b[i] = who;
        int w = ttt_winner(b, NULL);
        int s = w == who ? 10 : w == 3 ? 0 : -ttt_negamax(b, 3 - who, 1, -100, 100);
        b[i] = 0;
        if (s > best) {
            best = s;
            n = 0;
        }
        if (s == best)
            moves[n++] = i;
    }
    return n ? moves[rnd(n)] : -1;
}

static void ttt_move_cursor(int d) {
    int step = d ? d : 1;  /* 0: stay if free, otherwise the next free cell */
    for (int k = d ? 1 : 0; k <= 9; ++k) {
        int c = ((ttt_cursor + step*k) % 9 + 9) % 9;
        if (! ttt[c]) {
            ttt_cursor = c;
            return;
        }
    }
}

static void ttt_check_end(void) {
    ttt_result = ttt_winner(ttt, &ttt_line);
    if (ttt_result) {
        ++ttt_score[ttt_result - 1];
        printf("game: morpion %s\n", ttt_result == 1 ? "won" : ttt_result == 2 ? "lost" : "draw");
        end_sound(ttt_result);
    }
}

static void ttt_new_round(absolute_time_t now) {
    memset(ttt, 0, sizeof(ttt));
    ttt_result = 0;
    ttt_line = -1;
    ttt_cursor = 4;
    ttt_badge_turn = ! ttt_player_starts;
    ttt_badge_ts = delayed_by_ms(now, TTT_THINK_MS);
    ttt_player_starts = ! ttt_player_starts;  /* Each one starts in turn */
    leds(0, 0, 0);
}

static bool ttt_buttons(uint8_t pressed, absolute_time_t now) {
    if (ttt_result) {
        if (pressed & GAMES_BTN_B)
            ttt_new_round(now);
        return true;
    }
    if (ttt_badge_turn)
        return true;
    if (pressed & GAMES_BTN_Y)
        ttt_move_cursor(-1);
    if (pressed & GAMES_BTN_X)
        ttt_move_cursor(1);
    if ((pressed & GAMES_BTN_B) && ! ttt[ttt_cursor]) {
        ttt[ttt_cursor] = 1;
        tone(880, 60);
        ttt_check_end();
        if (! ttt_result) {
            ttt_badge_turn = true;
            ttt_badge_ts = delayed_by_ms(now, TTT_THINK_MS);
        }
    }
    return true;
}

static void ttt_task(absolute_time_t now) {
    if (ttt_result || ! ttt_badge_turn || ! reached(ttt_badge_ts, now))
        return;
    int m = -1;
    if (rnd(TTT_MISTAKES) == 0) {
        int n = 0, empty[9];
        for (int i = 0; i < 9; ++i)
            if (! ttt[i])
                empty[n++] = i;
        m = n ? empty[rnd(n)] : -1;
    } else {
        m = ttt_best_move(ttt, 2);
    }
    if (m >= 0) {
        ttt[m] = 2;
        tone(659, 60);
    }
    ttt_badge_turn = false;
    ttt_check_end();
    ttt_move_cursor(0);
    changed = true;
}

static void ttt_render(uint8_t *fb) {
    char title[32];
    snprintf(title, sizeof(title), "Morpion : %u - %u", ttt_score[0], ttt_score[1]);
    draw_title(fb, title);
    for (int i = 1; i < 3; ++i) {
        gfx_fill_rect(fb, TTT_X0 + i*TTT_CELL - 1, TTT_Y0, 3, 3*TTT_CELL, GFX_BLACK);
        gfx_fill_rect(fb, TTT_X0, TTT_Y0 + i*TTT_CELL - 1, 3*TTT_CELL, 3, GFX_BLACK);
    }
    for (int i = 0; i < 9; ++i) {
        int x = TTT_X0 + (i % 3)*TTT_CELL, y = TTT_Y0 + (i / 3)*TTT_CELL;
        if (ttt[i] == 1) {
            thick_line(fb, x + 11, y + 11, x + TTT_CELL - 12, y + TTT_CELL - 12, 4);
            thick_line(fb, x + TTT_CELL - 12, y + 11, x + 11, y + TTT_CELL - 12, 4);
        } else if (ttt[i] == 2) {
            ring(fb, x + TTT_CELL/2, y + TTT_CELL/2, 15, 4);
        } else if (i == ttt_cursor && ! ttt_result && ! ttt_badge_turn) {
            frame(fb, x + 5, y + 5, TTT_CELL - 10, TTT_CELL - 10, 2, GFX_BLACK);
        }
    }
    if (ttt_result == 1 || ttt_result == 2) {
        const uint8_t *l = TTT_LINES[ttt_line];
        thick_line(fb, TTT_X0 + (l[0] % 3)*TTT_CELL + TTT_CELL/2, TTT_Y0 + (l[0] / 3)*TTT_CELL + TTT_CELL/2,
                   TTT_X0 + (l[2] % 3)*TTT_CELL + TTT_CELL/2, TTT_Y0 + (l[2] / 3)*TTT_CELL + TTT_CELL/2, 6);
    }
    draw_footer(fb, ttt_result ? result_text(ttt_result) : ttt_badge_turn ? "La cigale réfléchit..." : "Flancs : case  D : jouer");
}


/* ------ Puissance 4 (connect four): the player has the discs (1), the cicada the rings (2) ------ */

#define C4_COLS 7
#define C4_ROWS 6
#define C4_DEPTH 5  /* Plies searched by the cicada (one root column per call of games_task()) */
#define C4_CELL 22
#define C4_X0 23
#define C4_Y0 42
#define C4_WIN 1000000

static uint8_t c4[C4_ROWS][C4_COLS];  /* Row 0 is the bottom */
static int c4_cursor = 3;
static uint8_t c4_result = 0;
static bool c4_player_starts = true;
static bool c4_badge_turn = false;
static absolute_time_t c4_badge_ts = 0;
static int c4_think_col = 0;
static int32_t c4_scores[C4_COLS];
static int8_t c4_win[4][2];  /* Row, column of the 4 aligned discs */
static uint16_t c4_score[3];

static const int8_t C4_ORDER[C4_COLS] = {3, 2, 4, 1, 5, 0, 6};  /* The center first: better pruning */
static const int8_t C4_DIRS[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};

static int c4_drop(int c, int who) {
    for (int r = 0; r < C4_ROWS; ++r)
        if (! c4[r][c]) {
            c4[r][c] = who;
            return r;
        }
    return -1;
}

static int c4_run(int r, int c, int dr, int dc) {
    int who = c4[r][c], n = 0;
    for (r += dr, c += dc; r >= 0 && r < C4_ROWS && c >= 0 && c < C4_COLS && c4[r][c] == who; r += dr, c += dc)
        ++n;
    return n;
}

/* The disc at (r, c) makes 4 in a row (and fills c4_win when \p mark) */
static bool c4_wins_at(int r, int c, bool mark) {
    for (int d = 0; d < 4; ++d) {
        int dr = C4_DIRS[d][0], dc = C4_DIRS[d][1];
        int back = c4_run(r, c, -dr, -dc);
        if (1 + back + c4_run(r, c, dr, dc) >= 4) {
            if (mark)
                for (int i = 0; i < 4; ++i) {
                    c4_win[i][0] = r + (i - back) * dr;
                    c4_win[i][1] = c + (i - back) * dc;
                }
            return true;
        }
    }
    return false;
}

static bool c4_full(void) {
    for (int c = 0; c < C4_COLS; ++c)
        if (! c4[C4_ROWS - 1][c])
            return false;
    return true;
}

/* Heuristic for the cicada: its open lines of 2 and 3 minus the player's, and the center column */
static int c4_eval_badge(void) {
    int s = 0;
    for (int r = 0; r < C4_ROWS; ++r)
        s += c4[r][3] == 2 ? 3 : c4[r][3] == 1 ? -3 : 0;
    for (int r = 0; r < C4_ROWS; ++r)
        for (int c = 0; c < C4_COLS; ++c)
            for (int d = 0; d < 4; ++d) {
                int dr = C4_DIRS[d][0], dc = C4_DIRS[d][1];
                int er = r + 3*dr, ec = c + 3*dc;
                if (er < 0 || er >= C4_ROWS || ec < 0 || ec >= C4_COLS)
                    continue;
                int n[3] = {0, 0, 0};
                for (int i = 0; i < 4; ++i)
                    ++n[c4[r + i*dr][c + i*dc]];
                if (n[1] && n[2])
                    continue;
                s += n[2] == 3 ? 5 : n[2] == 2 ? 2 : 0;
                s -= n[1] == 3 ? 5 : n[1] == 2 ? 2 : 0;
            }
    return s;
}

static int c4_negamax(int depth, int alpha, int beta, int who) {
    if (depth == 0)
        return who == 2 ? c4_eval_badge() : -c4_eval_badge();
    bool any = false;
    for (int k = 0; k < C4_COLS && alpha < beta; ++k) {
        int c = C4_ORDER[k];
        int r = c4_drop(c, who);
        if (r < 0)
            continue;
        any = true;
        int s = c4_wins_at(r, c, false) ? C4_WIN + depth : -c4_negamax(depth - 1, -beta, -alpha, 3 - who);
        c4[r][c] = 0;
        if (s > alpha)
            alpha = s;
    }
    return any ? alpha : 0;
}

/* Evaluates one move of the cicada */
static void c4_think_column(int c) {
    int r = c4_drop(c, 2);
    if (r < 0) {
        c4_scores[c] = INT32_MIN;
        return;
    }
    c4_scores[c] = c4_wins_at(r, c, false) ? C4_WIN * 2 : -c4_negamax(C4_DEPTH - 1, -C4_WIN * 2, C4_WIN * 2, 1);
    c4[r][c] = 0;
}

static int c4_choose(void) {
    int32_t best = INT32_MIN;
    int moves[C4_COLS], n = 0;
    for (int c = 0; c < C4_COLS; ++c) {
        if (c4_scores[c] == INT32_MIN)
            continue;
        if (c4_scores[c] > best) {
            best = c4_scores[c];
            n = 0;
        }
        if (c4_scores[c] == best)
            moves[n++] = c;
    }
    return n ? moves[rnd(n)] : -1;
}

static void c4_play(int c, int who) {
    int r = c4_drop(c, who);
    if (r < 0)
        return;
    tone(who == 1 ? 880 : 659, 60);
    if (c4_wins_at(r, c, true))
        c4_result = who;
    else if (c4_full())
        c4_result = 3;
    if (c4_result) {
        ++c4_score[c4_result - 1];
        printf("game: puissance 4 %s\n", c4_result == 1 ? "won" : c4_result == 2 ? "lost" : "draw");
        end_sound(c4_result);
    }
}

static void c4_start_thinking(absolute_time_t now) {
    c4_badge_turn = true;
    c4_think_col = 0;
    c4_badge_ts = delayed_by_ms(now, 400);  /* The screen shows the move of the player first */
}

static void c4_new_round(absolute_time_t now) {
    memset(c4, 0, sizeof(c4));
    c4_result = 0;
    c4_cursor = 3;
    c4_badge_turn = false;
    if (! c4_player_starts)
        c4_start_thinking(now);
    c4_player_starts = ! c4_player_starts;
    leds(0, 0, 0);
}

static bool c4_buttons(uint8_t pressed, absolute_time_t now) {
    if (c4_result) {
        if (pressed & GAMES_BTN_B)
            c4_new_round(now);
        return true;
    }
    if (c4_badge_turn)
        return true;
    if (pressed & GAMES_BTN_Y)
        c4_cursor = (c4_cursor + C4_COLS - 1) % C4_COLS;
    if (pressed & GAMES_BTN_X)
        c4_cursor = (c4_cursor + 1) % C4_COLS;
    if ((pressed & GAMES_BTN_B) && ! c4[C4_ROWS - 1][c4_cursor]) {
        c4_play(c4_cursor, 1);
        if (! c4_result)
            c4_start_thinking(now);
    }
    return true;
}

static void c4_task(absolute_time_t now) {
    if (c4_result || ! c4_badge_turn || ! reached(c4_badge_ts, now))
        return;
    if (c4_think_col < C4_COLS) {
        c4_think_column(c4_think_col++);  /* One column per call: the main loop keeps running */
        return;
    }
    int c = c4_choose();
    if (c >= 0)
        c4_play(c, 2);
    c4_badge_turn = false;
    changed = true;
}

static void c4_render(uint8_t *fb) {
    char title[32];
    snprintf(title, sizeof(title), "Puissance 4 : %u - %u", c4_score[0], c4_score[1]);
    draw_title(fb, title);
    frame(fb, C4_X0 - 2, C4_Y0 - 2, C4_COLS*C4_CELL + 4, C4_ROWS*C4_CELL + 4, 2, GFX_BLACK);
    for (int c = 1; c < C4_COLS; ++c)
        gfx_fill_rect(fb, C4_X0 + c*C4_CELL, C4_Y0, 1, C4_ROWS*C4_CELL, GFX_BLACK);
    for (int r = 0; r < C4_ROWS; ++r)
        for (int c = 0; c < C4_COLS; ++c) {
            int cx = C4_X0 + c*C4_CELL + C4_CELL/2, cy = C4_Y0 + (C4_ROWS - 1 - r)*C4_CELL + C4_CELL/2;
            if (c4[r][c] == 1)
                fill_circle(fb, cx, cy, 8, GFX_BLACK);
            else if (c4[r][c] == 2)
                ring(fb, cx, cy, 8, 2);
            else
                continue;
            if (c4_result == 1 || c4_result == 2)
                for (int i = 0; i < 4; ++i)
                    if (c4_win[i][0] == r && c4_win[i][1] == c)
                        fill_circle(fb, cx, cy, 3, c4[r][c] == 1 ? GFX_WHITE : GFX_BLACK);
        }
    if (! c4_result && ! c4_badge_turn) {
        /* Arrow above the chosen column */
        int cx = C4_X0 + c4_cursor*C4_CELL + C4_CELL/2;
        for (int i = 0; i < 7; ++i)
            gfx_fill_rect(fb, cx - 6 + i, 31 + i, 13 - 2*i, 1, GFX_BLACK);
    }
    draw_footer(fb, c4_result ? result_text(c4_result) : c4_badge_turn ? "La cigale réfléchit..." : "Flancs : colonne  D : jouer");
}


/* ------ Simon: one pad per button, repeat the growing sequence ------ */

#define SIMON_MAX 64
#define SIMON_INPUT_TIMEOUT_MS 5000

typedef enum { SIMON_START, SIMON_SHOW, SIMON_INPUT, SIMON_OVER } simon_phase_t;

static uint8_t simon_seq[SIMON_MAX];
static int simon_len = 0, simon_pos = 0;
static simon_phase_t simon_phase = SIMON_START;
static int simon_lit = -1;  /* Pad shown, -1 = none */
static bool simon_showing = false;  /* The lit pad is from the sequence (not the player's) */
static absolute_time_t simon_ts = 0, simon_lit_ts = 0;
static bool simon_new_record = false;

/* Pads as on the badge seen from the front: wings at the top, flanks at the bottom */
static const char *SIMON_LABELS[4] = {"Aile G", "Aile D", "Flanc G", "Flanc D"};
static const uint8_t SIMON_BUTTONS[4] = {GAMES_BTN_A, GAMES_BTN_B, GAMES_BTN_Y, GAMES_BTN_X};
static const uint16_t SIMON_HZ[4] = {659, 880, 1109, 1319};  /* E A C# E, like the original */
static const uint8_t SIMON_RGB[4][3] = {{0, 255, 0}, {255, 0, 0}, {255, 160, 0}, {0, 64, 255}};

static int simon_score(void) {
    return simon_phase == SIMON_OVER || simon_len == 0 ? simon_len - (simon_len > 0) : simon_len - 1;
}

static uint32_t simon_on_ms(void) {
    int ms = 550 - 25 * simon_len;  /* Faster and faster */
    return ms < 250 ? 250 : ms;
}

static void simon_light(int pad, uint32_t ms) {
    simon_lit = pad;
    if (pad >= 0) {
        tone(SIMON_HZ[pad], ms);
        leds(SIMON_RGB[pad][0], SIMON_RGB[pad][1], SIMON_RGB[pad][2]);
    } else {
        leds(0, 0, 0);
    }
    changed = true;
}

static void simon_next_round(absolute_time_t now, uint32_t delay_ms) {
    simon_seq[simon_len++] = rnd(4);
    simon_pos = 0;
    simon_showing = false;
    simon_phase = SIMON_SHOW;
    simon_ts = delayed_by_ms(now, delay_ms);
    changed = true;
}

static void simon_start(absolute_time_t now) {
    simon_len = 0;
    simon_lit = -1;
    simon_showing = false;
    simon_new_record = false;
    simon_phase = SIMON_START;
    simon_ts = delayed_by_ms(now, 1500);
    leds(0, 0, 0);
}

static void simon_over(void) {
    simon_phase = SIMON_OVER;
    simon_lit = -1;
    tone(262, 600);
    leds(255, 0, 0);
    simon_new_record = new_record(GAME_SIMON, simon_len - 1, false);
    printf("game: simon over, score %d\n", simon_len - 1);
    changed = true;
}

static bool simon_buttons(uint8_t pressed, absolute_time_t now) {
    if (simon_phase == SIMON_OVER) {
        if (pressed & GAMES_BTN_A)
            return false;
        if (pressed & GAMES_BTN_B)
            simon_start(now);
        return true;
    }
    if (simon_phase != SIMON_INPUT)
        return ! (pressed & GAMES_BTN_A);  /* While the sequence is shown, the left wing quits */
    for (int pad = 0; pad < 4; ++pad) {
        if (! (pressed & SIMON_BUTTONS[pad]))
            continue;
        if (simon_seq[simon_pos] != pad) {
            simon_over();
            return true;
        }
        simon_light(pad, 250);
        simon_showing = false;
        simon_lit_ts = delayed_by_ms(now, 250);
        simon_ts = delayed_by_ms(now, SIMON_INPUT_TIMEOUT_MS);
        if (++simon_pos == simon_len) {
            if (simon_len == SIMON_MAX)
                simon_over();  /* Nobody gets there */
            else
                simon_next_round(now, 1000);
        }
        return true;  /* One pad per call */
    }
    return true;
}

static void simon_task(absolute_time_t now) {
    /* The pad pressed by the player goes off */
    if (simon_lit >= 0 && ! simon_showing && reached(simon_lit_ts, now))
        simon_light(-1, 0);
    switch (simon_phase) {
    case SIMON_START:
        if (reached(simon_ts, now))
            simon_next_round(now, 300);
        break;
    case SIMON_SHOW:
        if (! reached(simon_ts, now) || (simon_lit >= 0 && ! simon_showing))
            break;
        if (simon_showing) {
            simon_light(-1, 0);
            simon_showing = false;
            if (++simon_pos >= simon_len) {
                simon_phase = SIMON_INPUT;
                simon_pos = 0;
                simon_ts = delayed_by_ms(now, SIMON_INPUT_TIMEOUT_MS);
            } else {
                simon_ts = delayed_by_ms(now, 200);
            }
        } else {
            simon_light(simon_seq[simon_pos], simon_on_ms());
            simon_showing = true;
            simon_ts = delayed_by_ms(now, simon_on_ms());
        }
        break;
    case SIMON_INPUT:
        if (reached(simon_ts, now))
            simon_over();  /* Too slow */
        break;
    case SIMON_OVER:
        break;
    }
}

static void simon_render(uint8_t *fb) {
    char text[64];
    snprintf(text, sizeof(text), "Simon : %d", simon_score() > 0 ? simon_score() : 0);
    draw_title(fb, text);
    for (int pad = 0; pad < 4; ++pad) {
        int x = 4 + (pad % 2) * 98, y = 32 + (pad / 2) * 74, w = 94, h = 70;
        bool lit = pad == simon_lit;
        if (lit)
            gfx_fill_rect(fb, x, y, w, h, GFX_BLACK);
        else
            frame(fb, x, y, w, h, 2, GFX_BLACK);
        gfx_text(fb, x + w/2, y + (h - gfx_font_medium.height)/2, &gfx_font_medium, SIMON_LABELS[pad],
                 lit ? GFX_WHITE : GFX_BLACK, GFX_ALIGN_CENTER);
    }
    if (simon_phase == SIMON_OVER) {
        char rec[24];
        games_record_text(GAME_SIMON, rec, sizeof(rec));
        snprintf(text, sizeof(text), "Perdu !\nScore : %d\n%s", simon_len - 1, simon_new_record ? "Nouveau record !" : rec);
        draw_box(fb, text);
    }
    if (simon_phase == SIMON_START)
        draw_footer(fb, "Retenez la séquence !");
    else if (simon_phase == SIMON_SHOW)
        draw_footer(fb, "Regardez, écoutez...");
    else if (simon_phase == SIMON_INPUT) {
        snprintf(text, sizeof(text), "A vous ! %d / %d", simon_pos, simon_len);
        draw_footer(fb, text);
    } else
        draw_footer(fb, "D : rejouer  G : quitter");
}


/* ------ Réflexes: press when the LEDs light up in green ------ */

#define REFLEX_ROUNDS 5
#define REFLEX_TOO_SLOW_MS 2000

typedef enum { REFLEX_READY, REFLEX_WAIT, REFLEX_GO, REFLEX_RESULT, REFLEX_MISSED, REFLEX_DONE } reflex_phase_t;

static reflex_phase_t reflex_phase = REFLEX_READY;
static absolute_time_t reflex_ts = 0;
static int reflex_round = 0;
static uint16_t reflex_times[REFLEX_ROUNDS];
static bool reflex_early = false;  /* Missed: too early (or too late) */
static uint16_t reflex_average = 0;
static bool reflex_new_record = false;

static void reflex_wait(absolute_time_t now) {
    leds(0, 0, 0);
    reflex_phase = REFLEX_WAIT;
    reflex_ts = delayed_by_ms(now, 1500 + rnd(3000));  /* Unpredictable */
    changed = true;
}

static bool reflex_buttons(uint8_t pressed, absolute_time_t now) {
    uint8_t tap = pressed & (GAMES_BTN_B | GAMES_BTN_X | GAMES_BTN_Y);
    if (! tap)
        return true;
    switch (reflex_phase) {
    case REFLEX_READY:
    case REFLEX_DONE:
        reflex_round = 0;
        reflex_new_record = false;
        reflex_wait(now);
        break;
    case REFLEX_WAIT:
        reflex_early = true;
        reflex_phase = REFLEX_MISSED;
        reflex_ts = delayed_by_ms(now, 1500);
        leds(255, 0, 0);
        tone(262, 300);
        changed = true;
        break;
    case REFLEX_GO: {
        int64_t ms = absolute_time_diff_us(reflex_ts, now) / 1000 - GAMES_INPUT_LAG_MS;
        reflex_times[reflex_round++] = ms < 0 ? 0 : (uint16_t)ms;
        printf("game: reflex %d ms\n", (int)reflex_times[reflex_round - 1]);
        leds(0, 0, 0);
        reflex_phase = REFLEX_RESULT;
        reflex_ts = delayed_by_ms(now, 1500);
        changed = true;
        break;
    }
    default:
        break;
    }
    return true;
}

static void reflex_task(absolute_time_t now) {
    switch (reflex_phase) {
    case REFLEX_WAIT:
        if (reached(reflex_ts, now)) {
            leds(0, 255, 0);
            tone(1319, 120);
            reflex_phase = REFLEX_GO;
            reflex_ts = now;
            changed = true;
        }
        break;
    case REFLEX_GO:
        if (absolute_time_diff_us(reflex_ts, now) > REFLEX_TOO_SLOW_MS * 1000ll) {
            reflex_early = false;
            reflex_phase = REFLEX_MISSED;
            reflex_ts = delayed_by_ms(now, 1500);
            leds(0, 0, 0);
            changed = true;
        }
        break;
    case REFLEX_RESULT:
        if (! reached(reflex_ts, now))
            break;
        if (reflex_round < REFLEX_ROUNDS) {
            reflex_wait(now);
        } else {
            uint32_t sum = 0;
            for (int i = 0; i < REFLEX_ROUNDS; ++i)
                sum += reflex_times[i];
            reflex_average = sum / REFLEX_ROUNDS;
            reflex_new_record = new_record(GAME_REFLEX, reflex_average, true);
            reflex_phase = REFLEX_DONE;
            if (reflex_new_record)
                tone(1319, 400);
            changed = true;
        }
        break;
    case REFLEX_MISSED:
        if (reached(reflex_ts, now))
            reflex_wait(now);  /* The same round again */
        break;
    default:
        break;
    }
}

static void reflex_render(uint8_t *fb) {
    char text[96];
    snprintf(text, sizeof(text), "Réflexes : %d / %d", reflex_round + (reflex_phase == REFLEX_WAIT || reflex_phase == REFLEX_GO
                                                                      || reflex_phase == REFLEX_MISSED), REFLEX_ROUNDS);
    draw_title(fb, reflex_phase == REFLEX_READY ? "Réflexes" : text);
    const char *footer = "G : quitter";
    switch (reflex_phase) {
    case REFLEX_READY:
        games_record_text(GAME_REFLEX, text + 64, 32);
        draw_lines(fb, 40, &gfx_font_small, "Quand les LEDs s'allument\nen vert, appuyez vite\nsur l'aile droite\nou sur un flanc.\n5 essais, la moyenne compte.",
                   GFX_BLACK);
        draw_lines(fb, 154, &gfx_font_small, text + 64, GFX_BLACK);
        footer = "D : commencer  G : quitter";
        break;
    case REFLEX_WAIT:
        draw_lines(fb, 85, &gfx_font_large, "Attendez...", GFX_BLACK);
        break;
    case REFLEX_GO:
        draw_lines(fb, 85, &gfx_font_large, "Maintenant !", GFX_BLACK);
        break;
    case REFLEX_RESULT:
        snprintf(text, sizeof(text), "%u ms", reflex_times[reflex_round - 1]);
        draw_lines(fb, 85, &gfx_font_large, text, GFX_BLACK);
        break;
    case REFLEX_MISSED:
        draw_lines(fb, 75, &gfx_font_large, reflex_early ? "Trop tôt !" : "Trop lent !", GFX_BLACK);
        draw_lines(fb, 110, &gfx_font_small, "On recommence cet essai.", GFX_BLACK);
        break;
    case REFLEX_DONE: {
        char rec[32];
        games_record_text(GAME_REFLEX, rec, sizeof(rec));
        snprintf(text, sizeof(text), "Moyenne : %u ms\n%s", reflex_average, reflex_new_record ? "Nouveau record !" : rec);
        draw_lines(fb, 50, &gfx_font_medium, text, GFX_BLACK);
        int n = 0;
        char *p = text;
        for (int i = 0; i < REFLEX_ROUNDS; ++i)
            n += snprintf(p + n, sizeof(text) - n, "%s%u", i ? "  " : "", reflex_times[i]);
        draw_lines(fb, 120, &gfx_font_small, text, GFX_BLACK);
        footer = "D : rejouer  G : quitter";
        break;
    }
    }
    draw_footer(fb, footer);
}


/* ------ Snake: the flanks turn left or right ------ */

#define SNAKE_COLS 19
#define SNAKE_ROWS 14
#define SNAKE_CELL 10
#define SNAKE_X0 5
#define SNAKE_Y0 33
#define SNAKE_MAX (SNAKE_COLS * SNAKE_ROWS)
#define SNAKE_START_MS 450  /* Time of a step, shorter with each food */
#define SNAKE_MIN_MS 220

typedef enum { SNAKE_READY, SNAKE_RUN, SNAKE_PAUSE, SNAKE_OVER } snake_phase_t;

static uint16_t snake_body[SNAKE_MAX];  /* Ring buffer of the cells (y * SNAKE_COLS + x), the head at snake_head */
static int snake_head = 0, snake_len = 0;
static uint8_t snake_dir = 0;  /* 0: right, 1: down, 2: left, 3: up */
static uint8_t snake_turns[2];  /* Turns pressed before the next step (1: right, 3: left) */
static int snake_n_turns = 0;
static int snake_food = -1;
static snake_phase_t snake_phase = SNAKE_READY;
static absolute_time_t snake_ts = 0;
static uint16_t snake_score = 0;
static bool snake_new_record = false;

static uint16_t snake_cell(int i) {  /* i = 0 is the head */
    return snake_body[(snake_head - i + SNAKE_MAX) % SNAKE_MAX];
}

static bool snake_on(int cell, int n) {  /* The cell is in the first n cells of the body */
    for (int i = 0; i < n; ++i)
        if (snake_cell(i) == cell)
            return true;
    return false;
}

static void snake_place_food(void) {
    int empty = SNAKE_MAX - snake_len;
    snake_food = -1;
    if (empty <= 0)
        return;
    int k = rnd(empty);
    for (int cell = 0; cell < SNAKE_MAX; ++cell)
        if (! snake_on(cell, snake_len) && k-- == 0) {
            snake_food = cell;
            return;
        }
}

static void snake_reset(void) {
    snake_len = 3;
    snake_head = 2;
    for (int i = 0; i < 3; ++i)
        snake_body[i] = (SNAKE_ROWS/2) * SNAKE_COLS + 3 + i;
    snake_dir = 0;
    snake_n_turns = 0;
    snake_score = 0;
    snake_new_record = false;
    snake_phase = SNAKE_READY;
    snake_place_food();
    leds(0, 0, 0);
}

static uint32_t snake_step_ms(void) {
    int ms = SNAKE_START_MS - 15 * snake_score;
    return ms < SNAKE_MIN_MS ? SNAKE_MIN_MS : ms;
}

static void snake_over(void) {
    snake_phase = SNAKE_OVER;
    tone(262, 500);
    leds(255, 0, 0);
    snake_new_record = new_record(GAME_SNAKE, snake_score, false);
    printf("game: snake over, score %u\n", snake_score);
}

static void snake_step(void) {
    if (snake_n_turns) {
        snake_dir = (snake_dir + snake_turns[0]) % 4;
        snake_turns[0] = snake_turns[1];
        --snake_n_turns;
    }
    int x = snake_cell(0) % SNAKE_COLS, y = snake_cell(0) / SNAKE_COLS;
    x += snake_dir == 0 ? 1 : snake_dir == 2 ? -1 : 0;
    y += snake_dir == 1 ? 1 : snake_dir == 3 ? -1 : 0;
    if (x < 0 || x >= SNAKE_COLS || y < 0 || y >= SNAKE_ROWS) {
        snake_over();
        return;
    }
    int next = y * SNAKE_COLS + x;
    bool eating = next == snake_food;
    /* The tail moves away at the same time, unless the snake grows */
    if (snake_on(next, eating ? snake_len : snake_len - 1)) {
        snake_over();
        return;
    }
    snake_head = (snake_head + 1) % SNAKE_MAX;
    snake_body[snake_head] = next;
    if (eating) {
        ++snake_len;
        ++snake_score;
        tone(1047, 50);
        snake_place_food();
        if (snake_food < 0)
            snake_over();  /* The whole board is full: nothing more to eat */
    }
}

static bool snake_buttons(uint8_t pressed, absolute_time_t now) {
    switch (snake_phase) {
    case SNAKE_READY:
    case SNAKE_PAUSE:
        if (pressed & GAMES_BTN_B) {
            snake_phase = SNAKE_RUN;
            snake_ts = delayed_by_ms(now, snake_step_ms());
        }
        break;
    case SNAKE_RUN:
        if (pressed & GAMES_BTN_B)
            snake_phase = SNAKE_PAUSE;
        for (uint8_t b = GAMES_BTN_X; b <= GAMES_BTN_Y; b <<= 1)
            if ((pressed & b) && snake_n_turns < 2)
                snake_turns[snake_n_turns++] = b == GAMES_BTN_X ? 1 : 3;
        break;
    case SNAKE_OVER:
        if (pressed & GAMES_BTN_B)
            snake_reset();
        break;
    }
    return true;
}

static void snake_task(absolute_time_t now) {
    if (snake_phase != SNAKE_RUN || ! reached(snake_ts, now))
        return;
    snake_step();
    snake_ts = delayed_by_ms(snake_ts, snake_step_ms());
    if (reached(snake_ts, now))
        snake_ts = delayed_by_ms(now, snake_step_ms());  /* Late (e.g. debugger): no burst of steps */
    changed = true;
}

static void snake_render(uint8_t *fb) {
    char text[64];
    snprintf(text, sizeof(text), "Snake : %u", snake_score);
    draw_title(fb, text);
    frame(fb, SNAKE_X0 - 2, SNAKE_Y0 - 2, SNAKE_COLS*SNAKE_CELL + 3, SNAKE_ROWS*SNAKE_CELL + 3, 1, GFX_BLACK);
    for (int i = 0; i < snake_len; ++i) {
        int cell = snake_cell(i), x = SNAKE_X0 + (cell % SNAKE_COLS)*SNAKE_CELL, y = SNAKE_Y0 + (cell / SNAKE_COLS)*SNAKE_CELL;
        gfx_fill_rect(fb, x, y, SNAKE_CELL - 1, SNAKE_CELL - 1, GFX_BLACK);
        if (i == 0)
            gfx_fill_rect(fb, x + 3, y + 3, 3, 3, GFX_WHITE);  /* The head has an eye */
    }
    if (snake_food >= 0) {
        int x = SNAKE_X0 + (snake_food % SNAKE_COLS)*SNAKE_CELL, y = SNAKE_Y0 + (snake_food / SNAKE_COLS)*SNAKE_CELL;
        frame(fb, x, y, SNAKE_CELL - 1, SNAKE_CELL - 1, 1, GFX_BLACK);
        gfx_fill_rect(fb, x + 3, y + 3, 3, 3, GFX_BLACK);
    }
    if (snake_phase == SNAKE_OVER) {
        char rec[24];
        games_record_text(GAME_SNAKE, rec, sizeof(rec));
        snprintf(text, sizeof(text), "Perdu !\nScore : %u\n%s", snake_score, snake_new_record ? "Nouveau record !" : rec);
        draw_box(fb, text);
    }
    draw_footer(fb, snake_phase == SNAKE_READY ? "D : partir  Flancs : tourner"
                  : snake_phase == SNAKE_RUN ? "Flancs : tourner  D : pause"
                  : snake_phase == SNAKE_PAUSE ? "Pause  D : reprendre"
                  : "D : rejouer  G : quitter");
}


/* ------ API ------ */

void games_init(const games_hooks_t *h, uint16_t *r) {
    hooks = h;
    records = r;
    for (int g = 0; g < GAME_COUNT; ++g)
        if (g == GAME_REFLEX && records[g] == 0)
            records[g] = GAMES_NO_RECORD;  /* A new store is zeroed: 0 ms is not a record */
}

const char *games_name(game_t g) {
    return g < GAME_COUNT ? NAMES[g] : "";
}

void games_record_text(game_t g, char *buf, int len) {
    buf[0] = 0;
    if (g != GAME_SIMON && g != GAME_REFLEX && g != GAME_SNAKE)
        return;
    if (records[g] == GAMES_NO_RECORD)
        snprintf(buf, len, "Pas encore de record");
    else
        snprintf(buf, len, g == GAME_REFLEX ? "Record : %u ms" : "Record : %u", records[g]);
}

void games_start(game_t g, absolute_time_t now) {
    game = g;
    printf("ui: %s\n", games_name(g));
    switch (g) {
    case GAME_TICTACTOE:
        memset(ttt_score, 0, sizeof(ttt_score));
        ttt_player_starts = true;
        ttt_new_round(now);
        break;
    case GAME_CONNECT4:
        memset(c4_score, 0, sizeof(c4_score));
        c4_player_starts = true;
        c4_new_round(now);
        break;
    case GAME_SIMON:
        simon_start(now);
        break;
    case GAME_REFLEX:
        reflex_phase = REFLEX_READY;
        reflex_round = 0;
        leds(0, 0, 0);
        break;
    case GAME_SNAKE:
        snake_reset();
        break;
    default:
        break;
    }
    changed = true;
}

bool games_buttons(uint8_t pressed, absolute_time_t now) {
    if (! pressed)
        return true;
    bool stay;
    if (game == GAME_SIMON)
        stay = simon_buttons(pressed, now);  /* The left wing is also a pad */
    else if (pressed & GAMES_BTN_A)
        stay = false;
    else if (game == GAME_TICTACTOE)
        stay = ttt_buttons(pressed, now);
    else if (game == GAME_CONNECT4)
        stay = c4_buttons(pressed, now);
    else if (game == GAME_REFLEX)
        stay = reflex_buttons(pressed, now);
    else
        stay = snake_buttons(pressed, now);
    if (! stay) {
        tone(0, 0);
        leds(0, 0, 0);
    }
    changed = true;
    return stay;
}

bool games_task(absolute_time_t now) {
    switch (game) {
    case GAME_TICTACTOE: ttt_task(now); break;
    case GAME_CONNECT4: c4_task(now); break;
    case GAME_SIMON: simon_task(now); break;
    case GAME_REFLEX: reflex_task(now); break;
    case GAME_SNAKE: snake_task(now); break;
    default: break;
    }
    bool c = changed;
    changed = false;
    return c;
}

void games_render(uint8_t *fb) {
    gfx_clear(fb, GFX_WHITE);
    switch (game) {
    case GAME_TICTACTOE: ttt_render(fb); break;
    case GAME_CONNECT4: c4_render(fb); break;
    case GAME_SIMON: simon_render(fb); break;
    case GAME_REFLEX: reflex_render(fb); break;
    case GAME_SNAKE: snake_render(fb); break;
    default: break;
    }
}

bool games_calm(void) {
    switch (game) {
    case GAME_TICTACTOE: return ! ttt_badge_turn;
    case GAME_CONNECT4: return ! c4_badge_turn;
    case GAME_SIMON: return simon_phase == SIMON_OVER;
    case GAME_REFLEX: return reflex_phase == REFLEX_READY || reflex_phase == REFLEX_DONE;
    case GAME_SNAKE: return snake_phase != SNAKE_RUN;
    default: return true;
    }
}
