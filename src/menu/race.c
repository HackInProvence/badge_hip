/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Mini-bolides: a race of small cars seen from above, like Micro Machines, for 2 to 4 cicadas, in the lobby of
 * party.c. Every badge drives its car on the same track (TRACK, 384 x 288 pixels: more than the screen, which follows
 * the car); LAPS laps, the checkpoints 1, 2, 3 in order (a shortcut through the grass does not count).
 * The right flank accelerates, the left one brakes then backs up, the wings turn; the grass slows the car down.
 * Each badge sends the state of its car now and then (K_STATE, to everybody) and draws the other cars from the last
 * state received, moved on with their speed (the screen shows ~4 images per second, the physics runs at 30 Hz).
 * The start is the same on every badge (the start time of party.c), the result too: the finish times travel in
 * the states.
 * K_STATE [x 2][y 2, pixels][angle: 0..31][speed: pixels/s, signed][laps done, signed][checkpoint][finish 2, 10 ms] */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "achievements.h"
#include "app.h"
#include "i18n.h"
#include "net.h"
#include "party.h"

#define MAX_CARS 4
#define MIN_PLAYERS 2
#define LAPS 3
#define START_DELAY_MS 2500
#define GRID_MS 3000  /* The cars on the grid */
#define COUNTDOWN_MS 3000
#define AFTER_FIRST_MS 30000  /* After the first car finished, the others have this long */
#define LEAVE_AFTER_MS 5000
#define TICK_MS 33
#define FRAME_MS 250  /* The e-paper takes ~0.3 s for a fast refresh */
#define SEND_BASE_MS 200  /* Period of the states: more cars, slower */
#define SEND_PER_CAR_MS 60
#define HELD_TIMEOUT_MS 120  /* The buttons callback comes at every turn of the loop while a button is held */
#define K_STATE PARTY_KIND_GAME

/* The physics, in pixels and seconds */
#define V_ROAD 62.0f
#define V_GRASS 22.0f
#define V_BACK (-18.0f)
#define ACCEL 55.0f
#define BRAKE 95.0f
#define DRAG 30.0f
#define TURN_MS 110  /* A step of the 32 directions while a wing is held */
#define DIRS 32

/* The track: '#' grass, '.' road, 'S' the start line */
#define TILE 16
#define TRACK_W 24
#define TRACK_H 18
static const char TRACK[TRACK_H][TRACK_W + 1] = {
    "########################",
    "##.....S..............##",
    "#......S...............#",
    "#......S...............#",
    "#...################...#",
    "#...################...#",
    "#...#####......#####...#",
    "#...#####......#####...#",
    "#...#####..##..#####...#",
    "#...#####..##..#####...#",
    "#...#####..##..#####...#",
    "#...#####..##..#####...#",
    "#...#####..##..#####...#",
    "#...#####..##..#####...#",
    "#..........##..........#",
    "#..........##..........#",
    "##.........##.........##",
    "########################",
};
/* The start line and the checkpoints 1, 2, 3: bands across the road and the grass beside it (cutting a turn over
 * the grass still passes them; skipping one makes the lap not count): x0, y0, x1, y1 in pixels */
static const int16_t LINES[4][4] = {
    {112, 0, 127, 63},  /* The start line, on the top straight */
    {320, 128, 383, 143},  /* 1: the right side */
    {176, 64, 191, 143},  /* 2: the top of the hairpin, and the grass above it */
    {0, 128, 63, 143},  /* 3: the left side */
};
#define WORLD_W (TRACK_W * TILE)
#define WORLD_H (TRACK_H * TILE)
/* The grid, behind the start line (x of the line: 7 * 16), heading east */
static const int16_t GRID[MAX_CARS][2] = {{98, 26}, {98, 54}, {74, 40}, {50, 26}};

enum { P_HOME, P_SCAN, P_LOBBY, P_GAME };
enum { PH_WAIT, PH_GRID, PH_COUNTDOWN, PH_RACE, PH_RESULT };

typedef struct {
    uint32_t id;
    char name[9];
    float x, y, speed;
    int angle;  /* 0..DIRS-1, 0 = east, clockwise (y down) */
    int8_t laps;  /* Laps done (-1 before the start line) */
    uint8_t cp;  /* The last checkpoint passed: 0 the start line, 1..3 */
    uint16_t finish;  /* Race time when it finished (10 ms), 0: not yet */
    absolute_time_t seen;  /* The last state received (the other cars) */
} car_t;

