/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file ook_rx.h
 *
 * \brief Receives the 433.92 MHz OOK signals (remotes, weather sensors): the CC1101 in asynchronous OOK mode
 * (registers of the Flipper "AM650" preset) gives the demodulated signal on GDO0, an interrupt measures the pulses,
 * the main loop cuts them in frames and decodes them with ookdec.c. Receive only.
 *
 * Several users can listen at the same time (the remote commands when a transmitter is heard, the decoder
 * page, the talk badge);
 * the network of the cicadas (net.c) is paused while someone listens.
 * The Princeton codes of the remote commands (remote.h) are given to remote_princeton().
 * */

#ifndef _OOK_RX_H
#define _OOK_RX_H

#include <stdbool.h>
#include <stdint.h>

#include "pico/time.h"

#include "ookdec.h"

#define OOK_RX_FRAME_GAP_US 50000  /* A silence this long ends a transmission (the guards between the repeats of a frame last up to 25 ms: the decoder wants the repeats) */

/** \brief One more user: takes the radio (pauses the network) and listens in OOK. */
void ook_rx_start(void);

/** \brief One user less: back to the network after the last one. */
void ook_rx_stop(void);

bool ook_rx_active(void);

/** \brief Listening again after another feature used the radio (radio_tools.c), when someone still listens. */
void ook_rx_resume(void);

/** \brief Debug: traces the decoding attempts (key O of the serial port). */
void ook_rx_set_debug(bool on);

/** \brief Decodes the frames, to call in the main loop. */
void ook_rx_task(absolute_time_t now);

/** \brief The last decoded frame, when it is newer than \p seen (updated): returns false otherwise. */
bool ook_rx_get(uint32_t *seen, ookdec_result_t *result);

/** \brief Debug: prints the durations of the last signal given to the decoder. */
void ook_rx_dump(void);

/** \brief Microseconds since the last pulse edge (activity on the air). */
uint32_t ook_rx_quiet_us(void);

/** \brief Pulses received and frames decoded since the boot (activity indicators). */
uint32_t ook_rx_pulses(void);
uint32_t ook_rx_frames(void);

#endif /* _OOK_RX_H */
