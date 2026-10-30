/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The link between the badge and its camera module: packets, CRC, COBS (camlink.h) */

#include <string.h>

#include "camlink.h"

uint16_t camlink_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;  /* CRC-16/CCITT-FALSE */
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; ++b)
            crc = crc & 0x8000 ? (uint16_t)(crc << 1) ^ 0x1021 : (uint16_t)(crc << 1);
    }
    return crc;
}

/* COBS: each 0 replaced by the distance to the next one; blocks of 254 bytes at most */
static size_t cobs_encode(const uint8_t *in, size_t len, uint8_t *out) {
    size_t code_at = 0, o = 1;
    uint8_t code = 1;
    for (size_t i = 0; i < len; ++i) {
        if (in[i] == 0) {
            out[code_at] = code;
            code_at = o++;
            code = 1;
        } else {
            out[o++] = in[i];
            if (++code == 0xFF) {
                out[code_at] = code;
                code_at = o++;
                code = 1;
            }
        }
    }
    out[code_at] = code;
    return o;
}

/* \return the decoded length, (size_t)-1 when the frame is malformed */
static size_t cobs_decode(const uint8_t *in, size_t len, uint8_t *out, size_t max) {
    size_t i = 0, o = 0;
    while (i < len) {
        uint8_t code = in[i++];
        if (code == 0)
            return (size_t)-1;
        for (uint8_t k = 1; k < code; ++k) {
            if (i >= len || o >= max)
                return (size_t)-1;
            out[o++] = in[i++];
        }
        if (code != 0xFF && i < len) {
            if (o >= max)
                return (size_t)-1;
            out[o++] = 0;
        }
    }
    return o;
}

size_t camlink_encode(uint8_t type, uint8_t seq, const uint8_t *payload, size_t len, uint8_t *out) {
    uint8_t packet[CAMLINK_PACKET_MAX];
    if (len > CAMLINK_MAX_PAYLOAD)
        return 0;
    packet[0] = type;
    packet[1] = seq;
    packet[2] = (uint8_t)len;
    packet[3] = (uint8_t)(len >> 8);
    if (len)
        memcpy(packet + 4, payload, len);
    uint16_t crc = camlink_crc16(packet, 4 + len);
    packet[4 + len] = (uint8_t)crc;
    packet[5 + len] = (uint8_t)(crc >> 8);
    size_t n = cobs_encode(packet, 6 + len, out);
    out[n++] = 0;
    return n;
}

void camlink_rx_init(camlink_rx_t *rx) {
    rx->n = 0;
    rx->overflow = false;
    rx->bad = 0;
    rx->len = 0;
}

bool camlink_feed(camlink_rx_t *rx, uint8_t byte) {
    if (byte != 0) {
        if (rx->n < sizeof(rx->buf))
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
    uint8_t packet[CAMLINK_PACKET_MAX];
    size_t m = overflow ? (size_t)-1 : cobs_decode(rx->buf, n, packet, sizeof(packet));
    if (m == (size_t)-1 || m < 6) {
        ++rx->bad;
        return false;
    }
    size_t len = packet[2] | (size_t)packet[3] << 8;
    uint16_t crc = packet[m - 2] | (uint16_t)packet[m - 1] << 8;
    if (len != m - 6 || len > CAMLINK_MAX_PAYLOAD || crc != camlink_crc16(packet, m - 2)) {
        ++rx->bad;
        return false;
    }
    rx->type = packet[0];
    rx->seq = packet[1];
    rx->len = len;
    memcpy(rx->payload, packet + 4, len);
    return true;
}
