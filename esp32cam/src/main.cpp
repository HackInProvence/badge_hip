/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The camera module of the badge: an ESP32-CAM (AI-Thinker) on the port J2 of the badge, its UART0 (GPIO1, GPIO3) at
 * CAMLINK_BAUD. The badge drives it (src/shared/camlink/camlink.h):
 * - the viewfinder: JPEG 320 x 240, decoded to grays, the center square at 200 x 200, dithered to 1 bit, sent in
 *   5 parts of 1000 bytes (CL_FRAME), each one acknowledged by the badge;
 * - a photo: the JPEG at the size of the settings (the flash if asked), sent as a file, then its .epi (4 grays);
 * - a video: frames of 1 bit (CL_VFRAME) at the rate asked, written by the badge in an .epv;
 * - the settings of the sensor (CL_SETTINGS). */

#include <Arduino.h>

#include "esp_camera.h"
#include "img_converters.h"

extern "C" {
#include "camlink.h"
#include "campix.h"
}

#define FIRMWARE "esp32cam 0.1"
#define FLASH_PIN 4  /* The white LED of the ESP32-CAM */

/* The pins of the camera of the AI-Thinker ESP32-CAM */
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

static const framesize_t SIZES[CAMLINK_SIZES] = {FRAMESIZE_QVGA, FRAMESIZE_VGA, FRAMESIZE_SVGA, FRAMESIZE_XGA,
                                                FRAMESIZE_SXGA, FRAMESIZE_UXGA};
#define PREVIEW_W 320
#define PREVIEW_H 240

static camlink_rx_t rx;
static uint8_t out_seq = 0;
static uint8_t frame_buf[CAMLINK_ENCODED_MAX];
static bool camera_ok = false;
static int8_t settings[CLS_COUNT] = {CAMLINK_SIZE_SVGA, 10, 0, 0, 0, 0, 0, 0, 0, CAMLINK_DITHER_ATKINSON};

/* What the badge asked (set while reading the link, done in loop()) */
static bool preview = false, video = false, shot_asked = false, settings_changed = true;
static uint8_t video_fps = 4;
static uint16_t frame_id = 0;

/* The pictures, in the PSRAM */
static uint8_t *rgb, *gray_qvga, *gray, *bits, *epi;
static int16_t work[2 * (CAMPIX_SIZE + 2)];

static void send_packet(uint8_t type, uint8_t seq, const uint8_t *payload, size_t len) {
    size_t n = camlink_encode(type, seq, payload, len, frame_buf);
    Serial.write(frame_buf, n);
}

/* A command of the badge: acknowledged at once, done in loop() */
static void command(void) {
    send_packet(CL_ACK, 0, &rx.seq, 1);
    switch (rx.type) {
    case CL_HELLO: {
        uint8_t hello[40] = {CAMLINK_PROTO};
        size_t n = strlen(FIRMWARE);
        memcpy(hello + 1, FIRMWARE, n);
        send_packet(CL_HELLO, 0, hello, n + 1);
        uint8_t st[] = {(uint8_t)(camera_ok ? CAMLINK_ST_READY : CAMLINK_ST_NO_CAMERA)};
        send_packet(CL_STATUS, 0, st, 1);
        break;
    }
    case CL_SETTINGS:
        for (size_t i = 0; i + 1 < rx.len; i += 2)
            if (rx.payload[i] < CLS_COUNT)
                settings[rx.payload[i]] = (int8_t)rx.payload[i + 1];
        settings_changed = true;
        break;
    case CL_PREVIEW:
        preview = rx.len && rx.payload[0];
        break;
    case CL_SHOT:
        shot_asked = true;
        break;
    case CL_VIDEO:
        video = rx.len && rx.payload[0];
        if (rx.len > 1 && rx.payload[1])
            video_fps = rx.payload[1];
        break;
    default:
        break;
    }
}

/* Reads the link: the commands are done; returns true when the ACK of \p seq came (wait < 0: no wait) */
static bool poll_link(int ack_seq) {
    bool acked = false;
    while (Serial.available()) {
        if (! camlink_feed(&rx, (uint8_t)Serial.read()))
            continue;
        if (rx.type == CL_ACK) {
            if (ack_seq >= 0 && rx.len && rx.payload[0] == (uint8_t)ack_seq)
                acked = true;
        } else {
            command();
        }
    }
    return acked;
}

