/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Host test of the loup-garou:
 * - the rules (menu/werewolf_logic.c): cards dealt with the thief, votes (captain), deaths, lovers, winners, and each
 *   special case of the rules;
 * - whole games between simulated badges: menu/werewolf.c is compiled once per badge (run_tests.py renames its
 *   public symbols: app_werewolf_<k>...), the party layer (party.h) is simulated here with a radio that loses
 *   packets, the players press random buttons; every page (and every card of the help) is drawn with the text check
 *   of ui.c on. The logs of the narrator tell which special cases happened: each one must happen at least once. */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "achievements.h"
#include "app.h"
#include "gfx.h"
#include "net.h"
#include "party.h"
#include "store.h"
#include "test.h"
#include "werewolf_cards.h"
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

/* A game with the cards given (no shuffle) */
static void set_game(ww_game_t *g, const uint8_t *roles, int n) {
    memset(g, 0, sizeof(*g));
    g->n = n;
    g->options = WW_OPT_ALL;
    memcpy(g->role, roles, n);
    g->alive = (1u << n) - 1;
    g->lovers[0] = g->lovers[1] = g->captain = WW_NONE;
    g->center[0] = g->center[1] = WW_NONE;
}

static void test_deal(void) {
    CHECK_EQ(ww_wolves_for(8), 2);
    CHECK_EQ(ww_wolves_for(11), 2);
    CHECK_EQ(ww_wolves_for(12), 3);
    CHECK_EQ(ww_wolves_for(18), 3);
    ww_game_t g;
    for (int n = WW_MIN_PLAYERS; n <= WW_MAX_PLAYERS; ++n)
        for (int k = 0; k < 40; ++k) {
            uint8_t opts = rnd() & WW_OPT_ALL;
            ww_deal(&g, n, opts, rnd);
            int roles = 0;
            for (int r = WW_SEER; r <= WW_THIEF; ++r)
                roles += count_role(&g, r);
            int in_center = (g.center[0] != WW_NONE && g.center[0] != WW_VILLAGER)
                            + (g.center[1] != WW_NONE && g.center[1] != WW_VILLAGER);
            CHECK_EQ(g.n, n);
            CHECK_EQ(ww_count_alive(&g), n);
            CHECK_EQ(g.captain, WW_NONE);
            CHECK_EQ(g.lovers[0], WW_NONE);
            /* Every card is somewhere: with a player or, with the thief, in the center */
            int wolves = count_role(&g, WW_WOLF) + (g.center[0] == WW_WOLF) + (g.center[1] == WW_WOLF);
            CHECK_EQ(wolves, ww_wolves_for(n));
            int expected = __builtin_popcount(opts & (WW_OPT_ALL & ~WW_OPT_CAPTAIN));
            CHECK_EQ(roles + in_center - (g.center[0] == WW_WOLF) - (g.center[1] == WW_WOLF), expected);
            if (opts & WW_OPT_THIEF) {
                CHECK(g.center[0] != WW_NONE && g.center[1] != WW_NONE);
            } else {
                CHECK(g.center[0] == WW_NONE && g.center[1] == WW_NONE);
            }
        }
    /* The cards are shuffled: a wolf is not always the first player */
    int first_wolf = 0;
    for (int k = 0; k < 100; ++k) {
        ww_deal(&g, 9, WW_OPT_BEGINNER, rnd);
        first_wolf += g.role[0] == WW_WOLF;
    }
    CHECK(first_wolf > 5 && first_wolf < 50);
}

static void test_thief(void) {
    ww_game_t g;
    const uint8_t roles[8] = {WW_THIEF, WW_WOLF, WW_SEER, WW_WITCH, WW_HUNTER, WW_CUPID, WW_GIRL, WW_VILLAGER};
    set_game(&g, roles, 8);
    g.center[0] = WW_WOLF;
    g.center[1] = WW_WOLF;
    /* Two wolves left: he must take one */
    CHECK(ww_thief_must_take(&g));
    CHECK(! ww_thief_swap(&g, 0, -1));
    CHECK_EQ(g.role[0], WW_THIEF);
    CHECK(ww_thief_swap(&g, 0, 1));
    CHECK_EQ(g.role[0], WW_WOLF);
    CHECK_EQ(g.center[1], WW_THIEF);
    CHECK_EQ(__builtin_popcount(ww_wolves(&g)), 2);
    /* Not the thief: refused */
    CHECK(! ww_thief_swap(&g, 2, 0));
    /* A villager and the seer left: he may keep his card, or take the seer */
    set_game(&g, roles, 8);
    g.center[0] = WW_VILLAGER;
    g.center[1] = WW_SEER;
    CHECK(! ww_thief_must_take(&g));
    CHECK(ww_thief_swap(&g, 0, -1));
    CHECK_EQ(g.role[0], WW_THIEF);
    set_game(&g, roles, 8);
    g.center[0] = WW_VILLAGER;
    g.center[1] = WW_SEER;
    CHECK(ww_thief_swap(&g, 0, 1));
    CHECK_EQ(ww_find(&g, WW_SEER), 0);  /* Two seers: the first one */
}

