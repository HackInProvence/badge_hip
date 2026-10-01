/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The rules of the loup-garou, see werewolf_logic.h. Pure C: no SDK, so that the PC tests compile it. */

#include <string.h>

#include "werewolf_logic.h"

static const char *ROLE_NAMES[WW_ROLES] = {"Villageois", "Loup-garou", "Voyante", "Sorcière", "Chasseur", "Cupidon"};

int ww_min_players(bool advanced) {
    return advanced ? WW_MIN_ADVANCED : WW_MIN_SIMPLE;
}

int ww_wolves_for(int n) {
    return n >= 15 ? 4 : n >= 11 ? 3 : n >= 7 ? 2 : 1;
}

void ww_deal(ww_game_t *g, int n, bool advanced, ww_rand_t rnd) {
    if (n > WW_MAX)
        n = WW_MAX;
    if (n < 0)
        n = 0;
    memset(g, 0, sizeof(*g));
    g->n = n;
    g->lovers[0] = g->lovers[1] = WW_NONE;
    g->alive = n >= 32 ? 0xFFFFFFFFu : (1u << n) - 1;
    /* The special roles first (a small debug game gets the most important ones), then the villagers */
    int k = 0;
    for (int w = ww_wolves_for(n); w > 0 && k < n; --w)
        g->role[k++] = WW_WOLF;
    if (k < n)
        g->role[k++] = WW_SEER;
    if (advanced) {
        if (k < n)
            g->role[k++] = WW_WITCH;
        if (k < n)
            g->role[k++] = WW_HUNTER;
        if (n >= WW_CUPID_MIN && k < n)
            g->role[k++] = WW_CUPID;
    }
    while (k < n)
        g->role[k++] = WW_VILLAGER;
    /* Fisher-Yates */
    for (int i = n - 1; i > 0; --i) {
        int j = rnd() % (uint32_t)(i + 1);
        uint8_t t = g->role[i];
        g->role[i] = g->role[j];
        g->role[j] = t;
    }
}

int ww_count_alive(const ww_game_t *g) {
    int c = 0;
    for (int i = 0; i < g->n; ++i)
        c += ww_alive(g, i);
    return c;
}

int ww_find(const ww_game_t *g, ww_role_t r) {
    for (int i = 0; i < g->n; ++i)
        if (g->role[i] == r)
            return i;
    return -1;
}

uint32_t ww_wolves(const ww_game_t *g) {
    uint32_t m = 0;
    for (int i = 0; i < g->n; ++i)
        if (g->role[i] == WW_WOLF)
            m |= 1u << i;
    return m;
}

int ww_tally(const uint8_t *votes, int n_votes, int n, ww_rand_t rnd) {
    uint8_t count[WW_MAX] = {0};
    int best = 0;
    for (int i = 0; i < n_votes; ++i)
        if (votes[i] < n && votes[i] < WW_MAX && ++count[votes[i]] > best)
            best = count[votes[i]];
    if (! best)
        return WW_NONE;
    int tied = 0;
    for (int t = 0; t < n; ++t)
        tied += count[t] == best;
    if (tied > 1 && ! rnd)
        return WW_NONE;
    int pick = tied > 1 ? (int)(rnd() % (uint32_t)tied) : 0;
    for (int t = 0; t < n; ++t)
        if (count[t] == best && pick-- == 0)
            return t;
    return WW_NONE;
}

int ww_kill(ww_game_t *g, int i, uint8_t *deaths, int n_deaths) {
    if (! ww_alive(g, i))
        return n_deaths;
    g->alive &= ~(1u << i);
    if (n_deaths < WW_MAX)
        deaths[n_deaths++] = i;
    if (g->lovers[0] == i || g->lovers[1] == i)  /* The other one dies of grief */
        n_deaths = ww_kill(g, g->lovers[0] == i ? g->lovers[1] : g->lovers[0], deaths, n_deaths);
    return n_deaths;
}

int ww_dawn(ww_game_t *g, int victim, bool healed, int poisoned, uint8_t *deaths) {
    int n = 0;
    if (healed && ! g->heal_used && victim != WW_NONE)
        g->heal_used = true;
    else if (victim != WW_NONE)
        n = ww_kill(g, victim, deaths, n);
    if (poisoned != WW_NONE && ! g->poison_used) {
        g->poison_used = true;
        n = ww_kill(g, poisoned, deaths, n);
    }
    return n;
}

bool ww_link(ww_game_t *g, int a, int b) {
    if (a == b || a < 0 || b < 0 || a >= g->n || b >= g->n)
        return false;
    g->lovers[0] = a;
    g->lovers[1] = b;
    return true;
}

bool ww_mixed_lovers(const ww_game_t *g) {
    if (g->lovers[0] == WW_NONE || g->lovers[1] == WW_NONE)
        return false;
    return (g->role[g->lovers[0]] == WW_WOLF) != (g->role[g->lovers[1]] == WW_WOLF);
}

ww_win_t ww_winner(const ww_game_t *g) {
    int wolves = 0, others = 0;
    for (int i = 0; i < g->n; ++i)
        if (ww_alive(g, i)) {
            if (g->role[i] == WW_WOLF)
                ++wolves;
            else
                ++others;
        }
    if (wolves + others == 0)
        return WW_WIN_DRAW;
    if (wolves + others == 2 && ww_mixed_lovers(g) && ww_alive(g, g->lovers[0]) && ww_alive(g, g->lovers[1]))
        return WW_WIN_LOVERS;
    if (! wolves)
        return WW_WIN_VILLAGE;
    if (wolves >= others)
        return WW_WIN_WOLVES;
    return WW_WIN_NONE;
}

bool ww_is_winner(const ww_game_t *g, int i, ww_win_t win) {
    if (i < 0 || i >= g->n)
        return false;
    bool lover = g->lovers[0] == i || g->lovers[1] == i;
    bool mixed = lover && ww_mixed_lovers(g);  /* Their own camp: they only win together */
    switch (win) {
    case WW_WIN_VILLAGE: return g->role[i] != WW_WOLF && ! mixed;
    case WW_WIN_WOLVES: return g->role[i] == WW_WOLF && ! mixed;
    case WW_WIN_LOVERS: return lover;
    default: return false;
    }
}

const char *ww_role_name(int role) {
    return role >= 0 && role < WW_ROLES ? ROLE_NAMES[role] : "?";
}

const char *ww_win_text(ww_win_t win) {
    switch (win) {
    case WW_WIN_VILLAGE: return "Le village gagne !";
    case WW_WIN_WOLVES: return "Les loups gagnent !";
    case WW_WIN_LOVERS: return "Les amoureux gagnent !";
    case WW_WIN_DRAW: return "Personne ne gagne";
    default: return "";
    }
}
