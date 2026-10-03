/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdio.h>
#include <string.h>

#include "puzzles.h"
#include "gfx.h"
#include "i18n.h"

#define PZ_TITLE_H 28
#define PZ_FOOTER_Y 180

static const games_hooks_t *pz_hooks = NULL;
static uint16_t *pz_records = NULL;
static puzzle_t pz_game = PUZZLE_MINES;
static bool pz_changed = false;  /* The screen must be redrawn */
static bool pz_help = false;  /* The help page (rules, controls, record) is shown */
static int pz_text_max = 0;  /* Widest footer or help line drawn, checked by the tests */

static const char *PZ_NAMES[PUZZLE_COUNT] = {N_("Démineur"), "2048", N_("Taquin"), "Sokoban", "Mastermind",
                                             N_("Pendu")};

/* Directions: the flanks go left and right, the wings up (left wing) and down (right wing) */
enum { DIR_UP, DIR_RIGHT, DIR_DOWN, DIR_LEFT };
static const int8_t DIR_DX[4] = {0, 1, 0, -1};
static const int8_t DIR_DY[4] = {-1, 0, 1, 0};
static const uint8_t DIR_BUTTONS[4] = {GAMES_BTN_A, GAMES_BTN_X, GAMES_BTN_B, GAMES_BTN_Y};

/* Footers, the buttons in the order of the badge: left wing on the left */
#define PZ_OVER_FOOTER N_("G : menu  D : rejouer")
#define PZ_PLAY_FOOTER N_("Long : G menu, flanc D aide")


/* ------ Common ------ */

static uint32_t pz_rnd(uint32_t n) {
    return n ? pz_hooks->random() % n : 0;
}

static void pz_tone(uint16_t hz, uint16_t ms) {
    if (pz_hooks->tone)
        pz_hooks->tone(hz, ms);
}

static void pz_leds(uint8_t r, uint8_t g, uint8_t b) {
    if (pz_hooks->leds)
        pz_hooks->leds(r, g, b);
}

static void pz_win_sound(void) {
    pz_tone(1319, 400);
    pz_leds(0, 255, 0);
}

static void pz_lose_sound(void) {
    pz_tone(262, 500);
    pz_leds(255, 0, 0);
}

/* Updates the record of the game, returns whether it is a new one */
static bool pz_new_record(puzzle_t p, uint32_t value, bool lower_is_better) {
    uint16_t v = value > 0xFFFE ? 0xFFFE : value, *r = &pz_records[p];
    if (*r != GAMES_NO_RECORD && (lower_is_better ? v >= *r : v <= *r))
        return false;
    *r = v;
    if (pz_hooks->records_changed)
        pz_hooks->records_changed();
    return true;
}

static void pz_measure(const gfx_font_t *font, const char *text) {
    int w = gfx_text_width(font, text);
    if (w > pz_text_max)
        pz_text_max = w;
}

static void pz_draw_title(uint8_t *fb, const char *title) {
    gfx_fill_rect(fb, 0, 0, GFX_WIDTH, PZ_TITLE_H, GFX_BLACK);
    gfx_text(fb, GFX_WIDTH/2, (PZ_TITLE_H - gfx_font_medium.height)/2, &gfx_font_medium, title, GFX_WHITE, GFX_ALIGN_CENTER);
    pz_measure(&gfx_font_medium, title);
}

static void pz_draw_footer(uint8_t *fb, const char *text) {
    gfx_fill_rect(fb, 0, PZ_FOOTER_Y - 2, GFX_WIDTH, 1, GFX_BLACK);
    gfx_text(fb, GFX_WIDTH/2, PZ_FOOTER_Y, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
    pz_measure(&gfx_font_small, text);
}

/* Centered lines separated by '\n', \p step pixels apart, returns the y after the last one */
static int pz_draw_lines(uint8_t *fb, int y, int step, const gfx_font_t *font, const char *text, gfx_color_t color) {
    char line[48];
    while (*text) {
        const char *end = strchr(text, '\n');
        size_t n = end ? (size_t)(end - text) : strlen(text);
        if (n >= sizeof(line))
            n = sizeof(line) - 1;
        memcpy(line, text, n);
        line[n] = 0;
        gfx_text(fb, GFX_WIDTH/2, y, font, line, color, GFX_ALIGN_CENTER);
        pz_measure(font, line);
        y += step;
        text += end ? (size_t)(end - text) + 1 : n;
    }
    return y;
}

static void pz_frame(uint8_t *fb, int x, int y, int w, int h, int t, gfx_color_t color) {
    gfx_fill_rect(fb, x, y, w, t, color);
    gfx_fill_rect(fb, x, y + h - t, w, t, color);
    gfx_fill_rect(fb, x, y, t, h, color);
    gfx_fill_rect(fb, x + w - t, y, t, h, color);
}

/* A message box over the game */
static void pz_draw_box(uint8_t *fb, const char *text) {
    int lines = 1, widest = 0;
    char line[48];
    for (const char *c = text; *c; ) {
        size_t n = strcspn(c, "\n");
        if (n >= sizeof(line))
            n = sizeof(line) - 1;
        memcpy(line, c, n);
        line[n] = 0;
        int w = gfx_text_width(&gfx_font_medium, line);
        widest = w > widest ? w : widest;
        c += n;
        if (*c == '\n') {
            ++lines;
            ++c;
        }
    }
    /* The small font when a line is too wide for the box */
    const gfx_font_t *font = widest > GFX_WIDTH - 36 ? &gfx_font_small : &gfx_font_medium;
    int step = font->height + 3, h = lines * step + 16;
    int y = PZ_TITLE_H + (PZ_FOOTER_Y - 2 - PZ_TITLE_H - h) / 2;
    gfx_fill_rect(fb, 12, y, GFX_WIDTH - 24, h, GFX_WHITE);
    pz_frame(fb, 12, y, GFX_WIDTH - 24, h, 3, GFX_BLACK);
    pz_draw_lines(fb, y + 8, step, font, text, GFX_BLACK);
}

static void pz_fill_circle(uint8_t *fb, int cx, int cy, int r, gfx_color_t color) {
    for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r; dx <= r; ++dx)
            if (dx*dx + dy*dy <= r*r + r)
                gfx_pixel(fb, cx + dx, cy + dy, color);
}

static void pz_ring(uint8_t *fb, int cx, int cy, int r, int thickness) {
    pz_fill_circle(fb, cx, cy, r, GFX_BLACK);
    pz_fill_circle(fb, cx, cy, r - thickness, GFX_WHITE);
}

