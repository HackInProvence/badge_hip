/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Badge > Télécommande (webremote.h): the remote control from a phone through the ESP32-CAM module (camport.h), and
 * the update of the module from the SD card.
 * - The service: the module told the access point (its name, a password drawn at each start) or the network saved in
 *   it (a PIN drawn at each start, asked by the page); the screen sent in parts (CL_SCREEN) when it changed and a
 *   page looks at it; the keys of the page (CL_KEY) given to main.c, the text typed to the text editors.
 * - The page: on / off, the access point or the network, how to connect (a QR code to join the access point), the
 *   update of the module (WEBREMOTE_OTA_FILE, in parts of CAMLINK_FILE_PART bytes, each one acknowledged). */

#include <stdio.h>
#include <string.h>

#include "pico/rand.h"

#include "app.h"
#include "camport.h"
#include "ff.h"
#include "i18n.h"
#include "score_code.h"
#include "screen.h"
#include "sd.h"
#include "shared_buf.h"
#include "social.h"
#include "store.h"
#include "webremote.h"

extern const app_t app_remote;

#define KEYS 16  /* The keys of the page waiting for the main loop */
#define AP_URL "http://192.168.4.1"
#define OTA_TIMEOUT_MS 30000  /* The module checks and writes the firmware: its answer after the last part */

/* ------ The service ------ */

static bool on = false;
static char ap_name[CAMLINK_WEB_TEXT], ap_pass[12], pin[7];
static uint8_t keys[KEYS];
static int keys_head = 0, keys_n = 0;
/* What the module says (CL_WEB_STATE) */
static int viewers = 0, wifi = CAMLINK_WIFI_OFF;
static uint8_t ip[4];
static bool network_saved = false;
static char network_name[CAMLINK_WEB_TEXT + 1];
static bool state_changed = false;
/* The screen sent: its id, the part sent next (parts: 0 when nothing is being sent) */
static uint16_t screen_id = 0;
static int screen_part = 0, screen_parts = 0, screen_kind = 0;
static const uint8_t *screen_planes[2];
static uint32_t screen_sent = 0xFFFFFFFF;  /* screen_shot_counter() of the last screen sent */
static bool screen_force = false;

/* The update of the module (the page) */
typedef enum { OTA_IDLE, OTA_SENDING, OTA_CHECKING, OTA_DONE, OTA_FAILED } ota_state_t;
static ota_state_t ota = OTA_IDLE;
static char ota_text[48];

static bool use_network(void) {
    return store_get()->web_network == STORE_WEB_NETWORK;
}

static void send_web(void) {
    uint8_t d[CAMPORT_SMALL];
    size_t n = 0, ln = strlen(ap_name), lp = strlen(ap_pass);
    d[n++] = on ? 1 : 0;
    d[n++] = use_network() ? CAMLINK_WEB_NETWORK : CAMLINK_WEB_AP;
    d[n++] = (uint8_t)ln;
    memcpy(d + n, ap_name, ln);
    n += ln;
    d[n++] = (uint8_t)lp;
    memcpy(d + n, ap_pass, lp);
    n += lp;
    memcpy(d + n, pin, 6);
    n += 6;
    camport_send(CL_WEB, d, n, NULL, 0);
}

static void web_ready(void) {
    if (camport_proto() < 2) {
        printf("webremote: module of protocol %d: no remote control (update it)\n", camport_proto());
        return;
    }
    if (on)
        send_web();
    screen_parts = 0;
    screen_force = true;
}

static void push_key(uint8_t k) {
    if (keys_n == KEYS)
        return;
    keys[(keys_head + keys_n) % KEYS] = k;
    ++keys_n;
}

