/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Host test of the loup-garou:
 * - the rules (menu/werewolf_logic.c): roles dealt, votes, deaths, lovers, winners;
 * - whole games between simulated badges: menu/werewolf.c is compiled once per badge (run_tests.py renames its
 *   public symbols: app_werewolf_<k>...), the party layer (party.h) is simulated here with a radio that loses
 *   packets, the players press random buttons; every page is drawn with the text check of ui.c on. */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "achievements.h"
#include "app.h"
#include "gfx.h"
#include "net.h"
#include "party.h"
#include "store.h"
#include "test.h"
#include "werewolf_logic.h"

uint64_t host_time_us = 0;
bool host_gpio[32];

static uint32_t lcg = 12345;
static uint32_t rnd(void) {
    lcg = lcg * 1103515245u + 12345u;
    return lcg >> 8;
}

static int count_role(const ww_game_t *g, int r) {
    int c = 0;
    for (int i = 0; i < g->n; ++i)
        c += g->role[i] == r;
    return c;
}

/* A game with the roles given (no shuffle) */
static void set_game(ww_game_t *g, const uint8_t *roles, int n) {
    memset(g, 0, sizeof(*g));
    g->n = n;
    memcpy(g->role, roles, n);
    g->alive = (1u << n) - 1;
    g->lovers[0] = g->lovers[1] = WW_NONE;
}

static void test_deal(void) {
    CHECK_EQ(ww_wolves_for(5), 1);
    CHECK_EQ(ww_wolves_for(6), 1);
    CHECK_EQ(ww_wolves_for(7), 2);
    CHECK_EQ(ww_wolves_for(10), 2);
    CHECK_EQ(ww_wolves_for(11), 3);
    CHECK_EQ(ww_wolves_for(14), 3);
    CHECK_EQ(ww_wolves_for(15), 4);
    CHECK_EQ(ww_wolves_for(20), 4);
    CHECK_EQ(ww_min_players(false), 5);
    CHECK_EQ(ww_min_players(true), 7);
    ww_game_t g;
    for (int n = 1; n <= WW_MAX; ++n)
        for (int adv = 0; adv < 2; ++adv)
            for (int k = 0; k < 20; ++k) {
                ww_deal(&g, n, adv, rnd);
                int wolves = ww_wolves_for(n) < n ? ww_wolves_for(n) : n;
                CHECK_EQ(g.n, n);
                CHECK_EQ(ww_count_alive(&g), n);
                CHECK_EQ(count_role(&g, WW_WOLF), wolves);
                if (n >= 5) {
                    CHECK_EQ(count_role(&g, WW_SEER), 1);
                    CHECK_EQ(count_role(&g, WW_WITCH), adv ? 1 : 0);  /* Advanced below 7: debug games only */
                    CHECK_EQ(count_role(&g, WW_HUNTER), adv ? 1 : 0);
                    CHECK_EQ(count_role(&g, WW_CUPID), adv && n >= WW_CUPID_MIN ? 1 : 0);
                }
                CHECK_EQ(g.lovers[0], WW_NONE);
                CHECK(ww_winner(&g) == WW_WIN_NONE || n < 3);
            }
    /* The roles are shuffled: the wolf is not always the first player */
    int first_wolf = 0;
    for (int k = 0; k < 100; ++k) {
        ww_deal(&g, 8, false, rnd);
        first_wolf += g.role[0] == WW_WOLF;
    }
    CHECK(first_wolf > 5 && first_wolf < 60);
}

static void test_tally(void) {
    uint8_t v1[] = {2, 2, 3, WW_NONE, 1};
    CHECK_EQ(ww_tally(v1, 5, 5, rnd), 2);
    CHECK_EQ(ww_tally(v1, 5, 5, NULL), 2);
    uint8_t v2[] = {1, 3, WW_NONE};
    CHECK_EQ(ww_tally(v2, 3, 5, NULL), WW_NONE);  /* Day vote: tie, nobody */
    int got1 = 0, got3 = 0;
    for (int k = 0; k < 100; ++k) {  /* Wolves: tie broken at random among the most voted */
        int t = ww_tally(v2, 3, 5, rnd);
        CHECK(t == 1 || t == 3);
        got1 += t == 1;
        got3 += t == 3;
    }
    CHECK(got1 > 10 && got3 > 10);
    uint8_t v3[] = {WW_NONE, WW_NONE};
    CHECK_EQ(ww_tally(v3, 2, 5, rnd), WW_NONE);  /* Nobody voted */
    uint8_t v4[] = {7, 9};
    CHECK_EQ(ww_tally(v4, 2, 5, rnd), WW_NONE);  /* Out of range */
}

