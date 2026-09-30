/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdio.h>
#include <string.h>

#include "hardware/gpio.h"
#include "pico/rand.h"
#include "pico/unique_id.h"

#include "net.h"
#include "pinouts.h"
#include "radio.h"
#include "radio_tools.h"

#define QUEUE_LEN 8
#define POLL_US 2000
#define TX_END_MIN_US 3000  /* The radio leaves IDLE (calibration) before sending */
#define TX_TIMEOUT_US 200000

typedef struct {
    uint8_t len;
    uint8_t flags;
    absolute_time_t not_before;
    uint8_t bytes[NET_HEADER + NET_MAX_DATA];
} tx_packet_t;

static uint32_t my_id = 0;
static net_handler_t handlers[NET_TYPES];
static tx_packet_t queue[QUEUE_LEN];
static int queue_head = 0, queue_count = 0;
static bool paused = false;
static bool owner = false;  /* The radio is configured for the network */
static bool tx_pending = false;
static absolute_time_t tx_ts = 0;
static absolute_time_t next_poll = 0;
static uint32_t n_sent = 0, n_received = 0, n_dropped = 0;
static bool loopback = false;
static bool verbose = false;
static uint8_t loop_bytes[NET_HEADER + NET_MAX_DATA];
static uint8_t loop_len = 0;  /* A packet sent, to deliver as received (loopback) */


static void handle_ping(const net_packet_t *p) {
    printf("net: ping #%u from %08lX, rssi %d dBm\n", p->len ? p->data[0] : 0, (unsigned long)p->src, p->rssi);
}


void net_ping(void) {
    static uint8_t n = 0;
    ++n;
    net_send(NET_PING, &n, 1, NET_LOUD);
    printf("net: ping #%u sent\n", n);
}


void net_init(void) {
    /* Identity: hash (FNV-1a) of the unique id of the flash */
    pico_unique_board_id_t uid;
    pico_get_unique_board_id(&uid);
    my_id = 2166136261u;
    for (int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; ++i)
        my_id = (my_id ^ uid.id[i]) * 16777619u;
    handlers[NET_PING] = handle_ping;
}


uint32_t net_id(void) {
    return my_id;
}


void net_subscribe(uint8_t type, net_handler_t handler) {
    if (type < NET_TYPES)
        handlers[type] = handler;
}


bool net_send(uint8_t type, const void *data, uint8_t len, uint8_t flags) {
    if (len > NET_MAX_DATA || queue_count >= QUEUE_LEN) {
        ++n_dropped;
        return false;
    }
    tx_packet_t *t = &queue[(queue_head + queue_count++) % QUEUE_LEN];
    t->bytes[0] = NET_MAGIC;
    t->bytes[1] = type;
    net_put_u32(t->bytes + 2, my_id);
    memcpy(t->bytes + NET_HEADER, data, len);
    t->len = NET_HEADER + len;
    t->flags = flags;
    uint32_t delay_ms = flags & NET_JITTER ? get_rand_32() % NET_JITTER_MS : 0;
    t->not_before = delayed_by_ms(get_absolute_time(), delay_ms);
    return true;
}


bool net_idle(void) {
    return ! tx_pending && ! queue_count && ! gpio_get(BADGE_RADIO_GDO0);
}


void net_pause(bool p) {
    paused = p;
    if (paused && owner && radio_tools_idle())
        radio_wait_state(CC1101_STATE_IDLE, true);
    if (paused) {
        /* The radio will be reconfigured (profile, sync word) when the network gets it back */
        owner = false;
        tx_pending = false;
    }
}


bool net_transmitting(void) {
    return tx_pending || (owner && gpio_get(BADGE_RADIO_GDO0));  /* GDO0: a packet on air (sent or received) */
}


void net_set_verbose(bool on) {
    verbose = on;
}


bool net_verbose(void) {
    return verbose;
}


void net_set_loopback(bool on) {
    loopback = on;
}


bool net_loopback(void) {
    return loopback;
}


void net_stats(uint32_t *sent, uint32_t *received, uint32_t *dropped) {
    *sent = n_sent;
    *received = n_received;
    *dropped = n_dropped;
}


static void start_rx(void) {
    radio_write_registers((const uint8_t[]){CC1101_SRX}, 1);
}


static void flush_rx(void) {
    radio_wait_state(CC1101_STATE_IDLE, true);
    radio_write_registers((const uint8_t[]){CC1101_SFRX}, 1);
    start_rx();
}


