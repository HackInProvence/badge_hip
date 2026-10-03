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
 * above SOCIAL_RSSI_CLOSE within SOCIAL_CLOSE_WINDOW_MS: points for a new badge, fewer for each new one
 * (social_meeting_points(): 20, 18, 16... at most SOCIAL_MEETING_POINTS_MAX in all), none for a known one
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

#define SOCIAL_RSSI_CLOSE (-80)  /* dBm with the beacons at +10 dBm (~-70 to -83 at 1 m), to calibrate on site (radar page) */
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
    uint32_t skills;  /* Its skills (skills.h), 0 for the badges of an older firmware */
    uint8_t level;  /* Its level (achievements.h), 0 when unknown */
    bool batt;  /* It sends its battery level (Réglages > Batterie par radio): the next fields are valid */
    uint16_t batt_raw;  /* ADC of its battery (0..4095), valid even when it is not calibrated */
    uint16_t batt_mv;  /* Its battery in mV, 0 when not calibrated (then only the raw value means something) */
    bool batt_usb;  /* Plugged in USB (charging) */
} social_neighbour_t;

#define SOCIAL_MEETING_POINTS_MAX 200

/** \brief Points of the n-th new cicada met (n from 1): 10 % of what remains up to SOCIAL_MEETING_POINTS_MAX
 * (20, 18, 16, 15, 13...), at least 1 until the maximum is reached, then 0. */
int social_meeting_points(int n);

/** \brief Whether the beacons carry the battery level (saved), and change it. */
bool social_battery_shared(void);
void social_share_battery(bool on);

/** \brief The battery of a cicada for a list row: "81 %" (calibrated) or "ADC 2533", and " USB" (battradio.c). */
void battradio_text(const social_neighbour_t *c, char *buf, size_t len);

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
