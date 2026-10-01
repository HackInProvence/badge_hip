/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Host tests of the smuggler cicada: the cargo (smuggler_goods.c) and the trade protocol (smuggler_trade.c) between
 * two simulated badges over a radio that loses, repeats and delays packets: a good is never created, and never lost
 * as long as the two badges meet again. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

/* The log lines of the protocol, only shown for the first scenarios (the random runs would print millions) */
static bool quiet = false;
static int log_printf(const char *fmt, ...) {
    if (quiet)
        return 0;
    va_list ap;
    va_start(ap, fmt);
    int n = vprintf(fmt, ap);
    va_end(ap);
    return n;
}
#define printf log_printf
#include "smuggler_goods.c"
#include "smuggler_trade.c"
#undef printf

#include "test.h"

/* ------ The simulated radio ------ */

#define QUEUE 256
#define ID_A 0x11111111u
#define ID_B 0x22222222u

typedef struct {
    uint32_t src, to;  /* to: the badge that hears it */
    uint32_t at;  /* Delivered at */
    uint8_t len;
    uint8_t data[TRADE_PACKET_MAX];
} air_t;

static air_t air[QUEUE];
static int n_air = 0;
static uint32_t now = 0;
static int loss_percent = 0, dup_percent = 0;
static bool apart = false;  /* The badges are too far: nothing goes through */
static uint32_t block_from = 0;  /* The packets of this badge are lost */
static uint32_t sending = 0;  /* The badge whose code runs (the callbacks have no context) */
static int n_finished[2];

static uint32_t rnd_state = 1;
static uint32_t rnd(void) {
    rnd_state = rnd_state * 1103515245u + 12345u;
    return rnd_state >> 8;
}

static void radio_send(const uint8_t *data, uint8_t len) {
    int copies = 1 + ((int)(rnd() % 100) < dup_percent);
    for (int c = 0; c < copies; ++c) {
        if (apart || sending == block_from || (int)(rnd() % 100) < loss_percent || n_air >= QUEUE)
            continue;
        air_t *a = &air[n_air++];
        a->src = sending;
        a->to = sending == ID_A ? ID_B : ID_A;
        a->at = now + 20 + rnd() % 150;  /* Queued, sent after a while: the order can change */
        a->len = len;
        memcpy(a->data, data, len);
    }
}

static uint8_t cargo_a[SMUGGLER_GOODS], cargo_b[SMUGGLER_GOODS];
static trade_t ta, tb;

static void changed(void) {
}

static void finished(trade_t *t, uint8_t gave, uint8_t got) {
    (void)gave;
    (void)got;
    ++n_finished[t == &ta ? 0 : 1];
}

static void deliver(void) {
    for (int i = 0; i < n_air; ) {
        if (air[i].at <= now) {
            air_t a = air[i];
            air[i] = air[--n_air];
            trade_t *t = a.to == ID_A ? &ta : &tb;
            sending = a.to;
            trade_receive(t, a.src, a.data, a.len, -40, now);
        } else {
            ++i;
        }
    }
}

static void step(uint32_t ms) {
    for (uint32_t i = 0; i < ms; i += 10) {
        now += 10;
        deliver();
        sending = ID_A;
        trade_task(&ta, now);
        sending = ID_B;
        trade_task(&tb, now);
    }
}

static void setup(void) {
    n_air = 0;
    memset(cargo_a, 0, sizeof(cargo_a));
    memset(cargo_b, 0, sizeof(cargo_b));
    for (int i = 0; i < 3; ++i) {
        cargo_add(cargo_a, 1);
        cargo_add(cargo_b, 7);
    }
    cargo_add(cargo_a, 23);
    cargo_add(cargo_b, 14);
    trade_init(&ta, ID_A, cargo_a, radio_send, changed, finished);
    trade_init(&tb, ID_B, cargo_b, radio_send, changed, finished);
    strcpy(ta.my_name, "Alice");
    strcpy(tb.my_name, "Bob");
    loss_percent = dup_percent = 0;
    apart = false;
    block_from = 0;
    n_finished[0] = n_finished[1] = 0;
}

