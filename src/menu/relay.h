/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file relay.h
 *
 * \brief Relay of the messages of an admin badge (remote commands, LEDs of the cicadas) from cicada to cicada, so
 * that a whole conference hears them even out of reach of the admin badge.
 *
 * The admin messages relayed: the remote commands, the LEDs of the cicadas, the announcements (each part); never the
 * messages between cicadas (games, contacts, messages, votes...).
 * The admin message carries a TTL (number of hops still allowed, Admin > Commandes radio: "Relais") and its origin
 * (the id of the admin badge: the duplicates are found by origin + message id, whoever relays them). A cicada that
 * receives it with a TTL above 0 sends it again once with TTL - 1, after a random delay (RELAY_DELAY_MIN_MS to
 * RELAY_DELAY_MAX_MS), unless:
 * - during that delay it heard RELAY_ENOUGH_COPIES relays of the same message by other cicadas (the place is
 *   covered);
 * - it relayed a message of the same kind (the same command, the same LEDs) less than RELAY_SAME_MS ago (the band
 *   is not saturated, the message already went on).
 * */

#ifndef _RELAY_H
#define _RELAY_H

#include <stdbool.h>
#include <stdint.h>

#include "pico/time.h"

#define RELAY_TTL_MAX 4
#define RELAY_TTL_DEFAULT 2
#define RELAY_DELAY_MIN_MS 100
#define RELAY_DELAY_MAX_MS 900
#define RELAY_ENOUGH_COPIES 2
#define RELAY_SAME_MS 10000
#define RELAY_DATA_MAX 55  /* NET_MAX_DATA */

/** \brief An admin message received (each copy, including the duplicates).
 * \param type the packet type (NET_COMMAND, NET_LEDS)
 * \param data the data to relay, its TTL already decremented by the caller
 * \param src the sender of this copy (the admin badge or a relay), \p origin the admin badge
 * \param id the message: its nonce (and the part of an announcement: nonce | part << 16)
 * \param key what makes two messages "the same kind" (the command, a hash of the LEDs order or of the part)
 * \param ttl the TTL received (0: not relayed) */
void relay_offer(uint8_t type, const uint8_t *data, int len, uint32_t src, uint32_t origin, uint32_t id,
                 uint32_t key, uint8_t ttl, absolute_time_t now);

/** \brief FNV-1a hash of bytes (the key of a message), from \p h (0x811C9DC5 to start). */
uint32_t relay_hash(uint32_t h, const uint8_t *data, int len);

/** \brief Sends the relays that are due, to call in the main loop. */
void relay_task(absolute_time_t now);

/** \brief A relay is waiting to be sent (the sleep waits for it: asleep, the radio is off). */
bool relay_pending(void);

/** \brief The TTL of the messages of this admin badge (saved), and its change. */
uint8_t relay_admin_ttl(void);
void relay_set_admin_ttl(uint8_t ttl);

#endif /* _RELAY_H */
