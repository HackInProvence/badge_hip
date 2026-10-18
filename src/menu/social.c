/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdio.h>
#include <string.h>

#include "pico/rand.h"

#include "achievements.h"
#include "battery.h"
#include "i18n.h"
#include "net.h"
#include "skills.h"
#include "social.h"
#include "store.h"


#define BEACON_LEN 11  /* Data of the NET_BEACON packets: sequence, score (2 bytes), name (8 bytes)... */
#define BEACON_LEN2 16  /* ...then (newer firmwares) skills (4 bytes), level */
#define BEACON_LEN3 21  /* ...then, when shared, the battery: raw ADC (2 bytes), mV (2 bytes, 0: not calibrated), flags */
#define BATT_USB 0x01
#define BATT_EST 0x02  /* No calibration in mV, an estimate of the automatic calibration: its % instead of the mV */
#define BEACON_PERIOD_MS 2000  /* While the channel is quiet */
#define BEACON_PERIOD_MAX_MS 8000
#define OCCUPANCY_TARGET 25  /* % of the channel (net_occupancy()): above, the beacons slow down proportionally */
/* The meeting window (SOCIAL_CLOSE_WINDOW_MS) and the forgetting of a neighbour (NEIGHBOUR_TIMEOUT_MS) are for
 * beacons every 2 s: they grow with the interval of the beacons of the neighbour (measured) or of this badge */
#define NEIGHBOUR_TIMEOUT_MS 15000
#define WINDOW_MAX_MS 40000
#define AGAIN_MINUTES 60  /* A cicada met again: counted (no points) at most once per hour */

typedef struct {
    social_neighbour_t pub;
    absolute_time_t last_seen;
    absolute_time_t close_start;
    uint8_t close_count;
    bool met_now;  /* Meeting counted during this visit */
    bool skills_told;  /* The shared skills were announced during this visit */
    uint16_t interval_ms;  /* Between its beacons (averaged; the beacons lost make it longer) */
} neighbour_t;

static bool enabled = true;
static absolute_time_t next_beacon = 0;
static uint8_t seq = 0;
static neighbour_t neighbours[SOCIAL_MAX_NEIGHBOURS];
static char event[48];
static bool event_pending = false;
static uint32_t n_sent = 0, n_received = 0;
static uint32_t n_ignored = 0;  /* Beacons of new badges ignored: the table full of stronger ones */
static social_detection_t detections[SOCIAL_DETECTIONS];  /* The newest first */
static int n_detections = 0;
static bool detect_pending = false;

bool social_detector(void) {
    return store_get()->detector == STORE_DETECTOR;
}

void social_set_detector(bool on) {
    store_get()->detector = on ? STORE_DETECTOR : 0;
    store_changed();
    printf("detect: %s\n", on ? "on" : "off");
}

bool social_detect_event(char *buf, int len) {
    if (! detect_pending)
        return false;
    detect_pending = false;
    snprintf(buf, len, _("Cigale détectée : %s (%d dBm)"), detections[0].name, detections[0].rssi);
    return true;
}

int social_detections(social_detection_t *out, int max) {
    int n = n_detections < max ? n_detections : max;
    memcpy(out, detections, n * sizeof(detections[0]));
    return n;
}

/* A cigale heard for the first time, or back after it was forgotten (NEIGHBOUR_TIMEOUT_MS) */
static void detected(const neighbour_t *nb, absolute_time_t now) {
    if (! social_detector())
        return;
    memmove(detections + 1, detections, (SOCIAL_DETECTIONS - 1) * sizeof(detections[0]));
    detections[0].id = nb->pub.id;
    memcpy(detections[0].name, nb->pub.name, sizeof(detections[0].name));
    detections[0].rssi = nb->pub.rssi;
    detections[0].at = now;
    if (n_detections < SOCIAL_DETECTIONS)
        ++n_detections;
    detect_pending = true;
    printf("detect: %08lX %s rssi %d\n", (unsigned long)nb->pub.id, nb->pub.name, nb->pub.rssi);
}


/* The period of the beacons: 2 s, longer when the channel is busy (the load stays about the same with more badges) */
uint32_t social_ignored(void) {
    return n_ignored;
}

uint32_t social_beacon_period_ms(void) {
    uint32_t occ = net_occupancy(), period = BEACON_PERIOD_MS;
    if (occ > OCCUPANCY_TARGET)
        period = BEACON_PERIOD_MS * occ / OCCUPANCY_TARGET;
    return period > BEACON_PERIOD_MAX_MS ? BEACON_PERIOD_MAX_MS : period;
}

