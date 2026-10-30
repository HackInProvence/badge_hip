/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The ESP32-CAM module of the badge (AI-Thinker), on an extension port of the badge: its UART0 (GPIO1, GPIO3) at
 * CAMLINK_BAUD. The badge drives it (src/shared/camlink/camlink.h):
 * - the camera (Badge > Appareil photo): the viewfinder (JPEG 320 x 240 decoded to grays, the center square at
 *   200 x 200, dithered to 1 bit, 5 parts of 1000 bytes, CL_FRAME), a photo (the JPEG at the size of the settings,
 *   then its .epi in 4 grays), a video (CL_VFRAME), the settings of the sensor;
 * - the remote control (Badge > Télécommande): a Wi-Fi access point (a captive portal: the page opens by itself) or
 *   the network saved here; a web server (its own task) with the page of the badge (its screen, sent by the badge in
 *   CL_SCREEN, its buttons and a text field, sent to it in CL_KEY), the settings of the Wi-Fi, and on a network the
 *   page of the admins with the screens of every badge found (each module says it is there by UDP);
 * - its own update: the firmware sent by the badge from its SD card (CL_OTA_*), in the other OTA partition. */

#include <Arduino.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "esp_camera.h"
#include "esp_http_server.h"
#include "lwip/sockets.h"
#include "img_converters.h"

extern "C" {
#include "camlink.h"
#include "campix.h"
}

#include "web_pages.h"

#define FIRMWARE "esp32cam 0.2"
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

#define CONNECT_MS 15000  /* The network not joined in this time: the access point instead */
#define STATE_MS 3000  /* The state of the Wi-Fi told to the badge (also: the badge knows the module is there) */
#define VIEWER_MS 3000  /* A page that asked the screen in this time looks at it */
#define VIEWERS 8
#define PEERS 32
#define PEER_PORT 4210  /* The modules of a network say they are there (UDP broadcast) */
#define PEER_MS 2000
#define PEER_LOST_MS 10000
#define SCREEN_MAX (2 * CAMLINK_FRAME_BYTES)

static camlink_rx_t rx;
static uint8_t rx_buf[CAMLINK_ENCODED_MAX];
static uint8_t out_seq = 0;
static bool camera_ok = false;
static int8_t settings[CLS_COUNT] = {CAMLINK_SIZE_SVGA, 10, 0, 0, 0, 0, 0, 0, 0, CAMLINK_DITHER_ATKINSON};

/* What the badge asked (set while reading the link, done in loop()) */
static bool preview = false, video = false, shot_asked = false, settings_changed = true;
static uint8_t video_fps = 4;
static uint16_t frame_id = 0;

/* The pictures, in the PSRAM */
static uint8_t *rgb, *gray_qvga, *gray, *bits, *epi;
static int16_t work[2 * (CAMPIX_SIZE + 2)];

/* ------ The remote control: its state ------ */

static SemaphoreHandle_t lock;  /* The screen, the viewers, the peers: shared with the tasks of the web and UDP */
static QueueHandle_t keys;  /* The keys of the page for the badge: [key] or CAMLINK_KEY_TYPE << 8 | character */
static bool web_asked = false, web_dirty = false;  /* CL_WEB: what the badge wants, applied in loop() */
static uint8_t web_mode = CAMLINK_WEB_AP;
static char ap_name[CAMLINK_WEB_TEXT + 1] = "Cigale", ap_pass[CAMLINK_WEB_TEXT + 1] = "", pin[7] = "";
static int wifi = CAMLINK_WIFI_OFF;
static uint32_t connect_ts = 0;
static String net_ssid, net_pass, admin_key;  /* Saved in the flash (Preferences "webremote") */
static Preferences prefs;
static httpd_handle_t server = NULL;
static DNSServer dns;
static bool dns_on = false;
static WiFiUDP udp;
/* The screen: built from the parts of the badge, then given to the pages */
static uint8_t *screen_build, *screen;
static uint8_t screen_kind = CAMLINK_SCREEN_WHITE, build_kind = 0;
static uint16_t screen_id = 0, build_id = 0xFFFF;
static uint32_t build_parts = 0;
static size_t screen_len = 0;
static struct {
    uint32_t ip, ts;
} viewers[VIEWERS];
static struct {
    uint32_t ip, ts;
    char name[CAMLINK_WEB_TEXT + 1];
} peers[PEERS];
/* The update of this firmware */
static bool ota_on = false;
static uint32_t ota_next = 0;
static bool ota_end_asked = false;

