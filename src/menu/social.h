/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file social.h
 *
 * \brief Network of the cicadas: the badges send beacons, and earn points when they meet (stay very close to each other).
 *
 * See docs/idees_reseau_extensions_ctf.md. Every ~2s (+-0.5s, to avoid systematic collisions),
 * the badge sends a beacon at low power (-20dBm): only a close badge receives it with a strong RSSI.
 * Between the beacons, the radio listens. A meeting is at least SOCIAL_CLOSE_BEACONS beacons of the same badge
 * above SOCIAL_RSSI_CLOSE within SOCIAL_CLOSE_WINDOW_MS: +10 points for a new badge, +1 for a known one
 * (at most once per hour). The score and the met badges are persistent (see store.h).
 *
 * The beacons use their own sync word (see radio_tools.h): the Flipper chat does not see them.
 * The network pauses while the other radio features (chat message, carrier, crystal measure) use the radio.
 *
 * Beacon (17 bytes after the length byte): 0xC1, type 0x01, id (4, LE), seq, score (2, LE), name (8, padded with 0).
 * */

#ifndef _SOCIAL_H
#define _SOCIAL_H

#include <stdbool.h>
#include <stdint.h>

#include "pico/time.h"

#define SOCIAL_RSSI_CLOSE (-50)  /* dBm, to calibrate with real badges (see the RSSI on the page of the network) */
#define SOCIAL_CLOSE_BEACONS 3
#define SOCIAL_CLOSE_WINDOW_MS 10000
#define SOCIAL_PATABLE 0x0E  /* -20dBm @433MHz */
#define SOCIAL_MAX_NEIGHBOURS 8

typedef struct {
    uint32_t id;
    char name[9];
    int16_t rssi;  /* dBm, of the last beacon */
    uint16_t score;
    bool met;  /* Already met */
} social_neighbour_t;

void social_init(void);

void social_set_enabled(bool enabled);
bool social_enabled(void);

/** \brief Run the network, to call in the main loop (only uses the radio when radio_tools_idle()). */
void social_task(absolute_time_t now);

const char *social_name(void);

/** \brief Id of this badge in the beacons (hash of its unique id). */
uint32_t social_id(void);
uint32_t social_score(void);
uint16_t social_met_count(void);

/** \brief Badges heard recently, sorted by RSSI (closest first). \return their number */
int social_neighbours(social_neighbour_t *out, int max);

/** \brief The last meeting, once: returns true and the message (e.g. "Rencontre : Cig 1A2B +10") when there is a new one. */
bool social_event(char *msg, int len);

/** \brief Number of beacons sent and received since boot (for the tests). */
void social_stats(uint32_t *sent, uint32_t *received);

#endif /* _SOCIAL_H */