static void web_packet(const camlink_rx_t *rx, absolute_time_t now) {
    (void)now;
    switch (rx->type) {
    case CL_KEY:
        if (! on || ! rx->len)
            break;
        if (rx->payload[0] == CAMLINK_KEY_TYPE) {
            if (rx->len >= 2 && rx->payload[1] && rx->payload[1] < 0x90)  /* ASCII, 0x80 + n: an accented letter */
                ui_edit_type((char)rx->payload[1]);
        } else if (rx->payload[0] == 's') {
            screen_force = true;  /* A page opened: the screen now */
        } else if (strchr("aAbBxXyY", rx->payload[0])) {  /* Only the buttons */
            push_key(rx->payload[0]);
        }
        break;
    case CL_WEB_STATE:
        if (rx->len >= 7) {
            int was_viewers = viewers;
            viewers = rx->payload[0];
            wifi = rx->payload[1];
            memcpy(ip, rx->payload + 2, 4);
            network_saved = rx->payload[6] != 0;
            snprintf(network_name, sizeof(network_name), "%.*s", (int)(rx->len - 7), rx->payload + 7);
            if (viewers && ! was_viewers)
                screen_force = true;
            state_changed = true;
        }
        break;
    case CL_STATUS:
        if (rx->len && rx->payload[0] == CAMLINK_ST_OTA_OK && ota == OTA_CHECKING) {
            ota = OTA_DONE;
            snprintf(ota_text, sizeof(ota_text), "%s", N_("Module mis à jour : il redémarre."));
            printf("webremote: module updated\n");
            state_changed = true;
        } else if (rx->len && rx->payload[0] == CAMLINK_ST_OTA_ERROR && (ota == OTA_SENDING || ota == OTA_CHECKING)) {
            ota = OTA_FAILED;
            snprintf(ota_text, sizeof(ota_text), "%s", N_("Le module refuse ce firmware."));
            printf("webremote: update refused: %.*s\n", (int)(rx->len - 1), rx->payload + 1);
            state_changed = true;
        }
        break;
    default:
        break;
    }
}

int webremote_key(void) {
    if (! keys_n)
        return -1;
    int k = keys[keys_head];
    keys_head = (keys_head + 1) % KEYS;
    --keys_n;
    return k;
}

bool webremote_on(void) {
    return on;
}

/* A text drawn at random: the password of the access point, the PIN */
static void draw_text(char *buf, int len, const char *chars) {
    int n = (int)strlen(chars);
    for (int i = 0; i < len; ++i)
        buf[i] = chars[get_rand_32() % n];
    buf[len] = 0;
}

static void remote_set(bool want) {
    if (want == on)
        return;
    on = want;
    if (on) {
        /* "Cigale-<name>": the letters and digits of the name only */
        char name[16] = "";
        int k = 0;
        for (const char *p = social_name(); *p && k < 12; ++p)
            if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9'))
                name[k++] = *p;
        name[k] = 0;
        snprintf(ap_name, sizeof(ap_name), "Cigale-%s", k ? name : "badge");
        draw_text(ap_pass, 10, "abcdefghjkmnpqrstuvwxyz23456789");  /* No 0/o, 1/l/i: read on the screen */
        draw_text(pin, 6, "0123456789");
        wifi = CAMLINK_WIFI_OFF;
        viewers = 0;
        keys_n = 0;
        camport_want(CAMPORT_WEB, true, web_packet, web_ready);
        printf("webremote: on, %s\n", use_network() ? "network" : ap_name);
    } else {
        if (camport_state() == CAMPORT_READY && camport_proto() >= 2)
            send_web();  /* Off: the Wi-Fi of the module stops */
        printf("webremote: off\n");
    }
    state_changed = true;
}

/* The next part of the screen, when the previous one was acknowledged */
static void screen_task(void) {
    if (camport_pending(CL_SCREEN))
        return;
    if (! screen_parts) {
        bool new_screen = screen_shot_counter() != screen_sent;
        if (! (screen_force || (new_screen && viewers)))
            return;
        screen_force = false;
        screen_sent = screen_shot_counter();
        screen_kind = screen_shot(&screen_planes[0], &screen_planes[1]);
        screen_parts = screen_kind == SCREEN_SHOT_4G ? 2 * CAMLINK_FRAME_BYTES / CAMLINK_PART
                       : screen_kind == SCREEN_SHOT_BW ? CAMLINK_FRAME_BYTES / CAMLINK_PART : 1;
        screen_part = 0;
        ++screen_id;
    }
    static const uint8_t KINDS[] = {[SCREEN_SHOT_BW] = CAMLINK_SCREEN_BW, [SCREEN_SHOT_4G] = CAMLINK_SCREEN_4G,
                                    [SCREEN_SHOT_WHITE] = CAMLINK_SCREEN_WHITE,
                                    [SCREEN_SHOT_BLACK] = CAMLINK_SCREEN_BLACK};
    uint8_t head[5] = {(uint8_t)screen_id, (uint8_t)(screen_id >> 8), (uint8_t)screen_part, (uint8_t)screen_parts,
                       KINDS[screen_kind]};
    const uint8_t *data = NULL;
    size_t len = 0;
    if (screen_kind == SCREEN_SHOT_BW || screen_kind == SCREEN_SHOT_4G) {
        int plane = screen_part / (CAMLINK_FRAME_BYTES / CAMLINK_PART);
        data = screen_planes[plane] + (screen_part % (CAMLINK_FRAME_BYTES / CAMLINK_PART)) * CAMLINK_PART;
        len = CAMLINK_PART;
    }
    if (camport_send(CL_SCREEN, head, sizeof(head), data, len) && ++screen_part == screen_parts)
        screen_parts = 0;
}