extern const app_t app_race;

static int page = P_HOME, sel = 0;
static bool changed = false;
static party_open_t found[PARTY_MAX_OPEN];
static int n_found = 0;
static absolute_time_t list_ts = 0;
static party_player_t lobby[MAX_CARS];
static int n_lobby = 0;
static party_state_t last_party = PARTY_IDLE;
/* The race */
static bool in_game = false, ready = false, finished_all = false, left_party = false, cancelled = false;
static car_t cars[MAX_CARS];
static int n = 0, me = -1, phase = PH_WAIT, countdown_shown = 0;
static absolute_time_t t0 = 0, tick_ts = 0, send_ts = 0, frame_ts = 0, turn_ts = 0, held_ts = 0, first_ts = 0,
                       end_ts = 0;
static uint8_t held = 0;  /* The buttons held now (buttons()) */
static int order[MAX_CARS];  /* The ranking, index in cars */

static bool on_page(void) {
    return app_current() == &app_race;
}

static absolute_time_t race_start(void) {
    return delayed_by_ms(t0, GRID_MS + COUNTDOWN_MS);
}

static char tile_at(float x, float y) {
    int c = (int)x / TILE, r = (int)y / TILE;
    if (x < 0 || y < 0 || c >= TRACK_W || r >= TRACK_H)
        return '#';
    return TRACK[r][c];
}

/* cos and sin of the 32 directions (0 = east, y down) */
static float dir_cos(int a) {
    return cosf(a * 6.2831853f / DIRS);
}

static float dir_sin(int a) {
    return sinf(a * 6.2831853f / DIRS);
}

/* The progress of a car (for the ranking): laps then checkpoints, a finished car ahead by its time */
static int32_t progress(const car_t *c) {
    if (c->finish)
        return 1000000 - c->finish;
    return (c->laps + 1) * 4 + c->cp;
}

static void rank(void) {
    for (int i = 0; i < n; ++i)
        order[i] = i;
    for (int i = 1; i < n; ++i)
        for (int j = i; j > 0 && progress(&cars[order[j]]) > progress(&cars[order[j - 1]]); --j) {
            int t = order[j];
            order[j] = order[j - 1];
            order[j - 1] = t;
        }
}

static int my_rank(void) {
    for (int i = 0; i < n; ++i)
        if (order[i] == me)
            return i + 1;
    return n;
}

/* The line (0: start, 1..3: checkpoints) at (x, y), -1: none */
static int line_at(float x, float y) {
    for (int i = 0; i < 4; ++i)
        if (x >= LINES[i][0] && x <= LINES[i][2] && y >= LINES[i][1] && y <= LINES[i][3])
            return i;
    return -1;
}

/* A car passes over a line: the checkpoints in order, a lap at the start line */
static void checkpoint(car_t *c, int line, absolute_time_t now) {
    if (line >= 1 && line == c->cp + 1)
        c->cp = line;
    else if (line == 0 && c->cp == 3) {
        c->cp = 0;
        ++c->laps;
        if (c->laps > 0) {
            printf("race: lap %d, %s\n", c->laps, c->name);
            if (on_page() && c->laps < LAPS)
                app_tone(1319, 120);
        }
        if (c->laps >= LAPS && ! c->finish) {
            int64_t ms = absolute_time_diff_us(race_start(), now) / 1000;
            c->finish = ms / 10 > 0 ? (ms / 10 > 65535 ? 65535 : ms / 10) : 1;
            if (! first_ts)
                first_ts = now;
            printf("race: %s finished in %u.%02u s\n", c->name, c->finish / 100, c->finish % 100);
            if (on_page())
                app_tone(1760, 400);
        }
    }
}

