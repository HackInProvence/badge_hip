/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file radio_tools.h
 *
 * \brief Non blocking radio features for the menu, compatible with the Flipper Zero (GFSK 9.99kbps preset):
 * - send a chat message (readable with the SubGHz chat applications),
 * - emit a continuous carrier (visible with the frequency analyzer),
 * - measure the CC1101 crystal against the RP2040 crystal (to check CC1101_fXOSC).
 * */

#ifndef _RADIO_TOOLS_H
#define _RADIO_TOOLS_H

#include <stdbool.h>
#include <stdint.h>

#include "pico/time.h"

#define RADIO_TOOLS_FREQ_HZ 433920000

/** \brief Reset and configure the radio (Flipper GFSK preset, +10dBm, 433.92MHz). */
void radio_tools_init(void);

/** \brief Send "<prefix> #n" as a chat message. \return the message number, 0 if busy */
unsigned radio_tools_send(void);

/** \brief Start / stop a continuous carrier (stops by itself after \p max_ms). */
void radio_tools_carrier_start(uint32_t max_ms);
void radio_tools_carrier_stop(void);
bool radio_tools_carrier_on(void);

/** \brief Start measuring the crystal (~1s), see radio_tools_xosc_hz(). */
void radio_tools_measure_xosc(void);
bool radio_tools_measuring(void);

/** \brief Measured crystal frequency in Hz, 0 when unknown. */
uint32_t radio_tools_xosc_hz(void);

/** \brief Version of the chip (0x14 for the CC1101), read at init. */
uint8_t radio_tools_chip_version(void);

/** \brief Human readable status of the last operation. */
const char *radio_tools_message(void);

/** \brief Text of the last message sent by radio_tools_send() (without its newline). */
const char *radio_tools_last_text(void);

/** \brief Whether no feature of this file uses the radio (the network of the cicadas can then use it). */
bool radio_tools_idle(void);

/** \brief Configures the radio again with the GFSK profile of the badges (after another mode, e.g. OOK). */
void radio_tools_reconfigure(void);

/** \brief Correction of the frequency (FSCTRL0 of the CC1101, steps of fXOSC / 2^14 ~ 1.6 kHz), see radio_tune.c. */
void radio_tools_set_freq_offset(int8_t steps);
int8_t radio_tools_freq_offset(void);

/** \brief Sync word of the network of the cicadas: different from the Flipper chat (0x464C), so that they ignore each other. */
#define RADIO_TOOLS_SOCIAL_SYNC1 0xC1
#define RADIO_TOOLS_SOCIAL_SYNC0 0x6A

/** \brief Configure the radio for the network of the cicadas (sync word, TX power), in IDLE. */
void radio_tools_profile_social(uint8_t patable);
/** \brief The profile of the chat of the Flipper Zero (sync word 0x464C). */
void radio_tools_profile_chat(uint8_t patable);

/** \brief Run the radio features, to call in the main loop. */
void radio_tools_task(absolute_time_t now);

#endif /* _RADIO_TOOLS_H */