void webremote_task(absolute_time_t now) {
    (void)now;
    if (! on) {
        /* Off and sent to the module: the link is no longer needed (unless the page shows it) */
        if (camport_pending(CL_WEB) == 0 && ota == OTA_IDLE && app_current() != &app_remote)
            camport_want(CAMPORT_WEB, false, NULL, NULL);
        return;
    }
    if (camport_state() == CAMPORT_READY && camport_proto() >= 2)
        screen_task();
}


/* ------ The page ------ */

typedef enum { V_MENU, V_CONNECT, V_OTA } view_t;
static view_t view;
static int sel = 0;
static bool changed = false;
#define ROWS 4

/* The update: the file read in parts into shared_buf (this page is alone on the screen) */
static FIL ota_file;
static uint32_t ota_size = 0, ota_off = 0, ota_sent_off = 0;
static absolute_time_t ota_ts;
#define OTA_BUF ((uint8_t *)shared_buf)
_Static_assert(SHARED_BUF_SIZE >= CAMLINK_FILE_PART, "the update reads its parts in shared_buf");

static void row_label(int i, char *buf, size_t len) {
    switch (i) {
    case 0: snprintf(buf, len, _("Télécommande : %s"), on ? _("oui") : _("non")); break;
    case 1:
        if (use_network())
            snprintf(buf, len, _("Wi-Fi : réseau %s"), network_name[0] ? network_name : "");
        else
            snprintf(buf, len, "%s", _("Wi-Fi : point d'accès"));
        break;
    case 2: snprintf(buf, len, "%s", _("Se connecter")); break;
    default: snprintf(buf, len, "%s", _("Mettre à jour le module")); break;
    }
}

static const char *module_text(void) {
    static char text[48];
    if (camport_state() != CAMPORT_READY)
        return N_("Recherche du module ESP32-CAM...");
    if (camport_proto() < 2)
        return N_("Module trop ancien : le flasher une fois avec le flasheur (docs).");
    snprintf(text, sizeof(text), _("Module %s"), camport_module());
    return text;
}

static void ota_stop(void) {
    if (ota == OTA_SENDING || ota == OTA_CHECKING)
        f_close(&ota_file);
}

static void ota_begin(absolute_time_t now) {
    ota_text[0] = 0;
    if (camport_state() != CAMPORT_READY || camport_proto() < 2) {
        snprintf(ota_text, sizeof(ota_text), "%s", N_("Module absent ou trop ancien."));
        ota = OTA_FAILED;
        return;
    }
    if (sd_mount() != FR_OK || f_open(&ota_file, WEBREMOTE_OTA_FILE, FA_READ) != FR_OK) {
        snprintf(ota_text, sizeof(ota_text), "%s", N_("Fichier absent de la carte SD."));
        ota = OTA_FAILED;
        return;
    }
    ota_size = f_size(&ota_file);
    ota_off = ota_sent_off = 0;
    uint8_t size[4] = {(uint8_t)ota_size, (uint8_t)(ota_size >> 8), (uint8_t)(ota_size >> 16),
                       (uint8_t)(ota_size >> 24)};
    camport_send(CL_OTA_BEGIN, size, 4, NULL, 0);
    ota = OTA_SENDING;
    ota_ts = now;
    printf("webremote: update of the module, %lu bytes\n", (unsigned long)ota_size);
}