/* The physics of this badge's car, one tick */
static void drive(car_t *c, float dt, absolute_time_t now) {
    bool racing = phase == PH_RACE && ! c->finish;
    bool gas = racing && (held & UI_BTN_X), brake = racing && (held & UI_BTN_Y);
    float vmax = tile_at(c->x, c->y) == '#' ? V_GRASS : V_ROAD;
    if (gas)
        c->speed += ACCEL * dt;
    else if (brake)
        c->speed -= (c->speed > 0 ? BRAKE : ACCEL * 0.5f) * dt;
    else if (c->speed > 0)
        c->speed = c->speed > DRAG * dt ? c->speed - DRAG * dt : 0;
    else if (c->speed < 0)
        c->speed = -c->speed > DRAG * dt ? c->speed + DRAG * dt : 0;
    if (c->speed > vmax)
        c->speed = c->speed - BRAKE * dt > vmax ? c->speed - BRAKE * dt : vmax;  /* Into the grass: slowed down */
    if (c->speed < V_BACK)
        c->speed = V_BACK;
    /* The wings turn while held, when the car moves or the gas / brake is held (stopped against an edge, it must
     * be able to turn away); backing up: the other way, like a car */
    if ((held & (UI_BTN_A | UI_BTN_B)) && (c->speed > 4 || c->speed < -4 || gas || brake)
            && absolute_time_diff_us(turn_ts, now) >= TURN_MS * 1000) {
        int d = (held & UI_BTN_B ? 1 : -1) * (c->speed < 0 ? -1 : 1);
        c->angle = (c->angle + d + DIRS) % DIRS;
        turn_ts = now;
    }
    float x = c->x + c->speed * dir_cos(c->angle) * dt, y = c->y + c->speed * dir_sin(c->angle) * dt;
    if (x < 6 || x > WORLD_W - 6 || y < 6 || y > WORLD_H - 6) {
        c->speed = 0;  /* The edge of the world: a wall */
        return;
    }
    c->x = x;
    c->y = y;
    checkpoint(c, line_at(x, y), now);
}

/* The other cars: moved on from their last state (at most 1 s: a car not heard any more stops) */
static void coast(car_t *c, float dt, absolute_time_t now) {
    if (c->finish || absolute_time_diff_us(c->seen, now) > 1000000)
        return;
    float x = c->x + c->speed * dir_cos(c->angle) * dt, y = c->y + c->speed * dir_sin(c->angle) * dt;
    if (x >= 6 && x <= WORLD_W - 6 && y >= 6 && y <= WORLD_H - 6) {
        c->x = x;
        c->y = y;
    }
}

static void handler(uint8_t kind, uint32_t from, uint32_t to, const uint8_t *d, uint8_t len, int rssi) {
    (void)to;
    (void)rssi;
    if (kind != K_STATE || len < 10 || ! in_game || ! ready)
        return;
    for (int i = 0; i < n; ++i) {
        if (cars[i].id != from || i == me)
            continue;
        car_t *c = &cars[i];
        c->x = d[0] | d[1] << 8;
        c->y = d[2] | d[3] << 8;
        c->angle = d[4] % DIRS;
        c->speed = (int8_t)d[5];
        c->laps = (int8_t)d[6];
        c->cp = d[7] & 3;
        uint16_t f = d[8] | d[9] << 8;
        if (f && ! c->finish) {
            c->finish = f;
            if (! first_ts)
                first_ts = get_absolute_time();
            printf("race: %s finished in %u.%02u s\n", c->name, f / 100, f % 100);
        }
        c->seen = get_absolute_time();
        changed = true;
    }
}

static void send_state(void) {
    const car_t *c = &cars[me];
    int x = (int)c->x, y = (int)c->y, v = (int)c->speed;
    uint8_t d[10] = {x, x >> 8, y, y >> 8, c->angle, (uint8_t)(int8_t)v, (uint8_t)c->laps, c->cp, c->finish,
                     c->finish >> 8};
    party_send(K_STATE, 0, d, sizeof(d));
}

static void reset_game(void) {
    in_game = ready = finished_all = left_party = cancelled = false;
    n = 0;
    me = -1;
    phase = PH_WAIT;
    countdown_shown = 0;
    first_ts = end_ts = 0;
    held = 0;
}

