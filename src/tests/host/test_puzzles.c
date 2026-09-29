/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Host tests of the puzzle games (menu/puzzles.c): rules, records, and every page drawn */

#include "pico_host.h"
#include "../../menu/puzzles.c"
#include "test.h"

uint64_t host_time_us = 0;
bool host_gpio[32];
uint8_t host_spi_log[65536];
size_t host_spi_len = 0;

/* Random numbers: a LCG, or a forced value when forced_random >= 0 */
static uint32_t seed = 1;
static int64_t forced_random = -1;
static uint32_t fake_random(void) {
    if (forced_random >= 0)
        return (uint32_t)forced_random;
    seed = seed * 1103515245u + 12345u;
    return seed >> 8;
}
static int tones = 0, saves = 0;
static void fake_tone(uint16_t hz, uint16_t ms) { (void)ms; tones += hz != 0; }
static void fake_leds(uint8_t r, uint8_t g, uint8_t b) { (void)r; (void)g; (void)b; }
static void fake_saved(void) { ++saves; }
static const games_hooks_t HOOKS = {fake_tone, fake_leds, fake_random, fake_saved, NULL, NULL};
static uint16_t recs[PUZZLE_COUNT];
static uint8_t fb[GFX_FB_SIZE];

static bool press(uint8_t b) {
    host_time_us += 100000;
    return puzzles_buttons(b, 0, host_time_us);
}

static bool hold(uint8_t b) {
    host_time_us += 1000000;
    return puzzles_buttons(0, b, host_time_us);
}

/* Starts the game and leaves its help page */
static void start(puzzle_t p) {
    puzzles_start(p, host_time_us);
    CHECK(pz_help);
    puzzles_render(fb);
    press(GAMES_BTN_B);
    CHECK(! pz_help);
    puzzles_render(fb);
}

static void render(void) {
    puzzles_render(fb);
}

/* ---- 2048 ---- */

/* Slides one line of exponents towards its first cell */
static uint32_t slide(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint32_t *gain, bool *moved) {
    uint8_t v[4] = {a, b, c, d};
    uint8_t *line[4] = {&v[0], &v[1], &v[2], &v[3]};
    *gain = 0;
    *moved = t48_slide(line, gain);
    return v[0] << 24 | v[1] << 16 | v[2] << 8 | v[3];
}

static void set_board(const uint8_t *b) {
    memcpy(t48, b, 16);
}

static int count_tiles(void) {
    int n = 0;
    for (int i = 0; i < 16; ++i)
        n += t48[i] != 0;
    return n;
}