static void test_deaths(void) {
    ww_game_t g;
    uint8_t deaths[WW_MAX];
    const uint8_t roles[7] = {WW_WOLF, WW_WOLF, WW_SEER, WW_WITCH, WW_HUNTER, WW_VILLAGER, WW_VILLAGER};
    set_game(&g, roles, 7);
    /* Victim healed: nobody dies, the potion is used */
    CHECK_EQ(ww_dawn(&g, 5, true, WW_NONE, deaths), 0);
    CHECK(g.heal_used);
    CHECK(! g.poison_used);
    /* Healed again: no potion left, the victim dies; the poison too */
    CHECK_EQ(ww_dawn(&g, 5, true, 0, deaths), 2);
    CHECK_EQ(deaths[0], 5);
    CHECK_EQ(deaths[1], 0);
    CHECK(! ww_alive(&g, 5));
    CHECK(! ww_alive(&g, 0));
    CHECK(g.poison_used);
    /* No more poison */
    CHECK_EQ(ww_dawn(&g, WW_NONE, false, 1, deaths), 0);
    CHECK(ww_alive(&g, 1));
    /* A dead player does not die twice */
    CHECK_EQ(ww_kill(&g, 5, deaths, 0), 0);

    /* Lovers: one dies, the other one too */
    set_game(&g, roles, 7);
    CHECK(! ww_link(&g, 2, 2));
    CHECK(ww_link(&g, 2, 6));
    CHECK(! ww_mixed_lovers(&g));
    int n = ww_dawn(&g, 6, false, WW_NONE, deaths);
    CHECK_EQ(n, 2);
    CHECK_EQ(deaths[0], 6);
    CHECK_EQ(deaths[1], 2);
    /* The victim and the poisoned are lovers: each dies once */
    set_game(&g, roles, 7);
    ww_link(&g, 3, 5);
    n = ww_dawn(&g, 5, false, 3, deaths);
    CHECK_EQ(n, 2);
}

static void test_winner(void) {
    ww_game_t g;
    uint8_t deaths[WW_MAX];
    const uint8_t roles[5] = {WW_WOLF, WW_SEER, WW_VILLAGER, WW_VILLAGER, WW_VILLAGER};
    set_game(&g, roles, 5);
    CHECK_EQ(ww_winner(&g), WW_WIN_NONE);
    ww_kill(&g, 0, deaths, 0);
    CHECK_EQ(ww_winner(&g), WW_WIN_VILLAGE);
    CHECK(ww_is_winner(&g, 1, WW_WIN_VILLAGE));
    CHECK(! ww_is_winner(&g, 0, WW_WIN_VILLAGE));
    /* 1 wolf, 1 villager: the wolves win */
    set_game(&g, roles, 5);
    ww_kill(&g, 1, deaths, 0);
    ww_kill(&g, 2, deaths, 0);
    CHECK_EQ(ww_winner(&g), WW_WIN_NONE);
    ww_kill(&g, 3, deaths, 0);
    CHECK_EQ(ww_winner(&g), WW_WIN_WOLVES);
    CHECK(ww_is_winner(&g, 0, WW_WIN_WOLVES));
    CHECK(! ww_is_winner(&g, 4, WW_WIN_WOLVES));
    /* A wolf and a villager in love, the last two: the lovers win */
    const uint8_t roles7[7] = {WW_WOLF, WW_WOLF, WW_SEER, WW_CUPID, WW_VILLAGER, WW_VILLAGER, WW_VILLAGER};
    set_game(&g, roles7, 7);
    ww_link(&g, 0, 4);
    CHECK(ww_mixed_lovers(&g));
    for (int i = 1; i < 7; ++i)
        if (i != 4)
            ww_kill(&g, i, deaths, 0);
    CHECK_EQ(ww_winner(&g), WW_WIN_LOVERS);
    CHECK(ww_is_winner(&g, 0, WW_WIN_LOVERS));
    CHECK(ww_is_winner(&g, 4, WW_WIN_LOVERS));
    CHECK(! ww_is_winner(&g, 3, WW_WIN_LOVERS));
    /* A mixed lover does not win with its camp */
    CHECK(! ww_is_winner(&g, 4, WW_WIN_VILLAGE));
    CHECK(! ww_is_winner(&g, 0, WW_WIN_WOLVES));
    /* Everybody dead */
    ww_kill(&g, 0, deaths, 0);
    CHECK_EQ(ww_count_alive(&g), 0);
    CHECK_EQ(ww_winner(&g), WW_WIN_DRAW);
    CHECK(! ww_is_winner(&g, 0, WW_WIN_DRAW));
}

