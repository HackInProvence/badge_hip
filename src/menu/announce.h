/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file announce.h
 *
 * \brief Announcements sent by an admin badge: the cicadas build the screen from the time, the text and the QR code
 * received (see announce.c).
 * */

#ifndef _ANNOUNCE_H
#define _ANNOUNCE_H

#include <stdbool.h>
#include <stdint.h>

#include "pico/time.h"

#include "store.h"

enum {
    ANNOUNCE_QR_NONE,
    ANNOUNCE_QR_URL,
    ANNOUNCE_QR_TEXT,
    ANNOUNCE_QR_TEL,
    ANNOUNCE_QR_SMS,
    ANNOUNCE_QR_EMAIL,
    ANNOUNCE_QR_WIFI,
    ANNOUNCE_QR_GEO,
    ANNOUNCE_QR_TYPES,
};

void announce_init(void);
void announce_task(absolute_time_t now);

/** \brief Sends \p a to all the cicadas (in the background). */
void announce_send(const store_announce_t *a);

/** \brief A new announcement was received: its notification text. */
bool announce_new(char *buf, int len);

/** \brief The next opening of the page Annonces shows the newest announcement right away. */
void announce_open_newest(void);

/** \brief The announcements of the admin menu (the defaults until one is changed). */
store_announce_t *announce_list(void);

/** \brief The text of the QR code of \p a in its standard form (tel:, SMSTO:, WIFI:...), "" when none. */
void announce_qr_text(const store_announce_t *a, char *buf, int len);

/** \brief Draws the screen of an announcement. */
void announce_draw(uint8_t *fb, const store_announce_t *a);

#endif /* _ANNOUNCE_H */