static void test_2048(void) {
    uint32_t gain;
    bool moved;
    /* [2,2,2,2] -> [4,4,0,0] */
    CHECK_EQ(slide(1, 1, 1, 1, &gain, &moved), 0x02020000);
    CHECK_EQ(gain, 8);
    CHECK(moved);
    /* [4,4,8,0] -> [8,8,0,0]: the new 8 doesn't merge again */
    CHECK_EQ(slide(2, 2, 3, 0, &gain, &moved), 0x03030000);
    CHECK_EQ(gain, 8);
    /* [2,2,4,0] -> [4,4,0,0] */
    CHECK_EQ(slide(1, 1, 2, 0, &gain, &moved), 0x02020000);
    /* [2,0,2,0] -> [4,0,0,0] */
    CHECK_EQ(slide(1, 0, 1, 0, &gain, &moved), 0x02000000);
    CHECK_EQ(gain, 4);
    /* [0,0,0,2] -> [2,0,0,0] */
    CHECK_EQ(slide(0, 0, 0, 1, &gain, &moved), 0x01000000);
    CHECK(moved);
    CHECK_EQ(gain, 0);
    /* [2,4,8,16] can't move */
    CHECK_EQ(slide(1, 2, 3, 4, &gain, &moved), 0x01020304);
    CHECK(! moved);
    /* [4,2,2,0] -> [4,4,0,0] */
    CHECK_EQ(slide(2, 1, 1, 0, &gain, &moved), 0x02020000);
    /* [8,8,8,0] -> [16,8,0,0]: the first pair merges */
    CHECK_EQ(slide(3, 3, 3, 0, &gain, &moved), 0x04030000);

    /* The directions on a board */
    start(PUZZLE_2048);
    CHECK_EQ(count_tiles(), 2);
    static const uint8_t B1[16] = {1, 1, 2, 2,  0, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0};
    set_board(B1);
    gain = 0;
    CHECK(t48_move(DIR_RIGHT, &gain));
    CHECK_EQ(t48[3], 3);
    CHECK_EQ(t48[2], 2);
    CHECK_EQ(t48[1], 0);
    CHECK_EQ(gain, 4 + 8);
    set_board(B1);
    CHECK(t48_move(DIR_UP, &gain));
    CHECK_EQ(t48[0], 2);  /* The 2 of the bottom row goes up and merges with the first one */
    CHECK_EQ(t48[12], 0);
    CHECK_EQ(t48[3], 2);
    set_board(B1);
    CHECK(t48_move(DIR_DOWN, &gain));
    CHECK_EQ(t48[12], 2);  /* 2 + 2 in the first column */
    CHECK_EQ(t48[13], 1);
    set_board(B1);
    CHECK(t48_move(DIR_LEFT, &gain));
    CHECK_EQ(t48[0], 2);
    CHECK_EQ(t48[1], 3);
    CHECK_EQ(t48[2], 0);

    /* Through the buttons: a tile appears after each real move, the score goes up */
    set_board(B1);
    t48_score = 0;
    press(GAMES_BTN_X);  /* Right flank: right */
    CHECK_EQ(t48_score, 12);
    CHECK_EQ(count_tiles(), 4);  /* 3 + the new one */
    static const uint8_t STUCK_LEFT[16] = {1, 0, 0, 0,  2, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0};
    set_board(STUCK_LEFT);
    press(GAMES_BTN_Y);  /* Left: nothing moves, no new tile */
    CHECK_EQ(count_tiles(), 2);
    press(GAMES_BTN_A);  /* Up: nothing moves either */
    CHECK_EQ(count_tiles(), 2);
    press(GAMES_BTN_B);  /* Down: moves */
    CHECK_EQ(count_tiles(), 3);
    CHECK_EQ(t48[12], 2);

    /* New tiles: a 2 nine times in ten, a 4 otherwise */
    int fours = 0;
    for (int i = 0; i < 1000; ++i) {
        memset(t48, 0, 16);
        t48_spawn();
        CHECK_EQ(count_tiles(), 1);
        fours += memchr(t48, 2, 16) != NULL;
    }
    CHECK(fours > 50 && fours < 160);

    /* Game over: a move that leaves no move, the new tile doesn't help */
    static const uint8_t FULL[16] = {1, 2, 1, 2,  2, 1, 2, 1,  1, 2, 1, 2,  2, 1, 2, 1};
    set_board(FULL);
    CHECK(! t48_can_move());
    static const uint8_t PAIR[16] = {1, 2, 1, 2,  2, 1, 2, 1,  1, 2, 1, 2,  2, 1, 2, 2};
    set_board(PAIR);
    CHECK(t48_can_move());
    static const uint8_t LAST[16] = {3, 4, 3, 0,  5, 6, 5, 6,  6, 5, 6, 5,  5, 6, 5, 6};
    set_board(LAST);
    t48_score = 100;
    forced_random = 1;  /* The new tile is a 2, in the only free cell */
    press(GAMES_BTN_X);
    forced_random = -1;
    CHECK_EQ(t48[0], 1);
    CHECK_EQ(t48_state, T48_OVER);
    CHECK_EQ(recs[PUZZLE_2048], 100);
    CHECK(puzzles_calm());
    render();
    CHECK(press(GAMES_BTN_B));  /* Plays again */
    CHECK_EQ(t48_state, T48_PLAY);
    CHECK_EQ(t48_score, 0);
    CHECK_EQ(count_tiles(), 2);

    /* 2048 reached: a message, then the game goes on */
    static const uint8_t ALMOST[16] = {10, 10, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0};
    set_board(ALMOST);
    press(GAMES_BTN_Y);
    CHECK_EQ(t48[0], T48_GOAL);
    CHECK_EQ(t48_state, T48_WON);
    render();
    press(GAMES_BTN_A);  /* Any button closes the message, without moving */
    CHECK_EQ(t48_state, T48_PLAY);
    CHECK_EQ(t48[0], T48_GOAL);
    set_board(ALMOST);
    press(GAMES_BTN_Y);
    CHECK_EQ(t48_state, T48_PLAY);  /* Only once */

    /* Leaving keeps the best score */
    t48_score = 5000;
    CHECK(hold(GAMES_BTN_A) == false);
    CHECK_EQ(recs[PUZZLE_2048], 5000);
}

