/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Finding a cicada (seek.h): the asks of the radar, the fast answers and the signals of the cicada sought */

#include <stdio.h>
#include <string.h>

#include "i18n.h"
#include "net.h"
#include "seek.h"

enum { SEEK_ASK = 1, SEEK_ANSWER = 2 };

/* The badge that seeks */
static uint32_t followed = 0;
static uint8_t follow_flags = 0;
static absolute_time_t ask_ts = 0;
static bool heard_new = false;
static int16_t heard_rssi = 0;
static int heard_count = 0;  /* Answers in the last 5 s (log) */
static absolute_time_t count_ts = 0;

/* The badge sought */
static uint32_t seeker = 0;
static uint8_t sought_flags = 0;
static absolute_time_t sought_until = 0;
static int answers_left = 0;
static absolute_time_t answer_ts = 0;
static uint8_t answer_n = 0;
static bool event_pending = false;

static void handle_seek(const net_packet_t *p) {
    if (p->len >= 6 && p->data[0] == SEEK_ASK) {
        if (net_u32(p->data + 2) != net_id())
            return;
        bool is_new = ! seeker || absolute_time_diff_us(sought_until, p->at) > 0 || p->src != seeker;
        if (is_new) {
            event_pending = true;
            printf("seek: sought by %08lX (eyes %s, song %s)\n", (unsigned long)p->src,
                   p->data[1] & SEEK_EYES ? "on" : "off", p->data[1] & SEEK_SING ? "on" : "off");
        } else if (p->data[1] != sought_flags) {
            printf("seek: eyes %s, song %s\n", p->data[1] & SEEK_EYES ? "on" : "off",
                   p->data[1] & SEEK_SING ? "on" : "off");
        }
        seeker = p->src;
        sought_flags = p->data[1] & (SEEK_EYES | SEEK_SING);
        sought_until = delayed_by_ms(p->at, SEEK_HOLD_MS);
        answers_left = SEEK_ANSWERS;
        answer_ts = p->at;  /* The first answer at once */
    } else if (p->len >= 2 && p->data[0] == SEEK_ANSWER && followed && p->src == followed) {
        heard_rssi = p->rssi;
        heard_new = true;
        ++heard_count;
    }
}

void seek_init(void) {
    net_subscribe(NET_SEEK, handle_seek);
}

void seek_follow(uint32_t id, uint8_t flags) {
    if (id != followed || flags != follow_flags) {
        printf("seek: following %08lX (eyes %s, song %s)\n", (unsigned long)id, flags & SEEK_EYES ? "on" : "off",
               flags & SEEK_SING ? "on" : "off");
        ask_ts = get_absolute_time();  /* Asked at once: the cicada sees the change now */
    }
    if (! id)
        printf("seek: stopped\n");
    followed = id;
    follow_flags = flags;
    heard_new = false;
}

bool seek_heard(int16_t *rssi) {
    if (! heard_new)
        return false;
    heard_new = false;
    *rssi = heard_rssi;
    return true;
}

uint8_t seek_sought(void) {
    return seeker ? sought_flags : 0;
}

bool seek_event(char *buf, int len) {
    if (! event_pending)
        return false;
    event_pending = false;
    snprintf(buf, len, "%s", N_("Une cigale te cherche !"));
    return true;
}

void seek_task(absolute_time_t now) {
    if (followed && absolute_time_diff_us(count_ts, now) >= 5000000) {
        if (count_ts)
            printf("seek: %d answer(s) in 5 s, last %d dBm\n", heard_count, heard_rssi);
        heard_count = 0;
        count_ts = now;
    } else if (! followed) {
        count_ts = 0;
    }
    if (followed && absolute_time_diff_us(ask_ts, now) >= 0) {
        uint8_t data[6] = {SEEK_ASK, follow_flags};
        net_put_u32(data + 2, followed);
        if (net_send(NET_SEEK, data, sizeof(data), NET_LOUD))
            ask_ts = delayed_by_ms(now, SEEK_ASK_MS);
    }
    if (seeker && absolute_time_diff_us(sought_until, now) >= 0) {
        printf("seek: no longer sought\n");
        seeker = 0;
        sought_flags = 0;
        answers_left = 0;
    }
    if (answers_left && absolute_time_diff_us(answer_ts, now) >= 0) {
        uint8_t data[2] = {SEEK_ANSWER, answer_n++};
        if (net_send(NET_SEEK, data, sizeof(data), NET_LOUD)) {
            --answers_left;
            answer_ts = delayed_by_ms(now, SEEK_ANSWER_MS);
        }
    }
}