/* Goods of each kind on the two badges (plus the sealed one, still owned by the trade) */
static int total(int good) {
    return cargo_count(cargo_a, good) + cargo_count(cargo_b, good);
}

static int sealed_total(int good) {
    return total(good) + (ta.sealed && ta.my_good == good) + (tb.sealed && tb.my_good == good);
}

static bool same_totals(const int *before) {
    for (int g = 0; g < SMUGGLER_GOODS; ++g)
        if (total(g) != before[g])
            return false;
    return true;
}

static void totals(int *out) {
    for (int g = 0; g < SMUGGLER_GOODS; ++g)
        out[g] = total(g);
}

/* ------ The scenarios ------ */

static void test_cargo(void) {
    uint8_t c[SMUGGLER_GOODS] = {0};
    CHECK(! cargo_seen(c, 3));
    CHECK(cargo_add(c, 3));
    CHECK(cargo_add(c, 3));
    CHECK_EQ(cargo_count(c, 3), 2);
    CHECK(cargo_take(c, 3));
    CHECK(cargo_take(c, 3));
    CHECK(! cargo_take(c, 3));
    CHECK_EQ(cargo_count(c, 3), 0);
    CHECK(cargo_seen(c, 3));  /* Owned once: shown in the Collection */
    CHECK_EQ(cargo_seen_kinds(c), 1);
    CHECK_EQ(cargo_kinds(c), 0);
    for (int i = 0; i < 120; ++i)
        cargo_add(c, 0);
    CHECK_EQ(cargo_count(c, 0), SMUGGLER_MAX_COUNT);
    CHECK(! cargo_add(c, SMUGGLER_GOODS));
    CHECK(! cargo_add(c, SMUGGLER_NO_GOOD));
    cargo_add(c, 23);
    CHECK_EQ(cargo_value(c), SMUGGLER_MAX_COUNT * smuggler_goods[0].value + smuggler_goods[23].value);
    CHECK_EQ(cargo_nth(c, 0), 0);
    CHECK_EQ(cargo_nth(c, 1), 23);
    CHECK_EQ(cargo_nth(c, 2), SMUGGLER_NO_GOOD);
    /* An erased store reads 0 (store.c): empty cargo */
    CHECK_EQ(SMUGGLER_GOODS, smuggler_n_goods);
    CHECK(SMUGGLER_GOODS <= 32);
    /* The goods: names, values, icons */
    int per_rarity[RARITIES] = {0};
    for (int i = 0; i < SMUGGLER_GOODS; ++i) {
        CHECK(smuggler_goods[i].name && smuggler_goods[i].icon && smuggler_goods[i].value > 0);
        ++per_rarity[smuggler_goods[i].rarity];
    }
    CHECK(per_rarity[RARITY_COMMON] && per_rarity[RARITY_RARE] && per_rarity[RARITY_LEGENDARY]);
    /* Random goods: the rarities in proportion, only commons when asked */
    int got[RARITIES] = {0};
    for (int i = 0; i < 20000; ++i) {
        int g = smuggler_random_good(rnd(), rnd(), RARITY_LEGENDARY);
        CHECK(g >= 0 && g < SMUGGLER_GOODS);
        ++got[smuggler_goods[g].rarity];
    }
    printf("random goods: %d common, %d rare, %d legendary\n", got[0], got[1], got[2]);
    CHECK(got[0] > 14000 && got[0] < 16000 && got[2] > 400 && got[2] < 800);
    bool only_common = true;
    for (int i = 0; i < 2000; ++i)
        only_common &= smuggler_goods[smuggler_random_good(rnd(), rnd(), RARITY_COMMON)].rarity == RARITY_COMMON;
    CHECK(only_common);
}