/* The roster complete: the cars on the grid, in the order of the roster */
static void take_roster(void) {
    n = party_players(lobby, MAX_CARS);
    for (int i = 0; i < n; ++i)
        if (! lobby[i].id)
            return;  /* A page is missing */
    me = party_index(net_id());
    if (me < 0 || me >= MAX_CARS)
        return;
    for (int i = 0; i < n; ++i) {
        car_t *c = &cars[i];
        memset(c, 0, sizeof(*c));
        c->id = lobby[i].id;
        snprintf(c->name, sizeof(c->name), "%s", lobby[i].name);
        c->x = GRID[i][0];
        c->y = GRID[i][1];
        c->laps = -1;
        c->cp = 3;  /* The start line ahead: crossing it starts the first lap */
    }
    ready = true;
    changed = true;
    rank();
    printf("race: %d cars, I am car %d\n", n, me + 1);
}

static void result(void) {
    finished_all = true;
    rank();
    achv_add(ACHV_CNT_GAMES, 1);
    bool won = order[0] == me && cars[me].finish;
    if (won)
        achv_add(ACHV_CNT_WINS, 1);
    printf("race: result, I am %d of %d%s\n", my_rank(), n, won ? " (won)" : "");
    for (int i = 0; i < n; ++i) {
        const car_t *c = &cars[order[i]];
        printf("race: %d. %s %s\n", i + 1, c->name, c->finish ? "finished" : "not finished");
    }
    if (on_page()) {
        app_tone(won ? 1319 : 440, 400);
        app_leds(won ? 0 : 255, won ? 255 : 80, 0);
    }
}

/* Runs the race, from the page and from the main loop (the race goes on while another page is shown) */
void race_service(absolute_time_t now) {
    if (! in_game) {
        if (party_state() == PARTY_STARTED && party_game() == PARTY_GAME_RACE) {
            reset_game();
            in_game = true;
            t0 = party_start_time();
            party_set_handler(handler);
            page = P_GAME;
            changed = true;
            printf("race: started, %d players\n", party_count());
        }
        return;
    }
    if (! finished_all && ! left_party && party_state() != PARTY_STARTED) {
        if (! cancelled)
            printf("race: party cancelled\n");
        cancelled = true;
        changed = true;
    }
    if (! ready) {
        take_roster();
        if (! ready)
            return;
    }
    int ph = absolute_time_diff_us(now, delayed_by_ms(t0, GRID_MS)) > 0 ? PH_GRID
             : absolute_time_diff_us(now, race_start()) > 0 ? PH_COUNTDOWN : finished_all ? PH_RESULT : PH_RACE;
    if (ph != phase) {
        phase = ph;
        changed = true;
        if (ph == PH_RACE) {
            printf("race: go!\n");
            if (on_page()) {
                app_tone(1760, 300);
                app_leds(0, 255, 0);
            }
        }
    }
    if (ph == PH_COUNTDOWN) {
        int left = (absolute_time_diff_us(now, race_start()) + 999999) / 1000000;
        if (left != countdown_shown) {
            countdown_shown = left;
            changed = true;
            if (on_page()) {
                app_tone(880, 120);
                app_leds(255, left > 1 ? 0 : 160, 0);
            }
        }
    }
    /* The buttons held: the callback comes while one is down; nothing for a while: all up */
    if (held && absolute_time_diff_us(held_ts, now) > HELD_TIMEOUT_MS * 1000)
        held = 0;
    /* The physics */
    if (absolute_time_diff_us(tick_ts, now) >= TICK_MS * 1000) {
        float dt = absolute_time_diff_us(tick_ts, now) / 1e6f;
        if (dt > 0.1f)
            dt = 0.1f;
        tick_ts = now;
        if (ph >= PH_RACE) {
            drive(&cars[me], dt, now);
            for (int i = 0; i < n; ++i)
                if (i != me)
                    coast(&cars[i], dt, now);
            rank();
            changed = true;
        }
    }
    /* This badge's car for the others (on the grid too: they see it before the start) */
    if (! left_party && absolute_time_diff_us(send_ts, now) >= 0) {
        send_state();
        send_ts = delayed_by_ms(now, SEND_BASE_MS + SEND_PER_CAR_MS * n);
    }
    /* The end: every car finished, or AFTER_FIRST_MS after the first one */
    if (ph == PH_RACE && ! finished_all) {
        bool all = true;
        for (int i = 0; i < n; ++i)
            all &= cars[i].finish != 0;
        if (all || (first_ts && absolute_time_diff_us(delayed_by_ms(first_ts, AFTER_FIRST_MS), now) >= 0)) {
            result();
            end_ts = now;
            changed = true;
        }
    }
    if (finished_all && ! left_party && absolute_time_diff_us(delayed_by_ms(end_ts, LEAVE_AFTER_MS), now) >= 0) {
        left_party = true;
        if (party_state() == PARTY_STARTED && party_game() == PARTY_GAME_RACE)
            party_leave();  /* Another game can start; the result stays on the page */
        printf("race: left the party\n");
    }
}