/* One part in flight at a time: the next one read when the previous one was acknowledged (its buffer free again) */
static void ota_task(absolute_time_t now) {
    if (ota == OTA_SENDING) {
        if (camport_state() != CAMPORT_READY) {
            f_close(&ota_file);
            ota = OTA_FAILED;
            snprintf(ota_text, sizeof(ota_text), "%s", N_("Module perdu pendant la mise à jour."));
            changed = true;
            return;
        }
        if (camport_pending(0))
            return;
        if (ota_off >= ota_size) {
            f_close(&ota_file);
            camport_send(CL_OTA_END, NULL, 0, NULL, 0);
            ota = OTA_CHECKING;
            ota_ts = now;
            changed = true;
            return;
        }
        UINT n = 0;
        f_lseek(&ota_file, ota_off);
        if (f_read(&ota_file, OTA_BUF, CAMLINK_FILE_PART, &n) != FR_OK || ! n) {
            f_close(&ota_file);
            ota = OTA_FAILED;
            snprintf(ota_text, sizeof(ota_text), "%s", N_("Lecture de la carte SD impossible."));
            changed = true;
            return;
        }
        uint8_t off[4] = {(uint8_t)ota_off, (uint8_t)(ota_off >> 8), (uint8_t)(ota_off >> 16),
                          (uint8_t)(ota_off >> 24)};
        if (camport_send(CL_OTA_DATA, off, 4, OTA_BUF, n)) {
            ota_off += n;
            if (ota_off * 20 / ota_size != ota_sent_off * 20 / ota_size)  /* The bar: every 5 % */
                changed = true;
            ota_sent_off = ota_off;
        }
    } else if (ota == OTA_CHECKING && absolute_time_diff_us(ota_ts, now) > OTA_TIMEOUT_MS * 1000ll) {
        ota = OTA_FAILED;
        snprintf(ota_text, sizeof(ota_text), "%s", N_("Pas de réponse du module."));
        changed = true;
    }
}

static void remote_start(absolute_time_t now) {
    (void)now;
    view = on ? V_CONNECT : V_MENU;
    sel = 0;
    changed = true;
    camport_want(CAMPORT_WEB, true, web_packet, web_ready);  /* The state of the module, even when off */
}

static void remote_stop(void) {
    ota_stop();
    ota = OTA_IDLE;
    if (! on)
        camport_want(CAMPORT_WEB, false, NULL, NULL);
}

static bool remote_buttons(const app_buttons_t *b, absolute_time_t now) {
    if (view == V_OTA) {
        if (ota == OTA_SENDING || ota == OTA_CHECKING)
            return true;  /* Not interrupted */
        if (b->pressed & UI_BTN_A) {
            ota = OTA_IDLE;
            view = V_MENU;
        } else if ((b->pressed & UI_BTN_B) && ota != OTA_DONE) {
            ota_begin(now);
        }
        changed = true;
        return true;
    }
    if (view == V_CONNECT) {
        if (b->pressed & (UI_BTN_A | UI_BTN_B)) {
            view = V_MENU;
            changed = true;
        }
        return true;
    }
    if (b->pressed & UI_BTN_A)
        return false;  /* On: it goes on in the background */
    if (b->pressed & (UI_BTN_X | UI_BTN_Y)) {
        sel = (sel + ((b->pressed & UI_BTN_X) ? 1 : ROWS - 1)) % ROWS;
        changed = true;
    }
    if (b->pressed & UI_BTN_B) {
        switch (sel) {
        case 0:
            remote_set(! on);
            if (on)
                view = V_CONNECT;
            break;
        case 1:
            store_get()->web_network = use_network() ? 0 : STORE_WEB_NETWORK;
            store_changed();
            if (on && camport_state() == CAMPORT_READY)
                send_web();
            break;
        case 2:
            view = V_CONNECT;
            break;
        default:
            ota = OTA_IDLE;
            ota_text[0] = 0;
            view = V_OTA;
            break;
        }
        changed = true;
    }
    return true;
}

static bool remote_task(absolute_time_t now) {
    ota_task(now);
    static camport_state_t last = CAMPORT_OFF;
    if (camport_state() != last) {
        last = camport_state();
        changed = true;
    }
    if (state_changed) {
        state_changed = false;
        changed = true;
    }
    bool c = changed;
    changed = false;
    return c;
}

