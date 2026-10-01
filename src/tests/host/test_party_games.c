/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Host tests of the rules of the group games: tug of war (tug_logic.c) and assassin (assassin_logic.c). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assassin_logic.h"
#include "tug_logic.h"
#include "test.h"

static void test_tug_teams(void) {
    for (int n = 2; n <= 40; ++n)
        for (uint32_t seed = 0; seed < 50; ++seed) {
            uint8_t team[64], again[64];
            tug_teams(seed * 2654435761u, n, team);
            tug_teams(seed * 2654435761u, n, again);
            int count[3] = {0, 0, 0};
            for (int i = 0; i < n; ++i)
                ++count[team[i] < 3 ? team[i] : 2];
            CHECK(! memcmp(team, again, n));  /* The same on every badge */
            CHECK_EQ(count[TUG_CIGALES], n / 2);
            CHECK_EQ(count[TUG_FOURMIS], n / 2);
            CHECK_EQ(count[TUG_REFEREE], n % 2);
        }
    /* The seed changes the teams */
    uint8_t a[10], b[10];
    int differ = 0;
    tug_teams(1, 10, a);
    for (uint32_t s = 2; s < 20; ++s) {
        tug_teams(s, 10, b);
        differ += memcmp(a, b, 10) != 0;
    }
    CHECK(differ > 10);
    /* Everybody is the referee now and then (5 players) */
    int referee[5] = {0};
    for (uint32_t s = 0; s < 500; ++s) {
        uint8_t t[5];
        tug_teams(s, 5, t);
        for (int i = 0; i < 5; ++i)
            referee[i] += t[i] == TUG_REFEREE;
    }
    for (int i = 0; i < 5; ++i)
        CHECK(referee[i] > 50);
}

static void test_tug_pulls(void) {
    tug_pull_t p = {false};
    CHECK(! tug_pull(&p, false, true));  /* Right first: nothing */
    CHECK(! tug_pull(&p, true, false));
    CHECK(tug_pull(&p, false, true));  /* Left then right: a pull */
    CHECK(! tug_pull(&p, false, true));  /* Right again: nothing */
    CHECK(! tug_pull(&p, true, false));
    CHECK(! tug_pull(&p, true, false));  /* Left twice: still one pull */
    CHECK(tug_pull(&p, false, true));
    CHECK(! tug_pull(&p, true, true));  /* Both at once: nothing */
    CHECK(! tug_pull(&p, false, true));
    int pulls = 0;
    for (int i = 0; i < 100; ++i)
        pulls += tug_pull(&p, i % 2 == 0, i % 2 == 1);
    CHECK_EQ(pulls, 50);

    CHECK_EQ(tug_winner(10, 5), TUG_CIGALES);
    CHECK_EQ(tug_winner(5, 10), TUG_FOURMIS);
    CHECK_EQ(tug_winner(7, 7), -1);
    CHECK_EQ(tug_knot(0, 40, 60, 88), 0);
    CHECK_EQ(tug_knot(40, 40, 60, 88), -60);  /* Cigales ahead: to the left, at the mark */
    CHECK_EQ(tug_knot(-20, 40, 60, 88), 30);
    CHECK_EQ(tug_knot(1000, 40, 60, 88), -88);  /* Clamped */
    CHECK(tug_margin(1) > 0);
    CHECK_EQ(tug_margin(4), 4 * tug_margin(1));
}

static uint32_t lcg = 12345;
static uint32_t rnd(void) {
    lcg = lcg * 1103515245u + 12345u;
    return lcg >> 8;
}

static void test_ring(void) {
    for (int n = 2; n <= 40; ++n)
        for (int k = 0; k < 20; ++k) {
            uint8_t next[64];
            assassin_ring(n, next, rnd);
            /* One cycle through everybody, nobody hunts himself */
            uint8_t seen[64] = {0};
            int p = 0;
            for (int i = 0; i < n; ++i) {
                CHECK(next[p] != p);
                seen[p] = 1;
                p = next[p];
            }
            CHECK_EQ(p, 0);
            for (int i = 0; i < n; ++i)
                CHECK(seen[i]);
        }
}

static void test_seal(void) {
    uint8_t s[ASSASSIN_SEAL];
    uint32_t id, code;
    assassin_seal(s, 0x12345678, 0xCAFEBABE, 0xDEADBEEF);
    assassin_unseal(s, 0xDEADBEEF, &id, &code);
    CHECK_EQ(id, 0x12345678);
    CHECK_EQ(code, 0xCAFEBABE);
    assassin_unseal(s, 0xDEADBEEE, &id, &code);
    CHECK(id != 0x12345678);
    CHECK(assassin_hash(1, 2) != assassin_hash(2, 1));
    CHECK(assassin_hash(1, 2) != assassin_hash(1, 3));
}

