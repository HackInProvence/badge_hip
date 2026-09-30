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
#define STORE_GAMES 8  /* At least GAME_COUNT */
#define STORE_TYPE_PARTICIPANT 0
#define STORE_TYPE_SPEAKER 1
#define STORE_TYPE_STAFF 2
#define STORE_ADMIN_ON 0xA5

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
    uint16_t game_records[STORE_GAMES];  /* Records of the mini games (games.h), 0xFFFF = none */
    uint8_t lamp_percent;  /* Brightness of the lamp, 0xFF = default */
    uint8_t badge_type;  /* STORE_TYPE_*, set in the admin menu; 0xFF = participant */
    uint8_t admin;  /* STORE_ADMIN_ON: the admin menu is shown (secret sequence of keys) */
    uint8_t remote_off;  /* 1: the remote commands (Flipper, admin badges) are ignored; 0xFF = default (obeyed) */
    uint8_t muted;  /* 1: no sound and no LEDs (remote command during the talks) */
    uint8_t infection;  /* INFECTION_* state of the virus game */
    uint16_t crypto_solved;  /* Bit n: challenge n solved; 0xFFFF = none */
    uint16_t puzzle_records[STORE_GAMES];  /* Records of the puzzles (puzzles.h), 0xFFFF = none */
} store_t;

/* Second store (2 sectors before the first one): the contact cards (contacts.c) */
#define CONTACT_BYTES 512  /* All the fields of a card (contacts.c, sizes of FIELDS) */
#define STORE_CONTACTS 12
#define STORE_EXT_SIZE 8192

typedef struct {
    char bytes[CONTACT_BYTES];  /* The fields at fixed offsets, each one a C string */
} contact_card_t;

typedef union {
    struct {
        uint32_t magic;
        uint16_t version;
        uint16_t send_mask;  /* Bit n: field n of my card is sent */
        contact_card_t mine;
        uint8_t n_contacts;
        uint8_t pad[3];
        contact_card_t contacts[STORE_CONTACTS];  /* Received, the newest last */
    };
    uint8_t raw[STORE_EXT_SIZE];  /* Whole sectors (the flash is written from this buffer) */
} store_ext_t;

store_ext_t *store_ext_get(void);
void store_ext_changed(void);

/** \brief Load the store from the flash (or initialize it). */
void store_init(void);

store_t *store_get(void);

/** \brief The store was modified: save it soon. */
void store_changed(void);

/** \brief Save when needed, to call in the main loop. */
void store_task(absolute_time_t now);

#endif /* _STORE_H */