static void render_connect(uint8_t *fb) {
    char text[80];
    ui_title(fb, N_("Se connecter"));
    int y = UI_TITLE_H + 2;
    if (! on) {
        ui_wrapped(fb, y + 20, &gfx_font_small, N_("La télécommande est arrêtée : B dans le menu pour la démarrer."), 5);
    } else if (camport_state() != CAMPORT_READY || camport_proto() < 2) {
        ui_wrapped(fb, y + 20, &gfx_font_small, module_text(), 5);
    } else if (wifi == CAMLINK_WIFI_NETWORK) {
        /* On the network: its address (and a QR code of it), the PIN asked by the page */
        snprintf(text, sizeof(text), "http://%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
        int size = score_code_draw(NULL, text, 0, 1);
        int scale = size && size * 3 <= 90 ? 3 : 2;
        score_code_draw(fb, text, y, scale);
        y += size * scale + 2;
        y = ui_lines(fb, y, &gfx_font_small, text);
        snprintf(text, sizeof(text), _("Réseau %s  Code : %s"), network_name, pin);
        ui_lines(fb, y, &gfx_font_small, text);
    } else if (wifi == CAMLINK_WIFI_CONNECTING) {
        snprintf(text, sizeof(text), _("Connexion au réseau %s..."), network_name);
        ui_wrapped(fb, y + 20, &gfx_font_small, text, 5);
    } else if (wifi == CAMLINK_WIFI_OFF) {
        ui_wrapped(fb, y + 20, &gfx_font_small, N_("Le Wi-Fi du module démarre..."), 5);
    } else {
        /* Its access point: a QR code to join it, then the page opens (or AP_URL) */
        snprintf(text, sizeof(text), "WIFI:T:WPA;S:%s;P:%s;;", ap_name, ap_pass);
        int size = score_code_draw(NULL, text, 0, 1);
        int scale = size && size * 3 <= 90 ? 3 : 2;
        score_code_draw(fb, text, y, scale);
        y += size * scale + 2;
        if (wifi == CAMLINK_WIFI_FAILED)
            y = ui_lines(fb, y, &gfx_font_small, N_("Réseau introuvable : point d'accès"));
        snprintf(text, sizeof(text), _("%s  mot de passe :"), ap_name);
        y = ui_lines(fb, y, &gfx_font_small, text);
        snprintf(text, sizeof(text), "%s  " AP_URL, ap_pass);
        ui_lines(fb, y, &gfx_font_small, text);
    }
    if (on && viewers)
        snprintf(text, sizeof(text), _("%d page(s) ouverte(s)  A : menu"), viewers);
    else
        snprintf(text, sizeof(text), "%s", _("A : menu"));
    ui_footer(fb, text);
}

static void render_ota(uint8_t *fb) {
    char text[64];
    ui_title(fb, N_("Mise à jour du module"));
    int y = UI_TITLE_H + 6;
    y = ui_wrapped(fb, y, &gfx_font_small, N_("Le firmware du module ESP32-CAM depuis la carte SD :"), 3);
    y = ui_lines(fb, y, &gfx_font_small, WEBREMOTE_OTA_FILE) + 6;
    if (ota == OTA_SENDING || ota == OTA_CHECKING) {
        ui_gauge(fb, 14, y, GFX_WIDTH - 28, 16, (int)(ota_sent_off / 1024), (int)(ota_size / 1024 + 1));
        y += 22;
        snprintf(text, sizeof(text), ota == OTA_CHECKING ? _("Vérification par le module...") : _("%lu / %lu Ko"),
                 (unsigned long)(ota_sent_off / 1024), (unsigned long)(ota_size / 1024));
        ui_lines(fb, y, &gfx_font_small, text);
        ui_footer(fb, N_("Ne pas débrancher"));
        return;
    }
    if (ota_text[0])
        y = ui_wrapped(fb, y, &gfx_font_small, ota_text, 3) + 4;
    ui_wrapped(fb, y, &gfx_font_small, module_text(), 2);
    ui_footer(fb, ota == OTA_DONE ? N_("A : retour") : N_("A : retour  B : mettre à jour"));
}

static void remote_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    if (view == V_CONNECT) {
        render_connect(fb);
        return;
    }
    if (view == V_OTA) {
        render_ota(fb);
        return;
    }
    ui_title(fb, N_("Télécommande"));
    ui_list(fb, ROWS, sel, row_label);
    ui_wrapped(fb, UI_FOOTER_Y - 3 - 2 * gfx_font_small.height, &gfx_font_small, module_text(), 2);
    ui_footer(fb, N_("A : retour  B : choisir"));
}

static bool remote_calm(void) {
    return ota != OTA_SENDING;
}

const app_t app_remote = {
    .name = N_("Télécommande"),
    .start = remote_start,
    .buttons = remote_buttons,
    .task = remote_task,
    .render = remote_render,
    .calm = remote_calm,
    .stop = remote_stop,
};
