/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file fsk_link.h
 *
 * \brief The acoustic link of docs/fsk-buzzer-link.html (the receiver, a web page: it is the reference): a text
 * sent by the buzzer in 4-FSK, decoded by the microphone of a phone or a PC.
 *
 * 4 tones (FSK_TONES_HZ), FSK_SYMBOL_MS per symbol, 2 bits per symbol in Gray code (tone 0 = 00, 1 = 01, 2 = 11,
 * 3 = 10). The frame: the sync word (28 symbols), the length in two Hamming(8,4) words (high nibble first), then
 * [data..., CRC-8] in a convolutional code K = 7, rate 1/2, polynomials 171 / 133 octal, 6 tail bits, interleaved in
 * blocks of 8 rows (padded to a multiple of 16 bits), 2 bits per symbol, MSB first. CRC-8: polynomial 0x07, init 0,
 * over [length, data...]. The same as buildFrame() of the page (checked by the host test "fsk", which runs the
 * JavaScript of the page with node).
 * */

#ifndef _FSK_LINK_H
#define _FSK_LINK_H

#include <stdint.h>

#define FSK_MAX_LEN 32  /* Bytes of text */
#define FSK_SYNC_LEN 28
/* The longest frame: the sync word, 8 symbols of length, the coded bits of 32 bytes + CRC (544 bits) */
#define FSK_MAX_SYMS (FSK_SYNC_LEN + 8 + 272)
#define FSK_SYMBOL_MS 20
#define FSK_REPEATS 3  /* The receiver combines the copies */
#define FSK_GAP_MS 300  /* Silence between the copies */

extern const uint16_t FSK_TONES_HZ[4];

/* A frame ready to play: its symbols after the sync word, 2 bits each (MSB first), packed */
typedef struct {
    uint8_t bits[(FSK_MAX_SYMS - FSK_SYNC_LEN) * 2 / 8];
    int n_syms;  /* With the sync word */
} fsk_frame_t;

/** \brief The frame of \p len bytes of \p data (at most FSK_MAX_LEN, cut beyond). \return its number of symbols */
int fsk_build(const uint8_t *data, int len, fsk_frame_t *f);

/** \brief The symbol \p i (tone 0..3) of the frame \p f (\p i < f->n_syms). */
uint8_t fsk_symbol(const fsk_frame_t *f, int i);

/** \brief The symbols (tones 0..3) of the frame into \p syms (FSK_MAX_SYMS) (the host test). \return their number */
int fsk_build_frame(const uint8_t *data, int len, uint8_t *syms);

uint8_t fsk_crc8(const uint8_t *bytes, int n);

#endif /* _FSK_LINK_H */