static void send_packet(uint8_t type, uint8_t seq, const uint8_t *payload, size_t len) {
    static uint8_t frame_buf[CAMLINK_ENCODED_MAX];
    size_t n = camlink_encode(type, seq, payload, len, frame_buf);
    Serial.write(frame_buf, n);
}

static void web_command(void) {
    /* [on][mode][name length][name][password length][password][PIN 6] */
    const uint8_t *p = rx.payload;
    size_t len = rx.len;
    if (len < 3)
        return;
    web_asked = p[0] != 0;
    web_mode = p[1];
    size_t i = 2, n = p[i++];
    if (i + n > len || n > CAMLINK_WEB_TEXT)
        return;
    memcpy(ap_name, p + i, n);
    ap_name[n] = 0;
    i += n;
    if (i >= len)
        return;
    n = p[i++];
    if (i + n > len || n > CAMLINK_WEB_TEXT)
        return;
    memcpy(ap_pass, p + i, n);
    ap_pass[n] = 0;
    i += n;
    if (i + 6 <= len) {
        memcpy(pin, p + i, 6);
        pin[6] = 0;
    }
    web_dirty = true;
}

static void screen_part(void) {
    /* [id lo][id hi][part][parts][kind][data] */
    if (rx.len < 5)
        return;
    uint16_t id = rx.payload[0] | rx.payload[1] << 8;
    int part = rx.payload[2], parts = rx.payload[3];
    if (parts < 1 || parts > 10 || part >= parts)
        return;
    if (id != build_id) {
        build_id = id;
        build_parts = 0;
        build_kind = rx.payload[4];
    }
    size_t n = rx.len - 5;
    if (n && (size_t)part * CAMLINK_PART + n <= SCREEN_MAX)
        memcpy(screen_build + part * CAMLINK_PART, rx.payload + 5, n);
    build_parts |= 1u << part;
    if (build_parts == (1u << parts) - 1) {  /* Complete: given to the pages */
        xSemaphoreTake(lock, portMAX_DELAY);
        screen_kind = build_kind;
        screen_len = build_kind == CAMLINK_SCREEN_4G ? 2 * CAMLINK_FRAME_BYTES
                     : build_kind == CAMLINK_SCREEN_BW ? CAMLINK_FRAME_BYTES : 0;
        memcpy(screen, screen_build, screen_len);
        ++screen_id;
        xSemaphoreGive(lock);
        build_parts = 0;
    }
}

static void ota_status(uint8_t code, const char *text) {
    uint8_t st[64] = {code};
    size_t n = strlen(text) < sizeof(st) - 1 ? strlen(text) : sizeof(st) - 1;
    memcpy(st + 1, text, n);
    send_packet(CL_STATUS, 0, st, n + 1);
}

/* A command of the badge: acknowledged at once, done here or in loop() */
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
    case CL_WEB:
        web_command();
        break;
    case CL_SCREEN:
        screen_part();
        break;
    case CL_OTA_BEGIN:
        if (rx.len >= 4) {
            uint32_t size = rx.payload[0] | rx.payload[1] << 8 | (uint32_t)rx.payload[2] << 16 |
                            (uint32_t)rx.payload[3] << 24;
            preview = video = false;
            if (ota_on)
                Update.abort();
            ota_on = Update.begin(size);
            ota_next = 0;
            if (! ota_on)
                ota_status(CAMLINK_ST_OTA_ERROR, Update.errorString());
        }
        break;
    case CL_OTA_DATA:
        if (ota_on && rx.len > 4) {
            uint32_t off = rx.payload[0] | rx.payload[1] << 8 | (uint32_t)rx.payload[2] << 16 |
                           (uint32_t)rx.payload[3] << 24;
            if (off == ota_next) {  /* In order; a part sent again (its ACK lost) is already written */
                size_t n = rx.len - 4;
                if (Update.write((uint8_t *)rx.payload + 4, n) != n) {
                    ota_status(CAMLINK_ST_OTA_ERROR, Update.errorString());
                    Update.abort();
                    ota_on = false;
                }
                ota_next += n;
            } else if (off > ota_next) {
                ota_status(CAMLINK_ST_OTA_ERROR, "missing data");
                Update.abort();
                ota_on = false;
            }
        }
        break;
    case CL_OTA_END:
        if (ota_on)
            ota_end_asked = true;  /* In loop(): the check takes a while */
        break;
    default:
        break;
    }
}

