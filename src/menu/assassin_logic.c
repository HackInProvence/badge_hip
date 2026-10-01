/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The rules of the assassin game, see assassin_logic.h. */

#include <string.h>

#include "assassin_logic.h"

static uint32_t get32(const uint8_t *p) {
    return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

static void put32(uint8_t *p, uint32_t v) {
    p[0] = v;
    p[1] = v >> 8;
    p[2] = v >> 16;
    p[3] = v >> 24;
}

void assassin_game_init(assassin_game_t *g, int n) {
    memset(g, 0, sizeof(*g));
    g->n = n > ASSASSIN_LOGIC_MAX ? ASSASSIN_LOGIC_MAX : n;
    g->winner = ASSASSIN_NONE;
}

void assassin_ring(int n, uint8_t *next, uint32_t (*rnd)(void)) {
    uint8_t perm[ASSASSIN_LOGIC_MAX];
    if (n > ASSASSIN_LOGIC_MAX)
        n = ASSASSIN_LOGIC_MAX;
    for (int i = 0; i < n; ++i)
        perm[i] = i;
    for (int i = n - 1; i > 0; --i) {
        int j = rnd() % (i + 1);
        uint8_t t = perm[i];
        perm[i] = perm[j];
        perm[j] = t;
    }
    for (int i = 0; i < n; ++i)  /* Each one hunts the next one in the shuffled order: a single cycle */
        next[perm[i]] = perm[(i + 1) % n];
}

uint32_t assassin_hash(uint32_t a, uint32_t b) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < 8; ++i)
        h = (h ^ (uint8_t)((i < 4 ? a : b) >> (8 * (i & 3)))) * 16777619u;
    return h;
}

void assassin_seal(uint8_t *out, uint32_t id, uint32_t code, uint32_t key) {
    put32(out, id ^ assassin_hash(key, 0x5EA1));
    put32(out + 4, code ^ assassin_hash(key, 0xC0DE));
}

void assassin_unseal(const uint8_t *in, uint32_t key, uint32_t *id, uint32_t *code) {
    *id = get32(in) ^ assassin_hash(key, 0x5EA1);
    *code = get32(in + 4) ^ assassin_hash(key, 0xC0DE);
}

int assassin_survivors(const assassin_game_t *g) {
    int alive = 0;
    for (int i = 0; i < g->n; ++i)
        alive += ! (g->dead >> i & 1);
    return alive;
}

uint8_t assassin_follow(const assassin_game_t *g, const uint32_t *ids, uint8_t target, uint32_t *code) {
    for (int steps = 0; target < g->n && (g->dead >> target & 1); ++steps) {
        if (steps > g->n || ! (g->left >> target & 1))
            return ASSASSIN_NONE;
        uint32_t id, c;
        assassin_unseal(g->record[target], *code, &id, &c);
        uint8_t next = ASSASSIN_NONE;
        for (int i = 0; i < g->n; ++i)
            if (ids[i] == id)
                next = i;
        if (next == ASSASSIN_NONE)
            return ASSASSIN_NONE;  /* Not sealed with this code: not our target */
        target = next;
        *code = c;
    }
    return target < g->n ? target : ASSASSIN_NONE;
}

int assassin_status_pack(const assassin_game_t *g, uint8_t *d, int max, uint8_t first) {
    if (max < 10)
        return 0;
    d[0] = g->n;
    for (int i = 0; i < 8; ++i)
        d[1 + i] = g->dead >> (8 * i);
    d[9] = g->winner;
    int len = 10, records = 0;
    static uint8_t rotate = 0;  /* The other records, in turn */
    int start = first < g->n ? first : g->n ? rotate++ % g->n : 0;
    for (int k = 0; k < g->n && records < ASSASSIN_STATUS_RECORDS && len + 1 + ASSASSIN_SEAL <= max; ++k) {
        int i = (start + k) % g->n;
        if (g->left >> i & 1) {
            d[len++] = i;
            memcpy(d + len, g->record[i], ASSASSIN_SEAL);
            len += ASSASSIN_SEAL;
            ++records;
        }
    }
    return len;
}

bool assassin_status_merge(assassin_game_t *g, const uint8_t *d, int len, uint8_t me) {
    if (len < 10 || d[0] != g->n)
        return false;  /* Not the same game */
    uint64_t dead = 0;
    for (int i = 0; i < 8; ++i)
        dead |= (uint64_t)d[1 + i] << (8 * i);
    if (g->n < 64)
        dead &= (1ull << g->n) - 1;
    if (me < 64)
        dead &= ~(1ull << me);
    bool changed = (dead & ~g->dead) != 0;
    g->dead |= dead;
    if (d[9] < g->n && g->winner == ASSASSIN_NONE) {
        g->winner = d[9];
        changed = true;
    }
    for (int k = 10; k + 1 + ASSASSIN_SEAL <= len; k += 1 + ASSASSIN_SEAL) {
        uint8_t i = d[k];
        if (i < g->n && i != me && ! (g->left >> i & 1)) {
            memcpy(g->record[i], d + k + 1, ASSASSIN_SEAL);
            g->left |= 1ull << i;
            g->dead |= 1ull << i;
            changed = true;
        }
    }
    return changed;
}