/* ------ The page ------ */

static void found_label(int i, char *buf, size_t len) {
    snprintf(buf, len, found[i].players > 1 ? _("%s  %u joueurs") : _("%s  %u joueur"), found[i].name,
             found[i].players);
}

static void home_label(int i, char *buf, size_t len) {
    snprintf(buf, len, "%s", i ? N_("Rejoindre une partie") : N_("Créer une partie"));
}

static void race_start_page(absolute_time_t now) {
    race_service(now);
    if (in_game)
        page = P_GAME;
    else if (party_game() == PARTY_GAME_RACE && (party_state() == PARTY_HOSTING || party_state() == PARTY_JOINING
                                                 || party_state() == PARTY_JOINED)) {
        page = P_LOBBY;
        party_set_handler(handler);
    } else
        page = P_HOME;
    list_ts = 0;
    changed = true;
}

static const char *busy_game(void) {
    if (party_state() != PARTY_STARTED || party_game() == PARTY_GAME_RACE)
        return NULL;
    return party_game() == PARTY_GAME_TUG ? N_("Tir à la corde") : party_game() == PARTY_GAME_ASSASSIN
           ? N_("Assassin") : party_game() == PARTY_GAME_WEREWOLF ? N_("Loup-garou") : "?";
}

static void back_home(void) {
    if (party_game() == PARTY_GAME_RACE && party_state() != PARTY_IDLE)
        party_leave();
    reset_game();
    page = P_HOME;
    sel = 0;
    changed = true;
}

static bool race_buttons(const app_buttons_t *b, absolute_time_t now) {
    if (page == P_GAME && (phase == PH_RACE || phase == PH_COUNTDOWN) && ! cancelled) {
        held = b->held;  /* The race: the buttons drive (a long press on G does not leave) */
        held_ts = now;
        return true;
    }
    if (b->long_pressed & UI_BTN_A)
        return false;  /* Leaves the page: the party goes on */
    switch (page) {
    case P_HOME:
        if (b->pressed & UI_BTN_A)
            return false;
        if (b->pressed & (UI_BTN_X | UI_BTN_Y))
            sel ^= 1;
        if ((b->pressed & UI_BTN_B) && ! busy_game()) {
            party_set_handler(handler);
            if (sel == 0) {
                party_host(PARTY_GAME_RACE, true, MAX_CARS, 0);
                printf("race: hosting\n");
                page = P_LOBBY;
            } else {
                party_scan(PARTY_GAME_RACE);
                printf("race: scanning\n");
                n_found = sel = 0;
                list_ts = 0;
                page = P_SCAN;
            }
        }
        break;
    case P_SCAN:
        if (b->released_short & UI_BTN_A) {
            party_leave();
            page = P_HOME;
            sel = 1;
        }
        if (n_found && (b->pressed & UI_BTN_Y))
            sel = (sel + n_found - 1) % n_found;
        if (n_found && (b->pressed & UI_BTN_X))
            sel = (sel + 1) % n_found;
        if (n_found && (b->pressed & UI_BTN_B)) {
            party_set_handler(handler);
            party_join(found[sel].host);
            printf("race: joining %s\n", found[sel].name);
            page = P_LOBBY;
        }
        break;
    case P_LOBBY:
        if (party_state() == PARTY_CANCELLED || party_state() == PARTY_IDLE) {
            if (b->pressed & (UI_BTN_A | UI_BTN_B))
                back_home();
            break;
        }
        if (b->released_short & UI_BTN_A) {
            printf("race: left the lobby\n");
            back_home();
            break;
        }
        if ((b->pressed & UI_BTN_B) && party_state() == PARTY_HOSTING && party_count() >= MIN_PLAYERS) {
            printf("race: start, %d players\n", party_count());
            party_start(START_DELAY_MS);
            race_service(now);
        }
        break;
    default:
        if ((phase == PH_RESULT || cancelled) && (b->pressed & UI_BTN_B))
            back_home();
        else if ((phase == PH_RESULT || cancelled) && (b->released_short & UI_BTN_A)) {
            back_home();
            return false;
        }
        break;
    }
    return true;
}