/* Reads the link: the commands are done; returns true when the ACK of \p seq came (ack_seq < 0: no wait) */
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

/* ------ The camera ------ */

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

/* ------ The web server (its own task) ------ */

static uint32_t client_ip(httpd_req_t *req) {
    int fd = httpd_req_to_sockfd(req);
    struct sockaddr_in6 addr;
    socklen_t len = sizeof(addr);
    if (getpeername(fd, (struct sockaddr *)&addr, &len) < 0)
        return 0;
    return addr.sin6_addr.un.u32_addr[3];  /* IPv4 mapped in IPv6 */
}

static String query(httpd_req_t *req, const char *name) {
    char q[200], v[160];
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) != ESP_OK)
        return "";
    if (httpd_query_key_value(q, name, v, sizeof(v)) != ESP_OK)
        return "";
    /* %xx and + decoded */
    String out;
    for (char *p = v; *p; ++p) {
        if (*p == '%' && p[1] && p[2]) {
            char h[3] = {p[1], p[2], 0};
            out += (char)strtol(h, NULL, 16);
            p += 2;
        } else {
            out += *p == '+' ? ' ' : *p;
        }
    }
    return out;
}

/* On its own access point (WPA2, its password shown by the badge) the phone is trusted; on a network, the PIN of the
 * badge (to control it) or the admin key (to look only) */
static bool allowed(httpd_req_t *req, bool control) {
    if (wifi != CAMLINK_WIFI_NETWORK)
        return true;
    if (pin[0] && query(req, "pin") == pin)
        return true;
    return ! control && admin_key.length() && query(req, "key") == admin_key;
}

static esp_err_t deny(httpd_req_t *req) {
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_sendstr(req, "PIN");
}