/* ---- Démineur ---- */

/* Sets the mines on the given cells and counts the neighbours, like mines_place() */
static void set_mines(const int *cells, int n) {
    mines_reset();
    for (int i = 0; i < n; ++i)
        mines[cells[i]] |= MINE_MINE;
    uint8_t nb[8];
    for (int i = 0; i < MINES_CELLS; ++i) {
        int k = mines_neighbours(i, nb), count = 0;
        for (int j = 0; j < k; ++j)
            count += (mines[nb[j]] & MINE_MINE) != 0;
        mines[i] |= count;
    }
    mines_state = MINES_RUN;
    mines_t0 = host_time_us - 100000000ull;  /* 100 s: slower than the record */
}

static int count_mines(void) {
    int n = 0;
    for (int i = 0; i < MINES_CELLS; ++i)
        n += (mines[i] & MINE_MINE) != 0;
    return n;
}

static void test_mines(void) {
    /* The first cell opened is never a mine, nor around it: it opens an area */
    for (int i = 1; i < 300; ++i) {
        seed = i;
        mines_reset();
        int c = fake_random() % MINES_CELLS;
        mines_reveal(c, host_time_us);
        CHECK_EQ(mines_state, MINES_RUN);
        CHECK_EQ(count_mines(), MINES_N);
        CHECK_EQ(mines[c] & MINE_COUNT, 0);
        CHECK(mines_opened >= 4);  /* At least the corner and its 3 neighbours */
        /* The flood fill: every open empty cell has its neighbours open, no mine is open */
        uint8_t nb[8];
        int opened = 0;
        for (int i = 0; i < MINES_CELLS; ++i) {
            if (! (mines[i] & MINE_OPEN))
                continue;
            ++opened;
            CHECK(! (mines[i] & MINE_MINE));
            if (mines[i] & MINE_COUNT)
                continue;
            int k = mines_neighbours(i, nb);
            for (int j = 0; j < k; ++j)
                CHECK(mines[nb[j]] & MINE_OPEN);
        }
        CHECK_EQ(opened, mines_opened);
    }

    /* Through the buttons: the cursor starts in the middle, a long press on the right wing opens */
    seed = 7;
    start(PUZZLE_MINES);
    CHECK_EQ(mines_cursor, 40);
    CHECK(puzzles_calm());
    press(GAMES_BTN_X);
    press(GAMES_BTN_B);
    CHECK_EQ(mines_cursor, 50);
    press(GAMES_BTN_A);
    press(GAMES_BTN_Y);
    CHECK_EQ(mines_cursor, 40);
    press(GAMES_BTN_Y);
    for (int i = 0; i < 4; ++i)
        press(GAMES_BTN_Y);
    CHECK_EQ(mines_cursor, 44);  /* Wraps around */
    hold(GAMES_BTN_B);
    CHECK_EQ(mines_state, MINES_RUN);
    CHECK(mines[44] & MINE_OPEN);
    CHECK(! puzzles_calm());  /* The time runs */
    render();

    /* The time: a redraw each second, stopped on the help page */
    puzzles_task(host_time_us);
    uint16_t s = mines_seconds;
    host_time_us += 1000000;
    CHECK(puzzles_task(host_time_us));
    CHECK_EQ(mines_seconds, s + 1);
    CHECK(! puzzles_task(host_time_us + 1000));
    hold(GAMES_BTN_X);  /* Right flank long: help */
    CHECK(pz_help);
    render();
    host_time_us += 60000000;
    puzzles_task(host_time_us);
    press(GAMES_BTN_B);
    CHECK(! pz_help);
    puzzles_task(host_time_us);
    CHECK(mines_seconds <= s + 3);

    /* A known board: mines in the right column and 3 above them. Opening a corner opens all the rest: won. */
    static const int COLUMN[MINES_N] = {8, 17, 26, 35, 44, 53, 62, 71, 80, 7, 16, 25};
    set_mines(COLUMN, MINES_N);
    mines_t0 = host_time_us - 82000000ull;  /* hold() takes 1 s */
    mines_cursor = 0;
    uint16_t before = saves;
    hold(GAMES_BTN_B);
    CHECK_EQ(mines_state, MINES_WON);
    CHECK_EQ(mines_seconds, 83);
    CHECK_EQ(recs[PUZZLE_MINES], 83);
    CHECK(saves > before);
    CHECK_EQ(mines_flags, MINES_N);
    CHECK(puzzles_calm());
    render();
    /* Slower: not a record */
    set_mines(COLUMN, MINES_N);
    mines_t0 = host_time_us - 90000000ull;
    mines_reveal(0, host_time_us);
    CHECK_EQ(mines_state, MINES_WON);
    CHECK_EQ(recs[PUZZLE_MINES], 83);

    /* Flags: a flagged cell can't be opened, the counter goes down */
    set_mines(COLUMN, MINES_N);
    mines_cursor = 8;
    hold(GAMES_BTN_Y);
    CHECK(mines[8] & MINE_FLAG);
    CHECK_EQ(mines_flags, 1);
    hold(GAMES_BTN_B);
    CHECK_EQ(mines_state, MINES_RUN);
    CHECK(! (mines[8] & MINE_OPEN));
    hold(GAMES_BTN_Y);
    CHECK(! (mines[8] & MINE_FLAG));
    CHECK_EQ(mines_flags, 0);
    render();

    /* Chord: on an open number with its mines flagged, the other cells around open */
    set_mines(COLUMN, MINES_N);
    mines_reveal(15, host_time_us);  /* Row 1, column 6: 2 mines around (7 and 16) */
    CHECK_EQ(mines[15] & MINE_COUNT, 3);  /* 7, 16, 25 */
    CHECK_EQ(mines_opened, 1);
    mines_toggle_flag(7);
    mines_toggle_flag(16);
    mines_reveal(15, host_time_us);  /* 2 flags for 3 mines: nothing */
    CHECK_EQ(mines_opened, 1);
    mines_toggle_flag(25);
    mines_reveal(15, host_time_us);
    CHECK_EQ(mines_state, MINES_WON);  /* The zero at 14 opened the whole left part */
    /* A wrong flag: the chord hits a mine */
    set_mines(COLUMN, MINES_N);
    mines_reveal(15, host_time_us);
    mines_toggle_flag(7);
    mines_toggle_flag(16);
    mines_toggle_flag(24);  /* Wrong */
    mines_reveal(15, host_time_us);
    CHECK_EQ(mines_state, MINES_LOST);
    CHECK_EQ(mines_exploded, 25);
    render();

    /* Lost: opening a mine */
    set_mines(COLUMN, MINES_N);
    mines_cursor = 80;
    hold(GAMES_BTN_B);
    CHECK_EQ(mines_state, MINES_LOST);
    CHECK_EQ(mines_exploded, 80);
    CHECK_EQ(recs[PUZZLE_MINES], 83);
    render();
    hold(GAMES_BTN_X);  /* No help page once finished */
    CHECK(! pz_help);
    press(GAMES_BTN_B);  /* Plays again */
    CHECK_EQ(mines_state, MINES_READY);
    CHECK_EQ(count_mines(), 0);
    press(GAMES_BTN_A);  /* Short left wing: up, doesn't quit while playing */
    CHECK(puzzles_buttons(GAMES_BTN_A, 0, host_time_us));
    CHECK(hold(GAMES_BTN_A) == false);
}