static bool race_task(absolute_time_t now) {
    race_service(now);
    if (page == P_SCAN && absolute_time_diff_us(list_ts, now) > 1000000) {
        list_ts = now;
        n_found = party_found(found, PARTY_MAX_OPEN);
        if (sel >= n_found)
            sel = n_found ? n_found - 1 : 0;
        changed = true;
    }
    if (page == P_LOBBY) {
        if (party_state() != last_party) {
            last_party = party_state();
            changed = true;
        }
        if (absolute_time_diff_us(list_ts, now) > 1000000) {
            list_ts = now;
            n_lobby = party_players(lobby, MAX_CARS);
            changed = true;
        }
    }
    if (! changed || absolute_time_diff_us(frame_ts, now) < FRAME_MS * 1000ll)
        return false;  /* Not faster than the screen */
    frame_ts = now;
    changed = false;
    return true;
}

/* The track seen from (cx, cy), the top left corner of the screen in the world, below the bar of the race */
#define BAR_H 18

static void draw_track(uint8_t *fb, int cx, int cy) {
    for (int sy = BAR_H; sy < GFX_HEIGHT; ++sy) {
        int wy = cy + sy;
        for (int sx = 0; sx < GFX_WIDTH; ++sx) {
            int wx = cx + sx;
            char t = tile_at(wx, wy);
            bool ink = false;
            if (t == '#')
                ink = wy % 4 == 0 && (wx + (wy / 4) % 2 * 2) % 4 == 0;  /* Grass: sparse dots */
            else if (t == 'S')
                ink = (wx / 4 + wy / 4) % 2;  /* The checkered start line */
            if (ink)
                gfx_pixel(fb, sx, sy, GFX_BLACK);
        }
    }
    /* The edges of the road: a line between road and grass (easier to see at 4 images per second) */
    for (int r = 0; r < TRACK_H; ++r)
        for (int c = 0; c < TRACK_W; ++c) {
            if (TRACK[r][c] == '#')
                continue;
            int x = c * TILE - cx, y = r * TILE - cy;
            if (x < -TILE || x > GFX_WIDTH || y < BAR_H - TILE || y > GFX_HEIGHT)
                continue;
            if (r > 0 && TRACK[r - 1][c] == '#' && y >= BAR_H)
                gfx_fill_rect(fb, x, y, TILE, 1, GFX_BLACK);
            if (r < TRACK_H - 1 && TRACK[r + 1][c] == '#' && y + TILE - 1 >= BAR_H)
                gfx_fill_rect(fb, x, y + TILE - 1, TILE, 1, GFX_BLACK);
            if (c > 0 && TRACK[r][c - 1] == '#')
                for (int k = 0; k < TILE; ++k)
                    if (y + k >= BAR_H)
                        gfx_pixel(fb, x, y + k, GFX_BLACK);
            if (c < TRACK_W - 1 && TRACK[r][c + 1] == '#')
                for (int k = 0; k < TILE; ++k)
                    if (y + k >= BAR_H)
                        gfx_pixel(fb, x + TILE - 1, y + k, GFX_BLACK);
        }
}

/* A car: a capsule of 3 squares along its direction, the front marked; this badge's car black, the others hollow */
static void draw_car(uint8_t *fb, const car_t *c, int cx, int cy, bool mine) {
    float dx = dir_cos(c->angle), dy = dir_sin(c->angle);
    int x0 = (int)c->x - cx, y0 = (int)c->y - cy;
    if (x0 < -10 || x0 > GFX_WIDTH + 10 || y0 < BAR_H - 10 || y0 > GFX_HEIGHT + 10)
        return;
    for (int k = -1; k <= 1; ++k) {
        int x = x0 + (int)lroundf(dx * 4 * k), y = y0 + (int)lroundf(dy * 4 * k);
        if (y - 3 >= BAR_H) {
            gfx_fill_rect(fb, x - 3, y - 3, 7, 7, GFX_BLACK);
            if (! mine)
                gfx_fill_rect(fb, x - 2, y - 2, 5, 5, GFX_WHITE);
        }
    }
    if (! mine)
        for (int k = -1; k <= 1; ++k) {  /* The outline again over the inside of the next square */
            int x = x0 + (int)lroundf(dx * 4 * k), y = y0 + (int)lroundf(dy * 4 * k);
            if (y - 2 >= BAR_H)
                gfx_fill_rect(fb, x - 1, y - 1, 3, 3, GFX_WHITE);
        }
    int fx = x0 + (int)lroundf(dx * 7), fy = y0 + (int)lroundf(dy * 7);  /* The front */
    if (fy - 1 >= BAR_H)
        gfx_fill_rect(fb, fx - 1, fy - 1, 3, 3, mine ? GFX_WHITE : GFX_BLACK);
}