static void test_votes(void) {
    ww_game_t g;
    const uint8_t roles[8] = {WW_WOLF, WW_WOLF, WW_SEER, WW_WITCH, WW_HUNTER, WW_CUPID, WW_GIRL, WW_VILLAGER};
    set_game(&g, roles, 8);
    uint8_t v[8];
    uint32_t tied;
    int out;
    /* A majority */
    uint8_t v1[8] = {2, 2, 3, WW_NONE, 2, 3, WW_NONE, 4};
    CHECK_EQ(ww_count_votes(&g, v1, WW_NONE, &tied), 2);
    CHECK_EQ(tied, 0);
    CHECK_EQ(ww_day_vote(&g, v1, false, &out, &tied), WW_DAY_OUT);
    CHECK_EQ(out, 2);
    /* The dead do not vote */
    g.alive &= ~(1u << 0 | 1u << 1);
    CHECK_EQ(ww_count_votes(&g, v1, WW_NONE, &tied), 3);  /* 2 votes for 3, 1 for 2, 1 for 4 */
    CHECK_EQ(tied, 0);
    uint8_t v5[8] = {2, 2, 3, WW_NONE, 2, 3, WW_NONE, 2};
    CHECK_EQ(ww_count_votes(&g, v5, WW_NONE, &tied), WW_NONE);  /* 2 for 2, 2 for 3 (the dead votes ignored) */
    CHECK_EQ(tied, 1u << 2 | 1u << 3);
    set_game(&g, roles, 8);
    /* The captain counts twice: 3 voters for 2, the captain and 1 for 5 */
    uint8_t v2[8] = {2, 2, 2, 5, 5, WW_NONE, WW_NONE, WW_NONE};
    g.captain = 3;
    CHECK_EQ(ww_count_votes(&g, v2, 3, &tied), WW_NONE);  /* 3 against 3: a tie */
    CHECK_EQ(ww_day_vote(&g, v2, false, &out, &tied), WW_DAY_OUT);  /* The vote of the captain decides */
    CHECK_EQ(out, 5);
    /* A tie, the captain voted for someone else: he decides among the tied */
    uint8_t v3[8] = {2, 2, 5, WW_NONE, 5, WW_NONE, WW_NONE, WW_NONE};  /* The captain: blank */
    CHECK_EQ(ww_day_vote(&g, v3, false, &out, &tied), WW_DAY_TIEBREAK);
    CHECK_EQ(tied, 1u << 2 | 1u << 5);
    /* The captain dead: no captain, a second vote, then nobody */
    g.alive &= ~(1u << 3);
    uint8_t v4[8] = {2, 2, 5, WW_NONE, 5, WW_NONE, WW_NONE, WW_NONE};
    CHECK_EQ(ww_day_vote(&g, v4, false, &out, &tied), WW_DAY_REVOTE);
    CHECK_EQ(tied, 1u << 2 | 1u << 5);
    CHECK_EQ(ww_day_vote(&g, v4, true, &out, &tied), WW_DAY_NOBODY);
    CHECK_EQ(out, WW_NONE);
    /* Nobody voted */
    memset(v, WW_NONE, sizeof(v));
    CHECK_EQ(ww_day_vote(&g, v, false, &out, &tied), WW_DAY_NOBODY);
    /* ww_pick: one of the mask */
    for (int k = 0; k < 50; ++k) {
        int p = ww_pick(1u << 3 | 1u << 7, rnd);
        CHECK(p == 3 || p == 7);
    }
    CHECK_EQ(ww_pick(0, rnd), WW_NONE);
}