/* ---- Taquin ---- */

/* Solvable: the inversions plus the row of the hole (from the bottom) must be odd */
static bool taq_solvable(void) {
    int inv = 0;
    for (int i = 0; i < 16; ++i)
        for (int j = i + 1; j < 16; ++j)
            inv += taq[i] && taq[j] && taq[i] > taq[j];
    return (inv + (4 - taq_hole / 4)) % 2 == 1;
}

static void test_taquin(void) {
    for (int i = 1; i < 200; ++i) {
        seed = i;
        taq_new();
        CHECK(taq_solvable());
        CHECK(! taq_is_solved());
        uint16_t seen = 0;
        for (int i = 0; i < 16; ++i)
            seen |= 1 << taq[i];
        CHECK_EQ(seen, 0xFFFF);  /* A permutation */
        CHECK_EQ(taq[taq_hole], 0);
    }
    /* Even a stuck random generator ends */
    forced_random = 0;
    taq_new();
    forced_random = -1;
    CHECK(! taq_is_solved());

    /* From the solved board, some moves, then the inverse moves with the buttons */
    start(PUZZLE_TAQUIN);
    for (int i = 0; i < 16; ++i)
        taq[i] = (i + 1) % 16;
    taq_hole = 15;
    CHECK(! taq_slide(DIR_UP));  /* Nothing under the hole */
    CHECK(! taq_slide(DIR_LEFT));
    static const int8_t SCRAMBLE[] = {DIR_DOWN, DIR_DOWN, DIR_RIGHT, DIR_UP, DIR_RIGHT, DIR_DOWN};
    for (unsigned i = 0; i < sizeof(SCRAMBLE); ++i)
        CHECK(taq_slide(SCRAMBLE[i]));
    CHECK_EQ(taq_hole, 5);
    CHECK(! taq_is_solved());
    uint16_t before = saves;
    for (int i = sizeof(SCRAMBLE) - 1; i >= 0; --i) {
        render();
        press(DIR_BUTTONS[(SCRAMBLE[i] + 2) % 4]);
    }
    CHECK(taq_solved);
    CHECK_EQ(taq_moves, 6);
    CHECK_EQ(recs[PUZZLE_TAQUIN], 6);
    CHECK(saves > before);
    render();
    press(GAMES_BTN_X);  /* Finished: the flanks do nothing */
    CHECK(taq_solved);
    /* A move against the border doesn't count */
    press(GAMES_BTN_B);  /* Plays again */
    CHECK(! taq_solved);
    CHECK_EQ(taq_moves, 0);
    while (taq_slide(DIR_LEFT))  /* The hole to the right border */
        ;
    press(GAMES_BTN_Y);
    CHECK_EQ(taq_moves, 0);
    CHECK(hold(GAMES_BTN_A) == false);
}

