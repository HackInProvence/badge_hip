/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file smuggler_trade.h
 *
 * \brief The trade protocol of the smuggler cicada between two badges (NET_TRADE), safe against the lost and repeated
 * packets: a good is never created by the radio, and a lost packet never loses one.
 *
 * Pure logic (no SDK: the time is given in ms, the packets go through a callback): the host test
 * (src/tests/host/test_smuggler.c) runs two badges over a lossy radio.
 *
 * Packets: [trade id 4][kind][to 4][payload], the trade id is chosen by the inviter.
 *   INVITE [mode][good of a gift][name 8], ACCEPT, REFUSE, ALIVE, OFFER [good],
 *   CONFIRM [good of the sender][good of the receiver], DONE [good of the inviter][good of the guest], ACK, ABORT.
 *
 * Exchange: the inviter invites, the guest accepts, each one offers a good (OFFER), both see the two offers and
 * confirm (CONFIRM). Two-phase commit, the inviter decides:
 * - the guest confirms: its good leaves its cargo ("sealed", kept aside) and it sends CONFIRM until it hears the
 *   decision; from then on it cannot cancel any more;
 * - the inviter commits when it has both confirmations: its cargo changes at once (gives its good, gets the one of
 *   the guest), the trade is recorded as committed, and it sends DONE (again and again until ACK);
 * - the guest gets DONE: it gets the good of the inviter (its own already left), records the trade, answers ACK.
 * - Until it commits, the inviter can cancel (button, peer silent for TRADE_LOST_MS): the trade is recorded as
 *   aborted and it answers ABORT; the guest then gets its sealed good back.
 * Every packet of a trade already decided is answered from the record (TRADE_LOG trades): a CONFIRM repeated after
 * the commit gets DONE again, a DONE repeated gets ACK again (applied once), anything after an abort gets ABORT.
 * The sealed guest never gives up by itself: if the inviter goes away it keeps asking (slowly), and the trade ends
 * when the badges meet again. Worst case: the inviter is switched off (or forgets the trade: TRADE_LOG newer trades)
 * before the guest heard the decision, then the guest is switched off: the sealed good is lost (never duplicated).
 *
 * Gift ("Donner"): the invitation carries the good; the guest accepts with CONFIRM (it gives nothing), the inviter
 * commits (its good leaves) and sends DONE, the guest gets it. Same records and answers.
 * */

#ifndef _SMUGGLER_TRADE_H
#define _SMUGGLER_TRADE_H

#include <stdbool.h>
#include <stdint.h>

#define TRADE_RESEND_MS 500  /* The current message, again and again */
#define TRADE_SLOW_MS 3000  /* A sealed guest whose inviter went away */
#define TRADE_LOST_MS 20000  /* The peer is silent: the trade is cancelled (unless sealed) */
#define TRADE_INVITE_MS 4000  /* An invitation no longer repeated is forgotten */
#define TRADE_DONE_MS 20000  /* The inviter repeats DONE at most this long without ACK (then answers the CONFIRMs) */
#define TRADE_LOG 16
#define TRADE_HEADER 9
#define TRADE_PACKET_MAX (TRADE_HEADER + 10)
#define TRADE_NAME_LEN 8

typedef enum {
    TK_INVITE = 1,
    TK_ACCEPT,
    TK_REFUSE,
    TK_ALIVE,
    TK_OFFER,
    TK_CONFIRM,
    TK_DONE,
    TK_ACK,
    TK_ABORT,
} trade_kind_t;

typedef enum {
    TRADE_EXCHANGE,
    TRADE_GIFT,
} trade_mode_t;

typedef enum {
    TS_IDLE,
    TS_INVITING,  /* Inviter: waits for the answer */
    TS_CHOOSE,  /* Exchange: choose a good to offer */
    TS_OFFERED,  /* Offered, waits for the offer of the peer */
    TS_REVIEW,  /* Both offers known: confirm or cancel */
    TS_CONFIRMED,  /* Confirmed, waits for the peer (guest: sealed, cannot cancel any more) */
    /* The end of the trade, shown until trade_close() */
    TS_DONE,
    TS_CANCELLED,
    TS_REFUSED,
    TS_LOST,
} trade_state_t;