static void test_lovers(void) {
    ww_game_t g;
    uint8_t deaths[WW_MAX];
    const uint8_t roles[8] = {WW_WOLF, WW_WOLF, WW_SEER, WW_WITCH, WW_HUNTER, WW_CUPID, WW_GIRL, WW_VILLAGER};
    set_game(&g, roles, 8);
    CHECK(! ww_link(&g, 2, 2));
    CHECK(ww_link(&g, 0, 7));
    CHECK_EQ(ww_partner(&g, 0), 7);
    CHECK_EQ(ww_partner(&g, 7), 0);
    CHECK_EQ(ww_partner(&g, 3), WW_NONE);
    /* They cannot harm each other */
    CHECK(! ww_may_harm(&g, 0, 7));
    CHECK(! ww_may_harm(&g, 7, 0));
    CHECK(ww_may_harm(&g, 1, 7));
    CHECK(! ww_may_harm(&g, 3, 3));
    /* One dies, the other dies of grief */
    int n = ww_kill(&g, 7, deaths, 0);
    CHECK_EQ(n, 2);
    CHECK_EQ(deaths[0], 7);
    CHECK_EQ(deaths[1], 0);
    /* The victim and the poisoned are the lovers: each dies once */
    set_game(&g, roles, 8);
    ww_link(&g, 3, 5);
    CHECK_EQ(ww_dawn(&g, 5, false, 3, deaths), 2);
}

static void test_witch(void) {
    ww_game_t g;
    uint8_t deaths[WW_MAX];
    const uint8_t roles[8] = {WW_WOLF, WW_WOLF, WW_SEER, WW_WITCH, WW_HUNTER, WW_CUPID, WW_GIRL, WW_VILLAGER};
    /* Both potions the same night: the victim saved, a wolf poisoned */
    set_game(&g, roles, 8);
    CHECK_EQ(ww_dawn(&g, 7, true, 0, deaths), 1);
    CHECK_EQ(deaths[0], 0);
    CHECK(ww_alive(&g, 7));
    CHECK(g.heal_used && g.poison_used);
    /* No potion left: the victim dies, no poison */
    CHECK_EQ(ww_dawn(&g, 7, true, 1, deaths), 1);
    CHECK(! ww_alive(&g, 7));
    CHECK(ww_alive(&g, 1));
    /* The witch saves herself */
    set_game(&g, roles, 8);
    CHECK_EQ(ww_dawn(&g, 3, true, WW_NONE, deaths), 0);
    CHECK(ww_alive(&g, 3));
    CHECK(g.heal_used && ! g.poison_used);
    /* 0, 1 or 2 deaths: the victim and the poisoned */
    set_game(&g, roles, 8);
    CHECK_EQ(ww_dawn(&g, 7, false, 2, deaths), 2);
    /* A hunter killed at night is among the deaths (he shoots: werewolf.c) */
    set_game(&g, roles, 8);
    CHECK_EQ(ww_dawn(&g, 4, false, WW_NONE, deaths), 1);
    CHECK_EQ(g.role[deaths[0]], WW_HUNTER);
}

static void test_girl(void) {
    ww_game_t g;
    const uint8_t roles[8] = {WW_WOLF, WW_SEER, WW_WOLF, WW_WITCH, WW_HUNTER, WW_CUPID, WW_GIRL, WW_VILLAGER};
    set_game(&g, roles, 8);
    uint8_t seen;
    CHECK(ww_spy(&g, 3, &seen));  /* r multiple of 3: caught */
    CHECK(seen == 0 || seen == 2);
    CHECK(! ww_spy(&g, 4, &seen));  /* Not caught: she saw a wolf */
    CHECK(seen == 0 || seen == 2);
    int caught = 0;
    for (uint32_t r = 0; r < 300; ++r)
        caught += ww_spy(&g, r, &seen);
    CHECK_EQ(caught, 100);  /* 1 in 3 */
    g.alive &= ~(1u << 0);
    ww_spy(&g, 1, &seen);
    CHECK_EQ(seen, 2);  /* A living wolf */
}