static esp_err_t page_remote(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, PAGE_REMOTE, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t page_admin(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, PAGE_ADMIN, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t get_screen(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");  /* The admin page of another module reads it */
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    if (! allowed(req, false))
        return deny(req);
    uint32_t ip = client_ip(req), now = millis();
    String since = query(req, "since");
    xSemaphoreTake(lock, portMAX_DELAY);
    int k = -1;  /* The pages that look at the screen: the badge sends it only to them */
    for (int i = 0; i < VIEWERS; ++i)
        if (viewers[i].ip == ip || (k < 0 && now - viewers[i].ts > VIEWER_MS))
            k = i;
    if (k >= 0) {
        viewers[k].ip = ip;
        viewers[k].ts = now;
    }
    bool same = since.length() && since.toInt() == screen_id;
    if (same) {
        xSemaphoreGive(lock);
        httpd_resp_set_status(req, "204 No Content");
        return httpd_resp_send(req, NULL, 0);
    }
    static uint8_t out[3 + SCREEN_MAX];
    out[0] = screen_kind;
    out[1] = (uint8_t)screen_id;
    out[2] = (uint8_t)(screen_id >> 8);
    size_t n = screen_len;
    memcpy(out + 3, screen, n);
    xSemaphoreGive(lock);
    httpd_resp_set_type(req, "application/octet-stream");
    return httpd_resp_send(req, (const char *)out, 3 + n);
}

static void push_key(uint16_t k) {
    xQueueSend(keys, &k, 0);
}

static esp_err_t get_key(httpd_req_t *req) {
    if (! allowed(req, true))
        return deny(req);
    String k = query(req, "k");
    if (k.length() == 1 && strchr("aAbBxXyY", k[0]))
        push_key((uint8_t)k[0]);
    return httpd_resp_sendstr(req, "ok");
}

static esp_err_t get_type(httpd_req_t *req) {
    if (! allowed(req, true))
        return deny(req);
    String t = query(req, "t");
    for (size_t i = 0; i < t.length() && i < 64; ++i) {
        uint8_t c = (uint8_t)t[i];
        if (c == '\b' || c == '\r' || (c >= 0x20 && c < 0x7F))  /* ASCII: the editors of the badge */
            push_key(CAMLINK_KEY_TYPE << 8 | c);
    }
    return httpd_resp_sendstr(req, "ok");
}

static esp_err_t get_info(httpd_req_t *req) {
    String j = "{\"name\":\"" + String(ap_name) + "\",\"ssid\":\"";
    if (allowed(req, true))
        j += net_ssid;
    j += "\"}";
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, j.c_str());
}

static esp_err_t post_config(httpd_req_t *req) {
    if (! allowed(req, true))
        return deny(req);
    String ssid = query(req, "ssid"), pass = query(req, "pass"), admin = query(req, "admin");
    xSemaphoreTake(lock, portMAX_DELAY);  /* Read by loop() (the state for the badge, the Wi-Fi) */
    prefs.putString("ssid", ssid);
    net_ssid = ssid;
    if (pass.length()) {
        prefs.putString("pass", pass);
        net_pass = pass;
    }
    if (admin.length()) {
        prefs.putString("admin", admin);
        admin_key = admin;
    }
    xSemaphoreGive(lock);  /* state_task() tells the badge that a network is saved */
    return httpd_resp_sendstr(req, ssid.length() ? "Enregistré. Sur le badge : Wi-Fi : réseau."
                                                 : "Réseau oublié : point d'accès.");
}

static esp_err_t get_peers(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    if (! admin_key.length() || query(req, "key") != admin_key)
        return deny(req);
    String j = "[{\"name\":\"" + String(ap_name) + "\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
    uint32_t now = millis();
    xSemaphoreTake(lock, portMAX_DELAY);
    for (int i = 0; i < PEERS; ++i)
        if (peers[i].ip && now - peers[i].ts < PEER_LOST_MS)
            j += ",{\"name\":\"" + String(peers[i].name) + "\",\"ip\":\"" + IPAddress(peers[i].ip).toString() + "\"}";
    xSemaphoreGive(lock);
    j += "]";
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, j.c_str());
}

/* The captive portal: on the access point, any other address goes to the page (the phone opens it by itself) */
static esp_err_t get_other(httpd_req_t *req) {
    if (wifi == CAMLINK_WIFI_NETWORK) {
        httpd_resp_send_404(req);
        return ESP_OK;
    }
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    return httpd_resp_send(req, NULL, 0);
}

static void server_start(void) {
    if (server)
        return;
    httpd_config_t c = HTTPD_DEFAULT_CONFIG();
    c.max_uri_handlers = 12;
    c.uri_match_fn = httpd_uri_match_wildcard;
    c.lru_purge_enable = true;
    c.stack_size = 8192;
    if (httpd_start(&server, &c) != ESP_OK) {
        server = NULL;
        return;
    }
    static const httpd_uri_t URIS[] = {
        {"/", HTTP_GET, page_remote, NULL},
        {"/admin", HTTP_GET, page_admin, NULL},
        {"/screen", HTTP_GET, get_screen, NULL},
        {"/key", HTTP_GET, get_key, NULL},
        {"/type", HTTP_GET, get_type, NULL},
        {"/info", HTTP_GET, get_info, NULL},
        {"/config", HTTP_POST, post_config, NULL},
        {"/peers.json", HTTP_GET, get_peers, NULL},
        {"/*", HTTP_GET, get_other, NULL},  /* Last: the captive portal */
    };
    for (const httpd_uri_t &u : URIS)
        httpd_register_uri_handler(server, &u);
}

/* ------ The Wi-Fi: applied in loop() ------ */

static void wifi_stop(void) {
    if (dns_on) {
        dns.stop();
        dns_on = false;
    }
    MDNS.end();
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    wifi = CAMLINK_WIFI_OFF;
}

