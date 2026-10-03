/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Relay of the admin messages from cicada to cicada, see relay.h. */

#include <stdio.h>
#include <string.h>

#include "pico/rand.h"

#include "net.h"
#include "relay.h"
#include "store.h"

#define PENDING 8  /* Messages waiting for their relay (the parts of an announcement: 5 at most) */
#define DONE 8  /* Kinds of message relayed recently */
#define PENDING_GIVE_UP_MS 3000  /* The network stayed busy: dropped */

typedef struct {
    bool used;
    uint8_t type;
    uint8_t data[RELAY_DATA_MAX];
    uint8_t len;
    uint32_t origin;
    uint32_t id;
    uint32_t key;
    uint8_t copies;  /* Relays of the same message heard from other cicadas while waiting */
    absolute_time_t at, give_up;
} pending_t;

typedef struct {
    uint8_t type;
    uint32_t key;
    absolute_time_t at;
} done_t;

static pending_t pending[PENDING];
static done_t done[DONE];
static int done_next = 0;

static bool relayed_recently(uint8_t type, uint32_t key, absolute_time_t now) {
    for (int i = 0; i < DONE; ++i)
        if (done[i].at && done[i].type == type && done[i].key == key
            && absolute_time_diff_us(done[i].at, now) < RELAY_SAME_MS * 1000ll)
            return true;
    return false;
}

uint32_t relay_hash(uint32_t h, const uint8_t *data, int len) {
    for (int i = 0; i < len; ++i)
        h = (h ^ data[i]) * 0x01000193u;
    return h;
}

void relay_offer(uint8_t type, const uint8_t *data, int len, uint32_t src, uint32_t origin, uint32_t id,
                 uint32_t key, uint8_t ttl, absolute_time_t now) {
    if (origin == net_id())
        return;  /* Our own message, coming back */
    /* A copy of a message waiting for its relay: another cicada relayed it */
    for (int i = 0; i < PENDING; ++i)
        if (pending[i].used && pending[i].type == type && pending[i].origin == origin && pending[i].id == id) {
            if (src != origin)
                ++pending[i].copies;
            return;
        }
    if (! ttl || len > RELAY_DATA_MAX || relayed_recently(type, key, now))
        return;
    for (int i = 0; i < PENDING; ++i) {
        if (pending[i].used)
            continue;
        pending_t *p = &pending[i];
        p->used = true;
        p->type = type;
        memcpy(p->data, data, len);
        p->len = len;
        p->origin = origin;
        p->id = id;
        p->key = key;
        p->copies = 0;
        uint32_t delay = RELAY_DELAY_MIN_MS + get_rand_32() % (RELAY_DELAY_MAX_MS - RELAY_DELAY_MIN_MS + 1);
        p->at = delayed_by_ms(now, delay);
        p->give_up = delayed_by_ms(now, PENDING_GIVE_UP_MS);
        return;
    }
}

void relay_task(absolute_time_t now) {
    for (int i = 0; i < PENDING; ++i) {
        pending_t *p = &pending[i];
        if (! p->used || absolute_time_diff_us(p->at, now) < 0)
            continue;
        if (p->copies >= RELAY_ENOUGH_COPIES || relayed_recently(p->type, p->key, now)) {
            printf("relay: type %u from %08lX not relayed (%u copies heard)\n", p->type, (unsigned long)p->origin,
                   p->copies);
            p->used = false;
        } else if (net_send(p->type, p->data, p->len, NET_LOUD)) {
            printf("relay: type %u from %08lX relayed\n", p->type, (unsigned long)p->origin);
            done[done_next] = (done_t){p->type, p->key, now};
            done_next = (done_next + 1) % DONE;
            p->used = false;
        } else if (absolute_time_diff_us(p->give_up, now) >= 0) {
            p->used = false;  /* The network never had room */
        }
    }
}

bool relay_pending(void) {
    for (int i = 0; i < PENDING; ++i)
        if (pending[i].used)
            return true;
    return false;
}

uint8_t relay_admin_ttl(void) {
    uint8_t t = store_get()->admin_ttl;
    return t >= 1 && t <= RELAY_TTL_MAX + 1 ? t - 1 : RELAY_TTL_DEFAULT;
}

void relay_set_admin_ttl(uint8_t ttl) {
    store_get()->admin_ttl = (ttl > RELAY_TTL_MAX ? RELAY_TTL_MAX : ttl) + 1;
    store_changed();
}