static void test_winner(void) {
    ww_game_t g;
    uint8_t deaths[WW_MAX];
    const uint8_t roles[8] = {WW_WOLF, WW_WOLF, WW_SEER, WW_WITCH, WW_HUNTER, WW_CUPID, WW_GIRL, WW_VILLAGER};
    set_game(&g, roles, 8);
    CHECK_EQ(ww_winner(&g), WW_WIN_NONE);
    ww_kill(&g, 0, deaths, 0);
    CHECK_EQ(ww_winner(&g), WW_WIN_NONE);
    ww_kill(&g, 1, deaths, 0);
    CHECK_EQ(ww_winner(&g), WW_WIN_VILLAGE);  /* The last wolf eliminated */
    CHECK(ww_is_winner(&g, 2, WW_WIN_VILLAGE));
    CHECK(ww_is_winner(&g, 3, WW_WIN_VILLAGE));  /* Dead or alive: the camp wins */
    CHECK(! ww_is_winner(&g, 0, WW_WIN_VILLAGE));
    /* The wolves win only when the last villager is eliminated (the rules), not when they are as many */
    set_game(&g, roles, 8);
    for (int i = 2; i < 7; ++i)
        ww_kill(&g, i, deaths, 0);
    CHECK_EQ(ww_winner(&g), WW_WIN_NONE);  /* 2 wolves, 1 villager */
    ww_kill(&g, 7, deaths, 0);
    CHECK_EQ(ww_winner(&g), WW_WIN_WOLVES);
    CHECK(ww_is_winner(&g, 0, WW_WIN_WOLVES));
    CHECK(! ww_is_winner(&g, 7, WW_WIN_WOLVES));
    /* A wolf and a villager in love: they win when they are the last two */
    set_game(&g, roles, 8);
    ww_link(&g, 0, 7);
    CHECK(ww_mixed_lovers(&g));
    for (int i = 1; i < 7; ++i)
        ww_kill(&g, i, deaths, 0);
    CHECK_EQ(ww_winner(&g), WW_WIN_LOVERS);
    CHECK(ww_is_winner(&g, 0, WW_WIN_LOVERS));
    CHECK(ww_is_winner(&g, 7, WW_WIN_LOVERS));
    CHECK(! ww_is_winner(&g, 3, WW_WIN_LOVERS));
    CHECK(! ww_is_winner(&g, 7, WW_WIN_VILLAGE));  /* Their own camp */
    CHECK(! ww_is_winner(&g, 0, WW_WIN_WOLVES));
    /* Mixed lovers alive with others: nobody wins yet (a wolf and a villager alive) */
    set_game(&g, roles, 8);
    ww_link(&g, 0, 7);
    ww_kill(&g, 1, deaths, 0);
    CHECK_EQ(ww_winner(&g), WW_WIN_NONE);
    /* The lovers die together: the last two die, nobody wins */
    set_game(&g, roles, 8);
    ww_link(&g, 0, 7);
    for (int i = 1; i < 7; ++i)
        ww_kill(&g, i, deaths, 0);
    CHECK_EQ(ww_kill(&g, 7, deaths, 0), 2);
    CHECK_EQ(ww_winner(&g), WW_WIN_DRAW);
}

static void test_texts(void) {
    for (int r = 0; r < WW_ROLES; ++r)
        CHECK(gfx_text_width(&gfx_font_small, ww_role_name(r)) < 100);
    CHECK_STR(ww_role_name(WW_ROLES), "?");
    for (int w = WW_WIN_VILLAGE; w <= WW_WIN_DRAW; ++w)
        CHECK(gfx_text_width(&gfx_font_small, ww_win_text(w)) <= GFX_WIDTH - 8);
    for (int c = 0; c < WW_CARDS; ++c) {
        CHECK(WW_CARD[c].icon != NULL);
        CHECK(gfx_text_width(&gfx_font_medium, WW_CARD[c].name) <= GFX_WIDTH - 86);  /* Beside the illustration */
        int ink = 0;
        for (int k = 0; k < WW_ICON_BYTES; ++k)
            ink += __builtin_popcount(WW_CARD[c].icon[k]);
        CHECK(ink > 60 && ink < 800);  /* A drawing, neither empty nor all black */
    }
}

/* ------ Simulation of whole games ------ */

