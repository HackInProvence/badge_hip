/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdio.h>
#include <string.h>

#include "pico/rand.h"

#include "net.h"
#include "social.h"
#include "store.h"


#define BEACON_LEN 11  /* Data of the NET_BEACON packets: sequence, score (2 bytes), name (8 bytes) */
#define BEACON_PERIOD_MS 2000
#define BEACON_JITTER_MS 500
#define NEIGHBOUR_TIMEOUT_MS 15000
#define POINTS_NEW 10
#define POINTS_AGAIN 1
#define AGAIN_MINUTES 60

typedef struct {
    social_neighbour_t pub;
    absolute_time_t last_seen;
    absolute_time_t close_start;
    uint8_t close_count;
    bool met_now;  /* Meeting counted during this visit */
} neighbour_t;

static bool enabled = true;
static absolute_time_t next_beacon = 0;
static uint8_t seq = 0;
static neighbour_t neighbours[SOCIAL_MAX_NEIGHBOURS];
static char event[48];
static bool event_pending = false;
static uint32_t n_sent = 0, n_received = 0;


static void schedule_beacon(absolute_time_t now) {
    uint32_t jitter = get_rand_32() % (2 * BEACON_JITTER_MS);
    next_beacon = delayed_by_ms(now, BEACON_PERIOD_MS - BEACON_JITTER_MS + jitter);
}


static void handle_beacon(const net_packet_t *p);

void social_init(void) {
    store_t *s = store_get();
    if (! s->name[0])
        snprintf(s->name, sizeof(s->name), "Cig %04X", (unsigned)(net_id() & 0xFFFF));
    net_subscribe(NET_BEACON, handle_beacon);
    schedule_beacon(get_absolute_time());
    /* Repair: the first loopback tests (net.h) counted meetings with the "twin" of this badge */
    for (uint16_t i = 0; i < s->n_met; ++i)
        if (s->met[i].id == (net_id() ^ NET_TWIN)) {
            uint32_t points = POINTS_NEW + (s->met[i].meets > 1 ? (s->met[i].meets - 1) * POINTS_AGAIN : 0);
            s->score = s->score > points ? s->score - points : 0;
            s->met[i] = s->met[--s->n_met];
            store_changed();
            printf("social: removed the meeting with the loopback twin (-%lu)\n", (unsigned long)points);
            break;
        }
}


void social_set_enabled(bool e) {
    enabled = e;  /* The badge still listens (the other badges are shown), it stops sending its beacons */
}


bool social_enabled(void) {
    return enabled;
}


uint32_t social_id(void) {
    return net_id();
}

const char *social_name(void) {
    return store_get()->name;
}


uint32_t social_score(void) {
    return store_get()->score;
}


uint16_t social_met_count(void) {
    return store_get()->n_met;
}


void social_stats(uint32_t *sent, uint32_t *received) {
    *sent = n_sent;
    *received = n_received;
}


bool social_event(char *msg, int len) {
    if (! event_pending)
        return false;
    event_pending = false;
    snprintf(msg, len, "%s", event);
    return true;
}


int social_neighbours(social_neighbour_t *out, int max) {
    /* Insertion sort by RSSI (closest first) of all the neighbours, then keep the first ones */
    social_neighbour_t all[SOCIAL_MAX_NEIGHBOURS];
    int n = 0;
    for (int i = 0; i < SOCIAL_MAX_NEIGHBOURS; ++i) {
        if (! neighbours[i].pub.id)
            continue;
        int j = n++;
        while (j > 0 && all[j-1].rssi < neighbours[i].pub.rssi) {
            all[j] = all[j-1];
            --j;
        }
        all[j] = neighbours[i].pub;
    }
    if (n > max)
        n = max;
    memcpy(out, all, n * sizeof(all[0]));
    return n;
}