enum { TRADE_COMMITTED = 1, TRADE_ABORTED };

typedef struct {
    uint32_t id, peer;
    uint8_t outcome;  /* TRADE_COMMITTED, TRADE_ABORTED */
    bool inviter;  /* This badge was the inviter */
    uint8_t inviter_good, guest_good;
} trade_log_t;

typedef struct trade trade_t;

struct trade {
    /* Environment (trade_init()) */
    uint32_t my_id;
    uint8_t *cargo;  /* smuggler_goods.h */
    int16_t invite_rssi_min;  /* The invitations received weaker than this are ignored (dBm) */
    void (*send)(const uint8_t *data, uint8_t len);
    void (*cargo_changed)(void);  /* Save it */
    void (*finished)(trade_t *t, uint8_t gave, uint8_t got);  /* A trade done (SMUGGLER_NO_GOOD: nothing) */
    char my_name[TRADE_NAME_LEN + 1];
    /* The trade in progress */
    uint8_t state, mode;
    bool inviter;
    uint32_t id, peer;
    char peer_name[TRADE_NAME_LEN + 1];
    uint8_t my_good, peer_good;  /* The offers, SMUGGLER_NO_GOOD when unknown (or nothing for a gift) */
    bool peer_confirmed;
    bool sealed;  /* Guest: my good left the cargo, waiting for the decision of the inviter */
    uint32_t peer_seen, resend_at, done_until;
    /* An invitation received */
    bool invited;
    uint32_t inv_id, inv_peer, inv_seen;
    uint8_t inv_mode, inv_good;
    int16_t inv_rssi;
    char inv_name[TRADE_NAME_LEN + 1];
    /* The trades decided */
    trade_log_t log[TRADE_LOG];
    uint8_t log_next;
    bool changed;  /* Something to show */
};

void trade_init(trade_t *t, uint32_t my_id, uint8_t *cargo, void (*send)(const uint8_t *, uint8_t),
                void (*cargo_changed)(void), void (*finished)(trade_t *, uint8_t, uint8_t));

/** \brief A NET_TRADE packet received (data after the network header). */
void trade_receive(trade_t *t, uint32_t src, const uint8_t *data, uint8_t len, int16_t rssi, uint32_t now_ms);

/** \brief Resends and timeouts, to call often (at least every TRADE_RESEND_MS). */
void trade_task(trade_t *t, uint32_t now_ms);

/** \brief Invites \p peer: exchange, or gift of \p good. \p id: random, not 0. \return false when busy */
bool trade_invite(trade_t *t, uint32_t peer, const char *peer_name, uint8_t mode, uint8_t good, uint32_t id,
                  uint32_t now_ms);
/** \brief Accepts the invitation received (a gift: accepted at once). */
bool trade_accept(trade_t *t, uint32_t now_ms);
void trade_refuse(trade_t *t);
/** \brief Exchange: offers \p good (TS_CHOOSE). */
bool trade_offer(trade_t *t, uint8_t good, uint32_t now_ms);
/** \brief Confirms the two offers (TS_REVIEW). */
bool trade_confirm(trade_t *t, uint32_t now_ms);
/** \brief The trade can still be cancelled by this badge (a sealed guest cannot). */
bool trade_cancellable(const trade_t *t);
void trade_cancel(trade_t *t, uint32_t now_ms);
/** \brief From the end of a trade (TS_DONE...) back to TS_IDLE. */
void trade_close(trade_t *t);

/** \brief A trade in progress (an invitation can't be accepted). */
bool trade_busy(const trade_t *t);
bool trade_final(const trade_t *t);

#endif /* _SMUGGLER_TRADE_H */
