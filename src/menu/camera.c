/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Badge > Appareil photo: the camera module (an ESP32-CAM on the port J2, docs/fr/appareil_photo.md) through the UART0
 * (GPIO12 TX, GPIO13 RX) at CAMLINK_BAUD, the link of src/shared/camlink/camlink.h:
 * - the viewfinder: the frames of the module (1 bit, 200 x 200) shown as they come (fast refresh);
 * - B: a photo, the module sends its JPEG (PHOTOS/IMG_nnnn.JPG) then its .epi (IMAGES/PHOTOS/IMG_nnnn.EPI: in
 *   Médias > Images); long B: a video, its frames in an .epv (VIDEOS/PHOTOS/VID_nnnn.EPV: in Médias > Vidéos);
 * - X: the settings of the camera, saved, sent to the module.
 * The UART is read by an interrupt into a ring buffer (the main loop can be busy with the screen); every packet of
 * data of the module is acknowledged once written, the module waits for it. */

#include <stdio.h>
#include <string.h>

#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/uart.h"

#include "app.h"
#include "camlink.h"
#include "ff.h"
#include "i18n.h"
#include "pinouts.h"
#include "sd.h"
#include "shared_buf.h"
#include "store.h"

#define CAM_UART uart0
_Static_assert(CLS_COUNT == STORE_CAMERA_SETTINGS, "the settings of the camera in the store");
#define RING_SIZE 3072  /* ~33 ms of the UART at 921600 bauds: more than the longest pause of the main loop */
#define HELLO_MS 1000
#define LINK_LOST_MS 4000
#define MESSAGE_MS 3000
#define PHOTO_DIR "PHOTOS"
#define EPI_DIR "IMAGES/PHOTOS"
#define VIDEO_DIR "VIDEOS/PHOTOS"
#define VIDEO_FPS 4
#define EPV_HEADER 512

/* ------ The UART: an interrupt fills the ring ------ */

static uint8_t ring[RING_SIZE];
static volatile uint16_t ring_head = 0, ring_tail = 0;
static volatile uint32_t ring_lost = 0;

static void uart_irq(void) {
    while (uart_is_readable(CAM_UART)) {
        uint8_t c = uart_getc(CAM_UART);
        uint16_t next = (ring_head + 1) % RING_SIZE;
        if (next == ring_tail)
            ++ring_lost;
        else {
            ring[ring_head] = c;
            ring_head = next;
        }
    }
}

static void link_open(void) {
    uart_init(CAM_UART, CAMLINK_BAUD);
    gpio_set_function(BADGE_UART0_TX, GPIO_FUNC_UART);
    gpio_set_function(BADGE_UART0_RX, GPIO_FUNC_UART);
    gpio_pull_up(BADGE_UART0_RX);  /* No module: no noise on the line */
    uart_set_fifo_enabled(CAM_UART, true);
    ring_head = ring_tail = 0;
    irq_set_exclusive_handler(UART0_IRQ, uart_irq);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(CAM_UART, true, false);
}

static void link_close(void) {
    uart_set_irq_enables(CAM_UART, false, false);
    irq_set_enabled(UART0_IRQ, false);
    irq_remove_handler(UART0_IRQ, uart_irq);
    uart_deinit(CAM_UART);
    gpio_set_function(BADGE_UART0_TX, GPIO_FUNC_NULL);
    gpio_set_function(BADGE_UART0_RX, GPIO_FUNC_NULL);
}

/* ------ The state of the page; the big buffers in shared_buf (this page is alone on the screen) ------ */

typedef struct {
    uint8_t shown[CAMLINK_FRAME_BYTES];  /* The viewfinder: its parts written as they come (no second copy: RAM) */
    camlink_rx_t rx;  /* The receiver, with the packet received */
    uint8_t enc[CAMLINK_ENCODED_MAX];  /* A packet to send */
} cam_buf_t;
_Static_assert(sizeof(cam_buf_t) <= SHARED_BUF_SIZE, "cam_buf_t does not fit in shared_buf");
#define B ((cam_buf_t *)shared_buf)