static void schedule_beacon(absolute_time_t now) {
    uint32_t period = social_beacon_period_ms(), jitter = period / 4;  /* +- 25 %: two badges don't stay in step */
    next_beacon = delayed_by_ms(now, period - jitter + get_rand_32() % (2 * jitter));
}

/* A window of 2 s beacons (\p base_ms) for the beacons of \p nb: scaled to the longest interval */
static int64_t scaled_us(const neighbour_t *nb, uint32_t base_ms) {
    uint32_t interval = social_beacon_period_ms();
    if (nb->interval_ms > interval)
        interval = nb->interval_ms;
    uint32_t ms = interval > BEACON_PERIOD_MS ? base_ms * interval / BEACON_PERIOD_MS : base_ms;
    return (ms > WINDOW_MAX_MS ? WINDOW_MAX_MS : ms) * 1000ll;
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
            /* The points of that time: 10 for a new cicada, 1 for each meeting again */
            uint32_t points = 10 + (s->met[i].meets > 1 ? (s->met[i].meets - 1) : 0);
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


social_neighbour_t social_tmp[SOCIAL_MAX_NEIGHBOURS];

int social_neighbours(social_neighbour_t *out, int max) {
    /* Insertion sort by RSSI (closest first), straight into \p out, keeping the \p max first ones */
    int n = 0;
    for (int i = 0; i < SOCIAL_MAX_NEIGHBOURS && max > 0; ++i) {
        if (! neighbours[i].pub.id)
            continue;
        if (n == max && out[n-1].rssi >= neighbours[i].pub.rssi)
            continue;  /* Weaker than all the ones kept */
        int j = n < max ? n++ : n - 1;
        while (j > 0 && out[j-1].rssi < neighbours[i].pub.rssi) {
            out[j] = out[j-1];
            --j;
        }
        out[j] = neighbours[i].pub;
    }
    return n;
}


int social_meeting_points(int n) {
    int total = 0, points = 0;
    for (int k = 1; k <= n; ++k) {
        int rest = SOCIAL_MEETING_POINTS_MAX - total;
        points = (rest + 5) / 10;  /* 10 % of what remains, rounded */
        if (points < 1)
            points = rest > 0 ? 1 : 0;
        total += points;
    }
    return points;
}

bool social_battery_shared(void) {
    return store_get()->batt_share == STORE_BATT_SHARE;
}

void social_share_battery(bool on) {
    store_get()->batt_share = on ? STORE_BATT_SHARE : 0;
    store_changed();
    printf("social: battery %s in the beacons\n", on ? "shared" : "not shared");
}

/* Counts a meeting with this badge: points for a new one only (fewer and fewer, social_meeting_points()) */
static void meet(neighbour_t *nb, absolute_time_t now) {
    store_t *s = store_get();
    uint16_t minute = to_ms_since_boot(now) / 60000;
    int points;
    store_met_t *m = NULL;
    for (uint16_t i = 0; i < s->n_met; ++i)
        if (s->met[i].id == nb->pub.id)
            m = &s->met[i];
    if (! m) {
        points = social_meeting_points(s->n_met + 1);
        if (s->n_met < STORE_MAX_MET) {
            m = &s->met[s->n_met++];
            m->id = nb->pub.id;
            m->meets = 0;
        }
    } else if (m->last_minute == 0xFFFF || minute - m->last_minute >= AGAIN_MINUTES) {
        points = 0;  /* Already met: counted, no points */
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
    if (! points) {
        printf("social: met %s again (no points)\n", nb->pub.name);
        return;
    }
    snprintf(event, sizeof(event), _("Rencontre : %s +%d"), nb->pub.name, points);
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

    /* Find the neighbour, or take a free slot. The table full: the oldest one makes room only when the new badge is
     * received stronger than the oldest one was at its last beacon (a crowd must not push out the badge at hand,
     * whose meeting needs several beacons) */
    neighbour_t *nb = NULL, *oldest = NULL, *free_slot = NULL;
    bool is_new = false;
    for (int i = 0; i < SOCIAL_MAX_NEIGHBOURS && ! nb; ++i) {
        if (neighbours[i].pub.id == id)
            nb = &neighbours[i];
        else if (! neighbours[i].pub.id)
            free_slot = free_slot ? free_slot : &neighbours[i];
        else if (! oldest || absolute_time_diff_us(neighbours[i].last_seen, oldest->last_seen) > 0)
            oldest = &neighbours[i];
    }
    if (nb) {
        uint32_t ms = absolute_time_diff_us(nb->last_seen, now) / 1000;
        if (ms > 0)
            nb->interval_ms = nb->interval_ms ? (nb->interval_ms * 3 + (ms > 30000 ? 30000 : ms)) / 4 : ms;
    } else {
        if (! free_slot && rssi <= oldest->pub.rssi) {
            ++n_ignored;
            return;
        }
        nb = free_slot ? free_slot : oldest;
        memset(nb, 0, sizeof(*nb));
        is_new = true;
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
    nb->pub.skills = packet->len >= BEACON_LEN2 ? net_u32(p + 11) : 0;
    nb->pub.level = packet->len >= BEACON_LEN2 ? p[15] : 0;
    nb->pub.batt = packet->len >= BEACON_LEN3;
    if (nb->pub.batt) {
        nb->pub.batt_raw = p[16] | p[17] << 8;
        nb->pub.batt_mv = p[18] | p[19] << 8;
        nb->pub.batt_usb = p[20] & BATT_USB;
        nb->pub.batt_est = (p[20] & BATT_EST) && ! nb->pub.batt_mv && p[18] <= 100 ? p[18] : -1;
        if (nb->pub.batt_est >= 0)
            nb->pub.batt_mv = 0;
        /* For the PC (tools/battery_log.py): the battery of every cicada heard */
        printf("battery: %08lX %s raw %u mv %u usb %u rssi %d est %d\n", (unsigned long)id, nb->pub.name,
               nb->pub.batt_raw, nb->pub.batt_mv, nb->pub.batt_usb, rssi, nb->pub.batt_est);
    }
    nb->last_seen = now;
    if (is_new)
        detected(nb, now);  /* The name and the RSSI are known now */
    uint32_t common = nb->pub.skills & store_get()->skills;
    if (common && rssi >= SOCIAL_RSSI_CLOSE && ! nb->skills_told) {
        nb->skills_told = true;  /* A cicada close by shares a skill: once per visit */
        int first = 0;
        while (! (common >> first & 1))
            ++first;
        snprintf(event, sizeof(event), _("%s aime aussi : %s"), nb->pub.name, tr(skills_name(first)));
        event_pending = true;
        printf("social: %s\n", event);
        achv_unlock(ACHV_SKILL_MATCH);
    }

    /* Close enough, long enough: meeting */
    if (rssi >= SOCIAL_RSSI_CLOSE && ! nb->met_now) {
        if (! nb->close_count || absolute_time_diff_us(nb->close_start, now) > scaled_us(nb, SOCIAL_CLOSE_WINDOW_MS)) {
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
    uint8_t p[BEACON_LEN3] = {seq++, s->score, s->score >> 8};
    memcpy(p + 3, s->name, 8);  /* The name is truncated to 8 characters in the beacon */
    net_put_u32(p + 11, s->skills);
    p[15] = achv_level();
    int len = BEACON_LEN2;
    if (social_battery_shared()) {
        uint16_t raw = battery_raw(), mv = battery_calibrated() ? battery_mv() : 0;
        p[16] = raw;
        p[17] = raw >> 8;
        p[18] = mv;
        p[19] = mv >> 8;
        p[20] = battery_charging() ? BATT_USB : 0;
        if (! mv && battery_percent() >= 0) {  /* The estimate of the automatic calibration, in place of the mV */
            p[18] = battery_percent();
            p[19] = 0;
            p[20] |= BATT_EST;
        }
        len = BEACON_LEN3;
    }
    if (net_send(NET_BEACON, p, len, NET_LOUD))  /* +10 dBm: at -10 dBm, ~-97 dBm at 1 m (edge of the sensitivity) */
        ++n_sent;
}


void social_task(absolute_time_t now) {
    /* Forget the badges that went away */
    for (int i = 0; i < SOCIAL_MAX_NEIGHBOURS; ++i)
        if (neighbours[i].pub.id
                && absolute_time_diff_us(neighbours[i].last_seen, now) > scaled_us(&neighbours[i], NEIGHBOUR_TIMEOUT_MS))
            memset(&neighbours[i], 0, sizeof(neighbours[i]));

    if (enabled && absolute_time_diff_us(now, next_beacon) <= 0) {
        schedule_beacon(now);
        send_beacon();
    }
}
