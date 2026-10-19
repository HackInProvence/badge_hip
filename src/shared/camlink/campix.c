/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The pictures of the camera for the e-Paper (campix.h) */

#include <string.h>

#include "camlink.h"
#include "campix.h"

#define S CAMPIX_SIZE

void campix_square(const uint8_t *gray, int w, int h, uint8_t *out) {
    int side = w < h ? w : h, x0 = (w - side) / 2, y0 = (h - side) / 2;
    for (int y = 0; y < S; ++y) {
        const uint8_t *row = gray + (size_t)(y0 + y * side / S) * w + x0;
        for (int x = 0; x < S; ++x)
            out[y * S + x] = row[x * side / S];
    }
}

void campix_stretch(uint8_t *gray) {
    uint32_t hist[256] = {0};
    for (int i = 0; i < S * S; ++i)
        ++hist[gray[i]];
    int lo = 0, hi = 255;
    uint32_t sum = 0, cut = S * S / 100;
    while (lo < 255 && (sum += hist[lo]) < cut)
        ++lo;
    sum = 0;
    while (hi > 0 && (sum += hist[hi]) < cut)
        --hi;
    if (hi - lo < 16)
        return;  /* A flat picture: kept */
    for (int i = 0; i < S * S; ++i) {
        int v = (gray[i] - lo) * 255 / (hi - lo);
        gray[i] = v < 0 ? 0 : v > 255 ? 255 : (uint8_t)v;
    }
}

static void set_bit(uint8_t *out, int x, int y, int white) {
    if (white)
        out[(y * S + x) >> 3] |= 0x80 >> (x & 7);
}

/* Error diffusion on the rows: Floyd-Steinberg (7, 3, 5, 1 / 16) or Atkinson (6 x 1/8: lighter, contrasted) */
static void diffuse(uint8_t *gray, int levels, int atkinson, uint8_t *out1, uint8_t *lsb, uint8_t *msb,
                    int16_t *work) {
    int16_t *cur = work, *next = work + S + 2;
    memset(work, 0, sizeof(int16_t) * 2 * (S + 2));
    int16_t next2[S + 2];  /* Atkinson: two rows below */
    memset(next2, 0, sizeof(next2));
    for (int y = 0; y < S; ++y) {
        for (int x = 0; x < S; ++x) {
            int v = gray[y * S + x] + cur[x + 1];
            v = v < 0 ? 0 : v > 255 ? 255 : v;
            int q = (v * (levels - 1) + 127) / 255;  /* 0 .. levels - 1 */
            int err = v - q * 255 / (levels - 1);
            if (out1)
                set_bit(out1, x, y, q);
            else {  /* 4 grays: level = MSB * 2 + LSB, 0 = black */
                set_bit(lsb, x, y, q & 1);
                set_bit(msb, x, y, q >> 1);
            }
            if (atkinson) {
                int e = err / 8;
                cur[x + 2] += e;
                if (x + 3 < S + 2)
                    cur[x + 3] += e;
                next[x] += e;
                next[x + 1] += e;
                next[x + 2] += e;
                next2[x + 1] += e;
            } else {
                cur[x + 2] += err * 7 / 16;
                next[x] += err * 3 / 16;
                next[x + 1] += err * 5 / 16;
                next[x + 2] += err / 16;
            }
        }
        int16_t *t = cur;
        cur = next;
        next = t;
        if (atkinson) {
            memcpy(next, next2, sizeof(next2));
            memset(next2, 0, sizeof(next2));
        } else {
            memset(next, 0, sizeof(int16_t) * (S + 2));
        }
    }
}

void campix_dither1(uint8_t *gray, int dither, uint8_t *out, int16_t *work) {
    static const uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    memset(out, 0, S * S / 8);
    switch (dither) {
    case CAMLINK_DITHER_BAYER:  /* Stable from a frame to the next: the best for the videos */
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x)
                set_bit(out, x, y, gray[y * S + x] > BAYER[y & 3][x & 3] * 16 + 8);
        break;
    case CAMLINK_DITHER_THRESHOLD:
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x)
                set_bit(out, x, y, gray[y * S + x] >= 128);
        break;
    default:
        diffuse(gray, 2, dither == CAMLINK_DITHER_ATKINSON, out, NULL, NULL, work);
        break;
    }
}

void campix_epi(uint8_t *gray, uint8_t *out, int16_t *work) {
    static const uint8_t HEADER[16] = {'E', 'P', 'I', 'M', 'A', 'G', 'E', '1', S, 0, S, 0, 2, 0, 0, 0};
    memcpy(out, HEADER, sizeof(HEADER));
    uint8_t *lsb = out + 16, *msb = lsb + S * S / 8;
    memset(lsb, 0, 2 * S * S / 8);
    diffuse(gray, 4, 0, NULL, lsb, msb, work);
}