static void ap_start(bool failed) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_name, strlen(ap_pass) >= 8 ? ap_pass : NULL, 6, 0, 4);
    dns.start(53, "*", WiFi.softAPIP());  /* Every name: this module (the captive portal) */
    dns_on = true;
    wifi = failed ? CAMLINK_WIFI_FAILED : CAMLINK_WIFI_AP;
    server_start();
}

static void web_apply(void) {
    web_dirty = false;
    if (! web_asked) {
        if (wifi != CAMLINK_WIFI_OFF)
            wifi_stop();
        return;
    }
    wifi_stop();
    xSemaphoreTake(lock, portMAX_DELAY);
    String ssid = net_ssid, pass = net_pass;
    xSemaphoreGive(lock);
    if (web_mode == CAMLINK_WEB_NETWORK && ssid.length()) {
        WiFi.mode(WIFI_STA);
        WiFi.setHostname(ap_name);
        WiFi.begin(ssid.c_str(), pass.c_str());
        wifi = CAMLINK_WIFI_CONNECTING;
        connect_ts = millis();
    } else {
        ap_start(web_mode == CAMLINK_WEB_NETWORK);  /* No network saved: the access point */
    }
}

static void wifi_task(void) {
    if (wifi == CAMLINK_WIFI_CONNECTING) {
        if (WiFi.status() == WL_CONNECTED) {
            wifi = CAMLINK_WIFI_NETWORK;
            String host = ap_name;
            host.toLowerCase();
            MDNS.begin(host.c_str());  /* http://cigale-<name>.local */
            MDNS.addService("http", "tcp", 80);
            udp.begin(PEER_PORT);
            server_start();
        } else if (millis() - connect_ts > CONNECT_MS) {
            WiFi.disconnect(true);
            ap_start(true);  /* The network not found: the access point, so that the phone can fix it */
        }
    } else if (wifi == CAMLINK_WIFI_NETWORK && WiFi.status() != WL_CONNECTED) {
        wifi = CAMLINK_WIFI_CONNECTING;  /* Lost: it reconnects by itself */
        connect_ts = millis();
    }
}

