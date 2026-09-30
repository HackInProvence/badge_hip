/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file ook_tx.h
 *
 * \brief OOK transmitter: Princeton frames (24 bits, te = 400 µs) like the remotes of a Flipper Zero, so that the
 * badges that only listen in OOK (talk badge) get the commands of the admin badge.
 * */

#ifndef _OOK_TX_H
#define _OOK_TX_H

#include <stdbool.h>
#include <stdint.h>

/** \brief Sends \p frames times the Princeton \p code (~51 ms each), in the background.
 * \return false if the radio is busy (OOK receiver, other feature): try again later. */
bool ook_tx_princeton(uint32_t code, int frames);

/** \brief Frames being sent (the radio is taken). */
bool ook_tx_busy(void);

/** \brief In the main loop: gives the radio back to the network when the frames are sent. */
void ook_tx_task(void);

#endif /* _OOK_TX_H */