#define rx (B->rx)
static FIL file;  /* The photo or the video being written */
static bool file_open = false;

typedef enum { L_SEARCHING, L_READY, L_NO_CAMERA } link_state_t;
static link_state_t state;
static char module_name[32];
static absolute_time_t hello_ts, last_rx_ts, message_ts;
static uint8_t out_seq = 0;
static int last_in_seq = -1;  /* The last data packet written: sent again (its ACK lost), only acknowledged */
static bool have_frame = false, changed = false;
static uint16_t frame_id = 0, parts_got = 0;
static bool settings_page = false;
static int settings_sel = 0;
static char message[48];

/* A photo: its number, the file being received (CAMLINK_FILE_*), a video: its frames */
static int photo_n = 0;
static int file_kind = 0;
static bool recording = false;
static uint32_t video_frames = 0;
static uint16_t video_frame_id = 0;
static uint16_t video_parts = 0;

/* The commands waiting for their ACK (sent again every CAMLINK_ACK_MS) */
#define PENDING 4
static struct {
    uint8_t type, seq, len, data[CLS_COUNT * 2];
    absolute_time_t ts;
    int tries;
} pending[PENDING];
static int n_pending = 0;

static void send_packet(uint8_t type, uint8_t seq, const uint8_t *payload, size_t len) {
    size_t n = camlink_encode(type, seq, payload, len, B->enc);
    uart_write_blocking(CAM_UART, B->enc, n);
}

static void command(uint8_t type, const uint8_t *data, uint8_t len) {
    if (n_pending == PENDING)
        return;
    pending[n_pending].type = type;
    pending[n_pending].seq = ++out_seq;
    pending[n_pending].len = len;
    memcpy(pending[n_pending].data, data, len);
    pending[n_pending].tries = 0;
    pending[n_pending].ts = nil_time;  /* Sent at once (pump_commands) */
    ++n_pending;
}

static void pump_commands(absolute_time_t now) {
    if (! n_pending)
        return;
    if (absolute_time_diff_us(pending[0].ts, now) < CAMLINK_ACK_MS * 1000ll && pending[0].tries)
        return;
    if (++pending[0].tries > 10) {  /* No module: given up */
        memmove(&pending[0], &pending[1], sizeof(pending[0]) * --n_pending);
        return;
    }
    send_packet(pending[0].type, pending[0].seq, pending[0].data, pending[0].len);
    pending[0].ts = now;
}

static void acked(uint8_t seq) {
    if (n_pending && pending[0].seq == seq)
        memmove(&pending[0], &pending[1], sizeof(pending[0]) * --n_pending);
}

/* ------ The settings: saved in the store, sent to the module ------ */

static const struct {
    const char *name;
    int8_t min, max, def;
} SETTINGS[CLS_COUNT] = {
    [CLS_FRAMESIZE] = {N_("Taille des photos"), 0, CAMLINK_SIZES - 1, CAMLINK_SIZE_SVGA},
    [CLS_QUALITY] = {N_("Qualité JPEG"), 0, 3, 1},  /* Index in QUALITIES */
    [CLS_BRIGHTNESS] = {N_("Luminosité"), -2, 2, 0},
    [CLS_CONTRAST] = {N_("Contraste"), -2, 2, 0},
    [CLS_SATURATION] = {N_("Saturation"), -2, 2, 0},
    [CLS_EFFECT] = {N_("Effet"), 0, 6, 0},
    [CLS_HMIRROR] = {N_("Miroir"), 0, 1, 0},
    [CLS_VFLIP] = {N_("Retourner"), 0, 1, 0},
    [CLS_FLASH] = {N_("Flash"), 0, 2, 0},
    [CLS_DITHER] = {N_("Trame du viseur"), 0, CAMLINK_DITHERS - 1, CAMLINK_DITHER_ATKINSON},
};
static const uint8_t QUALITIES[] = {6, 10, 15, 25};  /* JPEG quality of the sensor: lower is better */
static const char *const SIZE_NAMES[CAMLINK_SIZES] = {"320x240", "640x480", "800x600", "1024x768", "1280x1024",
                                                     "1600x1200"};