static void render_race(uint8_t *fb) {
    const car_t *m = &cars[me];
    int cx = (int)m->x - GFX_WIDTH / 2, cy = (int)m->y - (GFX_HEIGHT + BAR_H) / 2;
    if (cx < 0) cx = 0;
    if (cy < -BAR_H) cy = -BAR_H;
    if (cx > WORLD_W - GFX_WIDTH) cx = WORLD_W - GFX_WIDTH;
    if (cy > WORLD_H - GFX_HEIGHT) cy = WORLD_H - GFX_HEIGHT;
    draw_track(fb, cx, cy);
    for (int i = 0; i < n; ++i)
        if (i != me)
            draw_car(fb, &cars[i], cx, cy, false);
    draw_car(fb, m, cx, cy, true);
    /* The bar: the lap, the rank, the countdown */
    char text[40];
    gfx_fill_rect(fb, 0, 0, GFX_WIDTH, BAR_H - 2, GFX_BLACK);
    int lap = m->laps < 0 ? 1 : m->laps + 1 > LAPS ? LAPS : m->laps + 1;
    if (phase == PH_COUNTDOWN)
        snprintf(text, sizeof(text), "%d", countdown_shown);
    else if (phase == PH_GRID)
        snprintf(text, sizeof(text), "%s", _("Sur la grille"));
    else if (m->finish)
        snprintf(text, sizeof(text), _("Arrivé %u.%02u s"), m->finish / 100, m->finish % 100);
    else
        snprintf(text, sizeof(text), _("Tour %d/%d"), lap, LAPS);
    gfx_text(fb, 4, 0, &gfx_font_small, text, GFX_WHITE, GFX_ALIGN_LEFT);
    snprintf(text, sizeof(text), "%d/%d", my_rank(), n);
    gfx_text(fb, GFX_WIDTH - 4, 0, &gfx_font_small, text, GFX_WHITE, GFX_ALIGN_RIGHT);
    if (phase == PH_GRID) {  /* The controls, before the start */
        gfx_fill_rect(fb, 0, UI_FOOTER_Y - 3, GFX_WIDTH, GFX_HEIGHT - UI_FOOTER_Y + 3, GFX_WHITE);
        ui_footer(fb, N_("Flanc D : gaz  G / D : tourner"));
    }
}

static void render_result(uint8_t *fb) {
    char text[48];
    ui_title(fb, N_("Arrivée"));
    int y = UI_TITLE_H + 4;
    for (int i = 0; i < n; ++i) {
        const car_t *c = &cars[order[i]];
        if (c->finish)
            snprintf(text, sizeof(text), "%d. %s  %u.%02u s", i + 1, c->name, c->finish / 100, c->finish % 100);
        else
            snprintf(text, sizeof(text), _("%d. %s  (tour %d)"), i + 1, c->name, c->laps < 0 ? 1 : c->laps + 1);
        if (order[i] == me) {
            gfx_fill_rect(fb, 2, y - 1, GFX_WIDTH - 4, 20, GFX_BLACK);
            gfx_text(fb, 8, y, &gfx_font_medium, text, GFX_WHITE, GFX_ALIGN_LEFT);
        } else {
            gfx_text(fb, 8, y, &gfx_font_medium, text, GFX_BLACK, GFX_ALIGN_LEFT);
        }
        y += 24;
    }
    ui_footer(fb, N_("G : retour  D : rejouer"));
}