static void test_texts(void) {
    /* The names of the roles fit in a row of the lists, the results in a line of the medium font */
    for (int r = 0; r < WW_ROLES; ++r)
        CHECK(gfx_text_width(&gfx_font_small, ww_role_name(r)) < 100);
    CHECK_STR(ww_role_name(WW_ROLES), "?");
    CHECK_STR(ww_role_name(-1), "?");
    for (int w = WW_WIN_VILLAGE; w <= WW_WIN_DRAW; ++w)
        CHECK(gfx_text_width(&gfx_font_small, ww_win_text(w)) <= GFX_WIDTH - 8);
}


/* ------ Simulation of whole games ------ */

#define NB 10  /* Badges compiled by run_tests.py: 0 = the narrator, 1.. = the players */
#define BADGE(k) extern const app_t app_werewolf_##k; void werewolf_service_##k(absolute_time_t now);
BADGE(0) BADGE(1) BADGE(2) BADGE(3) BADGE(4) BADGE(5) BADGE(6) BADGE(7) BADGE(8) BADGE(9)
static const app_t *APPS[NB] = {&app_werewolf_0, &app_werewolf_1, &app_werewolf_2, &app_werewolf_3, &app_werewolf_4,
                                &app_werewolf_5, &app_werewolf_6, &app_werewolf_7, &app_werewolf_8, &app_werewolf_9};
static void (*const TASKS[NB])(absolute_time_t) = {werewolf_service_0, werewolf_service_1, werewolf_service_2, werewolf_service_3,
    werewolf_service_4, werewolf_service_5, werewolf_service_6, werewolf_service_7, werewolf_service_8, werewolf_service_9};

static int cur = 0;  /* The badge running */
static int n_badges = 0;
static bool in_app[NB];  /* The page is shown; otherwise only the hook of the main loop (werewolf_service) runs */
static uint32_t sim_rng = 1;
static bool verbose = false;

uint32_t get_rand_32(void) {
    sim_rng = sim_rng * 1664525u + 1013904223u;
    return sim_rng ^ (sim_rng >> 15);
}

/* What the logs of each badge tell */
typedef struct {
    int ended, uicheck, stopped;
    bool won, dead, left;
    char role[16];
    char winner[40];
} seen_t;
static seen_t seen[NB];
static char narrator_roles[NB][16];
static int narrator_end = 0, narrator_left = 0;
static char narrator_winner[40];

