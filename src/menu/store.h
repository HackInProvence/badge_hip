/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file store.h
 *
 * \brief Persistent storage of the menu application in the last sector (4kB) of the flash:
 * network of the cicadas (score, met badges), CTF flags, recorded IR signals.
 *
 * Changes are written 5s after the last one (store_changed() then store_task() in the main loop), to spare the flash.
 * Writing a sector takes ~50ms with the interrupts disabled (the DMA, hence the sound, keeps running).
 * */

#ifndef _STORE_H
#define _STORE_H

#include <stdbool.h>
#include <stdint.h>

#include "pico/time.h"

#include "ir.h"

#define STORE_MAX_MET 200
#define STORE_IR_SLOTS 4
#define STORE_NAME_LEN 12
#define STORE_SAVER_IMAGE_LEN 80

typedef struct {
    uint32_t id;
    uint16_t meets;  /* Number of meetings */
    uint16_t last_minute;  /* Uptime (minutes) of the last meeting, 0xFFFF = not since boot (not meaningful in flash) */
} store_met_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t n_met;
    uint32_t score;
    uint32_t flags_found;  /* CTF: bit n = flag n found */
    char name[STORE_NAME_LEN];
    ir_signal_t ir[STORE_IR_SLOTS];
    store_met_t met[STORE_MAX_MET];
    /* Added after the first version, at the end so that the stores already saved stay valid
     * (these fields read as 0xFF in them, checked by the users) */
    uint16_t rsvp_wpm;  /* Speed of the fast reading, in words per minute */
    uint32_t rsvp_hash;  /* Hash of the path of the last text read... */
    uint32_t rsvp_offset;  /* ...and where the reading stopped */
    uint8_t saver_minutes;  /* Screensaver delay (0 = off, 0xFF = default) */
    char saver_image[STORE_SAVER_IMAGE_LEN];  /* Path of the .EPI image on the SD card, empty (or 0xFF) = built-in */
} store_t;

/** \brief Load the store from the flash (or initialize it). */
void store_init(void);

store_t *store_get(void);

/** \brief The store was modified: save it soon. */
void store_changed(void);

/** \brief Save when needed, to call in the main loop. */
void store_task(absolute_time_t now);

#endif /* _STORE_H */