/* A packet the badge must acknowledge: sent again until it does (false: given up) */
static bool send_reliable(uint8_t type, const uint8_t *payload, size_t len) {
    uint8_t seq = ++out_seq;
    for (int tries = 0; tries < 20; ++tries) {
        send_packet(type, seq, payload, len);
        uint32_t t0 = millis();
        while (millis() - t0 < CAMLINK_ACK_MS) {
            if (poll_link(seq))
                return true;
            delay(1);
        }
    }
    return false;
}

static void apply_settings(void) {
    sensor_t *s = esp_camera_sensor_get();
    if (! s)
        return;
    s->set_quality(s, settings[CLS_QUALITY] < 4 ? 4 : settings[CLS_QUALITY]);
    s->set_brightness(s, settings[CLS_BRIGHTNESS]);
    s->set_contrast(s, settings[CLS_CONTRAST]);
    s->set_saturation(s, settings[CLS_SATURATION]);
    s->set_special_effect(s, settings[CLS_EFFECT]);
    s->set_hmirror(s, settings[CLS_HMIRROR]);
    s->set_vflip(s, settings[CLS_VFLIP]);
    digitalWrite(FLASH_PIN, settings[CLS_FLASH] == 2 ? HIGH : LOW);
    settings_changed = false;
}

static bool camera_init(void) {
    camera_config_t c = {};
    c.ledc_channel = LEDC_CHANNEL_0;
    c.ledc_timer = LEDC_TIMER_0;
    c.pin_d0 = Y2_GPIO_NUM;
    c.pin_d1 = Y3_GPIO_NUM;
    c.pin_d2 = Y4_GPIO_NUM;
    c.pin_d3 = Y5_GPIO_NUM;
    c.pin_d4 = Y6_GPIO_NUM;
    c.pin_d5 = Y7_GPIO_NUM;
    c.pin_d6 = Y8_GPIO_NUM;
    c.pin_d7 = Y9_GPIO_NUM;
    c.pin_xclk = XCLK_GPIO_NUM;
    c.pin_pclk = PCLK_GPIO_NUM;
    c.pin_vsync = VSYNC_GPIO_NUM;
    c.pin_href = HREF_GPIO_NUM;
    c.pin_sccb_sda = SIOD_GPIO_NUM;
    c.pin_sccb_scl = SIOC_GPIO_NUM;
    c.pin_pwdn = PWDN_GPIO_NUM;
    c.pin_reset = RESET_GPIO_NUM;
    c.xclk_freq_hz = 20000000;
    c.pixel_format = PIXFORMAT_JPEG;
    c.frame_size = FRAMESIZE_UXGA;  /* The biggest one allocates the buffers; the viewfinder is set to QVGA below */
    c.jpeg_quality = 10;
    c.fb_count = 2;
    c.fb_location = CAMERA_FB_IN_PSRAM;
    c.grab_mode = CAMERA_GRAB_LATEST;
    if (esp_camera_init(&c) != ESP_OK)
        return false;
    sensor_t *s = esp_camera_sensor_get();
    s->set_framesize(s, FRAMESIZE_QVGA);
    return true;
}

/* A JPEG of the viewfinder decoded to the 200 x 200 grays */
static bool grab_gray(void) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (! fb)
        return false;
    bool ok = fb->width == PREVIEW_W && fb->height == PREVIEW_H &&
              fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, rgb);
    esp_camera_fb_return(fb);
    if (! ok)
        return false;
    for (int i = 0; i < PREVIEW_W * PREVIEW_H; ++i)  /* Luma of the RGB888 (stored as BGR) */
        gray_qvga[i] = (uint8_t)((rgb[3 * i + 2] * 77 + rgb[3 * i + 1] * 150 + rgb[3 * i] * 29) >> 8);
    campix_square(gray_qvga, PREVIEW_W, PREVIEW_H, gray);
    campix_stretch(gray);
    return true;
}

/* A frame of 1 bit to the badge, in parts (CL_FRAME or CL_VFRAME) */
static bool send_frame(uint8_t type, int dither) {
    campix_dither1(gray, dither, bits, work);
    ++frame_id;
    uint8_t part[4 + CAMLINK_PART];
    const int parts = CAMLINK_FRAME_BYTES / CAMLINK_PART;
    for (int p = 0; p < parts; ++p) {
        part[0] = (uint8_t)frame_id;
        part[1] = (uint8_t)(frame_id >> 8);
        part[2] = (uint8_t)p;
        part[3] = (uint8_t)parts;
        memcpy(part + 4, bits + p * CAMLINK_PART, CAMLINK_PART);
        if (! send_reliable(type, part, sizeof(part)))
            return false;
    }
    return true;
}

