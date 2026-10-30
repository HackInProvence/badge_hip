/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file camlink.h
 *
 * \brief The link between the badge and its ESP32-CAM module (on an extension port), shared by both firmwares
 * (plain C, no SDK): the UART at CAMLINK_BAUD, packets in COBS frames ended by a 0.
 *
 * A packet: [type][seq][len lo][len hi][payload: len bytes][crc16 lo][crc16 hi], the CRC-16/CCITT-FALSE of the bytes
 * before it; COBS encoded (no 0 inside), then a 0. A 0 always ends a frame: after an error (a byte lost, the boot text
 * of the ESP32), the next frame is read again.
 *
 * The data (CL_FRAME, CL_FILE_*, CL_VFRAME, CL_KEY, CL_WEB_STATE from the module; CL_SCREEN from the badge) waits for
 * the CL_ACK of the other side (same seq) before the next packet, sent again after CAMLINK_ACK_MS: the other side may
 * be busy for a while (the screen, the SD card), nothing is lost. The commands of the badge are answered by a CL_ACK
 * too, sent again the same way.
 *
 * The remote control (protocol 2): the module is a Wi-Fi access point with a web page (CL_WEB); the badge sends it
 * its screen (CL_SCREEN) each time it changes, the page sends the buttons pressed and the text typed (CL_KEY). */

#ifndef _CAMLINK_H
#define _CAMLINK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CAMLINK_BAUD 921600
#define CAMLINK_PROTO 2  /* Version of the protocol, in CL_HELLO (2: the remote control) */
#define CAMLINK_MAX_PAYLOAD 1040  /* A part of a frame (1000) or of a file (1024), and its header */
#define CAMLINK_PART 1000  /* The parts of a 200 x 200 frame of 1 bit (5000 bytes) */
#define CAMLINK_FILE_PART 1024
#define CAMLINK_ACK_MS 300
#define CAMLINK_FRAME_BYTES 5000  /* 200 x 200, 1 bit per pixel, 1 = white, bit 7 = the leftmost pixel */
#define CAMLINK_PACKET_MAX (4 + CAMLINK_MAX_PAYLOAD + 2)
#define CAMLINK_ENCODED_MAX (CAMLINK_PACKET_MAX + CAMLINK_PACKET_MAX / 254 + 2)  /* COBS and the final 0 */
#define CAMLINK_WEB_TEXT 32  /* The name and the password of the access point, each at most */

typedef enum {
    CL_HELLO = 1,  /* Both ways: [proto][name of the firmware and version, text] */
    CL_ACK = 2,  /* [seq acknowledged] */
    CL_SETTINGS = 3,  /* Badge: [key][value]... (camlink_setting_t) */
    CL_PREVIEW = 4,  /* Badge: [on] the viewfinder on or off */
    CL_FRAME = 5,  /* Camera: [frame id lo][hi][part][parts][data]: a frame of the viewfinder */
    CL_SHOT = 6,  /* Badge: [what: CAMLINK_SHOT_*] take a photo */
    CL_FILE_OPEN = 7,  /* Camera: [kind: CAMLINK_FILE_*][size 4]: a file of the photo comes */
    CL_FILE_DATA = 8,  /* Camera: [offset 4][data] */
    CL_FILE_CLOSE = 9,  /* Camera: [ok] */
    CL_VIDEO = 10,  /* Badge: [on][fps] */
    CL_VFRAME = 11,  /* Camera: as CL_FRAME, a frame of the video (the badge writes it in the .epv) */
    CL_STATUS = 12,  /* Camera: [code: CAMLINK_ST_*][text] */
    CL_WEB = 13,  /* Badge: [on][mode: CAMLINK_WEB_*][name length][name][password length][password][PIN, 6 digits]:
                   * the Wi-Fi (its own access point "name" with "password", or the network saved in the module) and
                   * the page on or off; the PIN is asked by the page on a network (not on the access point) */
    CL_WEB_STATE = 14,  /* Module: [phones][wifi: CAMLINK_WIFI_*][IP 4 bytes][a network saved: 0 / 1][its name] */
    CL_SCREEN = 15,  /* Badge: [screen id lo][hi][part][parts][kind: CAMLINK_SCREEN_*][data]: its screen, in parts */
    CL_KEY = 16,  /* Module: [key] a button of the page (CAMLINK_KEY_*), or [CAMLINK_KEY_TYPE][character] typed */
    CL_OTA_BEGIN = 17,  /* Badge: [size 4]: a new firmware for the module comes (from the SD card of the badge) */
    CL_OTA_DATA = 18,  /* Badge: [offset 4][data]: in order, CAMLINK_FILE_PART bytes at most */
    CL_OTA_END = 19,  /* Badge: the module checks the firmware, answers CL_STATUS OTA_OK (then restarts) or OTA_ERROR */
} camlink_type_t;