/* A line drawn with a square brush of w pixels */
static void pz_thick_line(uint8_t *fb, int x0, int y0, int x1, int y1, int w, gfx_color_t color) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    while (true) {
        gfx_fill_rect(fb, x0 - w/2, y0 - w/2, w, w, color);
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

/* Text centered in a box, with the biggest font that fits */
static void pz_text_in(uint8_t *fb, int x, int y, int w, int h, const char *text, gfx_color_t color) {
    const gfx_font_t *font = &gfx_font_large;
    if (gfx_text_width(font, text) > w - 4)
        font = &gfx_font_medium;
    if (gfx_text_width(font, text) > w - 4)
        font = &gfx_font_small;
    gfx_text(fb, x + w/2, y + (h - font->height)/2, font, text, color, GFX_ALIGN_CENTER);
}


/* ------ Démineur (minesweeper) ------ */

#define MINES_W 9
#define MINES_H 9
#define MINES_CELLS (MINES_W * MINES_H)
#define MINES_N 12
#define MINES_CELL 16
#define MINES_X0 4
#define MINES_Y0 31
#define MINE_MINE 0x80
#define MINE_OPEN 0x40
#define MINE_FLAG 0x20
#define MINE_COUNT 0x0F  /* Mines around */
#define MINES_MAX_S 999

typedef enum { MINES_READY, MINES_RUN, MINES_WON, MINES_LOST } mines_state_t;

static uint8_t mines[MINES_CELLS];
static uint8_t mines_stack[MINES_CELLS];  /* Flood fill */
static int mines_cursor = 0;
static mines_state_t mines_state = MINES_READY;
static int mines_opened = 0, mines_flags = 0;
static int mines_exploded = -1;
static absolute_time_t mines_t0 = 0, mines_pause_ts = 0;
static uint16_t mines_seconds = 0;
static bool mines_new_rec = false;

static bool mines_near(int a, int b) {
    int dx = a % MINES_W - b % MINES_W, dy = a / MINES_W - b / MINES_W;
    return dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1;
}

/* The neighbours of a cell, returns how many */
static int mines_neighbours(int c, uint8_t *out) {
    int n = 0, x = c % MINES_W, y = c / MINES_W;
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
            if ((dx || dy) && x + dx >= 0 && x + dx < MINES_W && y + dy >= 0 && y + dy < MINES_H)
                out[n++] = (y + dy) * MINES_W + x + dx;
    return n;
}

static void mines_reset(void) {
    memset(mines, 0, sizeof(mines));
    mines_cursor = (MINES_H/2) * MINES_W + MINES_W/2;
    mines_state = MINES_READY;
    mines_opened = mines_flags = 0;
    mines_exploded = -1;
    mines_seconds = 0;
    mines_new_rec = false;
}

/* Places the mines at the first cell opened: never on it nor around it, so it opens an area */
static void mines_place(int safe) {
    for (int m = 0; m < MINES_N; ++m) {
        int free = 0;
        for (int i = 0; i < MINES_CELLS; ++i)
            free += ! (mines[i] & MINE_MINE) && ! mines_near(i, safe);
        int k = pz_rnd(free);
        for (int i = 0; i < MINES_CELLS; ++i)
            if (! (mines[i] & MINE_MINE) && ! mines_near(i, safe) && k-- == 0) {
                mines[i] |= MINE_MINE;
                break;
            }
    }
    uint8_t nb[8];
    for (int i = 0; i < MINES_CELLS; ++i) {
        int n = mines_neighbours(i, nb), count = 0;
        for (int j = 0; j < n; ++j)
            count += (mines[nb[j]] & MINE_MINE) != 0;
        mines[i] = (mines[i] & ~MINE_COUNT) | count;
    }
}

static uint16_t mines_elapsed(absolute_time_t now) {
    int64_t s = absolute_time_diff_us(mines_t0, now) / 1000000;
    return s < 0 ? 0 : s > MINES_MAX_S ? MINES_MAX_S : (uint16_t)s;
}

/* Opens a cell, and all the cells around when it has no mine around (without recursion) */
static void mines_flood(int start) {
    if (mines[start] & (MINE_OPEN | MINE_FLAG | MINE_MINE))
        return;
    int n = 0;
    uint8_t nb[8];
    mines[start] |= MINE_OPEN;
    ++mines_opened;
    mines_stack[n++] = start;
    while (n) {
        int c = mines_stack[--n];
        if (mines[c] & MINE_COUNT)
            continue;
        int k = mines_neighbours(c, nb);
        for (int j = 0; j < k; ++j)
            if (! (mines[nb[j]] & (MINE_OPEN | MINE_FLAG | MINE_MINE))) {
                mines[nb[j]] |= MINE_OPEN;  /* Opened when pushed: each cell is pushed once at most */
                ++mines_opened;
                mines_stack[n++] = nb[j];
            }
    }
}

static void mines_lose(int c, absolute_time_t now) {
    mines_state = MINES_LOST;
    mines_exploded = c;
    mines_seconds = mines_elapsed(now);
    pz_lose_sound();
    printf("game: mines lost after %u s\n", mines_seconds);
}

static void mines_check_win(absolute_time_t now) {
    if (mines_state != MINES_RUN || mines_opened != MINES_CELLS - MINES_N)
        return;
    mines_state = MINES_WON;
    mines_seconds = mines_elapsed(now);
    if (mines_seconds == 0)
        mines_seconds = 1;
    for (int i = 0; i < MINES_CELLS; ++i)
        if (mines[i] & MINE_MINE)
            mines[i] |= MINE_FLAG;
    mines_flags = MINES_N;
    mines_new_rec = pz_new_record(PUZZLE_MINES, mines_seconds, true);
    pz_win_sound();
    printf("game: mines won in %u s\n", mines_seconds);
}

/* Opens the cell. On an open number with as many flags around, opens the other cells around (chord). */
static void mines_reveal(int c, absolute_time_t now) {
    if (mines[c] & MINE_FLAG)
        return;
    if (mines_state == MINES_READY) {
        mines_place(c);
        mines_state = MINES_RUN;
        mines_t0 = now;
    }
    if (mines[c] & MINE_OPEN) {
        uint8_t nb[8];
        int n = mines_neighbours(c, nb), flags = 0;
        for (int j = 0; j < n; ++j)
            flags += (mines[nb[j]] & MINE_FLAG) != 0;
        if (! (mines[c] & MINE_COUNT) || flags != (mines[c] & MINE_COUNT))
            return;
        for (int j = 0; j < n && mines_state == MINES_RUN; ++j) {
            if (mines[nb[j]] & (MINE_FLAG | MINE_OPEN))
                continue;
            if (mines[nb[j]] & MINE_MINE)
                mines_lose(nb[j], now);  /* A wrong flag */
            else
                mines_flood(nb[j]);
        }
    } else if (mines[c] & MINE_MINE) {
        mines_lose(c, now);
    } else {
        mines_flood(c);
    }
    if (mines_state == MINES_RUN)
        pz_tone(880, 30);
    mines_check_win(now);
}

static void mines_toggle_flag(int c) {
    if (mines[c] & MINE_OPEN)
        return;
    mines[c] ^= MINE_FLAG;
    mines_flags += (mines[c] & MINE_FLAG) ? 1 : -1;
    pz_tone(660, 30);
}

static void mines_buttons(uint8_t pressed, uint8_t long_pressed, absolute_time_t now) {
    for (int d = 0; d < 4; ++d)
        if (pressed & DIR_BUTTONS[d]) {
            int x = (mines_cursor % MINES_W + DIR_DX[d] + MINES_W) % MINES_W;  /* Wraps around */
            int y = (mines_cursor / MINES_W + DIR_DY[d] + MINES_H) % MINES_H;
            mines_cursor = y * MINES_W + x;
        }
    if (long_pressed & GAMES_BTN_Y)
        mines_toggle_flag(mines_cursor);
    if (long_pressed & GAMES_BTN_B)
        mines_reveal(mines_cursor, now);
}

static void mines_task(absolute_time_t now) {
    if (mines_state != MINES_RUN || pz_help)
        return;
    uint16_t s = mines_elapsed(now);
    if (s != mines_seconds) {
        mines_seconds = s;
        pz_changed = true;
    }
}

static void mines_draw_mine(uint8_t *fb, int cx, int cy, gfx_color_t color) {
    pz_fill_circle(fb, cx, cy, 4, color);
    gfx_fill_rect(fb, cx - 6, cy, 13, 1, color);
    gfx_fill_rect(fb, cx, cy - 6, 1, 13, color);
}

static void mines_draw_flag(uint8_t *fb, int x, int y, gfx_color_t color) {
    gfx_fill_rect(fb, x + 6, y + 3, 2, 10, color);
    for (int i = 0; i < 5; ++i)
        gfx_fill_rect(fb, x + 8, y + 3 + i, 5 - i, 1, color);
    gfx_fill_rect(fb, x + 4, y + 12, 7, 2, color);
}

static void mines_render(uint8_t *fb) {
    char text[32];
    if (mines_state == MINES_WON)
        snprintf(text, sizeof(text), _("Gagné en %u s !"), mines_seconds);
    pz_draw_title(fb, mines_state == MINES_WON ? text : mines_state == MINES_LOST ? N_("Boum ! Perdu...")
                  : N_("Démineur"));
    /* The grid */
    for (int i = 0; i <= MINES_W; ++i)
        gfx_fill_rect(fb, MINES_X0 + i*MINES_CELL, MINES_Y0, 1, MINES_H*MINES_CELL + 1, GFX_BLACK);
    for (int i = 0; i <= MINES_H; ++i)
        gfx_fill_rect(fb, MINES_X0, MINES_Y0 + i*MINES_CELL, MINES_W*MINES_CELL + 1, 1, GFX_BLACK);
    bool over = mines_state == MINES_WON || mines_state == MINES_LOST;
    for (int c = 0; c < MINES_CELLS; ++c) {
        int x = MINES_X0 + (c % MINES_W)*MINES_CELL, y = MINES_Y0 + (c / MINES_W)*MINES_CELL;
        int cx = x + MINES_CELL/2, cy = y + MINES_CELL/2;
        uint8_t m = mines[c];
        if (c == mines_exploded) {
            gfx_fill_rect(fb, x + 1, y + 1, MINES_CELL - 1, MINES_CELL - 1, GFX_BLACK);
            mines_draw_mine(fb, cx, cy, GFX_WHITE);
        } else if (mines_state == MINES_LOST && (m & MINE_MINE) && ! (m & MINE_FLAG)) {
            mines_draw_mine(fb, cx, cy, GFX_BLACK);
        } else if (mines_state == MINES_LOST && (m & MINE_FLAG) && ! (m & MINE_MINE)) {
            pz_thick_line(fb, x + 4, y + 4, x + 12, y + 12, 2, GFX_BLACK);  /* Wrong flag */
            pz_thick_line(fb, x + 12, y + 4, x + 4, y + 12, 2, GFX_BLACK);
        } else if (! (m & MINE_OPEN)) {
            gfx_fill_rect(fb, x + 2, y + 2, MINES_CELL - 3, MINES_CELL - 3, GFX_BLACK);
            if (m & MINE_FLAG)
                mines_draw_flag(fb, x, y, GFX_WHITE);
        } else if (m & MINE_COUNT) {
            char digit[2] = {'0' + (m & MINE_COUNT), 0};
            int ty = y + (MINES_CELL - gfx_font_small.height)/2 + 1;
            gfx_text(fb, cx, ty, &gfx_font_small, digit, GFX_BLACK, GFX_ALIGN_CENTER);
            gfx_text(fb, cx + 1, ty, &gfx_font_small, digit, GFX_BLACK, GFX_ALIGN_CENTER);  /* Bold */
        }
        if (c == mines_cursor && ! over) {  /* The cell in negative, with a ring */
            gfx_fill_rect(fb, x + 1, y + 1, MINES_CELL - 1, MINES_CELL - 1, GFX_INVERT);
            gfx_rect(fb, x + 3, y + 3, MINES_CELL - 5, MINES_CELL - 5, GFX_INVERT);
        }
    }
    /* Side panel: mines left, time, record */
    int px = (MINES_X0 + MINES_W*MINES_CELL + GFX_WIDTH) / 2;
    gfx_text(fb, px, 36, &gfx_font_small, "Mines", GFX_BLACK, GFX_ALIGN_CENTER);
    snprintf(text, sizeof(text), "%d", MINES_N - mines_flags);
    gfx_text(fb, px, 52, &gfx_font_medium, text, GFX_BLACK, GFX_ALIGN_CENTER);
    gfx_text(fb, px, 84, &gfx_font_small, N_("Temps"), GFX_BLACK, GFX_ALIGN_CENTER);
    snprintf(text, sizeof(text), "%u", mines_seconds);
    gfx_text(fb, px, 100, &gfx_font_medium, text, GFX_BLACK, GFX_ALIGN_CENTER);
    gfx_text(fb, px, 132, &gfx_font_small, mines_new_rec ? N_("Record !") : N_("Record"), GFX_BLACK, GFX_ALIGN_CENTER);
    if (pz_records[PUZZLE_MINES] == GAMES_NO_RECORD)
        snprintf(text, sizeof(text), "-");
    else
        snprintf(text, sizeof(text), "%u s", pz_records[PUZZLE_MINES]);
    gfx_text(fb, px, 148, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
    pz_draw_footer(fb, over ? PZ_OVER_FOOTER : N_("Long : fl. G drapeau, D ouvre"));
}


/* ------ 2048: slide the tiles, two equal tiles merge (once per move) ------ */

#define T48_CELL 34
#define T48_GAP 2
#define T48_X0 29
#define T48_Y0 32
#define T48_GOAL 11  /* 2^11 = 2048 */

typedef enum { T48_PLAY, T48_WON, T48_OVER } t48_state_t;

static uint8_t t48[16];  /* Exponents: 0 = empty, 1 = 2, 2 = 4... */
static uint32_t t48_score = 0;
static t48_state_t t48_state = T48_PLAY;
static bool t48_reached = false;  /* 2048 was reached: the game goes on */
static bool t48_new_rec = false;

/* Slides a line towards line[0], merging each tile once at most. Returns whether something moved. */
static bool t48_slide(uint8_t *line[4], uint32_t *gain) {
    uint8_t out[4] = {0, 0, 0, 0};
    int n = 0;
    bool can_merge = false;  /* out[n - 1] was not merged yet */
    for (int i = 0; i < 4; ++i) {
        uint8_t v = *line[i];
        if (! v)
            continue;
        if (can_merge && out[n - 1] == v) {
            ++out[n - 1];
            *gain += 1u << out[n - 1];
            can_merge = false;
        } else {
            out[n++] = v;
            can_merge = true;
        }
    }
    bool moved = false;
    for (int i = 0; i < 4; ++i) {
        moved |= *line[i] != out[i];
        *line[i] = out[i];
    }
    return moved;
}

static bool t48_move(int dir, uint32_t *gain) {
    bool moved = false;
    for (int k = 0; k < 4; ++k) {
        uint8_t *line[4];
        for (int i = 0; i < 4; ++i) {
            int r = dir == DIR_UP ? i : dir == DIR_DOWN ? 3 - i : k;
            int c = dir == DIR_LEFT ? i : dir == DIR_RIGHT ? 3 - i : k;
            line[i] = &t48[r*4 + c];
        }
        moved |= t48_slide(line, gain);
    }
    return moved;
}

static bool t48_can_move(void) {
    for (int i = 0; i < 16; ++i) {
        if (! t48[i])
            return true;
        if ((i % 4 < 3 && t48[i] == t48[i + 1]) || (i < 12 && t48[i] == t48[i + 4]))
            return true;
    }
    return false;
}

static void t48_spawn(void) {
    int empty = 0;
    for (int i = 0; i < 16; ++i)
        empty += ! t48[i];
    int k = pz_rnd(empty);
    for (int i = 0; i < 16; ++i)
        if (! t48[i] && k-- == 0) {
            t48[i] = pz_rnd(10) == 0 ? 2 : 1;  /* A 4 one time in 10 */
            return;
        }
}

static void t48_new(void) {
    memset(t48, 0, sizeof(t48));
    t48_score = 0;
    t48_state = T48_PLAY;
    t48_reached = false;
    t48_new_rec = false;
    t48_spawn();
    t48_spawn();
}

static void t48_over(void) {
    t48_state = T48_OVER;
    t48_new_rec = pz_new_record(PUZZLE_2048, t48_score, false);
    pz_lose_sound();
    printf("game: 2048 over, score %lu\n", (unsigned long)t48_score);
}

static void t48_play(int dir) {
    uint32_t gain = 0;
    if (! t48_move(dir, &gain))
        return;
    t48_score += gain;
    if (gain)
        pz_tone(1047, 40);
    t48_spawn();
    if (! t48_reached)
        for (int i = 0; i < 16; ++i)
            if (t48[i] >= T48_GOAL) {
                t48_reached = true;
                t48_state = T48_WON;
                pz_win_sound();
                printf("game: 2048 reached, score %lu\n", (unsigned long)t48_score);
                break;
            }
    if (! t48_can_move())
        t48_over();
}

static void t48_buttons(uint8_t pressed) {
    if (t48_state == T48_WON) {
        t48_state = T48_PLAY;  /* Any button: go on */
        pz_leds(0, 0, 0);
        return;
    }
    for (int d = 0; d < 4 && t48_state == T48_PLAY; ++d)
        if (pressed & DIR_BUTTONS[d])
            t48_play(d);
}

/* The best score so far is kept when the player leaves */
static void t48_leave(void) {
    if (t48_state != T48_OVER && t48_score)
        pz_new_record(PUZZLE_2048, t48_score, false);
}

static void t48_render(uint8_t *fb) {
    char text[64];
    snprintf(text, sizeof(text), _("2048 : %lu"), (unsigned long)t48_score);
    pz_draw_title(fb, text);
    for (int i = 0; i < 16; ++i) {
        int x = T48_X0 + (i % 4)*(T48_CELL + T48_GAP), y = T48_Y0 + (i / 4)*(T48_CELL + T48_GAP);
        uint8_t v = t48[i];
        if (! v) {
            gfx_rect(fb, x, y, T48_CELL, T48_CELL, GFX_BLACK);
            continue;
        }
        snprintf(text, sizeof(text), "%lu", 1ul << v);
        if (v <= 2) {  /* 2 and 4: white */
            pz_frame(fb, x, y, T48_CELL, T48_CELL, 2, GFX_BLACK);
            pz_text_in(fb, x, y, T48_CELL, T48_CELL, text, GFX_BLACK);
        } else {
            gfx_fill_rect(fb, x, y, T48_CELL, T48_CELL, GFX_BLACK);
            pz_text_in(fb, x, y, T48_CELL, T48_CELL, text, GFX_WHITE);
        }
    }
    if (t48_state == T48_WON) {
        pz_draw_box(fb, _("2048 !\nBravo !\nOn continue ?"));
    } else if (t48_state == T48_OVER) {
        char rec[32];
        puzzles_record_text(PUZZLE_2048, rec, sizeof(rec));
        snprintf(text, sizeof(text), _("Bloqué !\nScore : %lu\n%s"), (unsigned long)t48_score,
                 t48_new_rec ? _("Nouveau record !") : rec);
        pz_draw_box(fb, text);
    }
    pz_draw_footer(fb, t48_state == T48_WON ? N_("Un bouton : continuer") : t48_state == T48_OVER ? PZ_OVER_FOOTER
                   : PZ_PLAY_FOOTER);
}


/* ------ Taquin (15-puzzle): the button pushes a tile into the hole ------ */

#define TAQ_SHUFFLE 200  /* Random moves from the solved board: always solvable */

static uint8_t taq[16];  /* 1..15, 0 = the hole */
static int taq_hole = 15;
static uint16_t taq_moves = 0;
static bool taq_solved = false, taq_new_rec = false;

static bool taq_is_solved(void) {
    for (int i = 0; i < 15; ++i)
        if (taq[i] != i + 1)
            return false;
    return true;
}

/* The tile next to the hole moves into it, in the direction \p dir */
static bool taq_slide(int dir) {
    int x = taq_hole % 4 - DIR_DX[dir], y = taq_hole / 4 - DIR_DY[dir];
    if (x < 0 || x > 3 || y < 0 || y > 3)
        return false;
    int s = y*4 + x;
    taq[taq_hole] = taq[s];
    taq[s] = 0;
    taq_hole = s;
    return true;
}

static void taq_new(void) {
    for (int i = 0; i < 16; ++i)
        taq[i] = (i + 1) % 16;
    taq_hole = 15;
    int last = -1;
    for (int n = 0; (n < TAQ_SHUFFLE || taq_is_solved()) && n < 4 * TAQ_SHUFFLE; ++n) {
        int d = pz_rnd(4);
        for (int k = 0; k < 4; ++k, d = (d + 1) % 4)
            if (d != (last + 2) % 4 && taq_slide(d))  /* Not the move back */
                break;
        last = d;
    }
    taq_moves = 0;
    taq_solved = false;
    taq_new_rec = false;
}

static void taq_buttons(uint8_t pressed) {
    for (int d = 0; d < 4 && ! taq_solved; ++d) {
        if (! (pressed & DIR_BUTTONS[d]) || ! taq_slide(d))
            continue;
        if (taq_moves < 0xFFFE)
            ++taq_moves;
        if (taq_is_solved()) {
            taq_solved = true;
            taq_new_rec = pz_new_record(PUZZLE_TAQUIN, taq_moves, true);
            pz_win_sound();
            printf("game: taquin solved in %u moves\n", taq_moves);
        }
    }
}

static void taq_render(uint8_t *fb) {
    char text[64];
    snprintf(text, sizeof(text), _("Taquin : %u coup%s"), taq_moves, taq_moves > 1 ? "s" : "");
    pz_draw_title(fb, text);
    for (int i = 0; i < 16; ++i) {
        int x = T48_X0 + (i % 4)*(T48_CELL + T48_GAP), y = T48_Y0 + (i / 4)*(T48_CELL + T48_GAP);
        if (! taq[i]) {
            gfx_rect(fb, x, y, T48_CELL, T48_CELL, GFX_BLACK);
            continue;
        }
        snprintf(text, sizeof(text), "%u", taq[i]);
        gfx_fill_rect(fb, x, y, T48_CELL, T48_CELL, GFX_BLACK);
        if (taq[i] == i + 1)  /* At its place: a white border */
            gfx_rect(fb, x + 2, y + 2, T48_CELL - 4, T48_CELL - 4, GFX_WHITE);
        pz_text_in(fb, x, y, T48_CELL, T48_CELL, text, GFX_WHITE);
    }
    if (taq_solved) {
        char rec[32];
        puzzles_record_text(PUZZLE_TAQUIN, rec, sizeof(rec));
        snprintf(text, sizeof(text), _("Bravo !\n%u coups\n%s"), taq_moves, taq_new_rec ? _("Nouveau record !") : rec);
        pz_draw_box(fb, text);
    }
    pz_draw_footer(fb, taq_solved ? PZ_OVER_FOOTER : PZ_PLAY_FOOTER);
}


/* ------ Sokoban: push the boxes on the targets ------ */

#define SOKO_W 12
#define SOKO_H 9
#define SOKO_CELL 16
#define SOKO_UNDO 64
#define SOKO_WALL 1
#define SOKO_GOAL 2
#define SOKO_BOX 4

/* '#' wall, '.' target, '$' box, '*' box on a target, '@' player, '+' player on a target. All checked solvable. */
static const char *const SOKO_LEVELS[] = {
    "  ####\n"
    "###  #\n"
    "#  $ #\n"
    "# #. ##\n"
    "# .$@ #\n"
    "##   #\n"
    " #####",

    "#######\n"
    "#     #\n"
    "# #$# #\n"
    "# . @ #\n"
    "# #$# #\n"
    "#  .  #\n"
    "#######",

    " #####\n"
    " #   #\n"
    " #$  #\n"
    "##  $##\n"
    "#. @ .#\n"
    "#######",

    "########\n"
    "#  .   #\n"
    "# $#$# #\n"
    "#  @ . #\n"
    "# ## # #\n"
    "#  .$  #\n"
    "########",

    " ######\n"
    "##    #\n"
    "#  ## ##\n"
    "# $ $  #\n"
    "#  ..#@#\n"
    "## #   #\n"
    " #  ####\n"
    " ####",

    "#########\n"
    "#   #   #\n"
    "# $ . $ #\n"
    "## .@. ##\n"
    "# $ . $ #\n"
    "#   #   #\n"
    "#########",
};
#define SOKO_LEVELS_N ((int)(sizeof(SOKO_LEVELS) / sizeof(SOKO_LEVELS[0])))

static uint8_t soko[SOKO_H][SOKO_W];
static int soko_w = 0, soko_h = 0;
static int soko_level = 0;
static int soko_x = 0, soko_y = 0;  /* The player */
static uint16_t soko_moves = 0;
static uint8_t soko_undo[SOKO_UNDO];  /* Ring buffer of the last moves: direction, | 4 when a box was pushed */
static int soko_undo_head = 0, soko_undo_n = 0;
static bool soko_solved = false;

static int soko_levels_solved(void) {
    uint16_t r = pz_records[PUZZLE_SOKOBAN];
    return r == GAMES_NO_RECORD ? 0 : r > SOKO_LEVELS_N ? SOKO_LEVELS_N : r;
}

/* Reads a level drawn with the characters of SOKO_LEVELS */
static void soko_parse(const char *text) {
    memset(soko, 0, sizeof(soko));
    soko_w = soko_h = 0;
    int x = 0, y = 0;
    for (const char *c = text; *c; ++c) {
        if (*c == '\n') {
            x = 0;
            ++y;
            continue;
        }
        if (x < SOKO_W && y < SOKO_H) {
            uint8_t *cell = &soko[y][x];
            *cell = *c == '#' ? SOKO_WALL : *c == '.' || *c == '+' ? SOKO_GOAL : *c == '$' ? SOKO_BOX
                  : *c == '*' ? SOKO_BOX | SOKO_GOAL : 0;
            if (*c == '@' || *c == '+') {
                soko_x = x;
                soko_y = y;
            }
            if (x + 1 > soko_w)
                soko_w = x + 1;
            if (y + 1 > soko_h)
                soko_h = y + 1;
        }
        ++x;
    }
    soko_moves = 0;
    soko_undo_n = 0;
    soko_solved = false;
}

static void soko_load(int level) {
    soko_level = level;
    soko_parse(SOKO_LEVELS[level]);
}

static uint8_t soko_at(int x, int y) {
    return x < 0 || x >= SOKO_W || y < 0 || y >= SOKO_H ? SOKO_WALL : soko[y][x];
}

static bool soko_is_solved(void) {
    for (int y = 0; y < SOKO_H; ++y)
        for (int x = 0; x < SOKO_W; ++x)
            if ((soko[y][x] & SOKO_BOX) && ! (soko[y][x] & SOKO_GOAL))
                return false;
    return true;
}

static bool soko_move(int dir) {
    int nx = soko_x + DIR_DX[dir], ny = soko_y + DIR_DY[dir];
    uint8_t next = soko_at(nx, ny);
    bool push = next & SOKO_BOX;
    if (next & SOKO_WALL)
        return false;
    if (push) {
        int bx = nx + DIR_DX[dir], by = ny + DIR_DY[dir];
        if (soko_at(bx, by) & (SOKO_WALL | SOKO_BOX))
            return false;  /* One box at a time */
        soko[ny][nx] &= ~SOKO_BOX;
        soko[by][bx] |= SOKO_BOX;
        if (soko[by][bx] & SOKO_GOAL)
            pz_tone(1047, 40);
    }
    soko_x = nx;
    soko_y = ny;
    if (soko_moves < 0xFFFE)
        ++soko_moves;
    soko_undo_head = (soko_undo_head + 1) % SOKO_UNDO;
    soko_undo[soko_undo_head] = dir | (push ? 4 : 0);
    if (soko_undo_n < SOKO_UNDO)
        ++soko_undo_n;
    if (soko_is_solved()) {
        soko_solved = true;
        if (soko_level + 1 > soko_levels_solved())
            pz_new_record(PUZZLE_SOKOBAN, soko_level + 1, false);
        pz_win_sound();
        printf("game: sokoban level %d solved in %u moves\n", soko_level + 1, soko_moves);
    }
    return true;
}

static bool soko_undo_move(void) {
    if (! soko_undo_n)
        return false;
    int dir = soko_undo[soko_undo_head] & 3;
    bool push = soko_undo[soko_undo_head] & 4;
    soko_undo_head = (soko_undo_head + SOKO_UNDO - 1) % SOKO_UNDO;
    --soko_undo_n;
    if (push) {  /* The box comes back with the player */
        soko[soko_y + DIR_DY[dir]][soko_x + DIR_DX[dir]] &= ~SOKO_BOX;
        soko[soko_y][soko_x] |= SOKO_BOX;
    }
    soko_x -= DIR_DX[dir];
    soko_y -= DIR_DY[dir];
    if (soko_moves)
        --soko_moves;
    pz_tone(440, 30);
    return true;
}

static void soko_buttons(uint8_t pressed, uint8_t long_pressed) {
    if (long_pressed & GAMES_BTN_Y)
        soko_undo_move();
    if (long_pressed & GAMES_BTN_B)
        soko_load(soko_level);  /* Restart the level */
    for (int d = 0; d < 4 && ! soko_solved; ++d)
        if (pressed & DIR_BUTTONS[d])
            soko_move(d);
}

/* Help page: the flanks choose among the levels already reached */
static void soko_choose(int delta) {
    int n = soko_levels_solved() + 1;
    if (n > SOKO_LEVELS_N)
        n = SOKO_LEVELS_N;
    soko_load((soko_level + delta + n) % n);
}

static void soko_render(uint8_t *fb) {
    char text[64];
    snprintf(text, sizeof(text), _("Sokoban %d/%d : %u"), soko_level + 1, SOKO_LEVELS_N, soko_moves);
    pz_draw_title(fb, text);
    int x0 = (GFX_WIDTH - soko_w*SOKO_CELL) / 2;
    int y0 = PZ_TITLE_H + (PZ_FOOTER_Y - 2 - PZ_TITLE_H - soko_h*SOKO_CELL) / 2;
    for (int y = 0; y < soko_h; ++y)
        for (int x = 0; x < soko_w; ++x) {
            int px = x0 + x*SOKO_CELL, py = y0 + y*SOKO_CELL;
            uint8_t c = soko[y][x];
            if (c & SOKO_WALL) {  /* Bricks */
                gfx_fill_rect(fb, px, py, SOKO_CELL, SOKO_CELL, GFX_BLACK);
                gfx_fill_rect(fb, px, py + 7, SOKO_CELL, 1, GFX_WHITE);
                gfx_fill_rect(fb, px, py + 15, SOKO_CELL, 1, GFX_WHITE);
                gfx_fill_rect(fb, px + 7, py, 1, 7, GFX_WHITE);
                gfx_fill_rect(fb, px + 15, py + 8, 1, 7, GFX_WHITE);
                continue;
            }
            bool player = x == soko_x && y == soko_y;
            if (c & SOKO_BOX) {
                if (c & SOKO_GOAL) {
                    gfx_fill_rect(fb, px + 1, py + 1, SOKO_CELL - 2, SOKO_CELL - 2, GFX_BLACK);
                    gfx_fill_rect(fb, px + 6, py + 6, 4, 4, GFX_WHITE);
                } else {
                    pz_frame(fb, px + 1, py + 1, SOKO_CELL - 2, SOKO_CELL - 2, 2, GFX_BLACK);
                    pz_thick_line(fb, px + 3, py + 3, px + 12, py + 12, 1, GFX_BLACK);
                    pz_thick_line(fb, px + 12, py + 3, px + 3, py + 12, 1, GFX_BLACK);
                }
            } else if (player) {
                pz_fill_circle(fb, px + 8, py + 8, 6, GFX_BLACK);
                gfx_fill_rect(fb, px + 5, py + 6, 2, 2, GFX_WHITE);  /* Eyes */
                gfx_fill_rect(fb, px + 10, py + 6, 2, 2, GFX_WHITE);
                if (c & SOKO_GOAL)
                    gfx_fill_rect(fb, px + 6, py + 10, 5, 2, GFX_WHITE);
            } else if (c & SOKO_GOAL) {
                for (int i = 0; i < 4; ++i) {  /* A small diamond */
                    gfx_fill_rect(fb, px + 8 - i, py + 5 + i, 2*i + 1, 1, GFX_BLACK);
                    gfx_fill_rect(fb, px + 8 - i, py + 11 - i, 2*i + 1, 1, GFX_BLACK);
                }
            }
        }
    if (soko_solved) {
        snprintf(text, sizeof(text), soko_level + 1 < SOKO_LEVELS_N ? _("Niveau réussi !\n%u coups")
                 : _("Bravo !\n%u coups\nTous les niveaux !"), soko_moves);
        pz_draw_box(fb, text);
    }
    pz_draw_footer(fb, soko_solved ? soko_level + 1 < SOKO_LEVELS_N ? N_("G : menu  D : niveau suivant")
                                                                    : PZ_OVER_FOOTER
                   : N_("Long : fl. G annuler, D refaire"));
}


/* ------ Mastermind: find the code of 4 symbols among 6 ------ */

#define MM_PEGS 4
#define MM_SYMBOLS 6
#define MM_TRIES 10
#define MM_ROW_H 13
#define MM_Y0 31
#define MM_X0 44
#define MM_STEP 22
#define MM_SIZE 11

typedef enum { MM_PLAY, MM_WON, MM_LOST } mm_state_t;

static uint8_t mm_secret[MM_PEGS];
static uint8_t mm_rows[MM_TRIES][MM_PEGS];
static uint8_t mm_marks[MM_TRIES][2];  /* Well placed, misplaced */
static int mm_try = 0, mm_cursor = 0;
static mm_state_t mm_state = MM_PLAY;
static bool mm_new_rec = false;

static void mm_score(const uint8_t *secret, const uint8_t *guess, uint8_t *well, uint8_t *misplaced) {
    uint8_t ns[MM_SYMBOLS] = {0}, ng[MM_SYMBOLS] = {0};
    *well = 0;
    for (int i = 0; i < MM_PEGS; ++i) {
        if (secret[i] == guess[i])
            ++*well;
        ++ns[secret[i]];
        ++ng[guess[i]];
    }
    int common = 0;
    for (int s = 0; s < MM_SYMBOLS; ++s)
        common += ns[s] < ng[s] ? ns[s] : ng[s];
    *misplaced = common - *well;
}

static void mm_new(void) {
    for (int i = 0; i < MM_PEGS; ++i)
        mm_secret[i] = pz_rnd(MM_SYMBOLS);
    memset(mm_rows, 0, sizeof(mm_rows));
    memset(mm_marks, 0, sizeof(mm_marks));
    for (int i = 0; i < MM_PEGS; ++i)
        mm_rows[0][i] = i;  /* A first guess with different symbols */
    mm_try = 0;
    mm_cursor = 0;
    mm_state = MM_PLAY;
    mm_new_rec = false;
}

static void mm_validate(void) {
    uint8_t *m = mm_marks[mm_try];
    mm_score(mm_secret, mm_rows[mm_try], &m[0], &m[1]);
    if (m[0] == MM_PEGS) {
        mm_state = MM_WON;
        mm_new_rec = pz_new_record(PUZZLE_MASTERMIND, mm_try + 1, true);
        pz_win_sound();
        printf("game: mastermind won in %d tries\n", mm_try + 1);
    } else if (mm_try + 1 == MM_TRIES) {
        mm_state = MM_LOST;
        pz_lose_sound();
        printf("game: mastermind lost\n");
    } else {
        ++mm_try;
        memcpy(mm_rows[mm_try], mm_rows[mm_try - 1], MM_PEGS);  /* Start from the previous guess */
        pz_tone(880, 30);
    }
}

static void mm_buttons(uint8_t pressed, uint8_t long_pressed) {
    if (pressed & GAMES_BTN_Y)
        mm_cursor = (mm_cursor + MM_PEGS - 1) % MM_PEGS;
    if (pressed & GAMES_BTN_X)
        mm_cursor = (mm_cursor + 1) % MM_PEGS;
    uint8_t *s = &mm_rows[mm_try][mm_cursor];
    if (pressed & GAMES_BTN_A)
        *s = (*s + MM_SYMBOLS - 1) % MM_SYMBOLS;
    if (pressed & GAMES_BTN_B)
        *s = (*s + 1) % MM_SYMBOLS;
    if (long_pressed & GAMES_BTN_B)
        mm_validate();
}

/* The 6 symbols, MM_SIZE pixels wide, in black */
static void mm_symbol(uint8_t *fb, int x, int y, int s) {
    int c = MM_SIZE / 2;
    switch (s) {
    case 0: pz_fill_circle(fb, x + c, y + c, c, GFX_BLACK); break;
    case 1: pz_ring(fb, x + c, y + c, c, 2); break;
    case 2: gfx_fill_rect(fb, x + 1, y + 1, MM_SIZE - 2, MM_SIZE - 2, GFX_BLACK); break;
    case 3: pz_frame(fb, x + 1, y + 1, MM_SIZE - 2, MM_SIZE - 2, 2, GFX_BLACK); break;
    case 4:
        for (int i = 0; i < MM_SIZE; ++i)
            gfx_fill_rect(fb, x + c - i/2, y + i, 2*(i/2) + 1, 1, GFX_BLACK);
        break;
    default:
        pz_thick_line(fb, x + 1, y + 1, x + MM_SIZE - 2, y + MM_SIZE - 2, 2, GFX_BLACK);
        pz_thick_line(fb, x + MM_SIZE - 2, y + 1, x + 1, y + MM_SIZE - 2, 2, GFX_BLACK);
        break;
    }
}

static void mm_render(uint8_t *fb) {
    char text[32];
    if (mm_state == MM_WON)
        snprintf(text, sizeof(text), _("Gagné en %d essai%s !"), mm_try + 1, mm_try ? "s" : "");
    else
        snprintf(text, sizeof(text), "Mastermind %d/%d", mm_try + 1, MM_TRIES);
    pz_draw_title(fb, mm_state == MM_LOST ? N_("Perdu...") : text);
    for (int t = 0; t <= mm_try; ++t) {
        int y = MM_Y0 + t*MM_ROW_H;
        snprintf(text, sizeof(text), "%d", t + 1);
        gfx_text(fb, 28, y - 1, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_RIGHT);
        for (int i = 0; i < MM_PEGS; ++i)
            mm_symbol(fb, MM_X0 + i*MM_STEP, y + 1, mm_rows[t][i]);
        bool current = t == mm_try && mm_state == MM_PLAY;
        if (current) {
            gfx_fill_rect(fb, MM_X0 + mm_cursor*MM_STEP - 3, y - 1, MM_SIZE + 6, MM_ROW_H + 1, GFX_INVERT);
            gfx_text(fb, 140, y - 1, &gfx_font_small, "<- ?", GFX_BLACK, GFX_ALIGN_LEFT);
            continue;
        }
        /* The marks: a black dot is well placed, a ring is misplaced */
        for (int k = 0; k < mm_marks[t][0] + mm_marks[t][1]; ++k) {
            int cx = 144 + k*11, cy = y + 6;
            if (k < mm_marks[t][0])
                pz_fill_circle(fb, cx, cy, 4, GFX_BLACK);
            else
                pz_ring(fb, cx, cy, 4, 1);
        }
    }
    /* The secret code, at the bottom */
    int y = MM_Y0 + MM_TRIES*MM_ROW_H + 2;
    gfx_fill_rect(fb, 0, y - 2, GFX_WIDTH, 1, GFX_BLACK);
    gfx_text(fb, 2, y, &gfx_font_small, "Code", GFX_BLACK, GFX_ALIGN_LEFT);
    for (int i = 0; i < MM_PEGS; ++i) {
        if (mm_state == MM_PLAY)
            gfx_text(fb, MM_X0 + i*MM_STEP + MM_SIZE/2, y, &gfx_font_small, "?", GFX_BLACK, GFX_ALIGN_CENTER);
        else
            mm_symbol(fb, MM_X0 + i*MM_STEP, y + 2, mm_secret[i]);
    }
    if (mm_state != MM_PLAY) {
        if (mm_new_rec)
            snprintf(text, sizeof(text), _("Record !"));
        else if (pz_records[PUZZLE_MASTERMIND] != GAMES_NO_RECORD)
            snprintf(text, sizeof(text), "Record %u", pz_records[PUZZLE_MASTERMIND]);
        else
            text[0] = 0;
        gfx_text(fb, GFX_WIDTH - 4, y, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_RIGHT);
    }
    pz_draw_footer(fb, mm_state != MM_PLAY ? PZ_OVER_FOOTER : N_("Ailes : symbole  D long : ok"));
}


/* ------ Pendu (hangman): find the word letter by letter ------ */

#define PENDU_ERRORS 8  /* Parts of the drawing */
#define PENDU_COLS 13
#define PENDU_CELL_W 15
#define PENDU_CELL_H 20
#define PENDU_X0 3
#define PENDU_Y0 135
#define PENDU_LETTER_W 18

typedef enum { PENDU_PLAY, PENDU_WON, PENDU_LOST } pendu_state_t;

static const char *const PENDU_WORDS[] = {
    "BADGE", "CIGALE", "SOLEIL", "PROVENCE", "LAVANDE", "OLIVIER", "CALANQUE", "MISTRAL", "TOMATE", "FROMAGE",
    "CHATEAU", "MONTAGNE", "RIVIERE", "ORDINATEUR", "CLAVIER", "ECRAN", "SOURIS", "PIRATE", "SECRET", "RADIO",
    "ANTENNE", "SIGNAL", "BATTERIE", "CIRCUIT", "SOUDURE", "PROCESSEUR", "MEMOIRE", "LOGICIEL", "RESEAU", "SERVEUR",
    "PAQUET", "CHIFFRE", "ENIGME", "MYSTERE", "TRESOR", "BOUSSOLE", "NAVIRE", "SARDINE", "POISSON", "BATEAU",
    "PHARE", "VAGUE", "ABEILLE", "PAPILLON", "GIRAFE", "ELEPHANT", "KANGOUROU", "JONGLEUR", "XYLOPHONE", "WAGON",
    "ZEBRE", "QUILLE", "HIBOU", "YAOURT", "CRAYON", "FENETRE", "JARDIN", "CUISINE", "BOUGIE", "ETOILE",
    "GALAXIE", "PLANETE", "FUSEE", "ROBOT", "LASER", "PIXEL", "BINAIRE", "OCTET", "PROTOCOLE", "SARIETTE",
    "PETANQUE", "CIGOGNE", "TAMBOURIN", "SANTON", "FOUGASSE", "NOUGAT", "CALISSON", "GARRIGUE", "PINEDE", "VOILIER",
};
#define PENDU_WORDS_N ((int)(sizeof(PENDU_WORDS) / sizeof(PENDU_WORDS[0])))

static int pendu_word = -1;
static uint32_t pendu_used = 0;  /* Letters proposed, bit 0 = 'A' */
static int pendu_errors = 0, pendu_cursor = 0;
static uint16_t pendu_streak = 0;  /* Words found in a row */
static pendu_state_t pendu_state = PENDU_PLAY;
static bool pendu_new_rec = false;

static bool pendu_in_word(int letter) {
    for (const char *c = PENDU_WORDS[pendu_word]; *c; ++c)
        if (*c - 'A' == letter)
            return true;
    return false;
}

static bool pendu_found(void) {
    for (const char *c = PENDU_WORDS[pendu_word]; *c; ++c)
        if (! (pendu_used & (1ul << (*c - 'A'))))
            return false;
    return true;
}

static void pendu_new(void) {
    int w = pz_rnd(PENDU_WORDS_N);
    if (w == pendu_word)
        w = (w + 1) % PENDU_WORDS_N;  /* Not the same word twice in a row */
    pendu_word = w;
    pendu_used = 0;
    pendu_errors = 0;
    pendu_cursor = 0;
    pendu_state = PENDU_PLAY;
    pendu_new_rec = false;
}

static void pendu_propose(int letter) {
    if (pendu_used & (1ul << letter))
        return;
    pendu_used |= 1ul << letter;
    if (! pendu_in_word(letter)) {
        if (++pendu_errors < PENDU_ERRORS) {
            pz_tone(330, 80);
            return;
        }
        pendu_state = PENDU_LOST;
        pendu_streak = 0;
        pz_lose_sound();
        printf("game: pendu lost, word %s\n", PENDU_WORDS[pendu_word]);
    } else if (pendu_found()) {
        pendu_state = PENDU_WON;
        ++pendu_streak;
        pendu_new_rec = pz_new_record(PUZZLE_PENDU, pendu_streak, false);
        pz_win_sound();
        printf("game: pendu won, streak %u\n", pendu_streak);
    } else {
        pz_tone(1047, 50);
    }
}

static void pendu_buttons(uint8_t pressed, uint8_t long_pressed) {
    if (pressed & GAMES_BTN_Y)
        pendu_cursor = (pendu_cursor + 25) % 26;
    if (pressed & GAMES_BTN_X)
        pendu_cursor = (pendu_cursor + 1) % 26;
    if (pressed & (GAMES_BTN_A | GAMES_BTN_B))  /* 2 rows: up and down both change the row */
        pendu_cursor = (pendu_cursor + PENDU_COLS) % 26;
    if (long_pressed & GAMES_BTN_B)
        pendu_propose(pendu_cursor);
}

static void pendu_draw_gallows(uint8_t *fb, int errors) {
    if (errors >= 1)
        gfx_fill_rect(fb, 6, 100, 56, 3, GFX_BLACK);  /* Base */
    if (errors >= 2)
        gfx_fill_rect(fb, 16, 34, 3, 66, GFX_BLACK);  /* Pole */
    if (errors >= 3) {
        gfx_fill_rect(fb, 16, 34, 46, 3, GFX_BLACK);  /* Beam */
        pz_thick_line(fb, 18, 50, 32, 36, 2, GFX_BLACK);
    }
    if (errors >= 4)
        gfx_fill_rect(fb, 57, 37, 2, 7, GFX_BLACK);  /* Rope */
    if (errors >= 5)
        pz_ring(fb, 58, 50, 6, 2);  /* Head */
    if (errors >= 6)
        gfx_fill_rect(fb, 57, 56, 2, 21, GFX_BLACK);  /* Body */
    if (errors >= 7) {  /* Arms */
        pz_thick_line(fb, 58, 62, 49, 71, 2, GFX_BLACK);
        pz_thick_line(fb, 58, 62, 67, 71, 2, GFX_BLACK);
    }
    if (errors >= 8) {  /* Legs */
        pz_thick_line(fb, 58, 76, 50, 90, 2, GFX_BLACK);
        pz_thick_line(fb, 58, 76, 66, 90, 2, GFX_BLACK);
    }
}

static void pendu_render(uint8_t *fb) {
    char text[32];
    if (pendu_state == PENDU_WON)
        snprintf(text, sizeof(text), _("Trouvé ! Série : %u"), pendu_streak);
    else if (pendu_state == PENDU_LOST)
        snprintf(text, sizeof(text), _("Pendu !"));
    else
        snprintf(text, sizeof(text), _("Pendu  série : %u"), pendu_streak);
    pz_draw_title(fb, text);
    pendu_draw_gallows(fb, pendu_errors);
    /* Errors and record, at the right of the drawing */
    int px = 135;
    gfx_text(fb, px, 36, &gfx_font_small, N_("Erreurs"), GFX_BLACK, GFX_ALIGN_CENTER);
    snprintf(text, sizeof(text), "%d / %d", pendu_errors, PENDU_ERRORS);
    gfx_text(fb, px, 52, &gfx_font_medium, text, GFX_BLACK, GFX_ALIGN_CENTER);
    gfx_text(fb, px, 78, &gfx_font_small, pendu_new_rec ? N_("Record !") : N_("Record"), GFX_BLACK, GFX_ALIGN_CENTER);
    if (pz_records[PUZZLE_PENDU] == GAMES_NO_RECORD)
        snprintf(text, sizeof(text), "-");
    else
        snprintf(text, sizeof(text), _("%u mot%s"), pz_records[PUZZLE_PENDU], pz_records[PUZZLE_PENDU] > 1 ? "s" : "");
    gfx_text(fb, px, 92, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
    /* The word: the letters found, the others when lost (in a black box) */
    const char *word = PENDU_WORDS[pendu_word];
    int len = strlen(word), x0 = GFX_WIDTH/2 - len*PENDU_LETTER_W/2;
    for (int i = 0; i < len; ++i) {
        int x = x0 + i*PENDU_LETTER_W, y = 108;
        char letter[2] = {word[i], 0};
        bool found = pendu_used & (1ul << (word[i] - 'A'));
        if (found) {
            gfx_text(fb, x + PENDU_LETTER_W/2, y, &gfx_font_medium, letter, GFX_BLACK, GFX_ALIGN_CENTER);
        } else if (pendu_state == PENDU_LOST) {
            gfx_fill_rect(fb, x + 1, y, PENDU_LETTER_W - 2, gfx_font_medium.height + 1, GFX_BLACK);
            gfx_text(fb, x + PENDU_LETTER_W/2, y, &gfx_font_medium, letter, GFX_WHITE, GFX_ALIGN_CENTER);
        }
        gfx_fill_rect(fb, x + 2, y + gfx_font_medium.height + 2, PENDU_LETTER_W - 4, 2, GFX_BLACK);
    }
    /* The alphabet: good letters in black, wrong ones crossed out */
    for (int l = 0; l < 26; ++l) {
        int x = PENDU_X0 + (l % PENDU_COLS)*PENDU_CELL_W, y = PENDU_Y0 + (l / PENDU_COLS)*PENDU_CELL_H;
        char letter[2] = {'A' + l, 0};
        bool used = pendu_used & (1ul << l), good = used && pendu_in_word(l);
        int ty = y + (PENDU_CELL_H - gfx_font_small.height)/2;
        if (good)
            gfx_fill_rect(fb, x + 1, y + 1, PENDU_CELL_W - 2, PENDU_CELL_H - 2, GFX_BLACK);
        gfx_text(fb, x + PENDU_CELL_W/2, ty, &gfx_font_small, letter, good ? GFX_WHITE : GFX_BLACK, GFX_ALIGN_CENTER);
        if (used && ! good)
            pz_thick_line(fb, x + 2, y + PENDU_CELL_H - 4, x + PENDU_CELL_W - 3, y + 3, 1, GFX_BLACK);
        if (l == pendu_cursor && pendu_state == PENDU_PLAY)
            pz_frame(fb, x - 1, y - 1, PENDU_CELL_W + 1, PENDU_CELL_H + 1, 2, good ? GFX_INVERT : GFX_BLACK);
    }
    pz_draw_footer(fb, pendu_state == PENDU_PLAY ? N_("D long : proposer la lettre")
                   : N_("G : menu  D : mot suivant"));
}


/* ------ Help pages ------ */

static const char *const PZ_HELP[PUZZLE_COUNT] = {
    /* The common lines ("Flancs : gauche...", "Long : G quitte...") are written in each text: the whole text is
     * translated (tr() in pz_help_render()) */
    N_("Ouvrez les cases sans mine.\nLe chiffre : mines autour.\n"
       "Flancs : gauche, droite\nAiles : haut (G), bas (D)\n"
       "Aile D longue : ouvrir\nFlanc G long : drapeau\nLong : G quitte, flanc D aide"),
    N_("Deux tuiles égales qui se\ntouchent fusionnent.\nAtteignez la tuile 2048 !\n"
       "Flancs : gauche, droite\nAiles : haut (G), bas (D)\n"
       "Tout glisse vers la touche.\nLong : G quitte, flanc D aide"),
    N_("Remettez les cases de 1 à 15\ndans l'ordre, trou à la fin.\n"
       "Une case voisine du trou\nglisse vers la touche :\n"
       "Flancs : gauche, droite\nAiles : haut (G), bas (D)\nLong : G quitte, flanc D aide"),
    N_("Poussez les caisses sur les\ncibles, une à la fois.\n"
       "Flancs : gauche, droite\nAiles : haut (G), bas (D)\nFlanc G long : annuler\n"
       "Aile D longue : recommencer\nLong : G quitte, flanc D aide"),
    N_("Trouvez le code secret :\n4 symboles parmi 6, 10 essais.\nPoint : bien placé,\nrond : mal placé.\n"
       "Flancs : case, ailes : symbole\nAile D longue : valider\nLong : G quitte, flanc D aide"),
    N_("Trouvez le mot, lettre par\nlettre, avant d'être pendu\n(8 erreurs).\n"
       "Flancs, ailes : choisir\nAile D longue : proposer\nLong : G quitte, flanc D aide"),
};

static void pz_help_render(uint8_t *fb) {
    char text[48];
    pz_draw_title(fb, PZ_NAMES[pz_game]);
    int y = pz_draw_lines(fb, 32, 15, &gfx_font_small, tr(PZ_HELP[pz_game]), GFX_BLACK);
    if (pz_game == PUZZLE_SOKOBAN) {
        snprintf(text, sizeof(text), _("Niveau %d/%d (flancs : changer)"), soko_level + 1, SOKO_LEVELS_N);
        y = pz_draw_lines(fb, y, 15, &gfx_font_small, text, GFX_BLACK);
    }
    puzzles_record_text(pz_game, text, sizeof(text));
    pz_draw_lines(fb, y + 2, 15, &gfx_font_small, text, GFX_BLACK);
    pz_draw_footer(fb, N_("G : menu  D : jouer"));
}


/* ------ API ------ */

void puzzles_init(const games_hooks_t *h, uint16_t *r) {
    pz_hooks = h;
    pz_records = r;
    for (int p = 0; p < PUZZLE_COUNT; ++p)
        if (pz_records[p] == 0)
            pz_records[p] = GAMES_NO_RECORD;  /* A new store is zeroed: no game has a record of 0 */
}

const char *puzzles_name(puzzle_t p) {
    return p < PUZZLE_COUNT ? PZ_NAMES[p] : "";
}

void puzzles_record_text(puzzle_t p, char *buf, int len) {
    buf[0] = 0;
    if (p >= PUZZLE_COUNT)
        return;
    unsigned r = pz_records[p];
    if (r == GAMES_NO_RECORD)
        snprintf(buf, len, _("Pas encore de record"));
    else if (p == PUZZLE_MINES)
        snprintf(buf, len, _("Record : %u s"), r);
    else if (p == PUZZLE_TAQUIN)
        snprintf(buf, len, _("Record : %u coups"), r);
    else if (p == PUZZLE_SOKOBAN)
        snprintf(buf, len, _("Niveaux réussis : %u / %d"), r, SOKO_LEVELS_N);
    else if (p == PUZZLE_MASTERMIND)
        snprintf(buf, len, _("Record : %u essai%s"), r, r > 1 ? "s" : "");
    else if (p == PUZZLE_PENDU)
        snprintf(buf, len, _("Record : %u mot%s d'affilée"), r, r > 1 ? "s" : "");
    else
        snprintf(buf, len, _("Record : %u"), r);
}

/* The game is finished: the right wing starts another one */
static bool pz_over(void) {
    switch (pz_game) {
    case PUZZLE_MINES: return mines_state == MINES_WON || mines_state == MINES_LOST;
    case PUZZLE_2048: return t48_state == T48_OVER;
    case PUZZLE_TAQUIN: return taq_solved;
    case PUZZLE_SOKOBAN: return soko_solved;
    case PUZZLE_MASTERMIND: return mm_state != MM_PLAY;
    case PUZZLE_PENDU: return pendu_state != PENDU_PLAY;
    default: return false;
    }
}

static void pz_new_round(void) {
    pz_leds(0, 0, 0);
    switch (pz_game) {
    case PUZZLE_MINES: mines_reset(); break;
    case PUZZLE_2048: t48_new(); break;
    case PUZZLE_TAQUIN: taq_new(); break;
    case PUZZLE_SOKOBAN: soko_load(soko_solved ? (soko_level + 1) % SOKO_LEVELS_N : soko_level); break;
    case PUZZLE_MASTERMIND: mm_new(); break;
    case PUZZLE_PENDU: pendu_new(); break;
    default: break;
    }
}

void puzzles_start(puzzle_t p, absolute_time_t now) {
    (void)now;
    pz_game = p < PUZZLE_COUNT ? p : PUZZLE_MINES;
    pz_help = true;
    pendu_streak = 0;
    if (pz_game == PUZZLE_SOKOBAN) {
        int level = soko_levels_solved();  /* The first level not solved yet */
        soko_load(level < SOKO_LEVELS_N ? level : SOKO_LEVELS_N - 1);
    } else {
        pz_new_round();
    }
    pz_leds(0, 0, 0);
    pz_changed = true;
}

static void pz_show_help(bool show, absolute_time_t now) {
    if (show == pz_help)
        return;
    pz_help = show;
    /* The time of the minesweeper stops on the help page */
    if (show)
        mines_pause_ts = now;
    else if (pz_game == PUZZLE_MINES && mines_state == MINES_RUN)
        mines_t0 = delayed_by_us(mines_t0, absolute_time_diff_us(mines_pause_ts, now));
}

static bool pz_leave(void) {
    if (pz_game == PUZZLE_2048)
        t48_leave();
    pz_tone(0, 0);
    pz_leds(0, 0, 0);
    return false;
}

bool puzzles_buttons(uint8_t pressed, uint8_t long_pressed, absolute_time_t now) {
    if (! pressed && ! long_pressed)
        return true;
    pz_changed = true;
    if (long_pressed & GAMES_BTN_A)
        return pz_leave();  /* Always: long press on the left wing */
    if (pz_help) {
        if (pressed & GAMES_BTN_A)
            return pz_leave();
        if (pz_game == PUZZLE_SOKOBAN && (pressed & (GAMES_BTN_X | GAMES_BTN_Y)))
            soko_choose(pressed & GAMES_BTN_X ? 1 : -1);
        if ((pressed | long_pressed) & GAMES_BTN_B)
            pz_show_help(false, now);
        return true;
    }
    if (pz_over()) {
        if (pressed & GAMES_BTN_A)
            return pz_leave();
        if ((pressed | long_pressed) & GAMES_BTN_B)
            pz_new_round();
        return true;
    }
    if (long_pressed & GAMES_BTN_X) {
        pz_show_help(true, now);
        return true;
    }
    switch (pz_game) {
    case PUZZLE_MINES: mines_buttons(pressed, long_pressed, now); break;
    case PUZZLE_2048: t48_buttons(pressed); break;
    case PUZZLE_TAQUIN: taq_buttons(pressed); break;
    case PUZZLE_SOKOBAN: soko_buttons(pressed, long_pressed); break;
    case PUZZLE_MASTERMIND: mm_buttons(pressed, long_pressed); break;
    case PUZZLE_PENDU: pendu_buttons(pressed, long_pressed); break;
    default: break;
    }
    return true;
}

bool puzzles_task(absolute_time_t now) {
    if (pz_game == PUZZLE_MINES)
        mines_task(now);
    bool c = pz_changed;
    pz_changed = false;
    return c;
}

void puzzles_render(uint8_t *fb) {
    gfx_clear(fb, GFX_WHITE);
    if (pz_help) {
        pz_help_render(fb);
        return;
    }
    switch (pz_game) {
    case PUZZLE_MINES: mines_render(fb); break;
    case PUZZLE_2048: t48_render(fb); break;
    case PUZZLE_TAQUIN: taq_render(fb); break;
    case PUZZLE_SOKOBAN: soko_render(fb); break;
    case PUZZLE_MASTERMIND: mm_render(fb); break;
    case PUZZLE_PENDU: pendu_render(fb); break;
    default: break;
    }
}

bool puzzles_calm(void) {
    /* Only the time of the minesweeper redraws the screen by itself */
    return pz_help || pz_game != PUZZLE_MINES || mines_state != MINES_RUN;
}