/* Counts a meeting with this badge */
static void meet(neighbour_t *nb, absolute_time_t now) {
    store_t *s = store_get();
    uint16_t minute = to_ms_since_boot(now) / 60000;
    int points;
    store_met_t *m = NULL;
    for (uint16_t i = 0; i < s->n_met; ++i)
        if (s->met[i].id == nb->pub.id)
            m = &s->met[i];
    if (! m) {
        points = POINTS_NEW;
        if (s->n_met < STORE_MAX_MET) {
            m = &s->met[s->n_met++];
            m->id = nb->pub.id;
            m->meets = 0;
        }
    } else if (m->last_minute == 0xFFFF || minute - m->last_minute >= AGAIN_MINUTES) {
        points = POINTS_AGAIN;
    } else {
        return;  /* Met less than an hour ago */
    }
    if (m) {
        ++m->meets;
        m->last_minute = minute;
    }
    s->score += points;
    store_changed();
    nb->pub.met = true;
    snprintf(event, sizeof(event), "Rencontre : %s +%d", nb->pub.name, points);
    event_pending = true;
    printf("social: %s (score %lu)\n", event, (unsigned long)s->score);
}


static void handle_beacon(const net_packet_t *packet) {
    if (packet->len < BEACON_LEN)
        return;
    const uint8_t *p = packet->data;
    uint32_t id = packet->src;
    if (id == (net_id() ^ NET_TWIN))
        return;  /* The loopback test (net.h) must not make meetings */
    int16_t rssi = packet->rssi;
    absolute_time_t now = packet->at;
    ++n_received;

    /* Find the neighbour, or take a free (or the oldest) slot */
    neighbour_t *nb = NULL, *oldest = &neighbours[0];
    for (int i = 0; i < SOCIAL_MAX_NEIGHBOURS && ! nb; ++i) {
        if (neighbours[i].pub.id == id)
            nb = &neighbours[i];
        else if (absolute_time_diff_us(neighbours[i].last_seen, oldest->last_seen) > 0 || ! neighbours[i].pub.id)
            oldest = &neighbours[i];
    }
    if (! nb) {
        nb = oldest;
        memset(nb, 0, sizeof(*nb));
        nb->pub.id = id;
        store_t *s = store_get();
        for (uint16_t i = 0; i < s->n_met; ++i)
            if (s->met[i].id == id)
                nb->pub.met = true;
    }
    memcpy(nb->pub.name, p + 3, 8);
    nb->pub.name[8] = 0;
    nb->pub.score = p[1] | p[2] << 8;
    nb->pub.rssi = rssi;
    nb->last_seen = now;

    /* Close enough, long enough: meeting */
    if (rssi >= SOCIAL_RSSI_CLOSE && ! nb->met_now) {
        if (! nb->close_count || absolute_time_diff_us(nb->close_start, now) > SOCIAL_CLOSE_WINDOW_MS * 1000ll) {
            nb->close_count = 0;
            nb->close_start = now;
        }
        if (++nb->close_count >= SOCIAL_CLOSE_BEACONS) {
            nb->met_now = true;
            meet(nb, now);
        }
    }
}


static void send_beacon(void) {
    store_t *s = store_get();
    uint8_t p[BEACON_LEN] = {seq++, s->score, s->score >> 8};
    memcpy(p + 3, s->name, 8);  /* The name is truncated to 8 characters in the beacon */
    if (net_send(NET_BEACON, p, sizeof(p), NET_LOUD))  /* +10 dBm: at -10 dBm, ~-97 dBm at 1 m (edge of the sensitivity) */
        ++n_sent;
}


void social_task(absolute_time_t now) {
    /* Forget the badges that went away */
    for (int i = 0; i < SOCIAL_MAX_NEIGHBOURS; ++i)
        if (neighbours[i].pub.id && absolute_time_diff_us(neighbours[i].last_seen, now) > NEIGHBOUR_TIMEOUT_MS * 1000ll)
            memset(&neighbours[i], 0, sizeof(neighbours[i]));

    if (enabled && absolute_time_diff_us(now, next_beacon) <= 0) {
        schedule_beacon(now);
        send_beacon();
    }
}