/* ---- Sokoban ---- */

/* The solutions found by a breadth first search (u, r, d, l), in the order of SOKO_LEVELS */
static const char *const SOKO_SOLUTIONS[] = {
    "luruuld",
    "ruulldurrddlld",
    "uuulddurrdldrll",
    "rudrruullrrddddlluullluurdldrrr",
    "uluulllddldrruluurrrddlldlluurdldr",
    "ururrdluldllulldrrdddllurdrurrdrrull",
};

static uint8_t soko_button(char m) {
    return m == 'u' ? GAMES_BTN_A : m == 'r' ? GAMES_BTN_X : m == 'd' ? GAMES_BTN_B : GAMES_BTN_Y;
}

static int count_boxes(void) {
    int n = 0;
    for (int y = 0; y < SOKO_H; ++y)
        for (int x = 0; x < SOKO_W; ++x)
            n += (soko[y][x] & SOKO_BOX) != 0;
    return n;
}

static void test_sokoban(void) {
    CHECK_EQ(SOKO_LEVELS_N, (int)(sizeof(SOKO_SOLUTIONS) / sizeof(SOKO_SOLUTIONS[0])));
    /* Every level fits the screen and has as many boxes as targets */
    for (int l = 0; l < SOKO_LEVELS_N; ++l) {
        soko_load(l);
        CHECK(soko_w <= SOKO_W && soko_h <= SOKO_H);
        int boxes = 0, goals = 0;
        for (int y = 0; y < SOKO_H; ++y)
            for (int x = 0; x < SOKO_W; ++x) {
                boxes += (soko[y][x] & SOKO_BOX) != 0;
                goals += (soko[y][x] & SOKO_GOAL) != 0;
            }
        CHECK_EQ(boxes, goals);
        CHECK(boxes > 0);
        CHECK(! soko_is_solved());
    }

    /* The push rules on a small level */
    start(PUZZLE_SOKOBAN);
    CHECK_EQ(soko_level, 0);  /* No level solved yet: the first one */
    soko_parse("#######\n"
               "#@ $ .#\n"
               "#  $$ #\n"
               "#######");
    CHECK(! soko_undo_move());  /* Nothing to undo */
    press(GAMES_BTN_A);  /* Up: a wall */
    press(GAMES_BTN_Y);  /* Left: a wall */
    CHECK_EQ(soko_moves, 0);
    CHECK(soko_x == 1 && soko_y == 1);
    press(GAMES_BTN_B);  /* Down */
    press(GAMES_BTN_X);  /* Right */
    CHECK(soko_x == 2 && soko_y == 2);
    press(GAMES_BTN_X);  /* Two boxes in a row: blocked */
    CHECK(soko_x == 2 && soko_y == 2);
    CHECK_EQ(soko_moves, 2);
    press(GAMES_BTN_A);  /* Up, then push the box of the first row */
    press(GAMES_BTN_X);
    CHECK(soko_x == 3 && soko_y == 1);
    CHECK(soko[1][4] & SOKO_BOX);
    CHECK(! (soko[1][3] & SOKO_BOX));
    render();
    /* Undo: the box comes back */
    hold(GAMES_BTN_Y);
    CHECK(soko_x == 2 && soko_y == 1);
    CHECK(soko[1][3] & SOKO_BOX);
    CHECK(! (soko[1][4] & SOKO_BOX));
    CHECK_EQ(soko_moves, 3);
    hold(GAMES_BTN_Y);
    CHECK(soko_x == 2 && soko_y == 2);
    CHECK_EQ(count_boxes(), 3);
    /* A box against a wall can't move */
    press(GAMES_BTN_A);
    press(GAMES_BTN_X);
    press(GAMES_BTN_X);
    press(GAMES_BTN_X);  /* The box is on the target, against the wall */
    CHECK(soko_x == 4 && soko_y == 1);
    CHECK_EQ(soko[1][5], SOKO_GOAL | SOKO_BOX);
    CHECK(! soko_solved);  /* 2 boxes are not on a target */
    /* Restart the level (long press on the right wing) */
    hold(GAMES_BTN_B);
    CHECK_EQ(soko_moves, 0);
    CHECK_EQ(soko_level, 0);  /* The level of the game is loaded again */
    CHECK(soko_x == 4 && soko_y == 4);

    /* Undo more than the history: stops at its size */
    soko_parse("##########\n"
               "#@       #\n"
               "#   $ .  #\n"
               "##########");
    for (int i = 0; i < SOKO_UNDO + 10; ++i)
        press(i % 16 < 7 ? GAMES_BTN_X : GAMES_BTN_Y);
    int undone = 0;
    while (soko_undo_move())
        ++undone;
    CHECK_EQ(undone, SOKO_UNDO);

    /* Every level is solved with its solution, through the buttons, then the next level comes */
    CHECK_EQ(recs[PUZZLE_SOKOBAN], GAMES_NO_RECORD);
    soko_load(0);
    for (int l = 0; l < SOKO_LEVELS_N; ++l) {
        CHECK_EQ(soko_level, l);
        for (const char *m = SOKO_SOLUTIONS[l]; *m; ++m) {
            CHECK(! soko_solved);
            press(soko_button(*m));
        }
        CHECK(soko_solved);
        CHECK_EQ(soko_moves, strlen(SOKO_SOLUTIONS[l]));
        CHECK_EQ(recs[PUZZLE_SOKOBAN], l + 1);
        render();
        press(GAMES_BTN_B);  /* Next level */
    }
    CHECK_EQ(soko_level, 0);  /* After the last one: the first one */
    /* Solving a level already solved doesn't lower the record */
    for (const char *m = SOKO_SOLUTIONS[0]; *m; ++m)
        press(soko_button(*m));
    CHECK_EQ(recs[PUZZLE_SOKOBAN], SOKO_LEVELS_N);
    CHECK(hold(GAMES_BTN_A) == false);

    /* The help page chooses among the levels reached */
    recs[PUZZLE_SOKOBAN] = 2;
    puzzles_start(PUZZLE_SOKOBAN, host_time_us);
    CHECK_EQ(soko_level, 2);  /* The first level not solved */
    render();
    press(GAMES_BTN_X);
    CHECK_EQ(soko_level, 0);  /* Only 0 to 2 */
    press(GAMES_BTN_Y);
    CHECK_EQ(soko_level, 2);
    press(GAMES_BTN_Y);
    CHECK_EQ(soko_level, 1);
    render();
    press(GAMES_BTN_B);
    CHECK(! pz_help);
    CHECK_EQ(soko_level, 1);
    render();
    recs[PUZZLE_SOKOBAN] = SOKO_LEVELS_N;
    puzzles_start(PUZZLE_SOKOBAN, host_time_us);
    CHECK_EQ(soko_level, SOKO_LEVELS_N - 1);  /* All solved: the last one */
}

