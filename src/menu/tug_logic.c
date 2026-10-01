/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The rules of the tug of war, see tug_logic.h. */

#include "tug_logic.h"

#define MAX_PLAYERS 64
#define MARGIN_PER_PLAYER 20  /* ~4 pulls/s for 20 s: a team 25 % stronger reaches the mark near the end */

static uint32_t xorshift(uint32_t *s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *s = x;
}

void tug_teams(uint32_t seed, int n, uint8_t *team) {
    uint8_t perm[MAX_PLAYERS];
    if (n > MAX_PLAYERS)
        n = MAX_PLAYERS;
    for (int i = 0; i < n; ++i)
        perm[i] = i;
    uint32_t s = seed ^ 0x9E3779B9u;
    if (! s)
        s = 1;
    for (int i = n - 1; i > 0; --i) {  /* Fisher-Yates, the same random numbers on every badge */
        int j = xorshift(&s) % (i + 1);
        uint8_t t = perm[i];
        perm[i] = perm[j];
        perm[j] = t;
    }
    int half = n / 2;
    for (int i = 0; i < n; ++i)
        team[perm[i]] = i < half ? TUG_CIGALES : i < 2 * half ? TUG_FOURMIS : TUG_REFEREE;
}

bool tug_pull(tug_pull_t *p, bool left, bool right) {
    if (left && right) {
        p->armed = false;  /* Both at once: not a pull */
        return false;
    }
    if (right && p->armed) {
        p->armed = false;
        return true;
    }
    if (left)
        p->armed = true;
    return false;
}

int tug_winner(uint32_t cigales, uint32_t fourmis) {
    return cigales > fourmis ? TUG_CIGALES : fourmis > cigales ? TUG_FOURMIS : -1;
}

int tug_margin(int team_size) {
    return MARGIN_PER_PLAYER * (team_size > 0 ? team_size : 1);
}

int tug_knot(int32_t diff, int margin, int half, int max) {
    int64_t x = -(int64_t)diff * half / (margin > 0 ? margin : 1);
    return x < -max ? -max : x > max ? max : (int)x;
}