static bool send_file(uint8_t kind, const uint8_t *data, size_t size) {
    uint8_t head[5] = {kind, (uint8_t)size, (uint8_t)(size >> 8), (uint8_t)(size >> 16), (uint8_t)(size >> 24)};
    if (! send_reliable(CL_FILE_OPEN, head, sizeof(head)))
        return false;
    uint8_t part[4 + CAMLINK_FILE_PART];
    for (size_t off = 0; off < size; off += CAMLINK_FILE_PART) {
        size_t n = size - off < CAMLINK_FILE_PART ? size - off : CAMLINK_FILE_PART;
        part[0] = (uint8_t)off;
        part[1] = (uint8_t)(off >> 8);
        part[2] = (uint8_t)(off >> 16);
        part[3] = (uint8_t)(off >> 24);
        memcpy(part + 4, data + off, n);
        if (! send_reliable(CL_FILE_DATA, part, 4 + n))
            return false;
    }
    uint8_t ok = 1;
    return send_reliable(CL_FILE_CLOSE, &ok, 1);
}

/* A photo: the JPEG at the size of the settings, then its .epi from the viewfinder */
static void take_photo(void) {
    sensor_t *s = esp_camera_sensor_get();
    int size = settings[CLS_FRAMESIZE] < CAMLINK_SIZES ? settings[CLS_FRAMESIZE] : CAMLINK_SIZE_SVGA;
    s->set_framesize(s, SIZES[size]);
    if (settings[CLS_FLASH] >= 1)
        digitalWrite(FLASH_PIN, HIGH);
    for (int i = 0; i < 3; ++i) {  /* The exposure settles on the new size: a few frames dropped */
        camera_fb_t *f = esp_camera_fb_get();
        if (f)
            esp_camera_fb_return(f);
    }
    camera_fb_t *fb = esp_camera_fb_get();
    if (settings[CLS_FLASH] != 2)
        digitalWrite(FLASH_PIN, LOW);
    if (fb) {
        send_file(CAMLINK_FILE_JPEG, fb->buf, fb->len);
        esp_camera_fb_return(fb);
    }
    s->set_framesize(s, FRAMESIZE_QVGA);
    for (int i = 0; i < 2; ++i) {
        camera_fb_t *f = esp_camera_fb_get();
        if (f)
            esp_camera_fb_return(f);
    }
    if (grab_gray()) {
        campix_epi(gray, epi, work);
        send_file(CAMLINK_FILE_EPI, epi, CAMPIX_EPI_BYTES);
    }
}

void setup() {
    Serial.setRxBufferSize(4096);
    Serial.begin(CAMLINK_BAUD);
    pinMode(FLASH_PIN, OUTPUT);
    digitalWrite(FLASH_PIN, LOW);
    camlink_rx_init(&rx);
    rgb = (uint8_t *)ps_malloc(PREVIEW_W * PREVIEW_H * 3);
    gray_qvga = (uint8_t *)ps_malloc(PREVIEW_W * PREVIEW_H);
    gray = (uint8_t *)ps_malloc(CAMPIX_SIZE * CAMPIX_SIZE);
    bits = (uint8_t *)ps_malloc(CAMLINK_FRAME_BYTES);
    epi = (uint8_t *)ps_malloc(CAMPIX_EPI_BYTES);
    camera_ok = rgb && gray_qvga && gray && bits && epi && camera_init();
    uint8_t hello[40] = {CAMLINK_PROTO};
    memcpy(hello + 1, FIRMWARE, strlen(FIRMWARE));
    send_packet(CL_HELLO, 0, hello, strlen(FIRMWARE) + 1);
}

void loop() {
    static uint32_t video_ts = 0;
    poll_link(-1);
    if (! camera_ok) {
        delay(5);
        return;
    }
    if (settings_changed)
        apply_settings();
    if (shot_asked) {
        shot_asked = false;
        take_photo();
        return;
    }
    if (video) {
        uint32_t period = 1000 / (video_fps ? video_fps : 1);
        if (millis() - video_ts >= period) {
            video_ts = millis();
            if (grab_gray())
                send_frame(CL_VFRAME, CAMLINK_DITHER_BAYER);  /* Stable from a frame to the next */
        }
        return;
    }
    if (preview && grab_gray())
        send_frame(CL_FRAME, settings[CLS_DITHER]);  /* As fast as the badge takes them (its screen) */
    else
        delay(5);
}
