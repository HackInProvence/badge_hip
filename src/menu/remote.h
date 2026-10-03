/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file remote.h
 *
 * \brief Remote commands: sent by an admin badge (NET_COMMAND packets, loud, repeated) or by a Flipper Zero
 * as a Princeton 433 MHz code (SubGHz > Add Manually > Princeton_433, key 0xC16Axx with xx the command):
 * the badges listen to the network all the time, and in OOK when they hear a transmitter that is not a badge
 * (see remote_task()).
 *
 * The badge obeys unless disabled in the settings. Commands:
 * - 0x01 cicada: the cicada sings a few seconds,
 * - 0x02 mute / 0x03 unmute: no sound and no LEDs during the talks,
 * - 0x05 sleep: everything off (radio, LEDs, sound) until the manual unlock (main.c), except a talk badge; only from
 *   the network of the badges (a Princeton code is ignored: any Flipper could put the conference to sleep),
 * - 0x10 to 0x14: lights of the talk badge (off, green, orange, red, red angry), see talk.c,
 * - 0x30 + n: starts the song n of the chorus (chorus.c).
 * The buttons of the remote of a Flipper (a saved Princeton file, see remote.c): 0x04 = end of the mute,
 * 0x18 = talk off, 0x1F = talk red, so that the files C16A01 and C16A11 drive their whole group.
 * */

#ifndef _REMOTE_H
#define _REMOTE_H

#include <stdbool.h>
#include <stdint.h>

#include "pico/time.h"

#define REMOTE_PRINCETON_ADDRESS 0xC16A00  /* Princeton key = address | command */

/* CTF "capture & rejeu": a secret Princeton code (not a remote command). An organizer badge emits it
 * (Admin > Commandes radio > Balise CTF), a player captures it with a Flipper Zero (SubGHz Read) and replays it
 * near a badge, which then reveals the "Rejeu radio" flag (ACHV_RADIO_REPLAY -> ctf_refresh). 0x5EC5EA ~ "SECSEA". */
#define CTF_RADIO_CODE 0x5EC5EA

enum {
    REMOTE_CIGALE = 0x01,
    REMOTE_MUTE = 0x02,
    REMOTE_UNMUTE = 0x03,
    REMOTE_SLEEP = 0x05,
    REMOTE_TALK = 0x10,  /* + state (talk.h) */
    REMOTE_SONG = 0x30,  /* + song number */
};

/* Handler of a group of commands (the high nibble: 0x10, 0x30), gets the low nibble */
typedef void (*remote_handler_t)(uint8_t arg);

void remote_init(void);
void remote_task(absolute_time_t now);

/** \brief Handles the commands of a group (0x10, 0x30). */
void remote_subscribe(uint8_t group, remote_handler_t handler);

/** \brief Executes a command (received, or local e.g. from the admin menu). */
void remote_execute(uint8_t command, const char *from);

/** \brief The sleep command was received (once): main.c puts the badge to sleep (not a talk badge). */
bool remote_sleep_requested(void);

/** \brief Sends a command to all the badges around (admin menu): loud, 5 times over 2 s. */
void remote_send(uint8_t command);

/** \brief Emits the CTF radio beacon (CTF_RADIO_CODE) several times in OOK, so a Flipper Zero can capture it
 * (Admin > Commandes radio > Balise CTF). Only the OOK Princeton frames, nothing on the network. */
void remote_send_ctf_beacon(void);

/** \brief The remote commands are obeyed (settings). */
bool remote_enabled(void);
void remote_set_enabled(bool enabled);

/** \brief Mute: no sound and no LEDs. */
bool remote_muted(void);
void remote_set_muted(bool muted);

/** \brief Stops (or allows again) the moments of OOK listening (remotes): for the features that need
 * all the packets of the network (chorus, image transfer...). Calls are counted. */
void remote_pause_windows(bool pause);

/** \brief The RSSI above which a transmitter opens a moment of listening to the remotes: the noise measured by the
 * tuning + 15 dB (radio_tune.c), -90 dBm without tuning. */
int remote_trigger_dbm(void);

/** \brief Debug ("!" on the serial port): the listening to the remotes. */
void remote_debug(void);

/** \brief Feeds a Princeton code received by the OOK receiver (ook_rx.c). */
void remote_princeton(uint32_t code);

/** \brief Short description of the last command, for the status line ("" when none since the last call). */
bool remote_event(char *buf, int len);

#endif /* _REMOTE_H */
