/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The rules of the loup-garou, see werewolf_logic.h. Pure C: no SDK, so that the PC tests compile it. */

#include <string.h>

#include "werewolf_logic.h"

static const char *ROLE_NAMES[WW_ROLES] = {"Villageois", "Loup-garou", "Voyante", "Sorcière", "Chasseur", "Cupidon",
                                           "Petite fille", "Voleur"};

int ww_wolves_for(int n) {
    return n >= 12 ? 3 : n >= 8 ? 2 : 1;  /* The rules from 8 players; 1 below (small games unlocked by the admin) */
}

void ww_deal(ww_game_t *g, int n, uint8_t options, ww_rand_t rnd) {
    if (n > WW_MAX_PLAYERS)
        n = WW_MAX_PLAYERS;
    if (n < 0)
        n = 0;
    memset(g, 0, sizeof(*g));
    g->n = n;
    g->options = options;
    g->lovers[0] = g->lovers[1] = g->captain = WW_NONE;
    g->center[0] = g->center[1] = WW_NONE;
    g->alive = (1u << n) - 1;
    /* The cards: the wolves, the roles (in this order when the players are too few: small test games), simple
     * villagers for the others, and 2 more simple villagers with the thief */
    bool thief = options & WW_OPT_THIEF;
    int cards = n + (thief ? 2 : 0);
    uint8_t deck[WW_MAX];
    int k = 0;
    for (int w = ww_wolves_for(n); w > 0 && k < n; --w)
        deck[k++] = WW_WOLF;
    static const struct { uint8_t opt, role; } ROLES[] = {
        {WW_OPT_SEER, WW_SEER}, {WW_OPT_WITCH, WW_WITCH}, {WW_OPT_HUNTER, WW_HUNTER}, {WW_OPT_CUPID, WW_CUPID},
        {WW_OPT_GIRL, WW_GIRL}, {WW_OPT_THIEF, WW_THIEF},
    };
    for (unsigned r = 0; r < sizeof(ROLES) / sizeof(ROLES[0]); ++r)
        if ((options & ROLES[r].opt) && k < n)
            deck[k++] = ROLES[r].role;
    while (k < cards)
        deck[k++] = WW_VILLAGER;
    /* Fisher-Yates */
    for (int i = cards - 1; i > 0; --i) {
        int j = rnd() % (uint32_t)(i + 1);
        uint8_t t = deck[i];
        deck[i] = deck[j];
        deck[j] = t;
    }
    memcpy(g->role, deck, n);
    if (thief) {
        g->center[0] = deck[n];
        g->center[1] = deck[n + 1];
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

int ww_partner(const ww_game_t *g, int i) {
    if (i < 0 || g->lovers[0] == WW_NONE)
        return WW_NONE;
    return g->lovers[0] == i ? g->lovers[1] : g->lovers[1] == i ? g->lovers[0] : WW_NONE;
}

bool ww_may_harm(const ww_game_t *g, int actor, int target) {
    return ww_alive(g, target) && target != actor && ww_partner(g, actor) != target;
}

bool ww_thief_must_take(const ww_game_t *g) {
    return g->center[0] == WW_WOLF && g->center[1] == WW_WOLF;
}

bool ww_thief_swap(ww_game_t *g, int thief, int k) {
    if (thief < 0 || thief >= g->n || g->role[thief] != WW_THIEF || g->center[0] == WW_NONE)
        return false;
    if (k < 0 || k > 1)
        return ! ww_thief_must_take(g);  /* Keeps his card */
    uint8_t card = g->center[k];
    g->center[k] = WW_THIEF;
    g->role[thief] = card;
    return true;
}

int ww_count_votes(const ww_game_t *g, const uint8_t *votes, int double_voter, uint32_t *tied) {
    uint8_t count[WW_MAX] = {0};
    int best = 0;
    for (int i = 0; i < g->n; ++i) {
        int t = votes[i];
        if (! ww_alive(g, i) || t >= g->n)
            continue;
        count[t] += i == double_voter ? 2 : 1;
        if (count[t] > best)
            best = count[t];
    }
    uint32_t top = 0;
    for (int t = 0; best && t < g->n; ++t)
        if (count[t] == best)
            top |= 1u << t;
    if (tied)
        *tied = __builtin_popcount(top) > 1 ? top : 0;
    if (__builtin_popcount(top) != 1)
        return WW_NONE;
    return __builtin_ctz(top);
}

int ww_pick(uint32_t mask, ww_rand_t rnd) {
    int n = __builtin_popcount(mask);
    if (! n)
        return WW_NONE;
    int k = rnd() % (uint32_t)n;
    for (int i = 0; i < 32; ++i)
        if ((mask >> i & 1) && k-- == 0)
            return i;
    return WW_NONE;
}

ww_day_t ww_day_vote(const ww_game_t *g, const uint8_t *votes, bool second, int *out, uint32_t *tied) {
    int captain = ww_alive(g, g->captain) ? g->captain : WW_NONE;
    uint32_t t = 0;
    int o = ww_count_votes(g, votes, captain, &t);
    *out = WW_NONE;
    *tied = t;
    if (o != WW_NONE) {
        *out = o;
        return WW_DAY_OUT;
    }
    if (! t)
        return WW_DAY_NOBODY;  /* Nobody voted */
    if (captain != WW_NONE) {
        int v = votes[captain];
        if (v < g->n && (t >> v & 1)) {
            *out = v;  /* The vote of the captain designates the victim */
            return WW_DAY_OUT;
        }
        return WW_DAY_TIEBREAK;
    }
    return second ? WW_DAY_NOBODY : WW_DAY_REVOTE;
}

bool ww_spy(const ww_game_t *g, uint32_t r, uint8_t *seen) {
    uint32_t wolves = ww_wolves(g) & g->alive;
    int n = __builtin_popcount(wolves);
    *seen = WW_NONE;
    if (n) {
        int k = (r / 3) % n;
        for (int i = 0; i < g->n; ++i)
            if ((wolves >> i & 1) && k-- == 0)
                *seen = i;
    }
    return r % 3 == 0;
}

int ww_kill(ww_game_t *g, int i, uint8_t *deaths, int n_deaths) {
    if (! ww_alive(g, i))
        return n_deaths;
    g->alive &= ~(1u << i);
    if (n_deaths < WW_MAX)
        deaths[n_deaths++] = i;
    int p = ww_partner(g, i);
    if (p != WW_NONE)  /* The other one dies of grief */
        n_deaths = ww_kill(g, p, deaths, n_deaths);
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
    if (! others)
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