static void render_lobby(uint8_t *fb) {
    char text[48];
    switch (party_state()) {
    case PARTY_HOSTING:
        ui_title(fb, N_("Ta partie"));
        snprintf(text, sizeof(text), _("%d / %d pilotes"), n_lobby, MAX_CARS);
        ui_lines(fb, UI_TITLE_H + 4, &gfx_font_small, n_lobby >= MIN_PLAYERS ? text : N_("Il faut 2 joueurs au moins."));
        break;
    case PARTY_JOINING:
        ui_title(fb, N_("Rejoindre"));
        ui_lines(fb, 70, &gfx_font_small, N_("Connexion..."));
        ui_footer(fb, N_("G : annuler"));
        return;
    case PARTY_JOINED:
        ui_title(fb, N_("Salle d'attente"));
        snprintf(text, sizeof(text), _("Partie de %s"), party_name(party_host_id()));
        ui_lines(fb, UI_TITLE_H + 4, &gfx_font_small, text);
        break;
    default:
        ui_title(fb, N_("Mini-bolides"));
        ui_lines(fb, 60, &gfx_font_small, N_("Partie annulée par l'hôte\n(ou lancée sans toi)."));
        ui_footer(fb, N_("D : OK"));
        return;
    }
    for (int i = 0; i < n_lobby; ++i) {
        snprintf(text, sizeof(text), "%d. %s", i + 1, lobby[i].id ? lobby[i].name : "...");
        gfx_text(fb, 20, UI_TITLE_H + 28 + i * 22, &gfx_font_medium, text, GFX_BLACK, GFX_ALIGN_LEFT);
    }
    ui_footer(fb, party_state() == PARTY_JOINED ? N_("G : quitter")
              : n_lobby >= MIN_PLAYERS ? N_("G : annuler  D : lancer") : N_("G : annuler"));
}

static void race_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    switch (page) {
    case P_HOME: {
        const char *busy = busy_game();
        ui_title(fb, N_("Mini-bolides"));
        if (busy) {
            char text[80];
            snprintf(text, sizeof(text), _("Une partie de %s est en cours sur ton badge : termine-la ou quitte-la "
                     "d'abord."), tr(busy));
            ui_wrapped(fb, 60, &gfx_font_small, text, 5);
            ui_footer(fb, N_("G : retour"));
        } else {
            ui_list(fb, 2, sel, home_label);
            ui_wrapped(fb, UI_TITLE_H + 2 * UI_ROW_H + 12, &gfx_font_small, N_("Course de petites voitures, 2 à 4 "
                       "pilotes, 3 tours. Flanc droit : gaz, flanc gauche : frein, ailes : tourner. L'herbe "
                       "ralentit !"), 5);
            ui_footer(fb, N_("G : retour  D : choisir"));
        }
        break;
    }
    case P_SCAN:
        ui_title(fb, N_("Rejoindre"));
        if (n_found) {
            ui_list(fb, n_found, sel, found_label);
            ui_footer(fb, N_("G : retour  D : rejoindre"));
        } else {
            ui_lines(fb, 60, &gfx_font_small, N_("Recherche des courses..."));
            ui_footer(fb, N_("G : retour"));
        }
        break;
    case P_LOBBY:
        render_lobby(fb);
        break;
    default:
        if (cancelled && phase != PH_RESULT) {
            ui_title(fb, N_("Mini-bolides"));
            ui_lines(fb, 60, &gfx_font_small, N_("Partie annulée par l'hôte."));
            ui_footer(fb, N_("G : retour  D : OK"));
        } else if (! ready) {
            ui_title(fb, N_("Mini-bolides"));
            ui_lines(fb, 60, &gfx_font_small, N_("Réception de la liste\ndes joueurs..."));
            ui_footer(fb, N_("G long : quitter la page"));
        } else if (phase == PH_RESULT) {
            render_result(fb);
        } else {
            render_race(fb);
        }
        break;
    }
}

static bool race_calm(void) {
    return page != P_GAME || phase == PH_WAIT || phase == PH_RESULT || cancelled;
}

static void race_stop(void) {
    app_leds(0, 0, 0);
}

const app_t app_race = {
    .name = N_("Mini-bolides"),
    .start = race_start_page,
    .buttons = race_buttons,
    .task = race_task,
    .render = race_render,
    .calm = race_calm,
    .stop = race_stop,
    .no_saver = true,
};
