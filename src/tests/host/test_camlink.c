/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Host test of the link with the camera module (shared/camlink): packets through COBS and back, errors dropped, the
 * stream read again after garbage; the pictures for the e-Paper (campix). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camlink.h"
#include "campix.h"

static int fails = 0;
#define CHECK(c, ...) do { if (! (c)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); \
                                        printf("\n"); ++fails; } } while (0)

static uint32_t rnd_state = 12345;
static uint32_t rnd(void) {
    rnd_state = rnd_state * 1103515245u + 12345u;
    return rnd_state >> 8;
}

/* Feeds bytes, returns how many packets came out (the last one stays in rx) */
static int feed_all(camlink_rx_t *rx, const uint8_t *b, size_t n) {
    int packets = 0;
    for (size_t i = 0; i < n; ++i)
        packets += camlink_feed(rx, b[i]);
    return packets;
}

static void test_round_trips(void) {
    static uint8_t payload[CAMLINK_MAX_PAYLOAD], enc[CAMLINK_ENCODED_MAX], buf[CAMLINK_ENCODED_MAX];
    camlink_rx_t rx;
    camlink_rx_init(&rx, buf, sizeof(buf));
    size_t lens[] = {0, 1, 2, 253, 254, 255, 508, 1000, CAMLINK_MAX_PAYLOAD};
    for (int k = 0; k < 300; ++k) {
        size_t len = k < 9 ? lens[k] : rnd() % (CAMLINK_MAX_PAYLOAD + 1);
        int kind = k % 4;  /* Random, all zeros, no zeros, long runs */
        for (size_t i = 0; i < len; ++i)
            payload[i] = kind == 0 ? (uint8_t)rnd() : kind == 1 ? 0 : kind == 2 ? (uint8_t)(1 + rnd() % 255)
                                                                     : (uint8_t)((i / 300) & 1 ? 0 : 0xAB);
        size_t n = camlink_encode(CL_FILE_DATA, (uint8_t)k, payload, len, enc);
        CHECK(n > 0 && n <= CAMLINK_ENCODED_MAX, "length %zu encoded in %zu", len, n);
        CHECK(memchr(enc, 0, n - 1) == NULL && enc[n - 1] == 0, "a 0 inside the frame (len %zu)", len);
        int got = feed_all(&rx, enc, n);
        CHECK(got == 1 && rx.type == CL_FILE_DATA && rx.seq == (uint8_t)k && rx.len == len &&
              memcmp(rx.payload, payload, len) == 0, "round trip of %zu bytes (kind %d)", len, kind);
    }
    CHECK(camlink_encode(CL_FILE_DATA, 0, payload, CAMLINK_MAX_PAYLOAD + 1, enc) == 0, "too long accepted");
    CHECK(rx.bad == 0, "%u bad frames", (unsigned)rx.bad);
}

static void test_errors(void) {
    static uint8_t payload[100], enc[CAMLINK_ENCODED_MAX], buf[CAMLINK_ENCODED_MAX];
    for (int i = 0; i < 100; ++i)
        payload[i] = (uint8_t)(i * 7);
    camlink_rx_t rx;
    camlink_rx_init(&rx, buf, sizeof(buf));
    /* The boot text of the ESP32 before the first frame, then a frame: read */
    const char *boot = "ets Jul 29 2019 12:21:46\r\nrst:0x1 (POWERON_RESET),boot:0x13\r\n";
    feed_all(&rx, (const uint8_t *)boot, strlen(boot));
    camlink_feed(&rx, 0);  /* The text ends with the 0 of the first frame */
    size_t n = camlink_encode(CL_HELLO, 1, payload, 10, enc);
    CHECK(feed_all(&rx, enc, n) == 1 && rx.type == CL_HELLO, "a frame after the boot text");
    /* A byte changed: dropped, counted */
    uint32_t bad = rx.bad;
    n = camlink_encode(CL_FRAME, 2, payload, 100, enc);
    for (size_t i = 0; i + 1 < n; i += 9) {
        uint8_t keep = enc[i];
        enc[i] ^= 0x10;
        if (enc[i] == 0)
            enc[i] = 0x55;
        CHECK(feed_all(&rx, enc, n) == 0, "a changed byte (at %zu) accepted", i);
        enc[i] = keep;
    }
    CHECK(rx.bad > bad, "the bad frames not counted");
    /* A byte lost: dropped, the next frame read */
    n = camlink_encode(CL_FRAME, 3, payload, 100, enc);
    uint8_t lost[CAMLINK_ENCODED_MAX];
    memcpy(lost, enc, 40);
    memcpy(lost + 40, enc + 41, n - 41);
    CHECK(feed_all(&rx, lost, n - 1) == 0, "a frame with a byte lost accepted");
    CHECK(feed_all(&rx, enc, n) == 1 && rx.seq == 3, "the next frame not read");
    /* Too long (no 0 for a while): dropped, then the stream goes on */
    for (int i = 0; i < 3000; ++i)
        camlink_feed(&rx, 0x42);
    CHECK(camlink_feed(&rx, 0) == false, "an endless frame accepted");
    CHECK(feed_all(&rx, enc, n) == 1, "no frame after an endless one");
    /* The known CRC of "123456789" (CRC-16/CCITT-FALSE) */
    CHECK(camlink_crc16((const uint8_t *)"123456789", 9) == 0x29B1, "crc %04x", camlink_crc16((const uint8_t *)"123456789", 9));
}