/* ---- Mastermind ---- */

static void check_score(const uint8_t *secret, const uint8_t *guess, int well, int misplaced) {
    uint8_t w, m;
    mm_score(secret, guess, &w, &m);
    CHECK_EQ(w, well);
    CHECK_EQ(m, misplaced);
}

static void test_mastermind(void) {
    check_score((uint8_t[]){0, 1, 2, 3}, (uint8_t[]){0, 1, 2, 3}, 4, 0);
    check_score((uint8_t[]){0, 1, 2, 3}, (uint8_t[]){3, 2, 1, 0}, 0, 4);
    check_score((uint8_t[]){0, 0, 1, 1}, (uint8_t[]){0, 1, 0, 5}, 1, 2);
    check_score((uint8_t[]){1, 1, 1, 1}, (uint8_t[]){1, 2, 2, 2}, 1, 0);
    check_score((uint8_t[]){1, 2, 2, 2}, (uint8_t[]){1, 1, 1, 1}, 1, 0);
    check_score((uint8_t[]){4, 5, 4, 5}, (uint8_t[]){5, 4, 5, 4}, 0, 4);
    check_score((uint8_t[]){0, 1, 2, 3}, (uint8_t[]){4, 5, 4, 5}, 0, 0);

    start(PUZZLE_MASTERMIND);
    memcpy(mm_secret, (uint8_t[]){5, 1, 2, 3}, 4);
    CHECK_EQ(mm_rows[0][0], 0);
    /* First guess 0 1 2 3: 3 well placed */
    hold(GAMES_BTN_B);
    CHECK_EQ(mm_marks[0][0], 3);
    CHECK_EQ(mm_marks[0][1], 0);
    CHECK_EQ(mm_try, 1);
    CHECK(! memcmp(mm_rows[1], mm_rows[0], 4));  /* The next row starts from the previous guess */
    render();
    /* The left wing goes to the previous symbol: 0 -> 5 */
    press(GAMES_BTN_A);
    CHECK_EQ(mm_rows[1][0], 5);
    press(GAMES_BTN_Y);  /* The cursor wraps to the last case */
    CHECK_EQ(mm_cursor, 3);
    press(GAMES_BTN_B);  /* 3 -> 4 */
    CHECK_EQ(mm_rows[1][3], 4);
    press(GAMES_BTN_A);
    press(GAMES_BTN_X);
    CHECK_EQ(mm_cursor, 0);
    CHECK_EQ(mm_rows[0][0], 0);  /* The previous rows don't change */
    render();
    hold(GAMES_BTN_B);
    CHECK_EQ(mm_state, MM_WON);
    CHECK_EQ(recs[PUZZLE_MASTERMIND], 2);
    render();
    press(GAMES_BTN_B);  /* Plays again */
    CHECK_EQ(mm_state, MM_PLAY);
    CHECK_EQ(mm_try, 0);
    /* 10 wrong tries: lost */
    memcpy(mm_secret, (uint8_t[]){5, 5, 5, 5}, 4);
    for (int t = 0; t < MM_TRIES; ++t) {
        CHECK_EQ(mm_state, MM_PLAY);
        hold(GAMES_BTN_B);
    }
    CHECK_EQ(mm_state, MM_LOST);
    CHECK_EQ(mm_try, MM_TRIES - 1);
    CHECK_EQ(recs[PUZZLE_MASTERMIND], 2);
    render();
    /* The secret codes use the 6 symbols */
    uint8_t seen = 0;
    for (int i = 1; i < 50; ++i) {
        seed = i;
        mm_new();
        for (int i = 0; i < MM_PEGS; ++i) {
            CHECK(mm_secret[i] < MM_SYMBOLS);
            seen |= 1 << mm_secret[i];
        }
    }
    CHECK_EQ(seen, 0x3F);
    CHECK(hold(GAMES_BTN_A) == false);
}