typedef enum {  /* The settings of the camera (CL_SETTINGS), each in a byte; the values of the sensor are signed */
    CLS_FRAMESIZE,  /* The size of the photos: CAMLINK_SIZE_* */
    CLS_QUALITY,  /* JPEG quality of the sensor: 4 (best) .. 40 */
    CLS_BRIGHTNESS,  /* -2 .. 2 */
    CLS_CONTRAST,  /* -2 .. 2 */
    CLS_SATURATION,  /* -2 .. 2 */
    CLS_EFFECT,  /* 0 none, 1 negative, 2 grayscale, 3 red, 4 green, 5 blue, 6 sepia */
    CLS_HMIRROR,  /* 0 / 1 */
    CLS_VFLIP,  /* 0 / 1 */
    CLS_FLASH,  /* 0 off, 1 on for the photo, 2 always (a lamp) */
    CLS_DITHER,  /* The viewfinder: CAMLINK_DITHER_* */
    CLS_COUNT
} camlink_setting_t;

enum { CAMLINK_SIZE_QVGA, CAMLINK_SIZE_VGA, CAMLINK_SIZE_SVGA, CAMLINK_SIZE_XGA, CAMLINK_SIZE_SXGA, CAMLINK_SIZE_UXGA,
       CAMLINK_SIZES };
enum { CAMLINK_DITHER_ATKINSON, CAMLINK_DITHER_FLOYD, CAMLINK_DITHER_BAYER, CAMLINK_DITHER_THRESHOLD,
       CAMLINK_DITHERS };
enum { CAMLINK_FILE_JPEG = 1, CAMLINK_FILE_EPI = 2 };  /* The files of a photo: the JPEG, then its .epi */
enum { CAMLINK_SHOT_BOTH = 0 };
enum { CAMLINK_ST_READY = 0, CAMLINK_ST_NO_CAMERA = 1, CAMLINK_ST_ERROR = 2, CAMLINK_ST_OTA_OK = 3,
       CAMLINK_ST_OTA_ERROR = 4 };
enum { CAMLINK_WEB_AP = 0, CAMLINK_WEB_NETWORK = 1 };  /* CL_WEB: its own access point, or the network saved */
/* CL_WEB_STATE: off, its access point, connected to the network, connecting, the network not found (then its own
 * access point) */
enum { CAMLINK_WIFI_OFF = 0, CAMLINK_WIFI_AP = 1, CAMLINK_WIFI_NETWORK = 2, CAMLINK_WIFI_CONNECTING = 3,
       CAMLINK_WIFI_FAILED = 4 };
/* The screen in CL_SCREEN: black and white (1 plane: 5 parts), 4 grays (the lsb plane then the msb one: 10 parts,
 * gray = msb * 2 + lsb, 0 = black), all white or all black (1 part, no data) */
enum { CAMLINK_SCREEN_BW = 0, CAMLINK_SCREEN_4G = 1, CAMLINK_SCREEN_WHITE = 2, CAMLINK_SCREEN_BLACK = 3 };
/* The keys of CL_KEY: the buttons as on the USB serial port of the badge (lower case: a press, upper case: a long
 * press), and the text typed for its text editors */
#define CAMLINK_KEY_TYPE 0x02  /* Then the character (ASCII, or 0x80 + n: the accented letter n of the editor) */

uint16_t camlink_crc16(const uint8_t *data, size_t len);

/** \brief Writes a packet without a buffer for it: its COBS frame (final 0 included) given to \p out in pieces of at
 * most 255 bytes. Its payload is \p head (\p head_len bytes) then \p body (\p body_len bytes, may be NULL).
 * \return the number of bytes written, 0 when the payload is too long */
size_t camlink_write(uint8_t type, uint8_t seq, const uint8_t *head, size_t head_len, const uint8_t *body,
                     size_t body_len, void (*out)(const uint8_t *data, size_t len, void *ctx), void *ctx);

/** \brief A packet COBS encoded, its final 0 included, into \p out (CAMLINK_ENCODED_MAX bytes).
 * \return the number of bytes, 0 when the payload is too long */
size_t camlink_encode(uint8_t type, uint8_t seq, const uint8_t *payload, size_t len, uint8_t *out);

/* The receiver: the bytes of the UART one by one, into a buffer of the caller (decoded in place) */
typedef struct {
    uint8_t *buf;  /* The bytes of the frame being received; CAMLINK_ENCODED_MAX takes any packet, a smaller one only
                    * the short packets (the longer ones are counted in bad) */
    size_t cap;
    size_t n;
    bool overflow;
    uint32_t bad;  /* Frames dropped: a wrong CRC, a wrong length, too long */
    /* The packet received (valid until the next call of camlink_feed()) */
    uint8_t type, seq;
    const uint8_t *payload;  /* In buf */
    size_t len;
} camlink_rx_t;

void camlink_rx_init(camlink_rx_t *rx, uint8_t *buf, size_t cap);

/** \brief A byte received. \return true when a packet is complete (rx->type, seq, payload, len) */
bool camlink_feed(camlink_rx_t *rx, uint8_t byte);

#endif /* _CAMLINK_H */
