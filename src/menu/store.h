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
#define STORE_ACHV_COUNTERS 16
#define STORE_CARGO_ITEMS 32

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
    /* Automatic tuning of the radio (radio_tune.c), done at the first start and from Réglages */
    uint8_t radio_tuned;  /* STORE_RADIO_TUNED once done */
    int8_t radio_noise_dbm;  /* Noise floor measured (dBm): the trigger of the listening to the remotes */
    int8_t radio_freq_offset;  /* FSCTRL0 (FREQOFF) of the CC1101, in steps of fXOSC / 2^14 (~1.6 kHz) */
    /* Second generation of fields: valid when v2_magic is STORE_V2_MAGIC, set to their defaults otherwise
     * (store_init()), so that 0xFF (erased flash) never means "all the achievements" */
    uint8_t v2_magic;
    uint32_t skills;  /* Bit n: skill n of skills.c checked (shown on the name tag, sent in the beacon) */
    uint64_t achievements;  /* Bit n: achievement n of achievements.c unlocked */
    uint16_t achv_counters[STORE_ACHV_COUNTERS];  /* Counters of the achievements (games won, trades...) */
    uint32_t book_hash;  /* Gamebook (gamebook.c): hash of the book being read... */
    uint16_t book_section;  /* ...and its current section */
    uint8_t cargo[STORE_CARGO_ITEMS];  /* Smuggler (smuggler.c): how many of each good */
    uint8_t cargo_seeded;  /* 1 once the first goods were given */
    int8_t hot_dbm;  /* Hot / cold beacon (hotcold.c): the RSSI of "BRÛLANT" set by the admin, outside -90..-40: default */
    uint8_t ww_unlock;  /* Loup-garou (werewolf.c), set in the admin menu: WW_UNLOCK_*, other values: WW_UNLOCK_RULE */
    uint8_t asleep;  /* STORE_ASLEEP: put to sleep by an admin (remote command 0x05): main.c starts in the sleep mode */
    char lang[2];  /* Language of the texts (i18n.c): "en"...; unknown (0, 0xFF): French */
} store_t;

#define STORE_ASLEEP 0x5A

#define STORE_V2_MAGIC 0x5A

#define STORE_RADIO_TUNED 0xA5
#define STORE_ANNOUNCE_MAGIC 0x4E4E4F41  /* "ANNO" */

/* Second store (2 sectors before the first one): the contact cards (contacts.c) */
#define CONTACT_BYTES 512  /* All the fields of a card (contacts.c, sizes of FIELDS) */
#define STORE_CONTACTS 12
#define STORE_EXT_SIZE 8192

typedef struct {
    char bytes[CONTACT_BYTES];  /* The fields at fixed offsets, each one a C string */
} contact_card_t;

/* An announcement of the admin menu (announce.c): UTF-8 strings */
#define STORE_ANNOUNCES 6
typedef struct {
    char time[6];  /* "10:30" */
    char text[112];  /* Up to 56 characters (2 bytes for an accented letter) */
    uint8_t qr_type;  /* ANNOUNCE_QR_* */
    char qr[64];  /* The content of the QR code (a URL, a phone number...) */
} store_announce_t;

typedef union {
    struct {
        uint32_t magic;
        uint16_t version;
        uint16_t send_mask;  /* Bit n: field n of my card is sent */
        contact_card_t mine;
        uint8_t n_contacts;
        uint8_t pad[3];
        contact_card_t contacts[STORE_CONTACTS];  /* Received, the newest last */
        /* Added later: valid when announce_magic is STORE_ANNOUNCE_MAGIC (otherwise the defaults of announce.c) */
        uint32_t announce_magic;
        store_announce_t announces[STORE_ANNOUNCES];
        uint32_t contact_skills[STORE_CONTACTS];  /* The skills of the cards received (skills.h), 0: none */
    };
    uint8_t raw[STORE_EXT_SIZE];  /* Whole sectors (the flash is written from this buffer) */
} store_ext_t;

/* Factory settings (the sector before the second store): never erased by the resets (Remise à zéro, new store
 * versions) nor by a new firmware; written at once, they are rarely changed (admin menu). */
#define STORE_FACTORY_MAGIC 0x54434146  /* "FACT" */
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t pad;
    /* Calibration of the battery measure (battery.c): 2 points (ADC raw value, millivolts), 0 = not set */
    uint16_t battery_raw[2];
    uint16_t battery_mv[2];
} store_factory_t;

/** \brief The factory settings (all 0 when never set). */
store_factory_t *store_factory_get(void);

/** \brief Write the factory settings to the flash now (~50ms). \return true when done */
bool store_factory_save(void);

store_ext_t *store_ext_get(void);
void store_ext_changed(void);

/** \brief Load the store from the flash (or initialize it). */
void store_init(void);

store_t *store_get(void);

/** \brief The store was modified: save it soon. */
void store_changed(void);

/** \brief The store was modified and must be written now (~50 ms), e.g. the goods of a trade: a badge switched off
 * within the delay of store_changed() would come back with the old ones (and could duplicate a good). */
void store_save_now(void);

/** \brief Save when needed, to call in the main loop. */
void store_task(absolute_time_t now);

#endif /* _STORE_H */