/* ---- Pendu ---- */

static void propose(char letter) {
    pendu_cursor = letter - 'A';
    hold(GAMES_BTN_B);
}

static void test_pendu(void) {
    /* The words: capital letters only, they fit the screen */
    for (int w = 0; w < PENDU_WORDS_N; ++w) {
        int len = strlen(PENDU_WORDS[w]);
        CHECK(len >= 4 && len * PENDU_LETTER_W <= GFX_WIDTH - 16);
        for (const char *c = PENDU_WORDS[w]; *c; ++c)
            CHECK(*c >= 'A' && *c <= 'Z');
    }

    start(PUZZLE_PENDU);
    pendu_word = 0;  /* BADGE */
    CHECK_STR(PENDU_WORDS[pendu_word], "BADGE");
    /* The cursor: flanks previous / next letter, wings the other row */
    press(GAMES_BTN_X);
    CHECK_EQ(pendu_cursor, 1);
    press(GAMES_BTN_B);
    CHECK_EQ(pendu_cursor, 14);
    press(GAMES_BTN_A);
    CHECK_EQ(pendu_cursor, 1);
    press(GAMES_BTN_Y);
    press(GAMES_BTN_Y);
    CHECK_EQ(pendu_cursor, 25);
    hold(GAMES_BTN_B);  /* Z: wrong */
    CHECK_EQ(pendu_errors, 1);
    hold(GAMES_BTN_B);  /* Z again: not counted */
    CHECK_EQ(pendu_errors, 1);
    propose('B');
    propose('A');
    propose('D');
    propose('G');
    CHECK_EQ(pendu_state, PENDU_PLAY);
    render();
    propose('E');
    CHECK_EQ(pendu_state, PENDU_WON);
    CHECK_EQ(pendu_streak, 1);
    CHECK_EQ(recs[PUZZLE_PENDU], 1);
    render();
    press(GAMES_BTN_B);  /* Next word: another one */
    CHECK_EQ(pendu_state, PENDU_PLAY);
    CHECK(pendu_word != 0);
    /* A second word found in a row */
    pendu_word = 0;
    for (const char *c = "BADGE"; *c; ++c)
        propose(*c);
    CHECK_EQ(pendu_streak, 2);
    CHECK_EQ(recs[PUZZLE_PENDU], 2);
    press(GAMES_BTN_B);
    /* 8 errors: lost, the series starts again */
    pendu_word = 0;
    for (const char *c = "QWXYZKLM"; *c; ++c) {
        CHECK_EQ(pendu_state, PENDU_PLAY);
        propose(*c);
        render();
    }
    CHECK_EQ(pendu_errors, PENDU_ERRORS);
    CHECK_EQ(pendu_state, PENDU_LOST);
    CHECK_EQ(pendu_streak, 0);
    CHECK_EQ(recs[PUZZLE_PENDU], 2);
    CHECK(press(GAMES_BTN_A) == false);  /* Finished: the short left wing goes back to the menu */
}