#define NB 19  /* Badges compiled by run_tests.py: 0 = the narrator, 1.. = the players (18 at most) */
#define BADGE(k) extern const app_t app_werewolf_##k; void werewolf_service_##k(absolute_time_t now);
BADGE(0) BADGE(1) BADGE(2) BADGE(3) BADGE(4) BADGE(5) BADGE(6) BADGE(7) BADGE(8) BADGE(9) BADGE(10) BADGE(11)
BADGE(12) BADGE(13) BADGE(14) BADGE(15) BADGE(16) BADGE(17) BADGE(18)
static const app_t *APPS[NB] = {&app_werewolf_0, &app_werewolf_1, &app_werewolf_2, &app_werewolf_3, &app_werewolf_4,
    &app_werewolf_5, &app_werewolf_6, &app_werewolf_7, &app_werewolf_8, &app_werewolf_9, &app_werewolf_10,
    &app_werewolf_11, &app_werewolf_12, &app_werewolf_13, &app_werewolf_14, &app_werewolf_15, &app_werewolf_16,
    &app_werewolf_17, &app_werewolf_18};
static void (*const TASKS[NB])(absolute_time_t) = {werewolf_service_0, werewolf_service_1, werewolf_service_2,
    werewolf_service_3, werewolf_service_4, werewolf_service_5, werewolf_service_6, werewolf_service_7,
    werewolf_service_8, werewolf_service_9, werewolf_service_10, werewolf_service_11, werewolf_service_12,
    werewolf_service_13, werewolf_service_14, werewolf_service_15, werewolf_service_16, werewolf_service_17,
    werewolf_service_18};

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

/* The special cases of the rules seen in the logs of the narrator, over all the games */
enum { EV_THIEF_TAKES, EV_THIEF_KEEPS, EV_LOVERS, EV_SPY, EV_CAUGHT, EV_HEAL, EV_POISON, EV_BOTH_POTIONS,
       EV_HUNTER_SHOOTS, EV_HUNTER_NIGHT, EV_ELECTED, EV_CAPTAIN_DECIDES, EV_TIEBREAK, EV_REVOTE, EV_SUCCESSOR,
       EV_NOBODY_OUT, EV_WIN_VILLAGE, EV_WIN_WOLVES, EV_WIN_LOVERS, EV_COUNT };
static const char *EV_NAMES[EV_COUNT] = {"thief takes a card", "thief keeps his card", "lovers",
    "little girl spies", "little girl caught", "witch heals", "witch poisons", "both potions the same night",
    "hunter shoots", "hunter killed at night", "captain elected", "tie decided by the captain's vote",
    "captain breaks a tie", "second vote", "captain's successor", "nobody eliminated", "village wins",
    "wolves win", "lovers win"};
