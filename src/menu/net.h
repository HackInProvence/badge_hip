/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file net.h
 *
 * \brief Radio network of the cicadas: every feature that talks to other badges shares the CC1101 through this layer.
 *
 * All the packets use the same radio profile (GFSK 9.99 kbps like the Flipper preset, sync word 0xC16A, CRC):
 * [0xC1][type][id of the sender, 4 bytes little endian][data...], at most 61 bytes.
 * The badge listens all the time; the packets to send are queued and sent when the channel is free,
 * quiet (-20 dBm, a few meters: proximity) or loud (+10 dBm, the whole room).
 * The features subscribe to the types they handle. See docs/fr/guide_developpeur.md for the list of the types.
 *
 * Other features can take the radio (message to the Flipper, carrier, OOK receiver): net_pause() or
 * radio_tools_idle() false, the network reconfigures the radio afterwards.
 * */

#ifndef _NET_H
#define _NET_H

#include <stdbool.h>
#include <stdint.h>

#include "pico/time.h"

#define NET_MAGIC 0xC1
#define NET_HEADER 6  /* Magic, type, sender id */
#define NET_MAX_DATA 55  /* 61 bytes (RX FIFO minus length and status bytes) minus the header */

/* Packet types */
typedef enum {
    NET_BEACON = 0x01,  /* Network of the cicadas: presence, name, score (social.c) */
    NET_COMMAND = 0x02,  /* Remote command: mute, cicada, talk lights, program... (remote.c) */
    NET_MESSAGE = 0x03,  /* Short message relayed by the badges (messages.c) */
    NET_VOTE_QUESTION = 0x04,  /* Question opened by an admin badge (vote.c) */
    NET_VOTE_ANSWER = 0x05,
    NET_GAME = 0x06,  /* Two player radio games (duel.c) */
    NET_CONTACT = 0x07,  /* Contact card exchange (contacts.c) */
    NET_HOTCOLD = 0x08,  /* Beacon of the hot / cold hunt (hotcold.c) */
    NET_INFECTION = 0x09,  /* The (harmless) virus of the cicadas (infection.c) */
    NET_IMAGE = 0x0A,  /* Image transfer (image_radio.c) */
    NET_SONG = 0x0B,  /* Chorus: song start and position (chorus.c) */
    NET_TYPES = 0x10,
} net_type_t;

/* Flags of net_send() */
#define NET_QUIET 0x00  /* -20 dBm: only the badges close by (a few meters) */
#define NET_LOUD 0x01  /* +10 dBm: the whole room */
#define NET_JITTER 0x02  /* Random delay (up to NET_JITTER_MS) before sending: many badges answer the same packet */
#define NET_JITTER_MS 300

#define NET_PATABLE_QUIET 0x0E  /* -20 dBm @433MHz */
#define NET_PATABLE_LOUD 0xC0  /* +10 dBm @433MHz */

typedef struct {
    uint8_t type;
    uint32_t src;  /* Id of the sender */
    const uint8_t *data;
    uint8_t len;
    int16_t rssi;  /* dBm */
    absolute_time_t at;  /* When it was received (end of the packet) */
} net_packet_t;

typedef void (*net_handler_t)(const net_packet_t *packet);

void net_init(void);

/** \brief Id of this badge (hash of the unique id of the RP2040). */
uint32_t net_id(void);

/** \brief Call \p handler for each received packet of \p type (one handler per type). */
void net_subscribe(uint8_t type, net_handler_t handler);

/** \brief Queue a packet, returns false when the queue is full or the data too long. */
bool net_send(uint8_t type, const void *data, uint8_t len, uint8_t flags);

/** \brief Stop using the radio (another feature needs it), or use it again. */
void net_pause(bool paused);

/** \brief Run the network, to call in the main loop. */
void net_task(absolute_time_t now);

/** \brief Counters for the diagnostics. */
void net_stats(uint32_t *sent, uint32_t *received, uint32_t *dropped);

/* Little endian helpers for the packets */
static inline uint32_t net_u32(const uint8_t *p) {
    return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

static inline void net_put_u32(uint8_t *p, uint32_t v) {
    p[0] = v;
    p[1] = v >> 8;
    p[2] = v >> 16;
    p[3] = v >> 24;
}

#endif /* _NET_H */
