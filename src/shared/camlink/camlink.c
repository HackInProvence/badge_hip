/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The link between the badge and its camera module: packets, CRC, COBS (camlink.h) */

#include <string.h>

#include "camlink.h"

static uint16_t crc16_add(uint16_t crc, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; ++b)
            crc = crc & 0x8000 ? (uint16_t)(crc << 1) ^ 0x1021 : (uint16_t)(crc << 1);
    }
    return crc;
}

uint16_t camlink_crc16(const uint8_t *data, size_t len) {
    return crc16_add(0xFFFF, data, len);  /* CRC-16/CCITT-FALSE */
}

/* The COBS writer: each 0 replaced by the distance to the next one; blocks of 254 bytes at most, each one given to
 * the output with its code byte in front */
typedef struct {
    uint8_t block[255];  /* [code][up to 254 bytes] */
    size_t n;  /* Bytes in the block, the code included */
    size_t total;
    void (*out)(const uint8_t *data, size_t len, void *ctx);
    void *ctx;
} cobs_writer_t;

static void cobs_flush(cobs_writer_t *w) {
    w->block[0] = (uint8_t)w->n;
    w->out(w->block, w->n, w->ctx);
    w->total += w->n;
    w->n = 1;
}

static void cobs_put(cobs_writer_t *w, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (data[i] == 0) {
            cobs_flush(w);
        } else {
            w->block[w->n++] = data[i];
            if (w->n == 0xFF)
                cobs_flush(w);
        }
    }
}

size_t camlink_write(uint8_t type, uint8_t seq, const uint8_t *head, size_t head_len, const uint8_t *body,
                     size_t body_len, void (*out)(const uint8_t *data, size_t len, void *ctx), void *ctx) {
    size_t len = head_len + (body ? body_len : 0);
    if (len > CAMLINK_MAX_PAYLOAD)
        return 0;
    cobs_writer_t w = {.n = 1, .total = 0, .out = out, .ctx = ctx};
    uint8_t h[4] = {type, seq, (uint8_t)len, (uint8_t)(len >> 8)};
    uint16_t crc = crc16_add(0xFFFF, h, 4);
    cobs_put(&w, h, 4);
    if (head_len) {
        crc = crc16_add(crc, head, head_len);
        cobs_put(&w, head, head_len);
    }
    if (body && body_len) {
        crc = crc16_add(crc, body, body_len);
        cobs_put(&w, body, body_len);
    }
    uint8_t c[2] = {(uint8_t)crc, (uint8_t)(crc >> 8)};
    cobs_put(&w, c, 2);
    cobs_flush(&w);  /* The last block, even empty (code 1): the same bytes as a whole-buffer COBS encoder */
    static const uint8_t zero = 0;
    out(&zero, 1, ctx);
    return w.total + 1;
}

typedef struct {
    uint8_t *p;
} mem_out_t;

static void mem_out(const uint8_t *data, size_t len, void *ctx) {
    mem_out_t *m = (mem_out_t *)ctx;
    memcpy(m->p, data, len);
    m->p += len;
}

size_t camlink_encode(uint8_t type, uint8_t seq, const uint8_t *payload, size_t len, uint8_t *out) {
    mem_out_t m = {out};
    return camlink_write(type, seq, payload, len, NULL, 0, mem_out, &m);
}

/* \return the decoded length, (size_t)-1 when the frame is malformed; \p out may be \p in (decoded in place: the
 * output never passes the input) */
static size_t cobs_decode(const uint8_t *in, size_t len, uint8_t *out) {
    size_t i = 0, o = 0;
    while (i < len) {
        uint8_t code = in[i++];
        if (code == 0)
            return (size_t)-1;
        for (uint8_t k = 1; k < code; ++k) {
            if (i >= len)
                return (size_t)-1;
            out[o++] = in[i++];
        }
        if (code != 0xFF && i < len)
            out[o++] = 0;
    }
    return o;
}

void camlink_rx_init(camlink_rx_t *rx, uint8_t *buf, size_t cap) {
    rx->buf = buf;
    rx->cap = cap;
    rx->n = 0;
    rx->overflow = false;
    rx->bad = 0;
    rx->payload = buf;
    rx->len = 0;
}

bool camlink_feed(camlink_rx_t *rx, uint8_t byte) {
    if (byte != 0) {
        if (rx->n < rx->cap)
            rx->buf[rx->n++] = byte;
        else
            rx->overflow = true;
        return false;
    }
    /* The end of a frame */
    size_t n = rx->n;
    bool overflow = rx->overflow;
    rx->n = 0;
    rx->overflow = false;
    if (n == 0)
        return false;  /* Two 0 in a row: nothing */
    size_t m = overflow ? (size_t)-1 : cobs_decode(rx->buf, n, rx->buf);
    if (m == (size_t)-1 || m < 6) {
        ++rx->bad;
        return false;
    }
    const uint8_t *packet = rx->buf;
    size_t len = packet[2] | (size_t)packet[3] << 8;
    uint16_t crc = packet[m - 2] | (uint16_t)packet[m - 1] << 8;
    if (len != m - 6 || len > CAMLINK_MAX_PAYLOAD || crc != camlink_crc16(packet, m - 2)) {
        ++rx->bad;
        return false;
    }
    rx->type = packet[0];
    rx->seq = packet[1];
    rx->len = len;
    rx->payload = packet + 4;
    return true;
}