/* A game of 6: kills, a player who leaves, the STATUS packets between two badges */
static void test_game(void) {
    enum { N = 6 };
    uint32_t ids[N], codes[N];
    uint8_t next[N];
    for (int i = 0; i < N; ++i) {
        ids[i] = 0x1000 + i * 77;
        codes[i] = rnd() ^ rnd() << 16;
    }
    assassin_ring(N, next, rnd);
    assassin_game_t a, b;
    assassin_game_init(&a, N);
    assassin_game_init(&b, N);
    CHECK_EQ(assassin_survivors(&a), N);
    CHECK_EQ(a.winner, ASSASSIN_NONE);

    /* Player 0 hunts t = next[0]; t leaves: its target goes, sealed with its code, to 0 */
    uint8_t t = next[0], tt = next[t];
    a.dead |= 1ull << t;
    a.left |= 1ull << t;
    assassin_seal(a.record[t], ids[tt], codes[tt], codes[t]);
    uint8_t d[47];
    int len = assassin_status_pack(&a, d, sizeof(d), t);
    CHECK_EQ(len, 10 + 1 + ASSASSIN_SEAL);
    CHECK(assassin_status_merge(&b, d, len, 0));
    CHECK(! assassin_status_merge(&b, d, len, 0));  /* The same again: nothing new */
    CHECK_EQ(assassin_survivors(&b), N - 1);
    uint32_t code = codes[t];
    CHECK_EQ(assassin_follow(&b, ids, t, &code), tt);
    CHECK_EQ(code, codes[tt]);
    /* Somebody else can't open it */
    code = codes[tt];
    CHECK_EQ(assassin_follow(&b, ids, t, &code), ASSASSIN_NONE);

    /* Two leavers in a row */
    uint8_t ttt = next[tt];
    b.dead |= 1ull << tt;
    b.left |= 1ull << tt;
    assassin_seal(b.record[tt], ids[ttt], codes[ttt], codes[tt]);
    code = codes[t];
    CHECK_EQ(assassin_follow(&b, ids, t, &code), ttt);
    CHECK_EQ(code, codes[ttt]);

    /* A killed player: no record */
    assassin_game_t c;
    assassin_game_init(&c, N);
    c.dead |= 1ull << 3;
    code = 0;
    CHECK_EQ(assassin_follow(&c, ids, 3, &code), ASSASSIN_NONE);
    CHECK_EQ(assassin_follow(&c, ids, 2, &code), 2);

    /* A badge is never marked dead by the others, the winner spreads, another game is ignored */
    assassin_game_t e;
    assassin_game_init(&e, N);
    e.dead = 0x3F;
    e.winner = 4;
    len = assassin_status_pack(&e, d, sizeof(d), ASSASSIN_NONE);
    assassin_game_t f;
    assassin_game_init(&f, N);
    CHECK(assassin_status_merge(&f, d, len, 4));
    CHECK_EQ(f.dead, 0x2F);
    CHECK_EQ(f.winner, 4);
    CHECK_EQ(assassin_survivors(&f), 1);
    assassin_game_t h;
    assassin_game_init(&h, N + 1);
    CHECK(! assassin_status_merge(&h, d, len, 0));

    /* Many leavers: at most ASSASSIN_STATUS_RECORDS per packet, all of them in turn */
    assassin_game_t m, r;
    assassin_game_init(&m, 30);
    assassin_game_init(&r, 30);
    for (int i = 0; i < 10; ++i) {
        m.dead |= 1ull << (i * 3);
        m.left |= 1ull << (i * 3);
        memset(m.record[i * 3], i, ASSASSIN_SEAL);
    }
    for (int k = 0; k < 40; ++k) {
        len = assassin_status_pack(&m, d, sizeof(d), ASSASSIN_NONE);
        CHECK(len <= 47);
        assassin_status_merge(&r, d, len, 1);
    }
    CHECK_EQ(r.left, m.left);
    CHECK(! memcmp(r.record, m.record, sizeof(r.record)));
}

int main(void) {
    test_tug_teams();
    test_tug_pulls();
    test_ring();
    test_seal();
    test_game();
    TEST_END();
}
