/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The encoder of the acoustic link (fsk_link.h): a port of buildFrame() of docs/fsk-buzzer-link.html, the same
 * functions (crc8, hammingEnc, convEnc, interleave, the Gray symbols). */

#include <string.h>

#include "fsk_link.h"

/* The tones of the page, set the same in its strip (16 to 17.5 kHz: hardly audible, the buzzer still renders them) */
const uint16_t FSK_TONES_HZ[4] = {16000, 16500, 17000, 17500};

/* The sync word of the page (PATTERN): 28 symbols */
static const uint8_t PATTERN[FSK_SYNC_LEN] = {1, 0, 1, 0, 2, 2, 1, 1, 0, 2, 1, 3, 1, 3, 0, 1, 2, 3, 3, 2, 3, 3, 2, 0,
                                              3, 0, 1, 1};
#define ROWS 8
#define TAIL 6
#define G1 0171
#define G2 0133
#define MAX_BITS ((8 * (FSK_MAX_LEN + 1) + TAIL) * 2 + 2 * ROWS)
/* Packed bits (the RAM is short): bit i of \p a, MSB first */
#define GET(a, i) ((a)[(i) / 8] >> (7 - (i) % 8) & 1)
#define PUT(a, i, v) ((a)[(i) / 8] = (uint8_t)(((a)[(i) / 8] & ~(0x80 >> (i) % 8)) | ((v) ? 0x80 >> (i) % 8 : 0)))

uint8_t fsk_crc8(const uint8_t *bytes, int n) {
    uint8_t c = 0;
    for (int k = 0; k < n; ++k) {
        c ^= bytes[k];
        for (int i = 0; i < 8; ++i)
            c = c & 0x80 ? (uint8_t)((c << 1) ^ 0x07) : (uint8_t)(c << 1);
    }
    return c;
}

static int parity(unsigned x) {
    x ^= x >> 4;
    x ^= x >> 2;
    x ^= x >> 1;
    return x & 1;
}

/* Hamming(8,4) of the nibble \p n: 8 bits into \p out */
static void hamming(int n, uint8_t *out) {
    int d0 = n >> 3 & 1, d1 = n >> 2 & 1, d2 = n >> 1 & 1, d3 = n & 1;
    uint8_t c[7] = {d0 ^ d1 ^ d3, d0 ^ d2 ^ d3, d0, d1 ^ d2 ^ d3, d1, d2, d3};
    uint8_t p = 0;
    for (int i = 0; i < 7; ++i) {
        out[i] = c[i];
        p ^= c[i];
    }
    out[7] = p;
}

/* The multiple of 2 * ROWS bits at least \p n */
static int pad_len(int n) {
    return (n + 2 * ROWS - 1) / (2 * ROWS) * 2 * ROWS;
}

int fsk_build(const uint8_t *data, int len, fsk_frame_t *f) {
    uint8_t coded[MAX_BITS / 8];
    memset(coded, 0, sizeof(coded));
    memset(f->bits, 0, sizeof(f->bits));
    if (len > FSK_MAX_LEN)
        len = FSK_MAX_LEN;
    if (len < 0)
        len = 0;
    uint8_t payload[FSK_MAX_LEN + 2];
    payload[0] = len;
    memcpy(payload + 1, data, len);
    uint8_t crc = fsk_crc8(payload, len + 1);
    /* The convolutional code over the bits of [data..., crc], MSB first, then the tail */
    int n = 0;
    unsigned sr = 0;
    for (int i = 0; i < 8 * (len + 1) + TAIL; ++i) {
        int b = 0;
        if (i < 8 * (len + 1)) {
            uint8_t byte = i / 8 < len ? data[i / 8] : crc;
            b = byte >> (7 - i % 8) & 1;
        }
        sr = ((sr << 1) | b) & 0x7F;
        PUT(coded, n, parity(sr & G1));
        ++n;
        PUT(coded, n, parity(sr & G2));
        ++n;
    }
    /* After the sync word: the length (2 Hamming words), then the coded bits, interleaved by blocks: bit i to row
     * i % ROWS, column i / ROWS (the padding: zeros) */
    uint8_t lb[16];
    hamming(len >> 4, lb);
    hamming(len & 15, lb + 8);
    for (int i = 0; i < 16; ++i)
        PUT(f->bits, i, lb[i]);
    int total = pad_len(n), cols = total / ROWS;
    for (int i = 0; i < total; ++i)
        PUT(f->bits, 16 + (i % ROWS) * cols + i / ROWS, i < n ? GET(coded, i) : 0);
    f->n_syms = FSK_SYNC_LEN + (16 + total) / 2;
    return f->n_syms;
}

uint8_t fsk_symbol(const fsk_frame_t *f, int i) {
    static const uint8_t GRAY_TONE[4] = {0, 1, 3, 2};  /* Bits b1 b0 -> tone: 00 -> 0, 01 -> 1, 11 -> 2, 10 -> 3 */
    if (i < FSK_SYNC_LEN)
        return PATTERN[i];
    int b = 2 * (i - FSK_SYNC_LEN);
    return GRAY_TONE[GET(f->bits, b) << 1 | GET(f->bits, b + 1)];
}

int fsk_build_frame(const uint8_t *data, int len, uint8_t *syms) {
    static fsk_frame_t f;
    int n = fsk_build(data, len, &f);
    for (int i = 0; i < n; ++i)
        syms[i] = fsk_symbol(&f, i);
    return n;
}
