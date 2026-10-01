/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The trade protocol of the smuggler cicada (smuggler_trade.h): pure logic, also compiled by the host tests. */

#include <stdio.h>
#include <string.h>

#include "smuggler_goods.h"
#include "smuggler_trade.h"

static uint32_t get_u32(const uint8_t *p) {
    return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

static void put_u32(uint8_t *p, uint32_t v) {
    p[0] = v;
    p[1] = v >> 8;
    p[2] = v >> 16;
    p[3] = v >> 24;
}

/* now - t in ms, correct across the wrap of the 32 bit counter */
static int32_t since(uint32_t now, uint32_t t) {
    return (int32_t)(now - t);
}

static bool valid_good(uint8_t good) {
    return good < SMUGGLER_GOODS;
}

static const char *good_name(uint8_t good) {
    return valid_good(good) ? smuggler_goods[good].name : "rien";
}

static void send_to(trade_t *t, uint32_t id, uint32_t to, uint8_t kind, const uint8_t *payload, uint8_t n) {
    uint8_t d[TRADE_PACKET_MAX];
    if (n > TRADE_PACKET_MAX - TRADE_HEADER)
        return;
    put_u32(d, id);
    d[4] = kind;
    put_u32(d + 5, to);
    if (n)
        memcpy(d + TRADE_HEADER, payload, n);
    t->send(d, TRADE_HEADER + n);
}

static void send_kind(trade_t *t, uint8_t kind, const uint8_t *payload, uint8_t n) {
    send_to(t, t->id, t->peer, kind, payload, n);
}

static void send_two(trade_t *t, uint8_t kind, uint8_t a, uint8_t b) {
    uint8_t p[2] = {a, b};
    send_kind(t, kind, p, 2);
}

static void copy_name(char *dst, const uint8_t *src, int n) {
    if (n > TRADE_NAME_LEN)
        n = TRADE_NAME_LEN;
    if (n < 0)
        n = 0;
    memcpy(dst, src, n);
    dst[n] = 0;
}

void trade_init(trade_t *t, uint32_t my_id, uint8_t *cargo, void (*send)(const uint8_t *, uint8_t),
                void (*cargo_changed)(void), void (*finished)(trade_t *, uint8_t, uint8_t)) {
    memset(t, 0, sizeof(*t));
    t->my_id = my_id;
    t->cargo = cargo;
    t->send = send;
    t->cargo_changed = cargo_changed;
    t->finished = finished;
    t->invite_rssi_min = -128;
    t->state = TS_IDLE;
    t->my_good = t->peer_good = SMUGGLER_NO_GOOD;
}

bool trade_final(const trade_t *t) {
    return t->state >= TS_DONE;
}

bool trade_busy(const trade_t *t) {
    return t->state != TS_IDLE && ! trade_final(t);
}

bool trade_cancellable(const trade_t *t) {
    if (! trade_busy(t))
        return false;
    return t->inviter || t->state != TS_CONFIRMED;
}


/* ------ The record of the trades decided ------ */

static trade_log_t *log_find(trade_t *t, uint32_t id, uint32_t peer) {
    for (int i = 0; i < TRADE_LOG; ++i)
        if (t->log[i].outcome && t->log[i].id == id && t->log[i].peer == peer)
            return &t->log[i];
    return NULL;
}

static void log_add(trade_t *t, uint8_t outcome) {
    trade_log_t *l = log_find(t, t->id, t->peer);
    if (! l) {
        l = &t->log[t->log_next];
        t->log_next = (t->log_next + 1) % TRADE_LOG;
    }
    l->id = t->id;
    l->peer = t->peer;
    l->outcome = outcome;
    l->inviter = t->inviter;
    l->inviter_good = t->inviter ? t->my_good : t->peer_good;
    l->guest_good = t->inviter ? t->peer_good : t->my_good;
}

/* A packet of a trade already decided (or of an older one): the same answer as the first time */
static void answer_decided(trade_t *t, trade_log_t *l, uint8_t kind) {
    uint8_t p[2] = {l->inviter_good, l->guest_good};
    if (l->outcome == TRADE_COMMITTED) {
        if (l->inviter && kind == TK_CONFIRM)
            send_to(t, l->id, l->peer, TK_DONE, p, 2);
        else if (! l->inviter && kind == TK_DONE)
            send_to(t, l->id, l->peer, TK_ACK, NULL, 0);
    } else if (kind != TK_ABORT && kind != TK_REFUSE && kind != TK_ACK) {
        send_to(t, l->id, l->peer, TK_ABORT, NULL, 0);
    }
}


/* ------ The end of a trade ------ */

static void end(trade_t *t, uint8_t state, const char *why) {
    if (t->sealed) {
        cargo_add(t->cargo, t->my_good);  /* The trade did not happen: the sealed good comes back */
        t->sealed = false;
        t->cargo_changed();
        printf("smuggler: %s back in the cargo\n", good_name(t->my_good));
    }
    log_add(t, TRADE_ABORTED);
    t->state = state;
    t->changed = true;
    printf("smuggler: trade %08lX %s\n", (unsigned long)t->id, why);
}

void trade_cancel(trade_t *t, uint32_t now_ms) {
    (void)now_ms;
    if (! trade_cancellable(t))
        return;
    send_kind(t, TK_ABORT, NULL, 0);
    end(t, TS_CANCELLED, "cancelled");
}

void trade_close(trade_t *t) {
    if (trade_final(t)) {
        t->state = TS_IDLE;
        t->changed = true;
    }
}

/* Inviter: both confirmed, the decision */
static void commit(trade_t *t, uint32_t now) {
    bool exchange = t->mode == TRADE_EXCHANGE;
    if (cargo_count(t->cargo, t->my_good) <= 0
            || (exchange && cargo_count(t->cargo, t->peer_good) >= SMUGGLER_MAX_COUNT)) {
        printf("smuggler: error: cannot commit trade %08lX (cargo changed)\n", (unsigned long)t->id);
        send_kind(t, TK_ABORT, NULL, 0);
        end(t, TS_CANCELLED, "aborted");
        return;
    }
    cargo_take(t->cargo, t->my_good);
    if (exchange)
        cargo_add(t->cargo, t->peer_good);
    log_add(t, TRADE_COMMITTED);
    t->state = TS_DONE;
    t->done_until = now + TRADE_DONE_MS;
    t->resend_at = now + TRADE_RESEND_MS;
    send_two(t, TK_DONE, t->my_good, t->peer_good);
    t->cargo_changed();
    t->changed = true;
    uint8_t got = exchange ? t->peer_good : SMUGGLER_NO_GOOD;
    printf("smuggler: done trade %08lX inviter gave=%d got=%d (%s -> %s)\n", (unsigned long)t->id, t->my_good,
           got == SMUGGLER_NO_GOOD ? -1 : got, good_name(t->my_good), good_name(got));
    if (t->finished)
        t->finished(t, t->my_good, got);
}

/* Guest: the decision of the inviter */
static void apply_done(trade_t *t, uint8_t inviter_good, uint8_t guest_good) {
    if (inviter_good != t->peer_good || guest_good != t->my_good) {
        printf("smuggler: error: DONE of trade %08lX doesn't match the offers\n", (unsigned long)t->id);
        return;
    }
    if (! cargo_add(t->cargo, t->peer_good))
        printf("smuggler: error: no room for %s\n", good_name(t->peer_good));
    t->sealed = false;
    log_add(t, TRADE_COMMITTED);
    t->state = TS_DONE;
    send_kind(t, TK_ACK, NULL, 0);
    t->cargo_changed();
    t->changed = true;
    printf("smuggler: done trade %08lX guest gave=%d got=%d (%s -> %s)\n", (unsigned long)t->id,
           t->my_good == SMUGGLER_NO_GOOD ? -1 : t->my_good, t->peer_good, good_name(t->my_good),
           good_name(t->peer_good));
    if (t->finished)
        t->finished(t, t->my_good, t->peer_good);
}

static void both_offered(trade_t *t) {
    if ((t->state == TS_OFFERED || t->state == TS_CHOOSE) && valid_good(t->my_good) && valid_good(t->peer_good)) {
        t->state = TS_REVIEW;
        printf("smuggler: review %s <-> %s\n", good_name(t->my_good), good_name(t->peer_good));
    }
}

/* The good offered by the peer (OFFER, or CONFIRM when the OFFER was lost) */
static bool peer_offers(trade_t *t, uint8_t good) {
    if (! valid_good(good))
        return false;
    if (t->peer_good == SMUGGLER_NO_GOOD) {
        t->peer_good = good;
        t->changed = true;
        printf("smuggler: peer offers %s\n", good_name(good));
        both_offered(t);
    }
    return t->peer_good == good;  /* An offer never changes */
}


/* ------ Receiving ------ */

static void receive_invite(trade_t *t, uint32_t src, uint32_t id, const uint8_t *p, int n, int16_t rssi,
                           uint32_t now) {
    if (n < 2 || p[0] > TRADE_GIFT || (p[0] == TRADE_GIFT && ! valid_good(p[1])))
        return;
    if (t->state != TS_IDLE && t->id == id && t->peer == src)
        return;  /* Repeated after the answer */
    trade_log_t *l = log_find(t, id, src);
    if (l) {
        answer_decided(t, l, TK_INVITE);  /* Refused: ABORT */
        return;
    }
    if (t->invited && t->inv_id == id && t->inv_peer == src) {
        t->inv_seen = now;
        return;
    }
    if (trade_busy(t) || rssi < t->invite_rssi_min)
        return;
    if (t->invited && since(now, t->inv_seen) < TRADE_INVITE_MS)
        return;  /* Another invitation is shown */
    t->invited = true;
    t->inv_id = id;
    t->inv_peer = src;
    t->inv_seen = now;
    t->inv_mode = p[0];
    t->inv_good = p[0] == TRADE_GIFT ? p[1] : SMUGGLER_NO_GOOD;
    t->inv_rssi = rssi;
    copy_name(t->inv_name, p + 2, n - 2);
    t->changed = true;
    printf("smuggler: invited by %s#%04X, trade %08lX, %s%s, rssi %d\n", t->inv_name, (unsigned)(src & 0xFFFF),
           (unsigned long)id, p[0] == TRADE_GIFT ? "gift of " : "exchange", p[0] == TRADE_GIFT ? good_name(p[1]) : "",
           rssi);
}

static void receive_session(trade_t *t, uint8_t kind, const uint8_t *p, int n, uint32_t now) {
    t->peer_seen = now;
    switch (kind) {
    case TK_ACCEPT:
        if (t->inviter && t->state == TS_INVITING && t->mode == TRADE_EXCHANGE) {
            t->state = TS_CHOOSE;
            t->changed = true;
            printf("smuggler: trade %08lX accepted by %s\n", (unsigned long)t->id, t->peer_name);
        }
        break;
    case TK_REFUSE:
        if (t->inviter && t->state == TS_INVITING)
            end(t, TS_REFUSED, "refused");
        break;
    case TK_OFFER:
        if (n < 1 || t->mode != TRADE_EXCHANGE)
            break;
        if (t->inviter && t->state == TS_INVITING) {
            t->state = TS_CHOOSE;  /* The ACCEPT was lost */
            printf("smuggler: trade %08lX accepted by %s\n", (unsigned long)t->id, t->peer_name);
        }
        if (t->state != TS_INVITING)
            peer_offers(t, p[0]);
        break;
    case TK_CONFIRM:  /* [good of the peer][my good] */
        if (n < 2)
            break;
        if (t->mode == TRADE_GIFT) {
            /* The guest accepts the gift: the inviter decides at once */
            if (t->inviter && t->state == TS_INVITING && p[1] == t->my_good) {
                t->peer_confirmed = true;
                printf("smuggler: gift accepted by %s\n", t->peer_name);
                commit(t, now);
            }
            break;
        }
        if (t->inviter && t->state == TS_INVITING) {
            t->state = TS_CHOOSE;  /* The ACCEPT was lost */
            printf("smuggler: trade %08lX accepted by %s\n", (unsigned long)t->id, t->peer_name);
        }
        if (t->state == TS_INVITING || p[1] != t->my_good || ! peer_offers(t, p[0]))
            break;  /* Not the offers of this trade */
        if (! t->peer_confirmed) {
            t->peer_confirmed = true;
            t->changed = true;
            printf("smuggler: %s confirmed\n", t->peer_name);
        }
        if (t->inviter && t->state == TS_CONFIRMED)
            commit(t, now);
        break;
    case TK_DONE:  /* [good of the inviter][good of the guest] */
        if (n >= 2 && ! t->inviter && t->state == TS_CONFIRMED)
            apply_done(t, p[0], p[1]);
        break;
    case TK_ABORT:
        end(t, t->inviter && t->state == TS_INVITING ? TS_REFUSED : TS_CANCELLED,
            t->inviter && t->state == TS_INVITING ? "refused" : "cancelled by the peer");
        break;
    default:  /* ALIVE, ACK: the peer is there */
        break;
    }
}

void trade_receive(trade_t *t, uint32_t src, const uint8_t *data, uint8_t len, int16_t rssi, uint32_t now_ms) {
    if (len < TRADE_HEADER || get_u32(data + 5) != t->my_id)
        return;  /* Not for this badge */
    uint32_t id = get_u32(data);
    uint8_t kind = data[4];
    const uint8_t *p = data + TRADE_HEADER;
    int n = len - TRADE_HEADER;
    if (kind == TK_INVITE) {
        receive_invite(t, src, id, p, n, rssi, now_ms);
        return;
    }
    if (t->invited && id == t->inv_id && src == t->inv_peer)
        return;  /* Nothing to answer before the invitation is accepted */
    if (trade_busy(t) && id == t->id && src == t->peer) {
        receive_session(t, kind, p, n, now_ms);
        return;
    }
    if (kind == TK_ACK && t->state == TS_DONE && id == t->id && src == t->peer) {
        t->done_until = now_ms;  /* The guest has it */
        printf("smuggler: trade %08lX acknowledged\n", (unsigned long)id);
    }
    trade_log_t *l = log_find(t, id, src);
    if (l)
        answer_decided(t, l, kind);
}


/* ------ The actions of this badge ------ */

bool trade_invite(trade_t *t, uint32_t peer, const char *peer_name, uint8_t mode, uint8_t good, uint32_t id,
                  uint32_t now_ms) {
    if (trade_busy(t) || ! id || mode > TRADE_GIFT || (mode == TRADE_GIFT && cargo_count(t->cargo, good) <= 0))
        return false;
    t->invited = false;
    t->state = TS_INVITING;
    t->mode = mode;
    t->inviter = true;
    t->id = id;
    t->peer = peer;
    snprintf(t->peer_name, sizeof(t->peer_name), "%s", peer_name);
    t->my_good = mode == TRADE_GIFT ? good : SMUGGLER_NO_GOOD;
    t->peer_good = SMUGGLER_NO_GOOD;
    t->peer_confirmed = false;
    t->sealed = false;
    t->peer_seen = now_ms;
    t->resend_at = now_ms;  /* Sent by trade_task() */
    t->changed = true;
    printf("smuggler: inviting %s#%04X, trade %08lX, %s%s\n", t->peer_name, (unsigned)(peer & 0xFFFF),
           (unsigned long)id, mode == TRADE_GIFT ? "gift of " : "exchange", mode == TRADE_GIFT ? good_name(good) : "");
    return true;
}

bool trade_accept(trade_t *t, uint32_t now_ms) {
    if (! t->invited || trade_busy(t))
        return false;
    t->invited = false;
    t->mode = t->inv_mode;
    t->inviter = false;
    t->id = t->inv_id;
    t->peer = t->inv_peer;
    memcpy(t->peer_name, t->inv_name, sizeof(t->peer_name));
    t->my_good = SMUGGLER_NO_GOOD;
    t->peer_good = t->inv_good;
    t->peer_confirmed = false;
    t->sealed = false;
    t->peer_seen = now_ms;
    t->resend_at = now_ms;
    t->state = t->mode == TRADE_GIFT ? TS_CONFIRMED : TS_CHOOSE;  /* A gift: nothing to give, nothing to seal */
    t->changed = true;
    printf("smuggler: accepted trade %08lX from %s\n", (unsigned long)t->id, t->peer_name);
    return true;
}

void trade_refuse(trade_t *t) {
    if (! t->invited)
        return;
    t->invited = false;
    t->changed = true;
    send_to(t, t->inv_id, t->inv_peer, TK_REFUSE, NULL, 0);
    /* Recorded: the repeated invitation gets ABORT */
    trade_log_t *l = &t->log[t->log_next];
    t->log_next = (t->log_next + 1) % TRADE_LOG;
    *l = (trade_log_t){t->inv_id, t->inv_peer, TRADE_ABORTED, false, SMUGGLER_NO_GOOD, SMUGGLER_NO_GOOD};
    printf("smuggler: refused trade %08lX from %s\n", (unsigned long)t->inv_id, t->inv_name);
}

bool trade_offer(trade_t *t, uint8_t good, uint32_t now_ms) {
    if (t->state != TS_CHOOSE || cargo_count(t->cargo, good) <= 0)
        return false;
    t->my_good = good;
    t->state = TS_OFFERED;
    t->resend_at = now_ms;
    t->changed = true;
    printf("smuggler: offer %s\n", good_name(good));
    both_offered(t);
    return true;
}

bool trade_confirm(trade_t *t, uint32_t now_ms) {
    if (t->state != TS_REVIEW || cargo_count(t->cargo, t->my_good) <= 0)
        return false;
    t->state = TS_CONFIRMED;
    t->resend_at = now_ms;
    t->changed = true;
    if (t->inviter) {
        printf("smuggler: confirmed\n");
        if (t->peer_confirmed)
            commit(t, now_ms);
    } else {
        /* Sealed: the good leaves the cargo until the decision of the inviter */
        cargo_take(t->cargo, t->my_good);
        t->sealed = true;
        t->cargo_changed();
        printf("smuggler: confirmed, %s sealed\n", good_name(t->my_good));
    }
    return true;
}


/* ------ Resends and timeouts ------ */

void trade_task(trade_t *t, uint32_t now_ms) {
    if (t->invited && since(now_ms, t->inv_seen) > TRADE_INVITE_MS) {
        t->invited = false;  /* The inviter gave up */
        t->changed = true;
        printf("smuggler: invitation of %s expired\n", t->inv_name);
    }
    bool silent = since(now_ms, t->peer_seen) > TRADE_LOST_MS;
    if (trade_busy(t) && silent && ! t->sealed && ! (! t->inviter && t->state == TS_CONFIRMED)) {
        send_kind(t, TK_ABORT, NULL, 0);
        end(t, TS_LOST, "lost (peer silent)");
        return;
    }
    if (t->state == TS_IDLE || since(now_ms, t->resend_at) < 0)
        return;
    t->resend_at = now_ms + TRADE_RESEND_MS;
    uint8_t p[2 + TRADE_NAME_LEN];
    switch (t->state) {
    case TS_INVITING:
        p[0] = t->mode;
        p[1] = t->my_good;
        memcpy(p + 2, t->my_name, TRADE_NAME_LEN);
        send_kind(t, TK_INVITE, p, 2 + strnlen(t->my_name, TRADE_NAME_LEN));
        break;
    case TS_CHOOSE:
        send_kind(t, t->inviter ? TK_ALIVE : TK_ACCEPT, NULL, 0);
        break;
    case TS_OFFERED:
    case TS_REVIEW:
        p[0] = t->my_good;
        send_kind(t, TK_OFFER, p, 1);
        break;
    case TS_CONFIRMED:
        send_two(t, TK_CONFIRM, t->my_good, t->peer_good);
        if (silent)
            t->resend_at = now_ms + TRADE_SLOW_MS;  /* A sealed guest: the inviter went away, ask now and then */
        break;
    case TS_DONE:
        if (t->inviter && since(now_ms, t->done_until) < 0)
            send_two(t, TK_DONE, t->my_good, t->peer_good);
        break;
    default:
        break;
    }
}