int main(void) {
    memset(recs, 0, sizeof(recs));  /* A new store */
    puzzles_init(&HOOKS, recs);
    for (int p = 0; p < PUZZLE_COUNT; ++p)
        CHECK_EQ(recs[p], GAMES_NO_RECORD);
    CHECK_STR(puzzles_name(PUZZLE_MINES), "Démineur");
    CHECK_STR(puzzles_name(PUZZLE_SOKOBAN), "Sokoban");
    char text[48];
    puzzles_record_text(PUZZLE_TAQUIN, text, sizeof(text));
    CHECK_STR(text, "Pas encore de record");

    /* The help page: the right wing plays, the left wing leaves */
    puzzles_start(PUZZLE_2048, host_time_us);
    CHECK(pz_help);
    CHECK(puzzles_calm());
    CHECK(puzzles_task(host_time_us));
    CHECK(! puzzles_task(host_time_us));
    CHECK(press(GAMES_BTN_A) == false);
    puzzles_start(PUZZLE_2048, host_time_us);
    CHECK(hold(GAMES_BTN_A) == false);
    puzzles_start(PUZZLE_2048, host_time_us);
    CHECK(press(GAMES_BTN_B));
    CHECK(! pz_help);
    CHECK(hold(GAMES_BTN_X));  /* Back to the help */
    CHECK(pz_help);
    CHECK(puzzles_buttons(0, 0, host_time_us));

    test_2048();
    test_mines();
    test_taquin();
    test_sokoban();
    test_mastermind();
    test_pendu();

    puzzles_record_text(PUZZLE_MINES, text, sizeof(text));
    CHECK_STR(text, "Record : 83 s");
    puzzles_record_text(PUZZLE_SOKOBAN, text, sizeof(text));
    CHECK_STR(text, "Niveaux réussis : 6 / 6");
    puzzles_record_text(PUZZLE_PENDU, text, sizeof(text));
    CHECK_STR(text, "Record : 2 mots d'affilée");

    /* Every page drawn, in every game: the texts fit the screen */
    for (int p = 0; p < PUZZLE_COUNT; ++p) {
        puzzles_start(p, host_time_us);
        render();
        press(GAMES_BTN_B);
        render();
    }
    CHECK(pz_text_max > 100);
    CHECK(pz_text_max <= GFX_WIDTH - 4);
    CHECK(tones > 0);
    CHECK(saves >= 6);

    TEST_END();
}