static void run_until_final(uint32_t max_ms) {
    for (uint32_t t = 0; t < max_ms && ! (trade_final(&ta) && trade_final(&tb)); t += 10)
        step(10);
}

static void test_exchange(void) {
    setup();
    sending = ID_A;
    CHECK(trade_invite(&ta, ID_B, "Bob", TRADE_EXCHANGE, SMUGGLER_NO_GOOD, 0xC0FFEE, now));
    step(300);
    CHECK(tb.invited);
    CHECK_STR(tb.inv_name, "Alice");
    sending = ID_B;
    CHECK(trade_accept(&tb, now));
    step(300);
    CHECK_EQ(ta.state, TS_CHOOSE);
    CHECK_EQ(tb.state, TS_CHOOSE);
    sending = ID_A;
    CHECK(! trade_offer(&ta, 7, now));  /* Not in its cargo */
    CHECK(trade_offer(&ta, 1, now));
    sending = ID_B;
    CHECK(trade_offer(&tb, 14, now));
    step(400);
    CHECK_EQ(ta.state, TS_REVIEW);
    CHECK_EQ(tb.state, TS_REVIEW);
    CHECK_EQ(ta.peer_good, 14);
    CHECK_EQ(tb.peer_good, 1);
    sending = ID_B;
    CHECK(trade_confirm(&tb, now));
    CHECK(tb.sealed);
    CHECK(! trade_cancellable(&tb));
    CHECK_EQ(cargo_count(cargo_b, 14), 0);
    step(400);
    CHECK(ta.peer_confirmed);
    CHECK_EQ(ta.state, TS_REVIEW);  /* The inviter decides when it confirms */
    sending = ID_A;
    CHECK(trade_confirm(&ta, now));
    CHECK_EQ(ta.state, TS_DONE);
    step(400);
    CHECK_EQ(tb.state, TS_DONE);
    CHECK_EQ(cargo_count(cargo_a, 1), 2);
    CHECK_EQ(cargo_count(cargo_a, 14), 1);
    CHECK_EQ(cargo_count(cargo_b, 1), 1);
    CHECK_EQ(cargo_count(cargo_b, 14), 0);
    CHECK_EQ(n_finished[0], 1);
    CHECK_EQ(n_finished[1], 1);
    step(TRADE_DONE_MS + 1000);  /* The repeated packets change nothing */
    CHECK_EQ(cargo_count(cargo_a, 14), 1);
    CHECK_EQ(cargo_count(cargo_b, 1), 1);
    CHECK_EQ(n_finished[1], 1);
}

static void test_gift_and_refuse(void) {
    setup();
    sending = ID_A;
    CHECK(! trade_invite(&ta, ID_B, "Bob", TRADE_GIFT, 7, 0xBEEF, now));  /* Not in its cargo */
    CHECK(trade_invite(&ta, ID_B, "Bob", TRADE_GIFT, 23, 0xBEEF, now));
    step(300);
    CHECK(tb.invited && tb.inv_mode == TRADE_GIFT && tb.inv_good == 23);
    sending = ID_B;
    trade_refuse(&tb);
    step(1000);
    CHECK_EQ(ta.state, TS_REFUSED);
    CHECK(! tb.invited);
    step(TRADE_INVITE_MS + 500);
    CHECK(! tb.invited);  /* Not invited again by the repeated packets */
    sending = ID_A;
    trade_close(&ta);
    CHECK(trade_invite(&ta, ID_B, "Bob", TRADE_GIFT, 23, 0xBEF0, now));
    step(300);
    sending = ID_B;
    CHECK(trade_accept(&tb, now));
    step(600);
    CHECK_EQ(ta.state, TS_DONE);
    CHECK_EQ(tb.state, TS_DONE);
    CHECK_EQ(cargo_count(cargo_a, 23), 0);
    CHECK_EQ(cargo_count(cargo_b, 23), 1);
    CHECK(cargo_seen(cargo_a, 23));
}

