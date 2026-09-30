/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file vcard.h
 *
 * \brief The contact cards as vCards (RFC 6350, version 3.0) in text packets of the chat of the Flipper Zero:
 * one line per packet, ended by "\r\n", at most VCARD_PACKET_MAX bytes; a longer line is folded (the next packet
 * starts with a space). Before END:VCARD, a line "X-SECSEA-CHECK:<lines>-<crc>" (an extension allowed by the
 * standard) lets the receiver reject a card with a lost or mixed line: <lines> is the number of lines from
 * BEGIN:VCARD, <crc> the CRC-16/CCITT (hex) of these lines joined by '\n'. A card without this line (typed on a
 * Flipper) is accepted as it is.
 *
 * The fields are those of contacts.c, in this order: first name, name, phone, e-mail, company, job title,
 * address, city, LinkedIn, Git, web site, Mastodon, comment.
 * */

#ifndef _VCARD_H
#define _VCARD_H

#include <stdbool.h>
#include <stdint.h>

#define VCARD_FIELDS 13
#define VCARD_VALUE_MAX 55  /* Characters of a value (the largest field of a card, without its 0) */
#define VCARD_PACKET_MAX 60  /* Bytes of a packet of the chat, with its "\r\n" */
#define VCARD_PACKETS_MAX 32
#define VCARD_LINE_MAX 128  /* An unfolded line */
#define VCARD_LINES_MAX 24

typedef struct {
    char text[VCARD_PACKET_MAX + 1];
    uint8_t len;
} vcard_packet_t;

/** \brief The packets of a card: \p values[f] ("" = empty), only the fields whose bit is set in \p mask.
 * \return the number of packets (0 if they don't fit in \p max). */
int vcard_build(const char *const values[VCARD_FIELDS], uint16_t mask, vcard_packet_t *packets, int max);

/* Receiving: the packets of the chat, one by one */
typedef struct {
    char lines[VCARD_LINES_MAX][VCARD_LINE_MAX];
    int n;  /* Lines of the card being received (from BEGIN:VCARD) */
    bool in_card;
    char values[VCARD_FIELDS][VCARD_VALUE_MAX + 1];  /* The last complete card */
    uint16_t crc;  /* Of the last complete card: the same card sent again is recognized */
} vcard_rx_t;

void vcard_rx_init(vcard_rx_t *rx);

/** \brief A packet of the chat. \return true when it completes a valid card (in rx->values, rx->crc). */
bool vcard_rx_packet(vcard_rx_t *rx, const char *text, int len);

uint16_t vcard_crc16(const char *s, int len, uint16_t crc);

#endif /* _VCARD_H */