/* The DNS of the captive portal, the modules of the network: a task of their own (loop() waits for the badge) */
static void net_task(void *arg) {
    (void)arg;
    uint32_t said = 0;
    for (;;) {
        if (dns_on)
            dns.processNextRequest();
        if (wifi == CAMLINK_WIFI_NETWORK) {
            if (millis() - said > PEER_MS) {
                said = millis();
                String m = String("CIGALE1 ") + ap_name;
                udp.beginPacket(IPAddress(255, 255, 255, 255), PEER_PORT);
                udp.write((const uint8_t *)m.c_str(), m.length());
                udp.endPacket();
            }
            int n = udp.parsePacket();
            if (n > 8 && n < 64) {
                char m[64];
                n = udp.read(m, sizeof(m) - 1);
                m[n < 0 ? 0 : n] = 0;
                uint32_t ip = (uint32_t)udp.remoteIP();
                if (! strncmp(m, "CIGALE1 ", 8) && ip != (uint32_t)WiFi.localIP()) {
                    xSemaphoreTake(lock, portMAX_DELAY);
                    int k = -1;
                    for (int i = 0; i < PEERS; ++i)
                        if (peers[i].ip == ip || (k < 0 && millis() - peers[i].ts > PEER_LOST_MS))
                            k = i;
                    if (k >= 0) {
                        peers[k].ip = ip;
                        peers[k].ts = millis();
                        snprintf(peers[k].name, sizeof(peers[k].name), "%s", m + 8);
                    }
                    xSemaphoreGive(lock);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static int count_viewers(void) {
    uint32_t now = millis();
    int n = 0;
    xSemaphoreTake(lock, portMAX_DELAY);
    for (int i = 0; i < VIEWERS; ++i)
        if (viewers[i].ip && now - viewers[i].ts < VIEWER_MS)
            ++n;
    xSemaphoreGive(lock);
    return n;
}

/* The state of the Wi-Fi to the badge: when it changes, and every STATE_MS while on */
static void state_task(void) {
    static uint32_t ts = 0;
    static uint8_t last[8 + CAMLINK_WEB_TEXT];
    static size_t last_n = 0;
    uint8_t s[8 + CAMLINK_WEB_TEXT];
    IPAddress ip = wifi == CAMLINK_WIFI_NETWORK ? WiFi.localIP() : WiFi.softAPIP();
    size_t n = 0;
    s[n++] = (uint8_t)count_viewers();
    s[n++] = (uint8_t)wifi;
    for (int i = 0; i < 4; ++i)
        s[n++] = wifi == CAMLINK_WIFI_OFF ? 0 : ip[i];
    xSemaphoreTake(lock, portMAX_DELAY);
    String ssid = net_ssid;
    xSemaphoreGive(lock);
    s[n++] = ssid.length() ? 1 : 0;
    size_t l = ssid.length() < CAMLINK_WEB_TEXT ? ssid.length() : CAMLINK_WEB_TEXT;
    memcpy(s + n, ssid.c_str(), l);
    n += l;
    bool changed = n != last_n || memcmp(s, last, n);
    if (! changed && (wifi == CAMLINK_WIFI_OFF || millis() - ts < STATE_MS))
        return;
    if (send_reliable(CL_WEB_STATE, s, n)) {
        memcpy(last, s, n);
        last_n = n;
        ts = millis();
    }
}

/* The keys of the pages to the badge */
static void keys_task(void) {
    uint16_t k;
    while (xQueueReceive(keys, &k, 0) == pdTRUE) {
        uint8_t d[2];
        size_t n = 0;
        if (k >> 8)
            d[n++] = (uint8_t)(k >> 8);
        d[n++] = (uint8_t)k;
        send_reliable(CL_KEY, d, n);
    }
}

void setup() {
    Serial.setRxBufferSize(4096);
    Serial.begin(CAMLINK_BAUD);
    pinMode(FLASH_PIN, OUTPUT);
    digitalWrite(FLASH_PIN, LOW);
    camlink_rx_init(&rx, rx_buf, sizeof(rx_buf));
    lock = xSemaphoreCreateMutex();
    keys = xQueueCreate(64, sizeof(uint16_t));
    prefs.begin("webremote", false);
    net_ssid = prefs.getString("ssid", "");
    net_pass = prefs.getString("pass", "");
    admin_key = prefs.getString("admin", "");
    WiFi.mode(WIFI_OFF);  /* Until the badge asks for it */
    rgb = (uint8_t *)ps_malloc(PREVIEW_W * PREVIEW_H * 3);
    gray_qvga = (uint8_t *)ps_malloc(PREVIEW_W * PREVIEW_H);
    gray = (uint8_t *)ps_malloc(CAMPIX_SIZE * CAMPIX_SIZE);
    bits = (uint8_t *)ps_malloc(CAMLINK_FRAME_BYTES);
    epi = (uint8_t *)ps_malloc(CAMPIX_EPI_BYTES);
    screen = (uint8_t *)ps_malloc(SCREEN_MAX);
    screen_build = (uint8_t *)ps_malloc(SCREEN_MAX);
    camera_ok = rgb && gray_qvga && gray && bits && epi && camera_init();
    xTaskCreatePinnedToCore(net_task, "net", 4096, NULL, 1, NULL, 0);
    uint8_t hello[40] = {CAMLINK_PROTO};
    memcpy(hello + 1, FIRMWARE, strlen(FIRMWARE));
    send_packet(CL_HELLO, 0, hello, strlen(FIRMWARE) + 1);
}

void loop() {
    static uint32_t video_ts = 0;
    poll_link(-1);
    if (ota_end_asked) {
        ota_end_asked = false;
        ota_on = false;
        if (Update.end(true)) {
            ota_status(CAMLINK_ST_OTA_OK, "ok");
            Serial.flush();
            delay(300);
            ESP.restart();
        }
        ota_status(CAMLINK_ST_OTA_ERROR, Update.errorString());
    }
    if (ota_on) {  /* Nothing else while the badge sends the firmware */
        delay(1);
        return;
    }
    if (web_dirty)
        web_apply();
    wifi_task();
    if (web_asked)
        state_task();
    keys_task();
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