static int events[EV_COUNT];
static int heal_night = -1, poison_night = -1, dawn_count = 0;
static bool hunter_at_night = false;

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
            events[EV_WIN_VILLAGE] += ! strcmp(w, "Le village gagne !");
            events[EV_WIN_WOLVES] += ! strcmp(w, "Les loups gagnent !");
            events[EV_WIN_LOVERS] += ! strcmp(w, "Les amoureux gagnent !");
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
    } else if (cur == 0) {
        if (sscanf(line, "werewolf: J%d is %15[^\n]", &k, role) == 2 && k > 0 && k < NB && ! strstr(line, "captain"))
            snprintf(narrator_roles[k], sizeof(narrator_roles[k]), "%s", role);
        if (sscanf(line, "werewolf: the thief J%d takes %15[^\n]", &k, role) == 2 && k > 0 && k < NB)
            snprintf(narrator_roles[k], sizeof(narrator_roles[k]), "%s", role);
        events[EV_THIEF_TAKES] += ! strncmp(line, "werewolf: the thief ", 20) && strstr(line, " takes ");
        events[EV_THIEF_KEEPS] += ! strcmp(line, "werewolf: the thief keeps his card");
        events[EV_LOVERS] += ! strncmp(line, "werewolf: lovers ", 17);
        events[EV_SPY] += ! strncmp(line, "werewolf: the little girl spies", 31);
        events[EV_CAUGHT] += ! strcmp(line, "werewolf: the little girl was caught, she dies instead");
        if (! strncmp(line, "werewolf: the witch heals", 25)) {
            ++events[EV_HEAL];
            heal_night = dawn_count;
        }
        if (! strncmp(line, "werewolf: the witch poisons", 27)) {
            ++events[EV_POISON];
            poison_night = dawn_count;
        }
        if (! strncmp(line, "werewolf: phase dawn", 20)) {
            events[EV_BOTH_POTIONS] += heal_night == dawn_count && poison_night == dawn_count;
            ++dawn_count;
            hunter_at_night = true;
        } else if (! strncmp(line, "werewolf: phase ", 16)) {
            if (! strncmp(line, "werewolf: phase hunter", 22))
                events[EV_HUNTER_NIGHT] += hunter_at_night;
            if (strncmp(line, "werewolf: phase shot", 20))
                hunter_at_night = false;
        }
        events[EV_HUNTER_SHOOTS] += ! strncmp(line, "werewolf: the hunter shoots", 27);
        events[EV_ELECTED] += strstr(line, " is elected captain") != NULL;
        events[EV_CAPTAIN_DECIDES] += ! strcmp(line, "werewolf: tie, the vote of the captain decides");
        events[EV_TIEBREAK] += ! strcmp(line, "werewolf: tie, the captain decides");
        events[EV_REVOTE] += ! strcmp(line, "werewolf: tie, second vote");
        events[EV_SUCCESSOR] += ! strncmp(line, "werewolf: the new captain is", 28);
        events[EV_NOBODY_OUT] += ! strcmp(line, "werewolf: the village eliminates nobody");
        if (strstr(line, " left the game"))
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

/* WEREWOLF_DUMP=<dir>: the pages drawn on badge 1 are saved there (PBM files, to look at them) */
static const char *dump_dir = NULL;
static int dumped = 0;

static void dump(void) {
    static uint8_t last[GFX_FB_SIZE];
    if (! dump_dir || dumped >= 400 || ! memcmp(last, fb, sizeof(last)))
        return;
    memcpy(last, fb, sizeof(last));
    char path[512];
    snprintf(path, sizeof(path), "%s/page_%03d.pbm", dump_dir, dumped++);
    FILE *f = fopen(path, "wb");
    if (! f)
        return;
    fprintf(f, "P4\n%d %d\n", GFX_WIDTH, GFX_HEIGHT);
    for (int i = 0; i < GFX_FB_SIZE; ++i)
        fputc(~fb[i] & 0xFF, f);  /* PBM: 1 = black */
    fclose(f);
}

static void render(int b) {
    cur = b;
    gfx_clear(fb, GFX_WHITE);
    APPS[b]->render(fb, host_time_us);
    if (b == 1)
        dump();
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

static uint8_t narrator_options = WW_OPT_ALL;  /* The setting kept by the narrator page between the games */

/* From the menu of the game (first line): the setup page, the roles of \p opts, "Ouvrir la partie" */
static void narrator_open(uint8_t opts) {
    keys(0, "b");  /* Mener une partie */
    for (int k = 0; k < 7; ++k) {  /* The 7 options, in the order of the page (werewolf.c OPTIONS) */
        static const uint8_t ORDER[7] = {WW_OPT_SEER, WW_OPT_WITCH, WW_OPT_HUNTER, WW_OPT_CUPID, WW_OPT_GIRL,
                                         WW_OPT_CAPTAIN, WW_OPT_THIEF};
        key(0, 'x');
        if ((opts ^ narrator_options) & ORDER[k])
            key(0, 'b');
    }
    narrator_options = opts;
    keys(0, "xxxb");  /* Débat, Loups, Ouvrir la partie */
}

/* A whole game: \p players badges join (+ the robots in test mode); \p away: a player who never opens the page
 * (only the hook of the main loop runs); \p leaver: a player who leaves during the game */
static void run_game(const char *name, int players, uint8_t opts, bool debug, int loss, int away, int leaver) {
    printf("--- %s: %d players, options 0x%02X, %s%d %% lost\n", name, players, opts, debug ? "test mode, " : "", loss);
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
    narrator_open(opts);
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
    bool left = false, left_alive = false;
    while (host_time_us - t0 < 6ull * 3600 * 1000000 && ! narrator_end) {
        run_ms(100);
        for (int b = 1; b <= players; ++b) {
            if (b == away || (b == leaver && left) || host_time_us < next_press[b])
                continue;
            next_press[b] = host_time_us + (1 + get_rand_32() % 8) * 1000000ull;
            for (int m = get_rand_32() % 4; m > 0; --m)
                key(b, 'x');
            key(b, 'b');
            if (get_rand_32() % 40 == 0) {
                key(b, 'B');  /* My card, for a while */
                key(b, 'x');  /* (the next card of the help) */
            }
        }
        if (leaver && ! left && host_time_us - t0 > 60 * 1000000ull) {
            in_app[leaver] = false;  /* From the menus (the page may show the help) */
            keys(leaver, "Ab");  /* Quitter la partie ? Oui */
            seen[leaver].left = left = true;
            left_alive = ! seen[leaver].dead;  /* The narrator removes a living player only */
        }
    }
    uint64_t secs = (host_time_us - t0) / 1000000;
    printf("    radio: narrator %.1f packets/s, players %.2f packets/s each\n", (double)sent_narrator / secs,
           (double)sent_players / secs / players);
    run_ms(5000);  /* The end reaches everybody */
    printf("    %s after %llu s, %d left\n", narrator_winner, (unsigned long long)secs, narrator_left);
    CHECK_EQ(narrator_end, 1);
    CHECK_EQ(narrator_left, left_alive ? 1 : 0);
    CHECK_EQ(seen[0].uicheck, 0);
    for (int b = 1; b <= players; ++b) {
        seen_t *s = &seen[b];
        CHECK_EQ(s->uicheck, 0);
        CHECK(s->role[0] && ! strcmp(s->role, narrator_roles[b]));  /* Each player got its card (after the thief) */
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
    n_badges = 9;
    loss_pct = 30;
    memset(seen, 0, sizeof(seen));
    for (int b = 0; b < NB; ++b) {
        in_app[b] = false;
        stores[b].admin = 0;
    }
    narrator_open(WW_OPT_ALL);
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

/* The help: every card and its details, drawn with the text check */
static void run_help(void) {
    printf("--- the help: the cards\n");
    n_badges = 2;
    memset(seen, 0, sizeof(seen));
    in_app[1] = false;
    keys(1, "yb");  /* Aide : les rôles (the last line) */
    for (int c = 0; c < WW_CARDS; ++c) {
        keys(1, "b");  /* The details */
        keys(1, "b");  /* The card again */
        key(1, 'x');
    }
    keys(1, "a");  /* Back to the menu of the game */
    keys(1, "a");  /* Back to the menus */
    CHECK_EQ(seen[1].uicheck, 0);
    CHECK(! in_app[1]);
}

int main(int argc, char **argv) {
    verbose = argc > 1;
    dump_dir = getenv("WEREWOLF_DUMP");
    test_deal();
    test_thief();
    test_votes();
    test_lovers();
    test_witch();
    test_girl();
    test_winner();
    test_texts();
    ui_check = true;  /* The texts cut or too wide are traced (uicheck:) */
    run_help();
    run_game("test mode, all the roles", 1, WW_OPT_ALL, true, 10, 0, 0);
    run_game("test mode, beginners", 1, WW_OPT_BEGINNER, true, 10, 0, 0);
    run_game("test mode, 3 players", 3, WW_OPT_ALL & ~WW_OPT_THIEF, true, 20, 0, 0);
    run_game("8 players, all the roles", 8, WW_OPT_ALL, false, 20, 0, 0);
    run_game("9 players, no captain (second vote)", 9, WW_OPT_ALL & ~WW_OPT_CAPTAIN, false, 20, 0, 0);
    run_game("12 players, 1 away, 1 leaves", 12, WW_OPT_ALL, false, 25, 4, 7);
    run_game("18 players, all the roles", 18, WW_OPT_ALL, false, 20, 0, 0);
    for (int seed = 0; seed < 12; ++seed) {
        sim_rng = 2000 + seed;
        run_game("test mode, roles at random", 1 + seed % 4, (get_rand_32() & WW_OPT_ALL) | WW_OPT_THIEF, true, 15,
                 0, 0);
    }
    for (int seed = 0; seed < 6; ++seed) {
        sim_rng = 3000 + seed;
        run_game("10 players, all the roles", 10, WW_OPT_ALL, false, 20, 0, 0);
    }
    run_abort();
    printf("at most %d packets sent in 10 ms\n", max_burst);
    printf("special cases seen:\n");
    for (int e = 0; e < EV_COUNT; ++e) {
        printf("    %-36s %d\n", EV_NAMES[e], events[e]);
        CHECK(events[e] > 0 || e == EV_WIN_LOVERS);
    }
    TEST_END();
}