static const char *const EFFECTS[] = {N_("aucun"), N_("négatif"), N_("gris"), N_("rouge"), N_("vert"), N_("bleu"),
                                      N_("sépia")};
static const char *const QUALITY_NAMES[] = {N_("maximale"), N_("haute"), N_("moyenne"), N_("basse")};
static const char *const FLASH_NAMES[] = {N_("non"), N_("pour la photo"), N_("toujours (lampe)")};
static const char *const DITHER_NAMES[] = {"Atkinson", "Floyd-Steinberg", "Bayer", N_("seuil")};

static int8_t setting(int i) {
    const store_t *s = store_get();
    int8_t v = s->camera_set == STORE_CAMERA_SET ? (int8_t)s->camera[i] : SETTINGS[i].def;
    return v < SETTINGS[i].min || v > SETTINGS[i].max ? SETTINGS[i].def : v;
}

static void set_setting(int i, int8_t v) {
    store_t *s = store_get();
    if (s->camera_set != STORE_CAMERA_SET) {
        for (int k = 0; k < CLS_COUNT; ++k)
            s->camera[k] = (uint8_t)SETTINGS[k].def;
        s->camera_set = STORE_CAMERA_SET;
    }
    s->camera[i] = (uint8_t)v;
    store_changed();
}

static void send_settings(void) {
    uint8_t data[CLS_COUNT * 2];
    for (int i = 0; i < CLS_COUNT; ++i) {
        data[2 * i] = (uint8_t)i;
        data[2 * i + 1] = (uint8_t)(i == CLS_QUALITY ? QUALITIES[setting(i)] : setting(i));
    }
    command(CL_SETTINGS, data, sizeof(data));
}

static void setting_text(int i, char *buf, size_t len) {
    int8_t v = setting(i);
    const char *t;
    char num[8];
    switch (i) {
    case CLS_FRAMESIZE: t = SIZE_NAMES[v]; break;
    case CLS_QUALITY: t = tr(QUALITY_NAMES[v]); break;
    case CLS_EFFECT: t = tr(EFFECTS[v]); break;
    case CLS_HMIRROR: case CLS_VFLIP: t = v ? _("oui") : _("non"); break;
    case CLS_FLASH: t = tr(FLASH_NAMES[v]); break;
    case CLS_DITHER: t = tr(DITHER_NAMES[v]); break;
    default:
        snprintf(num, sizeof(num), "%+d", v);
        t = v ? num : "0";
        break;
    }
    snprintf(buf, len, "%s : %s", tr(SETTINGS[i].name), t);
}

/* ------ The files on the SD card ------ */

static void make_dirs(const char *path) {
    char p[40];
    snprintf(p, sizeof(p), "%s", path);
    for (char *s = p; (s = strchr(s + 1, '/')) != NULL; ) {
        *s = 0;
        f_mkdir(p);  /* Already there: an error, nothing to do */
        *s = '/';
    }
    f_mkdir(p);
}

/* The first free number: IMG_nnnn.JPG in PHOTOS, VID_nnnn.EPV in the videos */
static int next_number(const char *dir, const char *prefix, const char *ext) {
    char path[48];
    FILINFO info;
    for (int n = 1; n < 10000; ++n) {
        snprintf(path, sizeof(path), "%s/%s_%04d.%s", dir, prefix, n, ext);
        if (f_stat(path, &info) != FR_OK)
            return n;
    }
    return 9999;
}

static void show_message(const char *text, absolute_time_t now) {
    snprintf(message, sizeof(message), "%s", text);
    message_ts = now;
    changed = true;
}