static void test_cancel_after_seal(void) {
    /* The guest sealed its good, the inviter cancels: the good comes back */
    setup();
    sending = ID_A;
    trade_invite(&ta, ID_B, "Bob", TRADE_EXCHANGE, SMUGGLER_NO_GOOD, 0x1234, now);
    step(300);
    sending = ID_B;
    trade_accept(&tb, now);
    step(300);
    sending = ID_A;
    trade_offer(&ta, 1, now);
    sending = ID_B;
    trade_offer(&tb, 7, now);
    step(400);
    sending = ID_B;
    trade_confirm(&tb, now);
    CHECK_EQ(cargo_count(cargo_b, 7), 2);
    step(300);
    sending = ID_A;
    CHECK(trade_cancellable(&ta));
    trade_cancel(&ta, now);
    step(1000);
    CHECK_EQ(tb.state, TS_CANCELLED);
    CHECK_EQ(cargo_count(cargo_b, 7), 3);
    CHECK_EQ(cargo_count(cargo_a, 1), 3);
}

static void test_apart_after_commit(void) {
    /* The inviter decided, then the badges are apart before the guest heard it: the guest waits (sealed), slowly
     * asking, and gets its good when they meet again, even long after */
    setup();
    int before[SMUGGLER_GOODS];
    totals(before);
    sending = ID_A;
    trade_invite(&ta, ID_B, "Bob", TRADE_EXCHANGE, SMUGGLER_NO_GOOD, 0x5678, now);
    step(300);
    sending = ID_B;
    trade_accept(&tb, now);
    step(300);
    sending = ID_A;
    trade_offer(&ta, 23, now);
    sending = ID_B;
    trade_offer(&tb, 14, now);
    step(400);
    sending = ID_A;
    trade_confirm(&ta, now);
    block_from = ID_A;  /* B hears nothing more from A: the CONFIRM of B arrives, the DONE of A never */
    sending = ID_B;
    trade_confirm(&tb, now);
    step(1000);
    CHECK_EQ(ta.state, TS_DONE);
    step(5 * TRADE_LOST_MS);
    CHECK_EQ(tb.state, TS_CONFIRMED);  /* Still waiting, sealed */
    CHECK(tb.sealed);
    CHECK_EQ(total(14), before[14]);  /* At A, and sealed out of B: counted once */
    CHECK_EQ(total(23), before[23] - 1);  /* On its way to B */
    for (int g = 0; g < SMUGGLER_GOODS; ++g)
        CHECK(total(g) <= before[g]);  /* Nothing created meanwhile */
    /* They meet again: the record of A answers the CONFIRM of B */
    block_from = 0;
    step(TRADE_SLOW_MS * 2);
    CHECK_EQ(tb.state, TS_DONE);
    CHECK(same_totals(before));
    CHECK_EQ(cargo_count(cargo_b, 23), 1);
    CHECK_EQ(cargo_count(cargo_a, 14), 1);
}