/* Reads a received packet when there is a complete one */
static void poll_rx(absolute_time_t now) {
    uint8_t n;
    radio_read_registers(CC1101_RXBYTES, &n, 1);
    if (n & 0x80) {
        flush_rx();  /* Overflow */
        return;
    }
    n &= 0x7F;
    /* A packet is complete when GDO0 went low again (0x06: asserted from the sync word to the end of the packet) */
    if (n > 0 && ! gpio_get(BADGE_RADIO_GDO0)) {
        uint8_t len;
        radio_read_registers(CC1101_RXFIFO, &len, 1);
        uint8_t buf[64];
        if (len + 2 > (int)sizeof(buf) || len + 2 > n - 1) {
            flush_rx();  /* Too long for us, or incomplete (should not happen once GDO0 is low) */
            return;
        }
        radio_read_registers(CC1101_RXFIFO, buf, len + 2);  /* Payload + RSSI + LQI/CRC */
        bool crc_ok = buf[len + 1] & 0x80;
        if (crc_ok && len >= NET_HEADER && buf[0] == NET_MAGIC && buf[1] < NET_TYPES) {
            net_packet_t p = {
                .type = buf[1],
                .src = net_u32(buf + 2),
                .data = buf + NET_HEADER,
                .len = len - NET_HEADER,
                .rssi = (int8_t)buf[len] / 2 - 74,
                .at = now,
            };
            if (verbose) {
                /* FREQEST: the offset between the sender and this badge, in fXOSC / 2^14 steps (~1.6 kHz) */
                uint8_t fe = 0;
                radio_read_registers(CC1101_FREQEST, &fe, 1);
                printf("net: rx type %u from %08lX, %u bytes, %d dBm, offset %+ld Hz\n", p.type, (unsigned long)p.src,
                       p.len, p.rssi, (long)((int64_t)(int8_t)fe * radio_get_xosc() >> 14));
            }
            if (p.src != my_id && p.src != 0) {
                ++n_received;
                if (handlers[p.type])
                    handlers[p.type](&p);
            }
        }
    }
    /* After a packet the radio goes IDLE (MCSM1.RXOFF_MODE): listen again */
    if (radio_state() == CC1101_STATE_IDLE)
        start_rx();
}


static void send_next(void) {
    tx_packet_t *t = &queue[queue_head];
    radio_wait_state(CC1101_STATE_IDLE, true);
    radio_set_power(t->flags & NET_LOUD ? NET_PATABLE_LOUD : t->flags & NET_MEDIUM ? NET_PATABLE_MEDIUM : NET_PATABLE_QUIET);
    if (verbose)
        printf("net: tx type %u, %u bytes, flags 0x%02x\n", t->bytes[1], t->len - NET_HEADER, t->flags);
    if (radio_tx_packet(t->bytes, t->len)) {
        tx_pending = true;
        tx_ts = get_absolute_time();
        ++n_sent;
        if (loopback) {
            memcpy(loop_bytes, t->bytes, t->len);
            loop_len = t->len;
        }
    } else {
        ++n_dropped;
        start_rx();
    }
    queue_head = (queue_head + 1) % QUEUE_LEN;
    --queue_count;
}


void net_task(absolute_time_t now) {
    if (paused)
        return;
    if (! radio_tools_idle()) {
        owner = false;  /* Another feature uses the radio, reconfigure afterwards */
        tx_pending = false;
        return;
    }
    if (! owner) {
        radio_tools_profile_social(NET_PATABLE_QUIET);
        start_rx();
        owner = true;
    }
    if (absolute_time_diff_us(now, next_poll) > 0)
        return;
    next_poll = delayed_by_us(now, POLL_US);

    if (loop_len && ! tx_pending) {
        /* Loopback: the packet sent comes back from the "twin" badge */
        net_packet_t p = {
            .type = loop_bytes[1],
            .src = my_id ^ NET_TWIN,
            .data = loop_bytes + NET_HEADER,
            .len = loop_len - NET_HEADER,
            .rssi = -40,
            .at = now,
        };
        loop_len = 0;
        if (p.type < NET_TYPES && handlers[p.type])
            handlers[p.type](&p);
    }

    if (tx_pending) {
        /* Wait for the end of the packet (~20-60ms), then listen */
        if (absolute_time_diff_us(tx_ts, now) < TX_END_MIN_US)
            return;
        radio_state_t st = radio_state();
        if (st == CC1101_STATE_IDLE || absolute_time_diff_us(tx_ts, now) > TX_TIMEOUT_US) {
            tx_pending = false;
            radio_wait_state(CC1101_STATE_IDLE, true);
            radio_write_registers((const uint8_t[]){CC1101_SFTX}, 1);
            start_rx();
        }
        return;
    }

    /* Send when the channel is free: not while receiving a packet (GDO0 high) */
    if (queue_count && absolute_time_diff_us(queue[queue_head].not_before, now) >= 0 && ! gpio_get(BADGE_RADIO_GDO0)) {
        send_next();
        return;
    }
    poll_rx(now);
}