static bool file_start(int kind) {
    char path[48];
    if (sd_mount() != FR_OK)
        return false;
    if (kind == CAMLINK_FILE_JPEG) {
        make_dirs(PHOTO_DIR);
        make_dirs(EPI_DIR);
        photo_n = next_number(PHOTO_DIR, "IMG", "JPG");
        snprintf(path, sizeof(path), "%s/IMG_%04d.JPG", PHOTO_DIR, photo_n);
    } else {
        snprintf(path, sizeof(path), "%s/IMG_%04d.EPI", EPI_DIR, photo_n ? photo_n : 1);
    }
    if (file_open)
        f_close(&file);
    file_open = f_open(&file, path, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK;
    file_kind = kind;
    printf("camera: %s %s\n", path, file_open ? "open" : "cannot be written");
    return file_open;
}

/* The header of an .epv without sound ("EPVIDEO1"), its number of frames written at the end */
static void epv_header(uint32_t frames) {
    uint8_t h[32] = {'E', 'P', 'V', 'I', 'D', 'E', 'O', '1', 200, 0, 200, 0, VIDEO_FPS, 0, 1, 0,
                     (uint8_t)frames, (uint8_t)(frames >> 8), (uint8_t)(frames >> 16), (uint8_t)(frames >> 24)};
    UINT n;
    f_lseek(&file, 0);
    f_write(&file, h, sizeof(h), &n);
}

static void video_start(absolute_time_t now) {
    char path[48];
    if (sd_mount() != FR_OK) {
        show_message(_("Pas de carte SD."), now);
        return;
    }
    make_dirs(VIDEO_DIR);
    int n = next_number(VIDEO_DIR, "VID", "EPV");
    snprintf(path, sizeof(path), "%s/VID_%04d.EPV", VIDEO_DIR, n);
    if (file_open)
        f_close(&file);
    file_open = f_open(&file, path, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK;
    if (! file_open) {
        show_message(_("Écriture impossible"), now);
        return;
    }
    static const uint8_t zero[64] = {0};  /* The header: its 512 bytes (the number of frames at the end) */
    UINT w;
    for (int i = 0; i < EPV_HEADER / 64; ++i)
        f_write(&file, zero, sizeof(zero), &w);
    epv_header(0);
    f_lseek(&file, EPV_HEADER);
    recording = true;
    video_frames = 0;
    video_parts = 0;
    uint8_t on[2] = {1, VIDEO_FPS};
    command(CL_VIDEO, on, 2);
    printf("camera: video %s\n", path);
    changed = true;
}

static void video_stop(absolute_time_t now) {
    uint8_t off[2] = {0, VIDEO_FPS};
    command(CL_VIDEO, off, 2);
    if (recording && file_open) {
        epv_header(video_frames);
        f_close(&file);
        file_open = false;
        char text[48];
        snprintf(text, sizeof(text), _("Vidéo : %lu images"), (unsigned long)video_frames);
        show_message(text, now);
        printf("camera: video saved, %lu frames\n", (unsigned long)video_frames);
    }
    recording = false;
    changed = true;
}

/* ------ The packets of the module ------ */

static void handle_packet(absolute_time_t now) {
    last_rx_ts = now;
    switch (rx.type) {
    case CL_ACK:
        if (rx.len)
            acked(rx.payload[0]);
        return;
    case CL_HELLO:
        if (state == L_SEARCHING) {
            snprintf(module_name, sizeof(module_name), "%.*s", (int)(rx.len > 1 ? rx.len - 1 : 0), rx.payload + 1);
            printf("camera: module %s, protocol %u\n", module_name, rx.len ? rx.payload[0] : 0);
            state = L_READY;
            send_settings();
            uint8_t on = 1;
            command(CL_PREVIEW, &on, 1);
            changed = true;
        }
        return;
    case CL_STATUS:
        if (rx.len && rx.payload[0] == CAMLINK_ST_NO_CAMERA) {
            state = L_NO_CAMERA;
            changed = true;
        }
        return;
    default:
        break;
    }
    /* The data: acknowledged (the module waits for it), written once */
    uint8_t seq = rx.seq;
    bool again = seq == last_in_seq;
    last_in_seq = seq;
    if (! again) {
        UINT n;
        switch (rx.type) {
        case CL_FRAME:
        case CL_VFRAME:
            if (rx.len == 4 + CAMLINK_PART && rx.payload[2] < CAMLINK_FRAME_BYTES / CAMLINK_PART) {
                uint16_t id = rx.payload[0] | rx.payload[1] << 8;
                int part = rx.payload[2], parts = rx.payload[3];
                if (id != frame_id) {
                    frame_id = id;
                    parts_got = 0;
                }
                memcpy(B->shown + part * CAMLINK_PART, rx.payload + 4, CAMLINK_PART);
                parts_got |= 1 << part;
                if (rx.type == CL_VFRAME && recording && file_open) {  /* Written as it comes, at its place */
                    if (id != video_frame_id) {
                        video_frame_id = id;
                        video_parts = 0;
                    }
                    f_lseek(&file, EPV_HEADER + video_frames * CAMLINK_FRAME_BYTES + part * CAMLINK_PART);
                    f_write(&file, rx.payload + 4, CAMLINK_PART, &n);
                    video_parts |= 1 << part;
                    if (video_parts == (1 << parts) - 1) {
                        ++video_frames;
                        video_parts = 0;
                    }
                }
                if (parts_got == (1 << parts) - 1) {  /* A whole frame: drawn */
                    have_frame = changed = true;
                    parts_got = 0;
                }
            }
            break;
        case CL_FILE_OPEN:
            if (rx.len >= 5)
                file_start(rx.payload[0]);
            break;
        case CL_FILE_DATA:
            if (file_open && rx.len > 4) {
                uint32_t off = rx.payload[0] | rx.payload[1] << 8 | (uint32_t)rx.payload[2] << 16 |
                               (uint32_t)rx.payload[3] << 24;
                f_lseek(&file, off);
                f_write(&file, rx.payload + 4, rx.len - 4, &n);
            }
            break;
        case CL_FILE_CLOSE:
            if (file_open) {
                f_close(&file);
                file_open = false;
                if (file_kind == CAMLINK_FILE_EPI) {
                    char text[48];
                    snprintf(text, sizeof(text), _("Photo IMG_%04d"), photo_n);
                    show_message(text, now);
                    printf("camera: photo %d saved\n", photo_n);
                }
            }
            break;
        default:
            break;
        }
    }
    send_packet(CL_ACK, 0, &seq, 1);
}

/* ------ The page ------ */

static void cam_start(absolute_time_t now) {
    camlink_rx_init(&rx);
    link_open();
    state = L_SEARCHING;
    hello_ts = nil_time;
    last_rx_ts = now;
    message_ts = nil_time;
    message[0] = 0;
    module_name[0] = 0;
    n_pending = 0;
    last_in_seq = -1;
    have_frame = false;
    settings_page = false;
    recording = false;
    file_open = false;
    changed = true;
}

static void cam_stop(void) {
    absolute_time_t now = get_absolute_time();
    if (recording)
        video_stop(now);
    if (file_open) {
        f_close(&file);
        file_open = false;
    }
    uint8_t off = 0;
    send_packet(CL_PREVIEW, ++out_seq, &off, 1);  /* Once: the module stops by itself if it does not hear us */
    link_close();
}

static bool cam_task(absolute_time_t now) {
    while (ring_tail != ring_head) {
        uint8_t c = ring[ring_tail];
        ring_tail = (ring_tail + 1) % RING_SIZE;
        if (camlink_feed(&rx, c))
            handle_packet(now);
    }
    if (state == L_SEARCHING && absolute_time_diff_us(hello_ts, now) > HELLO_MS * 1000ll) {
        hello_ts = now;  /* The module may start after the page: asked again */
        uint8_t hello[] = {CAMLINK_PROTO, 'b', 'a', 'd', 'g', 'e'};
        send_packet(CL_HELLO, ++out_seq, hello, sizeof(hello));
    }
    if (state != L_SEARCHING && absolute_time_diff_us(last_rx_ts, now) > LINK_LOST_MS * 1000ll) {
        state = L_SEARCHING;  /* Unplugged, restarted: found again */
        if (recording)
            video_stop(now);
        changed = true;
    }
    pump_commands(now);
    if (message[0] && absolute_time_diff_us(message_ts, now) > MESSAGE_MS * 1000ll) {
        message[0] = 0;
        changed = true;
    }
    bool c = changed;
    changed = false;
    return c;
}

static bool cam_buttons(const app_buttons_t *b, absolute_time_t now) {
    if (settings_page) {
        if (b->pressed & UI_BTN_A) {
            settings_page = false;
            changed = true;
        }
        if (b->pressed & (UI_BTN_X | UI_BTN_Y)) {
            settings_sel = (settings_sel + ((b->pressed & UI_BTN_X) ? 1 : CLS_COUNT - 1)) % CLS_COUNT;
            changed = true;
        }
        if (b->pressed & UI_BTN_B) {  /* The next value */
            int8_t v = setting(settings_sel) + 1;
            set_setting(settings_sel, v > SETTINGS[settings_sel].max ? SETTINGS[settings_sel].min : v);
            if (state == L_READY)
                send_settings();
            changed = true;
        }
        return true;
    }
    if (b->pressed & UI_BTN_A) {
        if (recording)
            video_stop(now);
        return false;
    }
    if (b->pressed & UI_BTN_X) {
        settings_page = true;
        changed = true;
    }
    if (state != L_READY)
        return true;
    if (b->long_pressed & UI_BTN_B) {  /* A video: on, off */
        if (recording)
            video_stop(now);
        else
            video_start(now);
    } else if ((b->released_short & UI_BTN_B) && ! recording) {
        if (sd_mount() != FR_OK) {
            show_message(_("Pas de carte SD."), now);
        } else {
            uint8_t what = CAMLINK_SHOT_BOTH;
            command(CL_SHOT, &what, 1);
            show_message(_("Photo..."), now);
            printf("camera: shot\n");
        }
    }
    return true;
}

static void settings_label(int i, char *buf, size_t len) {
    setting_text(i, buf, len);
}

static void cam_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    char text[48];
    if (settings_page) {
        ui_title(fb, N_("Réglages photo"));
        ui_list(fb, CLS_COUNT, settings_sel, settings_label);
        ui_footer(fb, N_("Y X : choisir  A : retour  B : changer"));
        return;
    }
    if (state == L_READY && have_frame) {
        memcpy(fb, B->shown, CAMLINK_FRAME_BYTES);  /* The viewfinder, the whole screen */
        /* A band at the bottom: the message, or what the buttons do */
        gfx_fill_rect(fb, 0, GFX_HEIGHT - 18, GFX_WIDTH, 18, GFX_BLACK);
        if (recording)
            snprintf(text, sizeof(text), _("REC %lu  B long : stop"), (unsigned long)video_frames);
        else
            snprintf(text, sizeof(text), "%s", message[0] ? message : _("B photo  B long vidéo  X"));
        gfx_text(fb, GFX_WIDTH / 2, GFX_HEIGHT - 17, &gfx_font_small, text, GFX_WHITE, GFX_ALIGN_CENTER);
        return;
    }
    ui_title(fb, N_("Appareil photo"));
    const char *t = state == L_NO_CAMERA ? N_("Le module répond mais sa caméra ne démarre pas (nappe de la caméra ?).")
                    : state == L_READY ? N_("Module trouvé, la première image arrive...")
                    : N_("Recherche du module ESP32-CAM sur le port J2 (UART0)...");
    int y = ui_wrapped(fb, UI_TITLE_H + 16, &gfx_font_small, t, 5);
    if (module_name[0])
        ui_lines(fb, y + 8, &gfx_font_small, module_name);
    ui_footer(fb, N_("A : retour  X : réglages"));
}

static bool cam_calm(void) {
    return settings_page || state != L_READY;  /* The viewfinder changes all the time: no full refresh then */
}

const app_t app_camera = {
    .name = N_("Appareil photo"),
    .start = cam_start,
    .buttons = cam_buttons,
    .task = cam_task,
    .render = cam_render,
    .calm = cam_calm,
    .stop = cam_stop,
    .no_saver = true,
};