/* camlink_write() in two pieces (a header and a body, as the screen of the badge) gives the bytes of camlink_encode()
 * of the whole payload; a small receiver takes the short packets and drops the long ones */
typedef struct {
    uint8_t *p;
    size_t max_piece;
} out_t;

static void out_bytes(const uint8_t *data, size_t len, void *ctx) {
    out_t *o = (out_t *)ctx;
    memcpy(o->p, data, len);
    o->p += len;
    if (len > o->max_piece)
        o->max_piece = len;
}

static void test_write_pieces(void) {
    static uint8_t payload[CAMLINK_MAX_PAYLOAD], enc[CAMLINK_ENCODED_MAX], streamed[CAMLINK_ENCODED_MAX];
    for (int k = 0; k < 200; ++k) {
        size_t len = 5 + rnd() % (CAMLINK_MAX_PAYLOAD - 4), head = k % 6;
        for (size_t i = 0; i < len; ++i)
            payload[i] = k % 3 == 0 ? 0 : (uint8_t)rnd();
        size_t n = camlink_encode(CL_SCREEN, (uint8_t)k, payload, len, enc);
        out_t o = {streamed, 0};
        size_t m = camlink_write(CL_SCREEN, (uint8_t)k, payload, head, payload + head, len - head, out_bytes, &o);
        CHECK(n == m && (size_t)(o.p - streamed) == m && memcmp(enc, streamed, n) == 0,
              "write in pieces differs (len %zu, head %zu)", len, head);
        CHECK(o.max_piece <= 255, "a piece of %zu bytes", o.max_piece);
    }
    uint8_t small[64];
    camlink_rx_t rx;
    camlink_rx_init(&rx, small, sizeof(small));
    uint8_t key[2] = {CAMLINK_KEY_TYPE, 'z'};
    size_t n = camlink_encode(CL_KEY, 7, key, 2, enc);
    CHECK(feed_all(&rx, enc, n) == 1 && rx.type == CL_KEY && rx.len == 2 && rx.payload[1] == 'z',
          "a short packet in a small receiver");
    n = camlink_encode(CL_FRAME, 8, payload, 500, enc);
    CHECK(feed_all(&rx, enc, n) == 0 && rx.bad == 1, "a long packet in a small receiver");
    n = camlink_encode(CL_KEY, 9, key, 1, enc);
    CHECK(feed_all(&rx, enc, n) == 1 && rx.seq == 9, "no packet after a long one");
}

static int count_white(const uint8_t *bits) {
    int n = 0;
    for (int i = 0; i < CAMLINK_FRAME_BYTES; ++i)
        for (int b = 0; b < 8; ++b)
            n += (bits[i] >> b) & 1;
    return n;
}

static void test_pictures(void) {
    static uint8_t src[320 * 240], gray[CAMPIX_SIZE * CAMPIX_SIZE], bits[CAMLINK_FRAME_BYTES], epi[CAMPIX_EPI_BYTES];
    static int16_t work[2 * (CAMPIX_SIZE + 2)];
    /* The center square of a QVGA picture: its left half black, its right half white */
    for (int y = 0; y < 240; ++y)
        for (int x = 0; x < 320; ++x)
            src[y * 320 + x] = x < 160 ? 0 : 255;
    campix_square(src, 320, 240, gray);
    CHECK(gray[100 * 200 + 50] == 0 && gray[100 * 200 + 150] == 255, "the center square");
    for (int d = 0; d < CAMLINK_DITHERS; ++d) {
        int want[] = {0, 20000, 40000};  /* Black, middle gray, white: white pixels expected */
        for (int k = 0; k < 3; ++k) {
            memset(gray, k * 255 / 2 + (k == 1), sizeof(gray));  /* 0, 128, 255 */
            campix_dither1(gray, d, bits, work);
            int w = count_white(bits);
            int tol = k == 1 ? (d == CAMLINK_DITHER_THRESHOLD ? 40000 : 2500) : 0;  /* Threshold: 128 is white */
            CHECK(abs(w - want[k]) <= tol, "dither %d, gray %d: %d white pixels", d, k * 127, w);
        }
    }
    memset(gray, 85, sizeof(gray));  /* One third: the gray level 1 everywhere (LSB 1, MSB 0) */
    campix_epi(gray, epi, work);
    CHECK(memcmp(epi, "EPIMAGE1", 8) == 0 && epi[8] == 200 && epi[10] == 200 && epi[12] == 2, "the .epi header");
    CHECK(count_white(epi + 16) > 39000 && count_white(epi + 16 + 5000) < 1000, "4 grays: level 1 (%d lsb, %d msb)",
          count_white(epi + 16), count_white(epi + 16 + 5000));
    memset(gray, 128, sizeof(gray));
    gray[0] = 0;
    gray[1] = 255;
    campix_stretch(gray);
    CHECK(gray[0] == 0 && gray[2] == 128, "a flat picture stretched");
}

int main(void) {
    test_round_trips();
    test_errors();
    test_write_pieces();
    test_pictures();
    printf("%s: %d failure(s)\n", fails ? "FAIL" : "ok", fails);
    return fails != 0;
}
