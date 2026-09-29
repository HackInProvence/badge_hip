/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdio.h>
#include <string.h>

#include "hardware/gpio.h"
#include "pico/rand.h"
#include "pico/unique_id.h"

#include "pinouts.h"
#include "radio.h"
#include "radio_tools.h"
#include "social.h"
#include "store.h"


#define MAGIC 0xC1
#define TYPE_BEACON 0x01
#define BEACON_LEN 17
#define BEACON_PERIOD_MS 2000
#define BEACON_JITTER_MS 500
#define NEIGHBOUR_TIMEOUT_MS 15000
#define POLL_US 2000
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
static bool owner = false;  /* The radio is configured for the network */
static bool tx_pending = false;
static absolute_time_t tx_ts = 0;
static absolute_time_t next_beacon = 0;
static absolute_time_t next_poll = 0;
static uint32_t my_id = 0;
static uint8_t seq = 0;
static neighbour_t neighbours[SOCIAL_MAX_NEIGHBOURS];
static char event[48];
static bool event_pending = false;
static uint32_t n_sent = 0, n_received = 0;


static void schedule_beacon(absolute_time_t now) {
    uint32_t jitter = get_rand_32() % (2 * BEACON_JITTER_MS);
    next_beacon = delayed_by_ms(now, BEACON_PERIOD_MS - BEACON_JITTER_MS + jitter);
}


void social_init(void) {
    /* Identity: hash (FNV-1a) of the unique id of the flash */
    pico_unique_board_id_t uid;
    pico_get_unique_board_id(&uid);
    my_id = 2166136261u;
    for (int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; ++i)
        my_id = (my_id ^ uid.id[i]) * 16777619u;

    store_t *s = store_get();
    if (! s->name[0])
        snprintf(s->name, sizeof(s->name), "Cig %04X", (unsigned)(my_id & 0xFFFF));
    schedule_beacon(get_absolute_time());
}


void social_set_enabled(bool e) {
    enabled = e;
    if (! enabled && owner && radio_tools_idle()) {
        radio_wait_state(CC1101_STATE_IDLE, true);
        owner = false;
    }
}


bool social_enabled(void) {
    return enabled;
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


static void handle_beacon(const uint8_t *p, int16_t rssi, absolute_time_t now) {
    uint32_t id = p[2] | p[3] << 8 | p[4] << 16 | (uint32_t)p[5] << 24;
    if (id == my_id || id == 0)
        return;
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
    memcpy(nb->pub.name, p + 9, 8);
    nb->pub.name[8] = 0;
    nb->pub.score = p[7] | p[8] << 8;
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
    uint8_t p[BEACON_LEN] = {MAGIC, TYPE_BEACON, my_id, my_id >> 8, my_id >> 16, my_id >> 24, seq++,
                             s->score, s->score >> 8};
    memcpy(p + 9, s->name, 8);  /* The name is truncated to 8 characters in the beacon */
    if (radio_tx_packet(p, sizeof(p))) {
        tx_pending = true;
        tx_ts = get_absolute_time();
        ++n_sent;
    }
}


static void start_rx(void) {
    radio_write_registers((const uint8_t[]){CC1101_SRX}, 1);
}


/* Reads the received packets, returns false when the RX FIFO is not complete yet */
static void poll_rx(absolute_time_t now) {
    uint8_t n;
    radio_read_registers(CC1101_RXBYTES, &n, 1);
    if (n & 0x80) {
        /* Overflow: flush and listen again */
        radio_wait_state(CC1101_STATE_IDLE, true);
        radio_write_registers((const uint8_t[]){CC1101_SFRX}, 1);
        start_rx();
        return;
    }
    n &= 0x7F;
    /* A packet is complete when GDO0 went low again (0x06: asserted from the sync word to the end of the packet) */
    if (n > 0 && ! gpio_get(BADGE_RADIO_GDO0)) {
        uint8_t len;
        radio_read_registers(CC1101_RXFIFO, &len, 1);
        uint8_t buf[64];
        if (len + 2 > (int)sizeof(buf) || len + 2 > n - 1) {
            /* Too long for us, or incomplete (should not happen once GDO0 is low): flush */
            radio_wait_state(CC1101_STATE_IDLE, true);
            radio_write_registers((const uint8_t[]){CC1101_SFRX}, 1);
            start_rx();
            return;
        }
        radio_read_registers(CC1101_RXFIFO, buf, len + 2);  /* Payload + RSSI + LQI/CRC */
        bool crc_ok = buf[len + 1] & 0x80;
        int16_t rssi = (int8_t)buf[len] / 2 - 74;
        if (crc_ok && len == BEACON_LEN && buf[0] == MAGIC && buf[1] == TYPE_BEACON)
            handle_beacon(buf, rssi, now);
    }
    /* After a packet the radio goes IDLE (MCSM1.RXOFF_MODE): listen again */
    if (radio_state() == CC1101_STATE_IDLE)
        start_rx();
}


void social_task(absolute_time_t now) {
    /* Forget the badges that went away */
    for (int i = 0; i < SOCIAL_MAX_NEIGHBOURS; ++i)
        if (neighbours[i].pub.id && absolute_time_diff_us(neighbours[i].last_seen, now) > NEIGHBOUR_TIMEOUT_MS * 1000ll)
            memset(&neighbours[i], 0, sizeof(neighbours[i]));

    if (! enabled)
        return;
    if (! radio_tools_idle()) {
        owner = false;  /* Another feature uses the radio, reconfigure afterwards */
        tx_pending = false;
        return;
    }
    if (! owner) {
        radio_tools_profile_social(SOCIAL_PATABLE);
        start_rx();
        owner = true;
    }
    if (absolute_time_diff_us(now, next_poll) > 0)
        return;
    next_poll = delayed_by_us(now, POLL_US);

    if (tx_pending) {
        /* Wait for the end of the beacon (~20ms), then listen */
        if (absolute_time_diff_us(tx_ts, now) < 3000)
            return;
        radio_state_t st = radio_state();
        if (st == CC1101_STATE_IDLE || absolute_time_diff_us(tx_ts, now) > 200000) {
            tx_pending = false;
            radio_wait_state(CC1101_STATE_IDLE, true);
            start_rx();
        }
        return;
    }

    if (absolute_time_diff_us(now, next_beacon) <= 0 && gpio_get(BADGE_RADIO_GDO0) == 0) {
        /* Not while receiving a packet (GDO0 high) */
        schedule_beacon(now);
        send_beacon();
        return;
    }
    poll_rx(now);
}
