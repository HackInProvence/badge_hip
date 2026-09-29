/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file remote.h
 *
 * \brief Remote commands: sent by an admin badge (NET_COMMAND packets, loud, repeated) or by a Flipper Zero
 * as a Princeton 433 MHz code (SubGHz > Add Manually > Princeton_433, key 0xC16Axx with xx the command):
 * the badges listen to the OOK remotes a short moment every second (see remote_task()).
 *
 * The badge obeys unless disabled in the settings. Commands:
 * - 0x01 cicada: the cicada sings a few seconds,
 * - 0x02 mute / 0x03 unmute: no sound and no LEDs during the talks,
 * - 0x10 to 0x14: lights of the talk badge (off, green, orange, red, red angry), see talk.c,
 * - 0x20 + n: shows the talk n of the program (program.c),
 * - 0x30 + n: starts the song n of the chorus (chorus.c).
 * */

#ifndef _REMOTE_H
#define _REMOTE_H

#include <stdbool.h>
#include <stdint.h>

#include "pico/time.h"

#define REMOTE_PRINCETON_ADDRESS 0xC16A00  /* Princeton key = address | command */

enum {
    REMOTE_CIGALE = 0x01,
    REMOTE_MUTE = 0x02,
    REMOTE_UNMUTE = 0x03,
    REMOTE_TALK = 0x10,  /* + state (talk.h) */
    REMOTE_PROGRAM = 0x20,  /* + talk number */
    REMOTE_SONG = 0x30,  /* + song number */
};

/* Handler of a group of commands (the high nibble: 0x10, 0x20, 0x30), gets the low nibble */
typedef void (*remote_handler_t)(uint8_t arg);

void remote_init(void);
void remote_task(absolute_time_t now);

/** \brief Handles the commands of a group (0x10, 0x20...). */
void remote_subscribe(uint8_t group, remote_handler_t handler);

/** \brief Executes a command (received, or local e.g. from the admin menu). */
void remote_execute(uint8_t command, const char *from);

/** \brief Sends a command to all the badges around (admin menu): loud, 3 times. */
void remote_send(uint8_t command);

/** \brief The remote commands are obeyed (settings). */
bool remote_enabled(void);
void remote_set_enabled(bool enabled);

/** \brief Mute: no sound and no LEDs. */
bool remote_muted(void);
void remote_set_muted(bool muted);

/** \brief Stops (or allows again) listening to the OOK remotes a moment every second: for the features that need
 * all the packets of the network (chorus, image transfer...). Calls are counted. */
void remote_pause_windows(bool pause);

/** \brief Feeds a Princeton code received by the OOK receiver (ook_rx.c). */
void remote_princeton(uint32_t code);

/** \brief Short description of the last command, for the status line ("" when none since the last call). */
bool remote_event(char *buf, int len);

#endif /* _REMOTE_H */