int sim_printf(const char *fmt, ...) {
    char line[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    char *nl = strchr(line, '\n');
    if (nl)
        *nl = 0;
    seen_t *s = &seen[cur];
    if (verbose)
        printf("%7.1f [%d] %s\n", host_time_us / 1e6, cur, line);
    int k;
    char role[16];
    if (! strncmp(line, "uicheck:", 8)) {
        ++s->uicheck;
        printf("[%d] %s\n", cur, line);
    } else if (sscanf(line, "werewolf: role %15[^\n]", role) == 1) {
        snprintf(s->role, sizeof(s->role), "%s", role);
    } else if (! strncmp(line, "werewolf: end, ", 15)) {
        const char *w = line + 15;
        if (cur == 0) {
            ++narrator_end;
            snprintf(narrator_winner, sizeof(narrator_winner), "%s", w);
        } else {
            ++s->ended;
            const char *c = strstr(w, ", I ");
            s->won = c && ! strcmp(c, ", I won");
            snprintf(s->winner, sizeof(s->winner), "%.*s", c ? (int)(c - w) : 0, w);
        }
    } else if (! strcmp(line, "werewolf: the narrator stopped the game")) {
        ++s->stopped;
    } else if (! strcmp(line, "werewolf: I am dead")) {
        s->dead = true;
    } else if (cur == 0 && sscanf(line, "werewolf: J%d is %15[^\n]", &k, role) == 2 && k > 0 && k < NB) {
        snprintf(narrator_roles[k], sizeof(narrator_roles[k]), "%s", role);
    } else if (cur == 0 && strstr(line, " left the game")) {
        ++narrator_left;
    }
    return n;
}

/* The rest of the firmware, as seen by werewolf.c */
static uint32_t bid(int b) { return 0x10000u + b; }
uint32_t net_id(void) { return bid(cur); }
int net_queue_free(void) { return 8; }
const char *social_name(void) {
    static char names[NB][9];
    snprintf(names[cur], sizeof(names[cur]), "J%d", cur);
    return names[cur];
}
const app_t *app_current(void) { return NULL; }
void app_tone(uint16_t hz, uint16_t ms) { (void)hz; (void)ms; }
void app_leds(uint8_t r, uint8_t g, uint8_t b) { (void)r; (void)g; (void)b; }
void app_open(const app_t *app) { (void)app; }
void app_show_still(void (*render)(uint8_t *fb)) { (void)render; }
void app_cough(void) {}
static store_t stores[NB];
store_t *store_get(void) { return &stores[cur]; }
static bool achv[NB][ACHV_COUNT];
static uint16_t counters[NB][8];
void achv_unlock(achv_id_t id) { achv[cur][id] = true; }
uint16_t achv_add(achv_counter_t c, uint16_t n) { return counters[cur][c] += n; }

/* The party: one at a time, the radio loses loss_pct % of the packets (each receiver on its own) */
static struct {
    bool open;
    int host;
    uint8_t flags;
    party_player_t roster[PARTY_MAX];
    int n;
    absolute_time_t start;
} P;
static party_state_t pst[NB];
static party_handler_t handlers[NB];
static uint32_t pkey[NB];
static int loss_pct = 0;
typedef struct {
    int from;
    uint32_t to;
    uint8_t kind, len;
    uint8_t data[PARTY_PAYLOAD];
} msg_t;
#define MAX_MSGS 512
static msg_t msgs[MAX_MSGS], flying[MAX_MSGS];
static int n_msgs = 0;
static int max_burst = 0;  /* Packets sent in 10 ms, at most */
static int sent_narrator = 0, sent_players = 0;  /* Packets of the game */

uint32_t party_mask(uint32_t key, uint8_t salt) {
    uint32_t h = 2166136261u;
    uint8_t d[5] = {key, key >> 8, key >> 16, key >> 24, salt};
    for (int i = 0; i < 5; ++i)
        h = (h ^ d[i]) * 16777619u;
    return h;
}

void party_host(uint8_t game, bool plays, uint8_t max, uint8_t flags) {
    (void)game; (void)plays; (void)max;
    P.open = true;
    P.host = cur;
    P.flags = flags;
    P.n = 0;
    pst[cur] = PARTY_HOSTING;
    pkey[cur] = get_rand_32();
}
void party_set_flags(uint8_t flags) { P.flags = flags; }
void party_scan(uint8_t game) { (void)game; pst[cur] = PARTY_SCANNING; }
int party_found(party_open_t *out, int max) {
    if (! P.open || pst[cur] != PARTY_SCANNING || max < 1)
        return 0;
    memset(out, 0, sizeof(*out));
    out->host = bid(P.host);
    snprintf(out->name, sizeof(out->name), "J%d", P.host);
    out->players = P.n;
    out->max = WW_MAX;
    out->flags = P.flags;
    return 1;
}
void party_join(uint32_t host) {
    if (! P.open || host != bid(P.host) || P.n >= PARTY_MAX)
        return;
    pkey[cur] = get_rand_32();
    party_player_t *p = &P.roster[P.n++];
    p->id = bid(cur);
    snprintf(p->name, sizeof(p->name), "J%d", cur);
    p->key = pkey[cur];
    pst[cur] = PARTY_JOINED;
}
void party_kick(uint32_t id) { (void)id; }
void party_start(uint16_t delay_ms) {
    P.open = false;
    P.start = host_time_us + delay_ms * 1000ull;
    pst[cur] = PARTY_STARTED;
    for (int i = 0; i < P.n; ++i)
        pst[P.roster[i].id - 0x10000u] = PARTY_STARTED;
}
bool party_send(uint8_t kind, uint32_t to, const void *data, uint8_t len) {
    if (n_msgs >= MAX_MSGS)
        return false;
    if (cur == P.host)
        ++sent_narrator;
    else
        ++sent_players;
    msg_t *m = &msgs[n_msgs++];
    m->from = cur;
    m->to = to;
    m->kind = kind;
    m->len = len > PARTY_PAYLOAD ? PARTY_PAYLOAD : len;
    if (m->len)
        memcpy(m->data, data, m->len);
    return true;
}
void party_leave(void) {
    if (pst[cur] == PARTY_HOSTING || pst[cur] == PARTY_JOINED || pst[cur] == PARTY_STARTED) {
        if (cur == P.host && pst[cur] == PARTY_STARTED) {
            party_send(5, 0, NULL, 0);  /* After the start: given to the game (the fixed party.c) */
        } else if (cur == P.host) {
            P.open = false;
            for (int i = 0; i < P.n; ++i) {
                int b = P.roster[i].id - 0x10000u;
                if (pst[b] == PARTY_JOINED || pst[b] == PARTY_STARTED)
                    pst[b] = PARTY_CANCELLED;
            }
        } else {
            party_send(5, 0, NULL, 0);  /* LEAVE of party.c */
        }
    }
    pst[cur] = PARTY_IDLE;
}
party_state_t party_state(void) { return pst[cur]; }
uint8_t party_game(void) { return PARTY_GAME_WEREWOLF; }
uint8_t party_flags(void) { return P.flags; }
bool party_is_host(void) { return cur == P.host && pst[cur] != PARTY_IDLE; }
uint32_t party_host_id(void) { return bid(P.host); }
int party_players(party_player_t *out, int max) {
    int n = P.n < max ? P.n : max;
    memcpy(out, P.roster, n * sizeof(*out));
    for (int i = 0; i < n && cur != P.host; ++i)
        out[i].key = 0;  /* Only the host knows the keys */
    return n;
}
int party_count(void) { return P.n; }
int party_index(uint32_t id) {
    for (int i = 0; i < P.n; ++i)
        if (P.roster[i].id == id)
            return i;
    return -1;
}
const char *party_name(uint32_t id) {
    int i = party_index(id);
    return i >= 0 ? P.roster[i].name : "?";
}
uint32_t party_key(void) { return pkey[cur]; }
absolute_time_t party_start_time(void) { return P.start; }
uint32_t party_seed(void) { return 0; }
void party_set_handler(party_handler_t h) { handlers[cur] = h; }

static void deliver(void) {
    int n = n_msgs;
    if (n > max_burst)
        max_burst = n;
    memcpy(flying, msgs, n * sizeof(msg_t));
    n_msgs = 0;
    for (int k = 0; k < n; ++k)
        for (int b = 0; b < n_badges; ++b) {
            msg_t *m = &flying[k];
            if (b == m->from || pst[b] != PARTY_STARTED || ! handlers[b] || (m->to && m->to != bid(b)))
                continue;
            if ((int)(get_rand_32() % 100) < loss_pct)
                continue;
            cur = b;
            handlers[b](m->kind, bid(m->from), m->to, m->data, m->len, -60);
        }
}

static uint8_t fb[GFX_FB_SIZE];

static void render(int b) {
    cur = b;
    gfx_clear(fb, GFX_WHITE);
    APPS[b]->render(fb, host_time_us);
}

/* A key like on the serial port: a, b, x, y short presses; A, B long presses */
static void key(int b, char k) {
    cur = b;
    app_buttons_t ev;
    memset(&ev, 0, sizeof(ev));
    char l = k | 0x20;
    uint8_t bit = l == 'a' ? UI_BTN_A : l == 'b' ? UI_BTN_B : l == 'x' ? UI_BTN_X : UI_BTN_Y;
    ev.pressed = bit;
    if (k == l)
        ev.released_short = bit;
    else
        ev.long_pressed = bit;
    if (! in_app[b]) {
        APPS[b]->start(host_time_us);
        in_app[b] = true;
    }
    if (! APPS[b]->buttons(&ev, host_time_us))
        in_app[b] = false;  /* Back to the menus */
    else
        render(b);
}

static void keys(int b, const char *k) {
    while (*k)
        key(b, *k++);
}

static void run_ms(int ms) {
    for (int t = 0; t < ms; t += 10) {
        host_time_us += 10000;
        for (int b = 0; b < n_badges; ++b) {
            cur = b;
            if (in_app[b]) {
                if (APPS[b]->task(host_time_us) || host_time_us % 1000000 == 0)
                    render(b);
            } else {
                TASKS[b](host_time_us);
            }
        }
        deliver();
    }
}

static bool narrator_advanced = false;  /* The setting kept by the narrator page between the games */

/* A whole game: \p players badges join (+ the robots in test mode); \p away: a player who never opens the page
 * (only the hook of the main loop runs); \p leaver: a player who leaves during the game */
static void run_game(const char *name, int players, bool advanced, bool debug, int loss, int away, int leaver) {
    printf("--- %s: %d players, %s, %s%d %% lost\n", name, players, advanced ? "advanced" : "simple",
           debug ? "test mode, " : "", loss);
    n_badges = players + 1;
    loss_pct = loss;
    memset(seen, 0, sizeof(seen));
    memset(narrator_roles, 0, sizeof(narrator_roles));
    memset(achv, 0, sizeof(achv));
    memset(counters, 0, sizeof(counters));
    narrator_end = narrator_left = 0;
    for (int b = 0; b < NB; ++b) {
        in_app[b] = false;
        stores[b].admin = b == 0 && debug ? STORE_ADMIN_ON : 0;
    }
    uint64_t t0 = host_time_us;
    sent_narrator = sent_players = 0;
    keys(0, "b");  /* Mener une partie */
    if (advanced != narrator_advanced)
        keys(0, "b");  /* Mode */
    narrator_advanced = advanced;
    keys(0, "xxb");  /* Ouvrir la partie */
    for (int b = 1; b <= players; ++b)
        keys(b, "xb");  /* Rejoindre une partie */
    run_ms(1500);
    for (int b = 1; b <= players; ++b)
        keys(b, "b");  /* The party found */
    run_ms(1000);
    keys(0, "b");  /* Lancer */
    if (away)
        keys(away, "a");  /* Back to the menus */
    uint64_t next_press[NB] = {0};
    bool left = false;
    while (host_time_us - t0 < 4ull * 3600 * 1000000 && ! narrator_end) {
        run_ms(100);
        for (int b = 1; b <= players; ++b) {
            if (b == away || (b == leaver && left) || host_time_us < next_press[b])
                continue;
            next_press[b] = host_time_us + (1 + get_rand_32() % 8) * 1000000ull;
            for (int m = get_rand_32() % 4; m > 0; --m)
                key(b, 'x');
            key(b, 'b');
        }
        if (leaver && ! left && host_time_us - t0 > 200 * 1000000ull) {
            keys(leaver, "Ab");  /* Quitter la partie ? Oui */
            seen[leaver].left = left = true;
        }
    }
    uint64_t secs = (host_time_us - t0) / 1000000;
    printf("    radio: narrator %.1f packets/s, players %.2f packets/s each\n", (double)sent_narrator / secs,
           (double)sent_players / secs / players);
    run_ms(5000);  /* The end reaches everybody */
    printf("    %s after %llu s, %d left\n", narrator_winner, (unsigned long long)((host_time_us - t0) / 1000000),
           narrator_left);
    CHECK_EQ(narrator_end, 1);
    CHECK_EQ(narrator_left, leaver ? 1 : 0);
    CHECK_EQ(seen[0].uicheck, 0);
    for (int b = 1; b <= players; ++b) {
        seen_t *s = &seen[b];
        CHECK_EQ(s->uicheck, 0);
        CHECK(s->role[0] && ! strcmp(s->role, narrator_roles[b]));  /* Each player got its role */
        if (b == leaver) {
            CHECK_EQ(s->ended, 0);
            continue;
        }
        CHECK_EQ(s->ended, 1);
        CHECK_STR(s->winner, narrator_winner);
        CHECK(achv[b][ACHV_WEREWOLF_PLAY]);
        CHECK_EQ(counters[b][ACHV_CNT_GAMES], 1);
        CHECK_EQ(achv[b][ACHV_WEREWOLF_WIN], s->won);
        CHECK_EQ(counters[b][ACHV_CNT_WINS], s->won ? 1 : 0);
        if (verbose)
            printf("    J%d: %s, %s%s\n", b, s->role, s->dead ? "dead, " : "", s->won ? "won" : "lost");
    }
    /* Everybody leaves (long press on the left wing, then "oui") */
    for (int b = 0; b <= players; ++b) {
        in_app[b] = false;
        keys(b, "Ab");
    }
    run_ms(1000);
}

/* The narrator stops the game: every player shows it, then everybody can play again */
static void run_abort(void) {
    printf("--- the narrator stops the game\n");
    n_badges = 6;
    loss_pct = 30;
    memset(seen, 0, sizeof(seen));
    for (int b = 0; b < NB; ++b) {
        in_app[b] = false;
        stores[b].admin = 0;
    }
    keys(0, "b");  /* Mener une partie */
    if (narrator_advanced)
        keys(0, "b");  /* Mode : simple */
    narrator_advanced = false;
    keys(0, "xxb");  /* Ouvrir */
    for (int b = 1; b < n_badges; ++b)
        keys(b, "xb");
    run_ms(1500);
    for (int b = 1; b < n_badges; ++b)
        keys(b, "b");
    run_ms(1000);
    keys(0, "b");  /* Lancer */
    run_ms(60000);
    keys(0, "Ab");  /* Arrêter la partie pour tous ? Oui */
    run_ms(5000);
    cur = 0;
    CHECK_EQ(party_state(), PARTY_IDLE);  /* The narrator left after its ABORT messages */
    for (int b = 1; b < n_badges; ++b) {
        CHECK_EQ(seen[b].stopped, 1);
        CHECK_EQ(seen[b].ended, 0);
        in_app[b] = false;
        keys(b, "Ab");
    }
    run_ms(1000);
    for (int b = 0; b < n_badges; ++b) {
        cur = b;
        CHECK_EQ(party_state(), PARTY_IDLE);
    }
}

int main(int argc, char **argv) {
    verbose = argc > 1;
    test_deal();
    test_tally();
    test_deaths();
    test_winner();
    test_texts();
    ui_check = true;  /* The texts cut or too wide are traced (uicheck:) */
    run_game("test mode", 1, false, true, 10, 0, 0);
    run_game("test mode, advanced", 1, true, true, 10, 0, 0);
    run_game("simple", 5, false, false, 20, 0, 0);
    run_game("simple, 1 player away", 6, false, false, 30, 3, 0);
    for (int seed = 0; seed < 4; ++seed) {
        sim_rng = 1000 + seed;
        run_game("advanced with Cupidon", 9, true, false, 20, 0, seed == 3 ? 5 : 0);
    }
    run_game("advanced, no Cupidon", 7, true, false, 25, 0, 0);
    run_abort();
    printf("at most %d packets sent in 10 ms\n", max_burst);
    TEST_END();
}