/* Random trades over a bad radio: lost, repeated and reordered packets, slow players, cancellations, separations */
static void test_lossy(void) {
    quiet = true;
    int done = 0, cancelled = 0, runs = 0;
    for (int run = 0; run < 3000; ++run) {
        setup();
        rnd_state = run * 7919 + 1;
        loss_percent = rnd() % 70;
        dup_percent = rnd() % 40;
        int before[SMUGGLER_GOODS];
        totals(before);
        bool gift = rnd() % 4 == 0;
        uint8_t a_good = rnd() % 2 ? 1 : 23, b_good = rnd() % 2 ? 7 : 14;
        int a_cancel_at = rnd() % 5 == 0 ? rnd() % 8000 : -1;
        int b_cancel_at = rnd() % 5 == 0 ? rnd() % 8000 : -1;
        int apart_at = rnd() % 4 == 0 ? rnd() % 8000 : -1;
        int apart_ms = rnd() % (3 * TRADE_LOST_MS);
        sending = ID_A;
        trade_invite(&ta, ID_B, "Bob", gift ? TRADE_GIFT : TRADE_EXCHANGE, a_good, run + 1, now);
        uint32_t start = now;
        bool ok = true;
        while (now - start < 120000 && ! (trade_final(&ta) && trade_final(&tb) && n_air == 0)) {
            uint32_t t = now - start;
            /* The players, slow or fast */
            if (rnd() % 30 == 0) {
                sending = ID_B;
                if (tb.invited && tb.state == TS_IDLE)
                    trade_accept(&tb, now);
                else if (tb.state == TS_CHOOSE)
                    trade_offer(&tb, b_good, now);
                else if (tb.state == TS_REVIEW)
                    trade_confirm(&tb, now);
                sending = ID_A;
                if (ta.state == TS_CHOOSE)
                    trade_offer(&ta, a_good, now);
                else if (ta.state == TS_REVIEW)
                    trade_confirm(&ta, now);
            }
            if (a_cancel_at >= 0 && t >= (uint32_t)a_cancel_at && trade_cancellable(&ta)) {
                sending = ID_A;
                trade_cancel(&ta, now);
                a_cancel_at = -1;
            }
            if (b_cancel_at >= 0 && t >= (uint32_t)b_cancel_at && trade_cancellable(&tb)) {
                sending = ID_B;
                trade_cancel(&tb, now);
                b_cancel_at = -1;
            }
            apart = apart_at >= 0 && t >= (uint32_t)apart_at && t < (uint32_t)(apart_at + apart_ms);
            step(10);
            /* Never a good created: the totals never exceed the start (a sealed good is out of the cargo: it is
             * either given back, or already in the cargo of the inviter) */
            for (int g = 0; g < SMUGGLER_GOODS; ++g)
                if (total(g) > before[g] || sealed_total(g) > before[g] + 1)
                    ok = false;
        }
        /* The guest only stays when it was never invited, or waits (a long separation): they meet again */
        apart = false;
        loss_percent = 0;
        if (tb.invited && tb.state == TS_IDLE) {
            sending = ID_B;
            trade_refuse(&tb);
        }
        step(2 * TRADE_SLOW_MS + 2 * TRADE_LOST_MS);
        bool ended = trade_final(&ta) && (trade_final(&tb) || tb.state == TS_IDLE);
        bool kept = same_totals(before);
        bool both_done = ta.state == TS_DONE && tb.state == TS_DONE;
        bool none_done = ta.state != TS_DONE && tb.state != TS_DONE;
        bool swapped = both_done && (gift ? cargo_count(cargo_b, a_good) == 1
                                          : cargo_count(cargo_b, a_good) >= 1 && cargo_count(cargo_a, b_good) >= 1);
        bool unchanged = none_done;
        if (none_done)
            for (int g = 0; g < SMUGGLER_GOODS; ++g)
                if (cargo_count(cargo_a, g) != (g == 1 ? 3 : g == 23) || cargo_count(cargo_b, g) != (g == 7 ? 3 : g == 14))
                    unchanged = false;
        if (! (ok && ended && kept && (swapped || unchanged) && ! ta.sealed && ! tb.sealed)) {
            printf("run %d (loss %d %%, dup %d %%, %s, apart at %d for %d ms): ok %d ended %d kept %d swapped %d "
                   "unchanged %d, states %d %d\n", run, loss_percent, dup_percent, gift ? "gift" : "exchange", apart_at,
                   apart_ms, ok, ended, kept, swapped, unchanged, ta.state, tb.state);
            CHECK(false);
            break;
        }
        done += both_done;
        cancelled += none_done;
        ++runs;
    }
    quiet = false;
    printf("lossy radio: %d runs, %d trades done, %d cancelled, no good created or lost\n", runs, done, cancelled);
    CHECK(done > 300 && cancelled > 100);
}

int main(void) {
    test_cargo();
    test_exchange();
    test_gift_and_refuse();
    test_cancel_after_seal();
    test_apart_after_commit();
    test_lossy();
    TEST_END();
}
