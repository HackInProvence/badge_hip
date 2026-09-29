/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdio.h>
#include <string.h>

#include "gfx.h"
#include "qrcodegen.h"
#include "score_code.h"

#define QR_MAX_VERSION 6

/* Not the key: the key XOR a xorshift32 stream (see unmask()). tools/score_check.py --make-key makes a new one. */
static const uint8_t DITHER_LUT[16] = {
    0x50, 0x40, 0x59, 0x88, 0xef, 0x9d, 0x41, 0x55, 0xeb, 0xa6, 0xfd, 0x70, 0x80, 0xac, 0xda, 0xdf,
};

static void unmask(uint8_t *out) {
    uint32_t s = 0x7A3C19E5;
    for (int i = 0; i < 16; ++i) {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        out[i] = DITHER_LUT[i] ^ (uint8_t)(s ^ (i * 37));
    }
}

#define ROTL(x, b) (uint64_t)(((x) << (b)) | ((x) >> (64 - (b))))
#define SIPROUND do { \
    v0 += v1; v1 = ROTL(v1, 13); v1 ^= v0; v0 = ROTL(v0, 32); \
    v2 += v3; v3 = ROTL(v3, 16); v3 ^= v2; \
    v0 += v3; v3 = ROTL(v3, 21); v3 ^= v0; \
    v2 += v1; v1 = ROTL(v1, 17); v1 ^= v2; v2 = ROTL(v2, 32); \
} while (0)

static uint64_t le64(const uint8_t *p) {
    uint64_t v = 0;
    for (int i = 7; i >= 0; --i)
        v = v << 8 | p[i];
    return v;
}

uint64_t score_code_siphash(const uint8_t key[16], const uint8_t *data, size_t len) {
    uint64_t k0 = le64(key), k1 = le64(key + 8);
    uint64_t v0 = 0x736f6d6570736575ULL ^ k0, v1 = 0x646f72616e646f6dULL ^ k1;
    uint64_t v2 = 0x6c7967656e657261ULL ^ k0, v3 = 0x7465646279746573ULL ^ k1;
    size_t n = len - len % 8;
    for (size_t i = 0; i < n; i += 8) {
        uint64_t m = le64(data + i);
        v3 ^= m;
        SIPROUND;
        SIPROUND;
        v0 ^= m;
    }
    uint64_t b = (uint64_t)(len & 0xFF) << 56;
    for (size_t i = n; i < len; ++i)
        b |= (uint64_t)data[i] << (8 * (i - n));
    v3 ^= b;
    SIPROUND;
    SIPROUND;
    v0 ^= b;
    v2 ^= 0xFF;
    SIPROUND;
    SIPROUND;
    SIPROUND;
    SIPROUND;
    return v0 ^ v1 ^ v2 ^ v3;
}

void score_code_text(char *buf, size_t len, const char *game, const char *score, uint32_t badge_id, const char *name) {
    int n = snprintf(buf, len, "HIP26:%s:%s:%08lX:%s", game, score, (unsigned long)badge_id, name);
    if (n < 0 || (size_t)n + 18 > len) {
        buf[0] = 0;
        return;
    }
    uint8_t key[16];
    unmask(key);
    uint64_t sig = score_code_siphash(key, (const uint8_t *)buf, n);
    memset(key, 0, sizeof(key));  /* Not left on the stack */
    snprintf(buf + n, len - n, ":%08lX%08lX", (unsigned long)(sig >> 32), (unsigned long)(sig & 0xFFFFFFFF));
}

int score_code_draw(uint8_t *fb, const char *text, int y, int scale) {
    uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(QR_MAX_VERSION)];
    uint8_t tmp[qrcodegen_BUFFER_LEN_FOR_VERSION(QR_MAX_VERSION)];
    if (! qrcodegen_encodeText(text, tmp, qr, qrcodegen_Ecc_MEDIUM, 1, QR_MAX_VERSION, qrcodegen_Mask_AUTO, true))
        return 0;
    int size = qrcodegen_getSize(qr), px = size * scale, x0 = (GFX_WIDTH - px) / 2;
    if (! fb)
        return px;  /* Only the size */
    for (int my = 0; my < size; ++my)
        for (int mx = 0; mx < size; ++mx)
            if (qrcodegen_getModule(qr, mx, my))
                gfx_fill_rect(fb, x0 + mx * scale, y + my * scale, scale, scale, GFX_BLACK);
    return px;
}
