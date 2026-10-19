/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file seek.h
 *
 * \brief Finding a cicada (the radar, hotcold.c): the badge that follows a cicada asks it, every second (NET_SEEK),
 * to answer fast (SEEK_ANSWERS packets in the second: the strength of its signal ~5 times a second instead of its
 * beacon every 2 s), and if wanted to blink its eyes in green and to sing, so that it is found by eye and by ear.
 *
 * Packet NET_SEEK: [SEEK_ASK][flags][id of the cicada sought, 4 bytes] or [SEEK_ANSWER][number of the answer].
 * The cicada sought shows it while the asks come (SEEK_HOLD_MS after the last one), not in mute mode (sound, LEDs).
 * An older firmware ignores the packet: the radar then follows its beacons. */

#ifndef _SEEK_H
#define _SEEK_H

#include <stdbool.h>
#include <stdint.h>

#include "pico/time.h"

#define SEEK_EYES 0x01  /* The cicada sought blinks its LEDs in green */
#define SEEK_SING 0x02  /* It sings (the sound of the cicada) */

#define SEEK_ASK_MS 1000
#define SEEK_ANSWERS 5  /* Answers to an ask, one every SEEK_ANSWER_MS */
#define SEEK_ANSWER_MS 200
#define SEEK_HOLD_MS 2500  /* The eyes and the song stop this long after the last ask */

void seek_init(void);

/** \brief The service, at each loop: the asks of the badge that seeks, the answers and the signals of the one sought. */
void seek_task(absolute_time_t now);

/** \brief Seek the cicada \p id (0: stop), with SEEK_EYES / SEEK_SING. */
void seek_follow(uint32_t id, uint8_t flags);

/** \brief A new answer of the cicada followed since the last call: its strength in \p rssi. */
bool seek_heard(int16_t *rssi);

/** \brief This badge is sought: the signals wanted (SEEK_EYES / SEEK_SING), 0 when not (main.c shows them). */
uint8_t seek_sought(void);

/** \brief Once per seek: "Une cigale te cherche !" for the footer. */
bool seek_event(char *buf, int len);

#endif /* _SEEK_H */
