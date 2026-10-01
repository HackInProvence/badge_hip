/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file main.c
 *
 * \brief Menu application: the features of the badge in a menu shown on the screen.
 *
 * Badge seen from the front, head up:
 * - flanks: left (Y) = up, right (X) = down (while playing a video or music: left = quieter, right = louder),
 * - wings: right (B) = validate (pause while playing), left (A) = cancel / back
 *   (in the main menu: stops the sound and the LEDs).
 * The same keys (a, b, x, y) on the USB serial port simulate the buttons.
 *
 * The videos (.EPV, see video2epaper.py) are listed from the VIDEOS directory of the SD card,
 * and the music (.WAV, see audio2wav.py) from the MUSIQUE directory (or from the root when these don't exist).
 *
 * Following the README rule, nothing blocks: the main loop polls the buttons and runs the state machines
 * (display, screen demo, video, music, radio), the sound and LEDs are handled by interrupts and DMA.
 */

#include <stdio.h>
#include <string.h>

#include "hardware/watchdog.h"
#include "pico/rand.h"
#include "pico/stdlib.h"

#include "app.h"
#include "apps.h"
#include "audio.h"
#include "battery.h"
#include "btns.h"
#include "display.h"
#include "gfx.h"
#include "leds.h"
#include "log.h"
#include "noise_gen.h"
#include "radio.h"
#include "radio_tools.h"
#include "remote.h"
#include "rsvp.h"
#include "credits.h"
#include "ctf.h"
#include "games.h"
#include "puzzles.h"
#include "ff.h"
#include "ir.h"
#include "oled.h"
#include "oled_demo.h"
#include "screen.h"
#include "screen_demo.h"
#include "sd.h"
#include "achievements.h"
#include "net.h"
#include "ook_rx.h"
#include "party.h"
#include "social.h"
#include "store.h"
#include "version.h"  /* Generated at each build (version.cmake) */
#include "video.h"
#include "wav.h"

void contacts_export(void);  /* contacts.c: the cards received as vCards on USB */


/* Buttons flags from btns_get_state() */
#define BTN_A 0x01  /* left wing */
#define BTN_B 0x02  /* right wing */
#define BTN_X 0x04  /* right flank */
#define BTN_Y 0x08  /* left flank */
#define BTN_UP     BTN_Y
#define BTN_DOWN   BTN_X
#define BTN_OK     BTN_B
#define BTN_CANCEL BTN_A

#define DEBOUNCE_US 20000
#define STATUS_MS 4000  /* How long the status replaces the help in the footer */
#define CARRIER_MAX_MS 30000
#define MUSIC_REFRESH_MS 5000  /* Refresh of the music page (position) */
#define MAX_FILES 48  /* Entries (directories + files) of the file browser */
#define BT_MAX_TRACKS 64  /* Tracks of a blind test */
#define BT_MAX_DIRS 16
#define PATH_MAX_LEN (2*SD_NAME_MAX + 16)

#define VIDEO_DIR "VIDEOS"
#define MUSIC_DIR "MUSIQUE"
#define TEXT_DIR "TEXTES"
#define IMAGE_DIR "IMAGES"


/* ------ Buttons ------ */

static uint8_t btn_stable = 0, btn_raw = 0;
static absolute_time_t btn_ts = 0;
static uint8_t btn_released = 0;  /* Buttons released during the last call of buttons_pressed() */
static absolute_time_t btn_down_ts[4];  /* When each button was pressed (A, B, X, Y) */
static uint8_t btn_simulated_long = 0;  /* Long presses simulated on USB (keys A, B, X, Y) */
static uint8_t btn_long_fired = 0;  /* Buttons whose long press was reported during the current press */
static bool fb_stream = false;  /* Send the screen on USB each time it changes */
static bool fb_send_now = false;  /* Send the screen once */

static int btn_index(uint8_t bit);
static uint32_t btn_held_ms(uint8_t bit, absolute_time_t now);

/* The buttons for the applications (app.h), to call once per loop after buttons_pressed() */
static void app_buttons_event(app_buttons_t *ev, uint8_t pressed, absolute_time_t now) {
    memset(ev, 0, sizeof(*ev));
    ev->pressed = pressed;
    ev->held = btn_stable;
    ev->long_pressed = btn_simulated_long;
    for (uint8_t bit = BTN_A; bit <= BTN_Y; bit <<= 1) {
        ev->held_ms[btn_index(bit)] = btn_held_ms(bit, now);
        if ((btn_stable & bit) && ! (btn_long_fired & bit) && btn_held_ms(bit, now) >= APP_LONG_PRESS_MS) {
            ev->long_pressed |= bit;
            btn_long_fired |= bit;
        }
    }
    ev->released_short = btn_released & ~btn_long_fired & ~btn_simulated_long;
    btn_long_fired &= ~btn_released;
}

static int btn_index(uint8_t bit) {
    return bit == BTN_A ? 0 : bit == BTN_B ? 1 : bit == BTN_X ? 2 : 3;
}

/* How long this button has been held, 0 when released */
static uint32_t btn_held_ms(uint8_t bit, absolute_time_t now) {
    if (! (btn_stable & bit))
        return 0;
    return absolute_time_diff_us(btn_down_ts[btn_index(bit)], now) / 1000;
}

static int admin_request = -1;  /* The serial port asked for the admin mode on (1) or off (0) */

/* Returns the buttons that were pressed since the last call (debounced rising edges), or simulated on USB
 * (keys a, b, x, y: press and release, B, X, Y: long press) */
static uint8_t buttons_pressed(absolute_time_t now) {
    uint8_t raw = btns_get_state();
    uint8_t pressed = 0;
    btn_released = 0;
    btn_simulated_long = 0;
    if (raw != btn_raw) {
        btn_raw = raw;
        btn_ts = now;
    } else if (raw != btn_stable && absolute_time_diff_us(btn_ts, now) > DEBOUNCE_US) {
        pressed = raw & ~btn_stable;
        btn_released = btn_stable & ~raw;
        btn_stable = raw;
        for (uint8_t b = BTN_A; b <= BTN_Y; b <<= 1)
            if (pressed & b)
                btn_down_ts[btn_index(b)] = now;
    }
    switch (getchar_timeout_us(0)) {
    case 0x01: {
        /* Admin mode from badge_remote.py: 0x01 then A (on) or a (off) */
        int c = getchar_timeout_us(50000);
        if (c == 'A' || c == 'a')
            admin_request = c == 'A' ? 1 : 0;
        break;
    }
    case 0x02: {
        /* Keyboard mode of badge_remote.py: 0x02 then a character typed on the PC, for the text editors */
        int c = getchar_timeout_us(50000);
        if (c > 0 && c < 0x90)  /* ASCII, and 0x80 + n: the accented letter n of the editor (ui.c) */
            ui_edit_type((char)c);
        break;
    }
    case 'a': pressed |= BTN_A; btn_released |= BTN_A; break;
    case 'A': pressed |= BTN_A; btn_released |= BTN_A; btn_simulated_long = BTN_A; break;
    case 'b': pressed |= BTN_B; btn_released |= BTN_B; break;
    case 'B': pressed |= BTN_B; btn_released |= BTN_B; btn_simulated_long = BTN_B; break;
    case 'x': pressed |= BTN_X; btn_released |= BTN_X; break;
    case 'X': pressed |= BTN_X; btn_released |= BTN_X; btn_simulated_long = BTN_X; break;
    case 'y': pressed |= BTN_Y; btn_released |= BTN_Y; break;
    case 'Y': pressed |= BTN_Y; btn_released |= BTN_Y; btn_simulated_long = BTN_Y; break;
    case '!': {
        /* Debug: state of the extensions, the network and the CTF */
        uint32_t sent, received;
        social_stats(&sent, &received);
        social_neighbour_t nb[SOCIAL_MAX_NEIGHBOURS];
        int n = social_neighbours(nb, SOCIAL_MAX_NEIGHBOURS);
        printf("oled: %s, IR RX pin = %d, IR recording %d sending %d\n", oled_present() ? "present" : "absent",
               gpio_get(BADGE_IR_RX), ir_recording(), ir_sending());
        printf("social: %s, %s, score %lu, met %u, beacons sent %lu received %lu, %d neighbour(s)\n",
               social_enabled() ? "on" : "off", social_name(), (unsigned long)social_score(), social_met_count(),
               (unsigned long)sent, (unsigned long)received, n);
        for (int i = 0; i < n; ++i)
            printf("  %s %d dBm%s\n", nb[i].name, nb[i].rssi, nb[i].met ? " (met)" : "");
        printf("ctf: %d/%d flags\n", ctf_found_count(), CTF_N_FLAGS);
        uint32_t ns, nr, nd;
        net_stats(&ns, &nr, &nd);
        printf("net: sent %lu, received %lu, dropped %lu%s\n", (unsigned long)ns, (unsigned long)nr, (unsigned long)nd,
               net_loopback() ? ", loopback" : "");
        net_debug();
        remote_debug();
        printf("remote: %s, %s, admin %s\n", remote_enabled() ? "enabled" : "disabled", remote_muted() ? "muted" : "not muted",
               store_get()->admin == STORE_ADMIN_ON ? "on" : "off");
        printf("version: " BADGE_VERSION " (" BADGE_BUILD ")\n");
        printf("radio: CC1101 version 0x%02x, crystal used %lu Hz, measured %lu Hz\n", radio_tools_chip_version(),
               (unsigned long)radio_get_xosc(), (unsigned long)radio_tools_xosc_hz());
        if (battery_calibrated())
            printf("battery: %u mV, %d %%, ADC raw %u%s\n", battery_mv(), battery_percent(), battery_raw(),
                   battery_charging() ? ", USB" : "");
        else
            printf("battery: not calibrated, ADC raw %u%s (Admin > Batterie (calibration))\n", battery_raw(),
                   battery_charging() ? ", USB" : "");
        {
            const store_factory_t *f = store_factory_get();
            printf("battery: factory points %u = %u mV, %u = %u mV\n", f->battery_raw[0], f->battery_mv[0],
                   f->battery_raw[1], f->battery_mv[1]);
        }
        break;
    }
    case 'i': {
        /* Test: build a NEC frame (address 0x04, command 0x08), decode it, and send it on the IR LED */
        static ir_signal_t nec;
        const uint32_t bits = 0x04 | (0xFBu << 8) | (0x08u << 16) | (0xF7u << 24);
        nec.n = 0;
        nec.us[nec.n++] = 9000;
        nec.us[nec.n++] = 4500;
        for (int i = 0; i < 32; ++i) {
            nec.us[nec.n++] = 562;
            nec.us[nec.n++] = (bits >> i) & 1 ? 1687 : 562;
        }
        nec.us[nec.n++] = 562;
        uint16_t addr;
        uint8_t cmd;
        bool ok = ir_decode_nec(&nec, &addr, &cmd);
        printf("ir test: decode %s, address 0x%04X, command 0x%02X\n", ok ? "ok" : "failed", addr, cmd);
        absolute_time_t t0 = get_absolute_time();
        ir_send(&nec);
        while (ir_sending() && absolute_time_diff_us(t0, get_absolute_time()) < 200000)
            tight_loop_contents();
        printf("ir test: sent in %lld us (expected ~67500)\n", absolute_time_diff_us(t0, get_absolute_time()));
        break;
    }
    case 'o': {
        /* Debug: state of the OOK receiver (ook_rx.c) */
        uint8_t rssi = 0, marc = 0;
        radio_read_registers(CC1101_RSSI, &rssi, 1);
        radio_read_registers(CC1101_MARCSTATE, &marc, 1);
        printf("ook: active %d, pulses %lu, frames %lu, rssi %d dBm, marcstate 0x%02x, gdo0 %d\n", ook_rx_active(),
               (unsigned long)ook_rx_pulses(), (unsigned long)ook_rx_frames(), (int8_t)rssi / 2 - 74, marc,
               gpio_get(BADGE_RADIO_GDO0));
        break;
    }
    case 'p':
        ook_rx_dump();
        break;
    case 'k':
        contacts_export();  /* tools/contacts_export.py */
        break;
    case 'V':
        net_set_verbose(! net_verbose());
        printf("net: verbose %s\n", net_verbose() ? "on" : "off");
        break;
    case 'P':
        net_ping();
        break;
    case 'L':
        /* Debug: the packets sent come back as sent by a twin badge (test the radio features alone) */
        net_set_loopback(! net_loopback());
        printf("net: loopback %s\n", net_loopback() ? "on" : "off");
        break;
    case 'R':
        /* Reboot (the automatic tests start from a known state) */
        printf("rebooting\n");
        watchdog_reboot(0, 0, 50);
        break;
    case '[':
        /* PC application (tools/badge_remote.py): send the screen each time it changes */
        fb_stream = true;
        fb_send_now = true;
        break;
    case ']':
        fb_stream = false;
        break;
    case 's':
        fb_send_now = true;
        break;
    case 'U':
        ui_check = ! ui_check;  /* Debug: the texts cut or under the footer are traced ("uicheck: ...") */
        printf("uicheck: %s\n", ui_check ? "on" : "off");
        break;
    case 'r':
        radio_print_registers();  /* Debug: the registers of the CC1101 and its PATABLE */
        break;
    case 'M':
        radio_tools_send();  /* Debug: the "Radio : message" (Flipper chat profile), without the menus */
        break;
    case 'O': {
        static bool ook_debug = false;
        ook_debug = ! ook_debug;
        ook_rx_set_debug(ook_debug);  /* Debug: the decoding attempts of the OOK receiver */
        printf("ook: debug %s\n", ook_debug ? "on" : "off");
        break;
    }
    case '?':
        /* Debug: state of the audio */
        printf("audio: %s, played %lu samples, queued %u, volume %u/%u, music %lus/%lus\n",
               audio_is_open() ? "open" : "closed", (unsigned long)audio_played(), (unsigned)audio_queued(),
               audio_get_volume(), AUDIO_VOLUME_MAX, (unsigned long)wav_position_s(), (unsigned long)wav_duration_s());
        break;
    default: break;
    }
    return pressed;
}


/* ------ Sound and LEDs ------ */

static bool sound_on = false;
static unsigned led_mode = 0;
static const char *LED_NAMES[] = {"éteintes", "arc-en-ciel", "respiration", "battement", "clignotement", "vert fixe"};
#define N_LED_MODES (sizeof(LED_NAMES)/sizeof(LED_NAMES[0]))

static void set_sound(bool on) {
    sound_on = on;
    noise_gen_set_enabled(on && ! remote_muted());
}

bool ledcast_show(void);

static void set_leds(unsigned mode) {
    led_mode = mode % N_LED_MODES;
    if (remote_muted()) {
        leds_cancel_anim(true);  /* Mute mode: the LED mode is kept for later */
        return;
    }
    if (ledcast_show())
        return;  /* The LEDs set by an admin badge (ledcast.c) */
    switch (led_mode) {
    case 1: leds_anim_wheel(2000000); break;
    case 2: leds_anim_breath(LED_RGB(255, 64, 0), 2000000); break;
    case 3: leds_anim_flashes(LED_RGB(255, 0, 0)); break;
    case 4: leds_anim_ook(LED_RGB(0, 64, 255), 500000); break;
    case 5: leds_anim_fixed(LED_RGB(0, 255, 0)); break;
    default: leds_cancel_anim(true); break;
    }
}


/* ------ User interface ------ */

typedef enum {
    M_SOUND,
    M_LEDS,
    M_SCREEN_DEMO,
    M_VIDEO,
    M_MUSIC,
    M_BLIND_TEST,
    M_VOLUME,
    M_SOCIAL,
    M_IR,
    M_OLED,
    M_CTF,
    M_RSVP,
    M_SETTINGS,
    M_RADIO_MSG,
    M_RADIO_CARRIER,
    M_INFO,
    M_IMAGES,
    M_TICTACTOE,  /* The games follow the order of game_t */
    M_CONNECT4,
    M_SIMON,
    M_REFLEX,
    M_SNAKE,
    M_CREDITS,
    M_REMOTE_TOGGLE,  /* Settings: obey the remote commands */
    M_MUTE_TOGGLE,  /* Settings: mute mode */
    M_ADMIN_OFF,  /* Admin menu: leave the admin mode */
    N_ITEMS,
} menu_item_t;
#define M_APP(id) (64 + (id))  /* The applications (apps.h) in the menus */

typedef enum {
    A_MENU,
    A_BROWSE,  /* List of the files (videos or music) */
    A_START_SCREEN_DEMO,  /* Waiting for the display to be idle */
    A_SCREEN_DEMO,
    A_START_VIDEO,
    A_VIDEO,
    A_MUSIC,
    A_BT_FOLDERS,  /* Blind test: choice of the directory */
    A_BLIND_TEST,
    A_SOCIAL,  /* Page of the network of the cicadas */
    A_NAME_EDIT,  /* Name of the cicada: the buttons edit the letters */
    A_IR,  /* List: record, then the slots to send */
    A_IR_RECORD,
    A_OLED,  /* List of the OLED demos */
    A_CTF,  /* List: type a code, found flags */
    A_CTF_CODE,  /* Typing a code: all the buttons are inputs */
    A_START_RSVP,  /* Waiting for the display to be idle */
    A_RSVP,  /* Fast reading: the reader owns the screen */
    A_SETTINGS,
    A_SAVER_IMAGES,  /* Choice of the image of the screensaver */
    A_START_SAVER,  /* Waiting for the display to be idle */
    A_SAVER,  /* Screensaver: the image, then the screen sleeps. Any button wakes up */
    A_START_IMAGE,  /* Image viewer: waiting for the screen */
    A_IMAGE,  /* Image viewer: the image is shown */
    A_CARRIER,
    A_INFO,
    A_PAGE,  /* A message page (e.g. an error), back to the menu with any wing */
    A_GAME,  /* A mini game (games.c) owns the buttons and the page */
    A_RADIO_TEST,  /* Radio test: a message every radio_test_s seconds (long press on "Radio : message") */
    A_APP,  /* An application (app.h) owns the buttons and the page */
    A_CREDITS,  /* One page per contributor, the flanks change the page */
} app_state_t;

static app_state_t app = A_MENU;
static const app_t *cur_app = NULL;  /* When app == A_APP */
static const app_t *app_pending = NULL;  /* Opened by app_open() at the next loop */

void app_open(const app_t *a) {
    app_pending = a;
}

/* A page shown like the screensaver (app_show_still()) */
static void (*still_render)(uint8_t *fb) = NULL;
static bool still_pending = false;

void app_show_still(void (*render)(uint8_t *fb)) {
    still_render = render;
    still_pending = true;
}

static void set_status(const char *msg);
static void game_tone(uint16_t hz, uint16_t ms);

/* A notification: opens \p a from the menus or the screensaver (true), otherwise a status in the footer */
static bool notify(const app_t *a, const char *msg) {
    game_tone(1319, 80);
    set_status(msg);
    printf("notify: %s\n", msg);
    if (a && (app == A_MENU || app == A_SAVER || app == A_START_SAVER)) {
        app_open(a);
        return true;
    }
    return false;
}

/* The group games run when their page is not shown (tug.c, assassin.c) */
void tug_service(absolute_time_t now);
void assassin_service(absolute_time_t now);
bool assassin_event(char *buf, int len);
void werewolf_service(absolute_time_t now);
bool werewolf_event(char *buf, int len);

/* The services of the social features (vote.c, program.c, infection.c, messages.c) */
void vote_task(absolute_time_t now);
bool vote_new(void);
void infection_init(void);
void infection_task(absolute_time_t now);
bool infection_event(void);
bool infection_coughed(void);
void messages_init(void);
void messages_task(absolute_time_t now);
void chorus_init(void);
void contacts_init(void);
void duel_init(void);
void image_radio_init(void);
bool duel_invited(char *buf, int len);
bool battle_invited(char *buf, int len);
void smuggler_init(void);
void smuggler_task(absolute_time_t now);
bool smuggler_invited(char *buf, int len);
bool smuggler_event(char *buf, int len);
bool radio_tune_needed(void);
void ledcast_init(void);
bool ledcast_show(void);
bool ledcast_changed(void);
#include "announce.h"
void chorus_task(absolute_time_t now);
bool messages_new(char *buf, int len);

const app_t *app_current(void) {
    return app == A_APP ? cur_app : NULL;
}
static int selected = M_SOUND;
static uint8_t fb[GFX_FB_SIZE];
static bool redraw = true;
static char status[64] = "";
static absolute_time_t status_ts = 0;
static char page_title[32] = "";
static char page_text[160] = "";
static app_state_t page_back = A_MENU;  /* Where a message page goes back to */
static int credits_page = 0;
static app_state_t credits_back = A_MENU;
static char last_radio_msg[64] = "";

/* File browser: the directories, then the files of the current directory */
static char files[MAX_FILES][SD_NAME_MAX];
static bool file_is_dir[MAX_FILES];
static size_t n_files = 0;
static int file_selected = 0;
/* What the file browser lists */
typedef enum { B_VIDEO, B_MUSIC, B_TEXT, B_IMAGE } browse_kind_t;
static browse_kind_t browsing = B_VIDEO;
static const char *BROWSE_TITLES[] = {"Vidéos", "Musique", "Lecture rapide", "Images"};
static const char *BROWSE_EXTS[] = {".EPV", ".WAV", ".TXT", ".EPI"};
static const char *BROWSE_DIRS[] = {VIDEO_DIR, MUSIC_DIR, TEXT_DIR, IMAGE_DIR};
static const char *BROWSE_TOOLS[] = {"video2epaper.py", "audio2wav.py", "un fichier texte", "image2epi.py"};
static char browse_root[16] = "";  /* Can't go up from here */
static char dir[PATH_MAX_LEN] = "";
static char path[PATH_MAX_LEN] = "";
static char playing_name[SD_NAME_MAX] = "";
static absolute_time_t music_refresh_ts = 0;

/* Blind test: the playlist is shuffled once at the start, so that no track is played twice */
static char bt_dirs[BT_MAX_DIRS][SD_NAME_MAX];
static size_t n_bt_dirs = 0;
static int bt_dir_selected = 0;
static char playlist[BT_MAX_TRACKS][PATH_MAX_LEN];
static uint8_t bt_order[BT_MAX_TRACKS];
static size_t n_playlist = 0;
static size_t bt_index = 0;
static bool bt_revealed = false;
static bool bt_finished = false;  /* The current track was played until the end */

/* Extensions and CTF */
#define SOCIAL_REFRESH_MS 3000
#define CTF_CODE_TIMEOUT_MS 8000
static absolute_time_t page_refresh_ts = 0;
static int ir_selected = 0;
static int ir_next_slot = 0;
static int oled_selected = 0;
static int oled_running = -1;
static int ctf_selected = 0;
static absolute_time_t ctf_last_input = 0;

/* Name editor, like the high scores of the arcade games: the flanks change the letter of the current cell */
#define NAME_LEN 8  /* The beacons carry 8 characters */
#define NAME_LONG_PRESS_MS 1000
#define NAME_REPEAT_DELAY_MS 500
#define NAME_REPEAT_MS 120
static const char NAME_CHARS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.!";
static char name_edit[NAME_LEN + 1];
static int name_cursor = 0;
static bool name_long_done = false;  /* The long press already saved: ignore the release */
static absolute_time_t name_repeat_ts = 0;

#define TITLE_H 28
#define ROW_H 21
#define VISIBLE_ROWS 7
#define FOOTER_Y 180

static void set_status(const char *msg) {
    snprintf(status, sizeof(status), "%s", msg);
    status_ts = get_absolute_time();
    redraw = true;
}

/* For the automatic tests (tools/badge_selftest.py): "ui: <title>" when the page shown changes */
static void ui_trace(const char *title) {
    static char last_title[40] = "";
    if (strncmp(last_title, title, sizeof(last_title) - 1)) {
        snprintf(last_title, sizeof(last_title), "%s", title);
        printf("ui: %s\n", title);
    }
}

static void draw_title(const char *title) {
    ui_trace(title);
    ui_check_width(&gfx_font_medium, title, GFX_WIDTH - 2, "title");
    gfx_fill_rect(fb, 0, 0, GFX_WIDTH, TITLE_H, GFX_BLACK);
    gfx_text(fb, GFX_WIDTH/2, (TITLE_H - gfx_font_medium.height)/2, &gfx_font_medium, title, GFX_WHITE, GFX_ALIGN_CENTER);
}

static void draw_footer(const char *text) {
    ui_check_width(&gfx_font_small, text, GFX_WIDTH - 2, "footer");
    gfx_fill_rect(fb, 0, FOOTER_Y - 2, GFX_WIDTH, 1, GFX_BLACK);
    gfx_text(fb, GFX_WIDTH/2, FOOTER_Y, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
}

/* Text truncated with "..." to fit in the width */
static void fit_text_font(const gfx_font_t *font, char *dst, size_t len, const char *src, int width) {
    ui_fit(font, dst, len, src, width);  /* The same, with the check of the texts (ui_check) */
}

static void fit_text(char *dst, size_t len, const char *src, int width) {
    fit_text_font(&gfx_font_small, dst, len, src, width);
}

/* A scrolling list: \p label gives the text of each row */
static void draw_list(int count, int sel, void (*label)(int, char *, size_t)) {
    int first = sel - VISIBLE_ROWS/2;
    if (first > count - VISIBLE_ROWS)
        first = count - VISIBLE_ROWS;
    if (first < 0)
        first = 0;
    char text[SD_NAME_MAX + 8], fitted[SD_NAME_MAX + 8];
    for (int i = first; i < count && i < first + VISIBLE_ROWS; ++i) {
        int y = TITLE_H + 3 + (i - first)*ROW_H;
        label(i, text, sizeof(text));
        ui_fit_preview(&gfx_font_small, fitted, sizeof(fitted), text, GFX_WIDTH - 20);  /* File names: cut expected */
        if (i == sel) {
            gfx_fill_rect(fb, 2, y, GFX_WIDTH-8, ROW_H-1, GFX_BLACK);
            gfx_text(fb, 8, y + 1, &gfx_font_small, fitted, GFX_WHITE, GFX_ALIGN_LEFT);
        } else {
            gfx_text(fb, 8, y + 1, &gfx_font_small, fitted, GFX_BLACK, GFX_ALIGN_LEFT);
        }
    }
    /* Scroll bar */
    if (count > VISIBLE_ROWS) {
        int h = VISIBLE_ROWS * ROW_H;
        int bar = h * VISIBLE_ROWS / count;
        gfx_fill_rect(fb, GFX_WIDTH - 4, TITLE_H + 3 + (h - bar) * first / (count - VISIBLE_ROWS), 3, bar, GFX_BLACK);
    }
}

static void item_label(int item, char *buf, size_t len) {
    switch (item) {
    case M_SOUND: snprintf(buf, len, "Cigale : %s", sound_on ? "activée" : "coupée"); break;
    case M_LEDS: snprintf(buf, len, "LEDs : %s", LED_NAMES[led_mode]); break;
    case M_SCREEN_DEMO: snprintf(buf, len, "Démo écran"); break;
    case M_VIDEO: snprintf(buf, len, "Vidéos (carte SD)"); break;
    case M_MUSIC: snprintf(buf, len, "Musique (carte SD)"); break;
    case M_BLIND_TEST: snprintf(buf, len, "Blind test"); break;
    case M_SOCIAL: snprintf(buf, len, "Réseau cigales : %lu pts", (unsigned long)social_score()); break;
    case M_IR: snprintf(buf, len, "Infrarouge"); break;
    case M_OLED: snprintf(buf, len, "Écran OLED"); break;
    case M_CTF: snprintf(buf, len, "CTF : %d/%d flags", ctf_found_count(), CTF_N_FLAGS); break;
    case M_RSVP: snprintf(buf, len, "Lecture rapide (PVSR)"); break;
    case M_SETTINGS: snprintf(buf, len, "Veille de l'écran"); break;
    case M_IMAGES: snprintf(buf, len, "Images"); break;
    case M_TICTACTOE: case M_CONNECT4: case M_SIMON: case M_REFLEX: case M_SNAKE:
        snprintf(buf, len, "%s", games_name(item - M_TICTACTOE));
        break;
    case M_VOLUME: snprintf(buf, len, "Volume : %u/%u", audio_get_volume(), AUDIO_VOLUME_MAX); break;
    case M_RADIO_MSG: snprintf(buf, len, "Radio : message"); break;
    case M_RADIO_CARRIER: snprintf(buf, len, "Radio : porteuse"); break;
    case M_INFO: snprintf(buf, len, "Infos"); break;
    case M_CREDITS: snprintf(buf, len, "Crédits"); break;
    case M_REMOTE_TOGGLE: snprintf(buf, len, "Télécommande : %s", remote_enabled() ? "oui" : "non"); break;
    case M_MUTE_TOGGLE: snprintf(buf, len, "Mode muet : %s", remote_muted() ? "oui" : "non"); break;
    case M_ADMIN_OFF: snprintf(buf, len, "Quitter le mode admin"); break;
    default:
        if (item >= M_APP(0) && item < M_APP(APP_COUNT)) {
            const app_t *a = APPS[item - M_APP(0)];
            if (a->label)
                a->label(buf, len);
            else
                snprintf(buf, len, "%s", a->name);
            return;
        }
        buf[0] = 0;
        break;
    }
}

static void file_label(int i, char *buf, size_t len) {
    snprintf(buf, len, file_is_dir[i] ? "> %s" : "%s", files[i]);
}

static void bt_dir_label(int i, char *buf, size_t len) {
    if (i == 0)
        snprintf(buf, len, "Tout");
    else
        snprintf(buf, len, "> %s", bt_dirs[i-1]);
}

/* Title of a track: the file name without the directory and extension */
static void track_title(const char *track_path, char *buf, size_t len) {
    const char *name = strrchr(track_path, '/');
    snprintf(buf, len, "%s", name ? name + 1 : track_path);
    char *dot = strrchr(buf, '.');
    if (dot)
        *dot = 0;
}

/* The menus by theme: the main menu lists the themes, each theme lists its features */
typedef struct {
    const char *title;
    uint8_t n;
    int items[24];
} submenu_t;

static const submenu_t SUBMENUS[] = {
    {"Médias", 7, {M_IMAGES, M_VIDEO, M_MUSIC, M_APP(APP_RTTTL), M_RSVP, M_APP(APP_GAMEBOOK), M_VOLUME}},
    {"Jeux", 19, {M_TICTACTOE, M_CONNECT4, M_SIMON, M_REFLEX, M_SNAKE, M_APP(APP_MINES), M_APP(APP_2048),
                  M_APP(APP_TAQUIN), M_APP(APP_SOKOBAN), M_APP(APP_MASTERMIND), M_APP(APP_PENDU), M_BLIND_TEST, M_CTF,
                  M_APP(APP_CRYPTO), M_APP(APP_DUEL), M_APP(APP_BATTLE), M_APP(APP_WEREWOLF), M_APP(APP_ASSASSIN),
                  M_APP(APP_TUG)}},
    {"Social", 12, {M_SOCIAL, M_APP(APP_MESSAGES), M_APP(APP_CONTACTS), M_APP(APP_SKILLS), M_APP(APP_PROGRAM),
                    M_APP(APP_VOTE), M_APP(APP_RADAR), M_APP(APP_HOTCOLD), M_APP(APP_INFECTION), M_APP(APP_CHORUS),
                    M_APP(APP_ANNOUNCES), M_APP(APP_SMUGGLER)}},
    {"Radio & IR", 9, {M_RADIO_MSG, M_RADIO_CARRIER, M_APP(APP_DECODER), M_APP(APP_WEATHER), M_APP(APP_IMAGE_SEND),
                       M_APP(APP_IMAGE_RECV), M_IR, M_APP(APP_HUNT433), M_APP(APP_PIRATE_LISTEN)}},
    {"Badge", 8, {M_APP(APP_NAMETAG), M_APP(APP_LAMP), M_APP(APP_TALK), M_SOUND, M_LEDS, M_SCREEN_DEMO, M_OLED,
                  M_APP(APP_ACHIEVEMENTS)}},
    {"Réglages", 6, {M_SETTINGS, M_REMOTE_TOGGLE, M_MUTE_TOGGLE, M_INFO, M_CREDITS, M_APP(APP_RADIO_TUNE)}},
    {"Admin", 14, {M_APP(APP_ADMIN_COMMANDS), M_APP(APP_LEDCAST), M_APP(APP_ANNOUNCE_ADMIN),
                   M_APP(APP_VOTE_ADMIN), M_APP(APP_CHORUS_LEAD),
                   M_APP(APP_HOTCOLD_MASTER), M_APP(APP_INFECTION_ZERO), M_APP(APP_SMUGGLER_ADMIN), M_APP(APP_RESET),
                   M_APP(APP_BATTCAL),
                   M_APP(APP_PIRATE_RADIO), M_APP(APP_DEMO), M_APP(APP_ADMIN_TYPE),
                   M_ADMIN_OFF}},  /* Last: hidden unless admin */
};
/* The admin menu is only shown in admin mode */
#define N_SUBMENUS ((int)(sizeof(SUBMENUS) / sizeof(SUBMENUS[0])) - (store_get()->admin == STORE_ADMIN_ON ? 0 : 1))

/* Secret sequence of the flanks in the main menu that shows the admin menu (L = left flank, R = right flank) */
#define ADMIN_SEQUENCE "LLRRLRLR"
#define ADMIN_SEQUENCE_MS 8000
static char admin_keys[sizeof(ADMIN_SEQUENCE)] = "";
static absolute_time_t admin_keys_ts = 0;

static bool admin_sequence(uint8_t flank, absolute_time_t now) {
    size_t n = strlen(admin_keys);
    if (n && absolute_time_diff_us(admin_keys_ts, now) > ADMIN_SEQUENCE_MS * 1000ll)
        n = 0;  /* Too slow: start again */
    if (n == sizeof(admin_keys) - 1) {
        memmove(admin_keys, admin_keys + 1, n);
        --n;
    }
    if (! n)
        admin_keys_ts = now;
    admin_keys[n] = flank == BTN_Y ? 'L' : 'R';
    admin_keys[n + 1] = 0;
    return ! strcmp(admin_keys, ADMIN_SEQUENCE);
}
static int menu_level = 0;  /* 0: the themes, 1: the features of the theme */
static int top_selected = 0;
static int sub_selected = 0;
static bool redraw_menu = false;  /* The menu changed outside the handling of the buttons (set_admin()) */

static void set_status(const char *msg);

/* The admin mode: by the secret sequence of the flanks, "Quitter le mode admin", or the serial port (0x01 A / a,
 * the check box of tools/badge_remote.py) */
static void set_admin(bool on) {
    bool admin_theme = top_selected == (int)(sizeof(SUBMENUS) / sizeof(SUBMENUS[0])) - 1;
    store_get()->admin = on ? STORE_ADMIN_ON : 0;
    store_changed();
    if (on) {
        top_selected = N_SUBMENUS - 1;  /* The Admin theme (the last one) is selected */
    } else if (admin_theme) {
        menu_level = 0;  /* Its theme disappears */
        top_selected = 0;
    }
    printf("admin: %s\n", on ? "on" : "off");
    set_status(on ? "Mode admin activé" : "Mode admin désactivé");
    redraw_menu = true;
}

static void top_label(int i, char *buf, size_t len) {
    snprintf(buf, len, "%s  >", SUBMENUS[i].title);
}

static void sub_label(int i, char *buf, size_t len) {
    item_label(SUBMENUS[top_selected].items[i], buf, len);
}

/* Battery in the title bar (white on black), 4 bars */
static int battery_bars(void) {
    return battery_mv() ? (battery_percent() + 12) / 25 : -1;
}

static void draw_battery(void) {
    int bars = battery_bars();
    if (bars < 0)
        return;
    int x = GFX_WIDTH - 27, y = 8;
    gfx_rect(fb, x, y, 22, 12, GFX_WHITE);
    gfx_fill_rect(fb, x + 22, y + 3, 2, 6, GFX_WHITE);
    for (int i = 0; i < bars; ++i)
        gfx_fill_rect(fb, x + 2 + i*5, y + 2, 4, 8, GFX_WHITE);
    if (battery_charging()) {
        /* Lightning bolt on the left */
        static const int8_t BOLT[][2] = {{4, 0}, {3, 1}, {2, 2}, {1, 3}, {0, 4}, {1, 4}, {2, 4}, {3, 4}, {3, 5}, {2, 6},
                                         {1, 7}, {0, 8}, {1, 8}, {2, 9}, {3, 10}};
        for (unsigned i = 0; i < sizeof(BOLT)/sizeof(BOLT[0]); ++i)
            gfx_fill_rect(fb, x - 9 + BOLT[i][0], y + BOLT[i][1], 2, 1, GFX_WHITE);
    }
}

static void render_menu(absolute_time_t now) {
    gfx_clear(fb, GFX_WHITE);
    if (menu_level == 0) {
        draw_title("Badge SecSea");
        draw_list(N_SUBMENUS, top_selected, top_label);
    } else {
        draw_title(SUBMENUS[top_selected].title);
        draw_list(SUBMENUS[top_selected].n, sub_selected, sub_label);
    }
    draw_battery();
    bool show_status = status[0] && absolute_time_diff_us(status_ts, now) < STATUS_MS*1000ll;
    draw_footer(show_status ? status : "Flancs : choix  D : OK  G : retour");
}

static void render_browser(void) {
    gfx_clear(fb, GFX_WHITE);
    /* The title is the current directory (or the kind of files at the top) */
    const char *sub = strrchr(dir, '/');
    char title[32];
    fit_text_font(&gfx_font_medium, title, sizeof(title),
                  strcmp(dir, browse_root) ? (sub ? sub + 1 : dir) : BROWSE_TITLES[browsing],
                  GFX_WIDTH - 8);
    draw_title(title);
    draw_list(n_files, file_selected, file_label);
    draw_footer("D : ouvrir  G : retour");
}

static void render_bt_folders(void) {
    gfx_clear(fb, GFX_WHITE);
    draw_title("Blind test");
    draw_list(n_bt_dirs + 1, bt_dir_selected, bt_dir_label);
    draw_footer("D : lancer  G : retour");
}

static void render_blind_test(void) {
    char text[48];
    gfx_clear(fb, GFX_WHITE);
    draw_title("Blind test");
    snprintf(text, sizeof(text), "Morceau %u / %u", (unsigned)(bt_index + 1), (unsigned)n_playlist);
    gfx_text(fb, GFX_WIDTH/2, TITLE_H + 6, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);

    if (bt_revealed) {
        /* "Composer - Title": one line each, in the medium font */
        char title[PATH_MAX_LEN], fitted[PATH_MAX_LEN];
        track_title(playlist[bt_order[bt_index]], title, sizeof(title));
        char *second = strstr(title, " - ");
        if (second) {
            *second = 0;
            second += 3;
        }
        int y = second ? 75 : 90;
        fit_text_font(&gfx_font_medium, fitted, sizeof(fitted), title, GFX_WIDTH - 8);
        gfx_text(fb, GFX_WIDTH/2, y, &gfx_font_medium, fitted, GFX_BLACK, GFX_ALIGN_CENTER);
        if (second) {
            fit_text_font(&gfx_font_medium, fitted, sizeof(fitted), second, GFX_WIDTH - 8);
            gfx_text(fb, GFX_WIDTH/2, y + gfx_font_medium.height + 6, &gfx_font_medium, fitted, GFX_BLACK, GFX_ALIGN_CENTER);
        }
    } else {
        gfx_text(fb, GFX_WIDTH/2, 70, &gfx_font_large, "? ? ?", GFX_BLACK, GFX_ALIGN_CENTER);
    }

    const char *state = bt_finished ? "Terminé" : (wav_is_paused() ? "Pause" : "Lecture...");
    gfx_text(fb, GFX_WIDTH/2, 150, &gfx_font_small, state, GFX_BLACK, GFX_ALIGN_CENTER);
    draw_footer("D : titre  Flanc D : suivant");
}

static void render_social(void) {
    char text[64], fitted[64];
    gfx_clear(fb, GFX_WHITE);
    draw_title("Réseau cigales");
    snprintf(text, sizeof(text), "%s : %lu pts, %u rencontres", social_name(), (unsigned long)social_score(),
             social_met_count());
    fit_text(fitted, sizeof(fitted), text, GFX_WIDTH - 12);
    gfx_text(fb, 6, TITLE_H + 4, &gfx_font_small, fitted, GFX_BLACK, GFX_ALIGN_LEFT);
    gfx_text(fb, 6, TITLE_H + 24, &gfx_font_small, social_enabled() ? "Balises : actives" : "Balises : coupées",
             GFX_BLACK, GFX_ALIGN_LEFT);
    gfx_fill_rect(fb, 0, TITLE_H + 45, GFX_WIDTH, 1, GFX_BLACK);

    social_neighbour_t nb[5];
    int n = social_neighbours(nb, 5);
    if (n == 0)
        gfx_text(fb, GFX_WIDTH/2, TITLE_H + 70, &gfx_font_small, "Aucune cigale à portée", GFX_BLACK, GFX_ALIGN_CENTER);
    for (int i = 0; i < n; ++i) {
        /* The RSSI helps to calibrate the meeting threshold (SOCIAL_RSSI_CLOSE) */
        snprintf(text, sizeof(text), "%s%s", nb[i].met ? "* " : "", nb[i].name);
        int y = TITLE_H + 50 + i * 20;
        gfx_text(fb, 6, y, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_LEFT);
        snprintf(text, sizeof(text), "%d dBm", nb[i].rssi);
        gfx_text(fb, GFX_WIDTH - 6, y, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_RIGHT);
    }
    draw_footer("D : nom  Flanc D : balises");
}

static void render_name_edit(void) {
    gfx_clear(fb, GFX_WHITE);
    draw_title("Nom de la cigale");
    /* One cell per character, the current one inverted */
    const int cell = 23, x0 = (GFX_WIDTH - NAME_LEN * cell) / 2, y0 = TITLE_H + 10, h = gfx_font_large.height + 4;
    for (int i = 0; i < NAME_LEN; ++i) {
        int x = x0 + i * cell;
        char c[2] = {name_edit[i], 0};
        if (i == name_cursor) {
            gfx_fill_rect(fb, x, y0, cell - 2, h, GFX_BLACK);
            gfx_text(fb, x + (cell - 2) / 2, y0 + 2, &gfx_font_large, c, GFX_WHITE, GFX_ALIGN_CENTER);
        } else {
            gfx_text(fb, x + (cell - 2) / 2, y0 + 2, &gfx_font_large, c, GFX_BLACK, GFX_ALIGN_CENTER);
            gfx_fill_rect(fb, x + 2, y0 + h - 2, cell - 6, 1, GFX_BLACK);  /* Underline the cells */
        }
    }
    /* Usage */
    static const char *help[] = {
        "Flancs : changer la lettre",
        "Aile D : case suivante",
        "Aile G : case précédente",
        "Aile D longue : enregistrer",
    };
    for (int i = 0; i < 4; ++i)
        gfx_text(fb, 6, 88 + i * 21, &gfx_font_small, help[i], GFX_BLACK, GFX_ALIGN_LEFT);
    draw_footer(name_cursor == 0 ? "Aile G ici : annuler" : "Espace = effacer une lettre");
}

static void name_edit_start(void) {
    memset(name_edit, ' ', NAME_LEN);
    name_edit[NAME_LEN] = 0;
    const char *current = social_name();
    for (int i = 0; i < NAME_LEN && current[i]; ++i)
        name_edit[i] = strchr(NAME_CHARS, current[i]) ? current[i] : ' ';
    name_cursor = 0;
    name_long_done = true;  /* Ignore the release of the press that opened the editor */
    app = A_NAME_EDIT;
}

static void name_edit_change(int delta) {
    const int n = sizeof(NAME_CHARS) - 1;
    const char *p = strchr(NAME_CHARS, name_edit[name_cursor]);
    int i = p ? p - NAME_CHARS : 0;
    name_edit[name_cursor] = NAME_CHARS[(i + delta + n) % n];
    redraw = true;
}

static void name_edit_save(void) {
    char name[NAME_LEN + 1];
    memcpy(name, name_edit, sizeof(name));
    for (int i = NAME_LEN - 1; i >= 0 && name[i] == ' '; --i)
        name[i] = 0;  /* Remove the trailing spaces */
    if (! name[0]) {
        printf("name: empty, not saved\n");
        return;  /* Stay in the editor */
    }
    snprintf(store_get()->name, STORE_NAME_LEN, "%s", name);
    store_changed();
    printf("name: saved \"%s\"\n", name);
    app = A_SOCIAL;
    redraw = true;
}

/* ------ Settings and screensaver ------ */

#define SAVER_DEFAULT_MINUTES 5
#define SAVER_MAX_IMAGES 32
static const uint8_t SAVER_DELAYS[] = {0, 1, 2, 5, 10, 30};  /* Minutes, 0 = off */
static int settings_selected = 0;
static char saver_files[SAVER_MAX_IMAGES][SD_NAME_MAX];
static size_t n_saver_files = 0;
static int saver_selected = 0;
static absolute_time_t last_activity = 0;
static app_state_t saver_return = A_MENU;  /* Where the screensaver goes back to */
static bool saver_shown = false;
static int saver_clean_step = 0;  /* Full refreshes in black then white before the image (erase the ghosts) */

static void start_saver(app_state_t back) {
    saver_return = back;
    saver_clean_step = 0;
    app = A_START_SAVER;
}

static uint8_t saver_planes[2][GFX_FB_SIZE];

static unsigned saver_minutes(void) {
    uint8_t m = store_get()->saver_minutes;
    return m == 0xFF ? SAVER_DEFAULT_MINUTES : m;
}

/* The image of the screensaver ("" = the built-in SecSea image) */
static const char *saver_image(void) {
    const char *p = store_get()->saver_image;
    return (uint8_t)p[0] == 0xFF ? "" : p;
}

/* A file name without the directory and the extension, for display */
static void display_name(const char *p, char *buf, size_t len) {
    const char *name = strrchr(p, '/');
    snprintf(buf, len, "%s", name ? name + 1 : p);
    char *dot = strrchr(buf, '.');
    if (dot)
        *dot = 0;
}

static void settings_label(int i, char *buf, size_t len) {
    char name[SD_NAME_MAX];
    switch (i) {
    case 0:
        if (saver_minutes())
            snprintf(buf, len, "Veille après : %u min", saver_minutes());
        else
            snprintf(buf, len, "Veille : désactivée");
        break;
    case 1:
        display_name(saver_image(), name, sizeof(name));
        snprintf(buf, len, "Image : %s", saver_image()[0] ? name : "SecSea");
        break;
    default:
        snprintf(buf, len, "Aperçu de la veille");
        break;
    }
}

static void saver_image_label(int i, char *buf, size_t len) {
    if (i == 0)
        snprintf(buf, len, "SecSea (intégrée)");
    else
        display_name(saver_files[i - 1], buf, len);
}

static void render_settings(void) {
    gfx_clear(fb, GFX_WHITE);
    draw_title("Veille de l'écran");  /* Not "Réglages": the name of its theme */
    draw_list(3, settings_selected, settings_label);
    draw_footer("D : changer  G : retour");
}

static void render_saver_images(void) {
    gfx_clear(fb, GFX_WHITE);
    draw_title("Image de veille");
    draw_list(n_saver_files + 1, saver_selected, saver_image_label);
    draw_footer("D : choisir  G : retour");
}

static void open_saver_images(void) {
    n_saver_files = sd_list_files(IMAGE_DIR, ".EPI", saver_files, SAVER_MAX_IMAGES);
    saver_selected = 0;
    char current[SD_NAME_MAX];
    display_name(saver_image(), current, sizeof(current));
    for (size_t i = 0; i < n_saver_files; ++i) {
        char name[SD_NAME_MAX];
        display_name(saver_files[i], name, sizeof(name));
        if (saver_image()[0] && ! strcmp(name, current))
            saver_selected = i + 1;
    }
    app = A_SAVER_IMAGES;
}

static void choose_saver_image(void) {
    store_t *s = store_get();
    if (saver_selected == 0)
        s->saver_image[0] = 0;
    else
        snprintf(s->saver_image, STORE_SAVER_IMAGE_LEN, "%s/%s", IMAGE_DIR, saver_files[saver_selected - 1]);
    store_changed();
    app = A_SETTINGS;
}

static void settings_validate(void) {
    store_t *s = store_get();
    switch (settings_selected) {
    case 0: {
        /* Next delay */
        size_t i = 0, n = sizeof(SAVER_DELAYS);
        while (i < n && SAVER_DELAYS[i] != saver_minutes())
            ++i;
        s->saver_minutes = SAVER_DELAYS[(i + 1) % n];
        store_changed();
        break;
    }
    case 1:
        open_saver_images();
        break;
    default:
        start_saver(A_SETTINGS);
        break;
    }
}

/* Loads the .EPI image of the screensaver (see image2epi.py), returns its bits per pixel (0 on error) */
static int load_saver_image(const char *p) {
    FIL f;
    uint8_t header[16];
    UINT n;
    FRESULT fr = sd_mount();
    if (fr == FR_OK)
        fr = f_open(&f, p, FA_READ);
    if (fr != FR_OK && fr != FR_NO_FILE && fr != FR_NO_PATH) {
        /* The card was removed or changed since it was mounted: mount it again */
        sd_unmount();
        fr = sd_mount();
        if (fr == FR_OK)
            fr = f_open(&f, p, FA_READ);
    }
    if (fr != FR_OK)
        return 0;  /* The built-in image instead */
    int bpp = 0;
    if (f_read(&f, header, sizeof(header), &n) == FR_OK && n == sizeof(header) && ! memcmp(header, "EPIMAGE1", 8)
            && (header[8] | header[9] << 8) == GFX_WIDTH && (header[10] | header[11] << 8) == GFX_HEIGHT) {
        bpp = header[12];
        for (int plane = 0; plane < bpp && bpp <= 2; ++plane)
            if (f_read(&f, saver_planes[plane], GFX_FB_SIZE, &n) != FR_OK || n != GFX_FB_SIZE)
                bpp = 0;
    }
    f_close(&f);
    return bpp <= 2 ? bpp : 0;
}

/* Shows the image of the screensaver (full refresh with the OTP waveform, dithered black and white) */
/* 4 grays -> 1 bit, ordered 2x2 dithering (0 black .. 3 white, 1 and 2 are 1 and 2 white pixels out of 4), in lsb */
static void dither_4g(uint8_t *lsb, const uint8_t *msb) {
    static const uint8_t THRESHOLD[2][2] = {{1, 3}, {3, 2}};
    for (int y = 0; y < GFX_HEIGHT; ++y)
        for (int bx = 0; bx < GFX_WIDTH / 8; ++bx) {
            int i = y * (GFX_WIDTH / 8) + bx;
            uint8_t out = 0;
            for (int b = 0; b < 8; ++b) {
                uint8_t mask = 0x80 >> b;
                int gray = (msb[i] & mask ? 2 : 0) + (lsb[i] & mask ? 1 : 0);
                if (gray >= THRESHOLD[y & 1][b & 1])
                    out |= mask;
            }
            lsb[i] = out;
        }
}

static void show_saver(void) {
    if (still_render) {
        gfx_clear(saver_planes[0], GFX_WHITE);
        still_render(saver_planes[0]);
        screen_show_image_bw_otp(saver_planes[0]);
        printf("saver: still page\n");
        return;
    }
    const char *p = saver_image();
    int bpp = p[0] ? load_saver_image(p) : 0;
    if (! bpp) {
        if (p[0])
            printf("saver: cannot load %s, built-in image instead\n", p);
        const uint8_t *lsb, *msb;
        screen_demo_secsea_4g(&lsb, &msb);
        memcpy(saver_planes[0], lsb, GFX_FB_SIZE);
        memcpy(saver_planes[1], msb, GFX_FB_SIZE);
        bpp = 2;
    }
    /* Dithered black and white with the full waveform of the screen (OTP): the 4 grays waveform leaves the screen
     * unstable, a ghost comes back a few seconds after the image (tried: also with EOPT 0x22); the OTP one holds */
    if (bpp == 2)
        dither_4g(saver_planes[0], saver_planes[1]);
    screen_show_image_bw_otp(saver_planes[0]);
    printf("saver: on (%s)\n", p[0] ? p : "SecSea");
}

/* Whether the screensaver can start in this state (not while playing, reading, typing...) */
static bool saver_allowed(app_state_t a) {
    if (a == A_APP)
        return cur_app && ! cur_app->no_saver;
    return a == A_MENU || a == A_BROWSE || a == A_PAGE || a == A_INFO || a == A_CREDITS || a == A_SOCIAL || a == A_IR || a == A_CTF
           || a == A_BT_FOLDERS || a == A_SETTINGS || a == A_SAVER_IMAGES;
}

/* Sends the screen on USB for the PC application: "@FB <kind> [<base64 lsb> [<base64 msb>]]" */
static void send_screen(void) {
    static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    static const char *KINDS[] = {"BW", "4G", "WHITE", "BLACK"};
    const uint8_t *planes[2];
    screen_shot_kind_t kind = screen_shot(&planes[0], &planes[1]);
    printf("@FB %s", KINDS[kind]);
    int n_planes = kind == SCREEN_SHOT_4G ? 2 : kind == SCREEN_SHOT_BW ? 1 : 0;
    for (int p = 0; p < n_planes; ++p) {
        char out[4 * 64 + 1];
        int n = 0;
        putchar(' ');
        for (int i = 0; i < GFX_FB_SIZE; i += 3) {
            uint32_t v = planes[p][i] << 16 | (i + 1 < GFX_FB_SIZE ? planes[p][i+1] << 8 : 0)
                         | (i + 2 < GFX_FB_SIZE ? planes[p][i+2] : 0);
            out[n++] = B64[v >> 18];
            out[n++] = B64[(v >> 12) & 63];
            out[n++] = i + 1 < GFX_FB_SIZE ? B64[(v >> 6) & 63] : '=';
            out[n++] = i + 2 < GFX_FB_SIZE ? B64[v & 63] : '=';
            if (n == (int)sizeof(out) - 1 || i + 3 >= GFX_FB_SIZE) {
                out[n] = 0;
                printf("%s", out);  /* Not fputs(): it is buffered, the printf of the SDK is not (order kept) */
                n = 0;
            }
        }
    }
    putchar('\n');
    fflush(stdout);
}


/* Fast reading: short press on a flank = speed -/+, long press = back/forward (repeated while held),
 * right wing = pause/resume, left wing = quit */
#define RSVP_LONG_PRESS_MS 700
#define RSVP_REPEAT_MS 800
static bool rsvp_flank_long[2];  /* Left, right flank: the long press was used (ignore the release) */
static absolute_time_t rsvp_repeat_ts[2];

static void rsvp_buttons(uint8_t pressed, absolute_time_t now) {
    if (pressed & BTN_A) {
        rsvp_stop();
        return;
    }
    if (pressed & BTN_B)
        rsvp_toggle_pause();
    const uint8_t flanks[2] = {BTN_Y, BTN_X};
    for (int f = 0; f < 2; ++f) {
        int direction = f ? 1 : -1;
        if (pressed & flanks[f])
            rsvp_flank_long[f] = false;
        if (! rsvp_flank_long[f] && ((btn_simulated_long & flanks[f]) || btn_held_ms(flanks[f], now) >= RSVP_LONG_PRESS_MS)) {
            rsvp_flank_long[f] = true;
            rsvp_seek(direction);
            rsvp_repeat_ts[f] = delayed_by_ms(now, RSVP_REPEAT_MS);
        } else if (rsvp_flank_long[f] && btn_held_ms(flanks[f], now) && absolute_time_diff_us(rsvp_repeat_ts[f], now) >= 0) {
            rsvp_seek(direction);  /* Still held: keep seeking */
            rsvp_repeat_ts[f] = delayed_by_ms(now, RSVP_REPEAT_MS);
        }
        if ((btn_released & flanks[f]) && ! rsvp_flank_long[f])
            rsvp_speed(direction);
    }
}

/* The buttons in the name editor (the generic handling is skipped) */
static void name_edit_buttons(uint8_t pressed, absolute_time_t now) {
    /* The keyboard of the PC (badge_remote.py): the same editing as ui_edit_apply_typed() */
    if (ui_edit_typed_pending()) {
        ui_edit_t e = {.max_len = NAME_LEN, .cursor = name_cursor, .charset = NAME_CHARS};
        memcpy(e.text, name_edit, NAME_LEN + 1);
        int r = ui_edit_apply_typed(&e);
        memcpy(name_edit, e.text, NAME_LEN + 1);
        name_cursor = e.cursor;
        redraw = true;
        if (r == UI_EDIT_DONE) {
            name_edit_save();
            return;
        }
        if (r == UI_EDIT_CANCEL) {
            printf("name: cancelled\n");
            app = A_SOCIAL;
            return;
        }
    }
    /* Flanks: previous / next letter, repeated while held */
    if (pressed & (BTN_Y | BTN_X)) {
        name_edit_change((pressed & BTN_Y) ? -1 : 1);
        name_repeat_ts = delayed_by_ms(now, NAME_REPEAT_DELAY_MS);
    } else if ((btn_held_ms(BTN_Y, now) || btn_held_ms(BTN_X, now)) && absolute_time_diff_us(name_repeat_ts, now) >= 0) {
        name_edit_change(btn_held_ms(BTN_Y, now) ? -1 : 1);
        name_repeat_ts = delayed_by_ms(now, NAME_REPEAT_MS);
    }

    /* Right wing: long press = save (as soon as the delay is reached), short press = next cell (on release) */
    if (pressed & BTN_B)
        name_long_done = false;
    if (! name_long_done && ((btn_simulated_long & BTN_B) || btn_held_ms(BTN_B, now) >= NAME_LONG_PRESS_MS)) {
        name_long_done = true;
        name_edit_save();
        return;
    }
    if ((btn_released & BTN_B) && ! name_long_done) {
        if (name_cursor < NAME_LEN - 1)
            ++name_cursor;
        redraw = true;
    }

    /* Left wing: previous cell, or leave without saving from the first one */
    if (pressed & BTN_A) {
        if (name_cursor > 0) {
            --name_cursor;
        } else {
            printf("name: cancelled\n");
            app = A_SOCIAL;
        }
        redraw = true;
    }
}

static void ir_label(int i, char *buf, size_t len) {
    if (i == 0) {
        snprintf(buf, len, "Enregistrer un signal");
        return;
    }
    const ir_signal_t *s = &store_get()->ir[i - 1];
    uint16_t addr;
    uint8_t cmd;
    if (s->n == 0)
        snprintf(buf, len, "%d : vide", i);
    else if (ir_decode_nec(s, &addr, &cmd))
        snprintf(buf, len, "%d : NEC %04X / %02X", i, addr, cmd);
    else
        snprintf(buf, len, "%d : brut, %u impulsions", i, (s->n + 1) / 2);
}

static void render_ir(void) {
    gfx_clear(fb, GFX_WHITE);
    draw_title("Infrarouge");
    draw_list(1 + STORE_IR_SLOTS, ir_selected, ir_label);
    draw_footer(ir_sending() ? "Emission..." : (status[0] ? status : "D : enregistrer/émettre"));
}

static void oled_label(int i, char *buf, size_t len) {
    snprintf(buf, len, "%s%s", i == oled_running ? "> " : "", oled_demo_name(i));
}

static void render_oled(void) {
    gfx_clear(fb, GFX_WHITE);
    draw_title("Écran OLED");
    draw_list(OLED_DEMO_COUNT, oled_selected, oled_label);
    draw_footer("D : lancer  G : retour");
}

static void ctf_label(int i, char *buf, size_t len) {
    if (i == 0)
        snprintf(buf, len, "Saisir un code");
    else
        snprintf(buf, len, "Flags trouvés : %d/%d", ctf_found_count(), CTF_N_FLAGS);
}

static void render_ctf(void) {
    gfx_clear(fb, GFX_WHITE);
    draw_title("CTF");
    draw_list(2, ctf_selected, ctf_label);
    draw_footer("D : OK  G : retour");
}

static void render_ctf_code(void) {
    char code[48];
    ctf_code_text(code, sizeof(code));
    gfx_clear(fb, GFX_WHITE);
    draw_title("Code secret");
    gfx_text(fb, GFX_WIDTH/2, 50, &gfx_font_small, "Tapez le code avec", GFX_BLACK, GFX_ALIGN_CENTER);
    gfx_text(fb, GFX_WIDTH/2, 68, &gfx_font_small, "les 4 boutons.", GFX_BLACK, GFX_ALIGN_CENTER);
    gfx_text(fb, GFX_WIDTH/2, 105, &gfx_font_medium, code[0] ? code : "_", GFX_BLACK, GFX_ALIGN_CENTER);
    draw_footer("Attendre 8 s : annuler");
}

/* A page with a title and some lines of text separated by \n */
static void render_page(const char *title, const char *text, const char *footer) {
    char line[64], fitted[64];
    gfx_clear(fb, GFX_WHITE);
    draw_title(title);
    int y = TITLE_H + 8;
    while (*text) {
        const char *end = strchr(text, '\n');
        size_t n = end ? (size_t)(end - text) : strlen(text);
        if (n >= sizeof(line))
            n = sizeof(line) - 1;
        memcpy(line, text, n);
        line[n] = 0;
        fit_text(fitted, sizeof(fitted), line, GFX_WIDTH - 12);
        gfx_text(fb, 6, y, &gfx_font_small, fitted, GFX_BLACK, GFX_ALIGN_LEFT);
        ui_check_bottom(y + gfx_font_small.height, line);
        y += gfx_font_small.height + 4;
        text += end ? (size_t)(end - text) + 1 : n;
    }
    draw_footer(footer);
}

static void render_info(void) {
    char text[280];
    /* Only a calibrated measure is shown (see battery.h) */
    char bat[32];
    if (battery_mv())
        snprintf(bat, sizeof(bat), "%u,%02u V (%d %%)", battery_mv() / 1000, battery_mv() % 1000 / 10, battery_percent());
    else
        snprintf(bat, sizeof(bat), "non calibrée");
    /* 6 lines fit above the footer: the measured crystal and the default of the firmware are on the serial port ("!") */
    char xosc[24];
    if (radio_tools_measuring())
        snprintf(xosc, sizeof(xosc), "mesure...");
    else
        snprintf(xosc, sizeof(xosc), "%.4f MHz", radio_get_xosc() / 1e6);
    char build[32];
    snprintf(build, sizeof(build), "%s", BADGE_BUILD);
    char *date = strchr(build, ' ');
    if (date)
        *date++ = 0;
    snprintf(text, sizeof(text),
             "Version " BADGE_VERSION " (%s)\nCompilée le %s\n"
             "Radio : CC1101 v0x%02x\nQuartz : %s\nCarte SD : %s\n"
             "Batterie : %s",
             build, date ? date : "?", radio_tools_chip_version(), xosc,
             sd_is_ready() ? "prête" : "absente", bat);
    render_page("Infos", text, "D : crédits  G : retour");
}

static void render_music(void) {
    char text[160];
    uint32_t pos = wav_position_s(), dur = wav_duration_s();
    unsigned vol = audio_get_volume();
    char bar[AUDIO_VOLUME_MAX + 1];
    for (unsigned i = 0; i < AUDIO_VOLUME_MAX; ++i)
        bar[i] = i < vol ? '#' : '-';
    bar[AUDIO_VOLUME_MAX] = 0;
    char name[SD_NAME_MAX + 4];
    ui_fit_preview(&gfx_font_small, name, sizeof(name), playing_name, GFX_WIDTH - 12);  /* A file name of the SD */
    snprintf(text, sizeof(text), "%s\n\n%s   %lu:%02lu / %lu:%02lu\n\nVolume : %s",
             name, wav_is_paused() ? "Pause" : "Lecture",
             (unsigned long)pos/60, (unsigned long)pos%60, (unsigned long)dur/60, (unsigned long)dur%60, bar);
    render_page("Musique", text, "D : pause  G : stop  Flancs : - +");
    /* Progress bar */
    int y = FOOTER_Y - 14;
    gfx_rect(fb, 6, y, GFX_WIDTH - 12, 8, GFX_BLACK);
    if (dur)
        gfx_fill_rect(fb, 8, y + 2, (GFX_WIDTH - 16) * (pos > dur ? dur : pos) / dur, 4, GFX_BLACK);
}


/* ------ Actions ------ */

/* Joins a directory and a name */
static void join_path(char *dst, size_t len, const char *d, const char *name) {
    snprintf(dst, len, "%s%s%s", d, d[0] ? "/" : "", name);
}

/* Loads the directories then the files of dir in the browser */
static void load_browser(void) {
    size_t n_dirs = sd_list_dirs(dir, files, MAX_FILES);
    for (size_t i = 0; i < n_dirs; ++i)
        file_is_dir[i] = true;
    size_t n = sd_list_files(dir, BROWSE_EXTS[browsing], files + n_dirs, MAX_FILES - n_dirs);
    for (size_t i = 0; i < n; ++i)
        file_is_dir[n_dirs + i] = false;
    n_files = n_dirs + n;
    file_selected = 0;
    printf("browser: /%s, %u dir(s), %u file(s):", dir, (unsigned)n_dirs, (unsigned)n);
    for (size_t i = 0; i < n_files && i < 8; ++i)
        printf(" [%s%s]", file_is_dir[i] ? "/" : "", files[i]);
    printf("\n");
}

static void open_browser(browse_kind_t kind) {
    browsing = kind;
    const char *ext = BROWSE_EXTS[kind];
    snprintf(browse_root, sizeof(browse_root), "%s", BROWSE_DIRS[kind]);
    snprintf(dir, sizeof(dir), "%s", browse_root);
    load_browser();
    if (n_files == 0) {
        browse_root[0] = dir[0] = 0;  /* Root */
        load_browser();
    }
    if (n_files == 0) {
        snprintf(page_title, sizeof(page_title), "%s", BROWSE_TITLES[kind]);
        if (! sd_is_ready())
            snprintf(page_text, sizeof(page_text), "Pas de carte SD.");
        else
            snprintf(page_text, sizeof(page_text), "Aucun fichier %s dans\n/%s ni à la racine.\n\nVoir %s.", ext,
                     BROWSE_DIRS[kind], BROWSE_TOOLS[kind]);
        sd_unmount();  /* Mount again next time, the card may be changed */
        app = A_PAGE;
    } else {
        file_selected = 0;
        app = A_BROWSE;
    }
}

/* Goes to the parent directory, returns false at the top of the browser */
static bool browser_up(void) {
    if (! strcmp(dir, browse_root))
        return false;
    char *sep = strrchr(dir, '/');
    if (sep)
        *sep = 0;
    else
        dir[0] = 0;
    load_browser();
    return true;
}

/* Image viewer: index of the image in the browser, the screen sleeps once it is drawn */
static int image_index = 0;
static bool image_asleep = false;

/* Next (delta > 0) or previous image of the directory (the directories of the list are skipped) */
static void image_next(int delta) {
    for (size_t k = 0; k < n_files; ++k) {
        image_index = (image_index + delta + (int)n_files) % (int)n_files;
        if (! file_is_dir[image_index])
            break;
    }
    app = A_START_IMAGE;
}

static void play_selected(void) {
    if (file_is_dir[file_selected]) {
        char sub[PATH_MAX_LEN];
        join_path(sub, sizeof(sub), dir, files[file_selected]);
        snprintf(dir, sizeof(dir), "%s", sub);
        load_browser();
        return;
    }
    join_path(path, sizeof(path), dir, files[file_selected]);
    snprintf(playing_name, sizeof(playing_name), "%s", files[file_selected]);
    set_sound(false);  /* The cicada uses the buzzer too */
    if (browsing == B_VIDEO) {
        app = A_START_VIDEO;
    } else if (browsing == B_TEXT) {
        app = A_START_RSVP;
    } else if (browsing == B_IMAGE) {
        image_index = file_selected;
        app = A_START_IMAGE;
    } else if (wav_start(path)) {
        music_refresh_ts = get_absolute_time();
        app = A_MUSIC;
    } else {
        snprintf(page_title, sizeof(page_title), "Musique");
        snprintf(page_text, sizeof(page_text), "%s\n\n%s", playing_name, wav_message());
        page_back = A_BROWSE;
        app = A_PAGE;
    }
}

/* Blind test: choice of the directory, "Tout" or a sub-directory of MUSIQUE */
static void open_blind_test(void) {
    if (sd_mount() != 0) {
        snprintf(page_title, sizeof(page_title), "Blind test");
        snprintf(page_text, sizeof(page_text), "Pas de carte SD.");
        sd_unmount();
        app = A_PAGE;
        return;
    }
    n_bt_dirs = sd_list_dirs(MUSIC_DIR, bt_dirs, BT_MAX_DIRS);
    bt_dir_selected = 0;
    app = A_BT_FOLDERS;
}

/* Adds the .WAV files of a directory to the playlist */
static void add_tracks(const char *d) {
    size_t n = sd_list_files(d, ".WAV", files, MAX_FILES);
    for (size_t i = 0; i < n && n_playlist < BT_MAX_TRACKS; ++i)
        join_path(playlist[n_playlist++], PATH_MAX_LEN, d, files[i]);
}

static void bt_play_current(void) {
    bt_revealed = false;
    bt_finished = false;
    const char *track = playlist[bt_order[bt_index]];
    if (! wav_start(track)) {
        bt_finished = true;
        bt_revealed = true;
        set_status(wav_message());
    }
    printf("blind test: track %u/%u\n", (unsigned)(bt_index + 1), (unsigned)n_playlist);
}

static void start_blind_test(void) {
    n_playlist = 0;
    if (bt_dir_selected == 0) {
        /* Everything: MUSIQUE (or the root when it does not exist) and its sub-directories */
        add_tracks(n_bt_dirs || sd_list_files(MUSIC_DIR, ".WAV", files, 1) ? MUSIC_DIR : "");
        for (size_t i = 0; i < n_bt_dirs; ++i) {
            char d[PATH_MAX_LEN];
            join_path(d, sizeof(d), MUSIC_DIR, bt_dirs[i]);
            add_tracks(d);
        }
    } else {
        char d[PATH_MAX_LEN];
        join_path(d, sizeof(d), MUSIC_DIR, bt_dirs[bt_dir_selected - 1]);
        add_tracks(d);
    }
    if (n_playlist == 0) {
        snprintf(page_title, sizeof(page_title), "Blind test");
        snprintf(page_text, sizeof(page_text), "Aucun fichier .WAV ici.\n\nVoir audio2wav.py.");
        page_back = A_BT_FOLDERS;
        app = A_PAGE;
        return;
    }

    /* Shuffle once (Fisher-Yates): each track is played once, in a random order */
    for (size_t i = 0; i < n_playlist; ++i)
        bt_order[i] = i;
    for (size_t i = n_playlist - 1; i > 0; --i) {
        size_t j = get_rand_32() % (i + 1);
        uint8_t o = bt_order[i];
        bt_order[i] = bt_order[j];
        bt_order[j] = o;
    }
    bt_index = 0;
    set_sound(false);  /* The cicada uses the buzzer too */
    app = A_BLIND_TEST;
    bt_play_current();
}

static void bt_next(void) {
    wav_stop();
    if (++bt_index >= n_playlist) {
        snprintf(page_title, sizeof(page_title), "Blind test");
        snprintf(page_text, sizeof(page_text), "Terminé !\n\n%u morceaux joués.", (unsigned)n_playlist);
        app = A_PAGE;
        return;
    }
    bt_play_current();
}

/* A short chime to hear the volume (and to test the audio without SD card): 3 notes, 0.36s */
static bool chime_playing = false;

static void play_chime(void) {
    static uint8_t samples[3 * 1920];
    static const uint16_t notes_hz[3] = {1047, 1319, 1568};  /* C6 E6 G6: around the resonance of the buzzer */
    const uint32_t rate = 16000;
    set_sound(false);
    if (wav_is_paused() || ! audio_open(rate))
        return;
    for (int n = 0; n < 3; ++n) {
        /* Square wave with a decreasing amplitude (no floats needed) */
        uint32_t period = rate / notes_hz[n];
        for (int i = 0; i < 1920; ++i) {
            int amp = 100 * (1920 - i) / 1920;
            samples[n*1920 + i] = (uint8_t)(128 + (((uint32_t)i % period) < period / 2 ? amp : -amp));
        }
    }
    audio_write(samples, sizeof(samples));
    chime_playing = true;
}

/* Sound of the games: a square wave, like the chime */
static void game_tone(uint16_t hz, uint16_t ms) {
    if (wav_is_paused())
        return;
    if (chime_playing || audio_is_open()) {
        audio_close();
        chime_playing = false;
    }
    if (hz == 0 || ms == 0)
        return;
    set_sound(false);
    const uint32_t rate = 16000;
    if (! audio_open(rate))
        return;
    uint32_t period = rate / hz, n = rate * (ms > 450 ? 450 : ms) / 1000;  /* Fits in the audio buffer */
    uint8_t chunk[256];
    for (uint32_t i = 0; i < n; ) {
        uint32_t k = 0;
        for (; k < sizeof(chunk) && i < n; ++k, ++i) {
            int amp = 100 * (n - i) / n;
            chunk[k] = (uint8_t)(128 + ((i % period) < period / 2 ? amp : -amp));
        }
        audio_write(chunk, k);
    }
    chime_playing = true;
}

/* A cough (virus of the cicadas): two bursts of low-pass filtered noise. Not over a music or a video. */
static void game_cough(void) {
    if (wav_is_paused() || (audio_is_open() && ! chime_playing))
        return;
    game_tone(0, 0);  /* Stops a previous tone */
    set_sound(false);
    const uint32_t rate = 16000;
    if (! audio_open(rate))
        return;
    static const uint16_t PARTS_MS[4] = {110, 90, 160, 0};  /* Kof, silence, kof */
    int level = 0;
    uint8_t chunk[256];
    for (int p = 0; PARTS_MS[p]; ++p) {
        uint32_t n = rate * PARTS_MS[p] / 1000;
        for (uint32_t i = 0; i < n; ) {
            uint32_t k = 0;
            for (; k < sizeof(chunk) && i < n; ++k, ++i) {
                int amp = p % 2 ? 0 : 110 * (n - i) / n;  /* Decays */
                level += ((int)(get_rand_32() & 0xFF) - 128 - level) / 3;  /* Low-pass: a deep sound */
                chunk[k] = (uint8_t)(128 + level * amp / 128);
            }
            audio_write(chunk, k);
        }
    }
    chime_playing = true;
}

static void game_leds(uint8_t r, uint8_t g, uint8_t b) {
    if ((r || g || b) && ! remote_muted())
        leds_anim_fixed(LED_RGB(r, g, b));
    else
        leds_cancel_anim(true);
}

/* For the applications (app.h) */
void app_tone(uint16_t hz, uint16_t ms) {
    game_tone(hz, ms);  /* Silent in mute mode (audio_set_mute()) */
}

void app_cough(void) {
    game_cough();
}

void app_leds(uint8_t r, uint8_t g, uint8_t b) {
    if (r || g || b)
        game_leds(r, g, b);
    else
        set_leds(led_mode);  /* Back to the animation of the badge */
}

/* A new record (or a win against the cicada): saved, and an achievement */
static void records_changed(void) {
    store_changed();
    achv_unlock(ACHV_RECORD);
}

static const games_hooks_t GAME_HOOKS = {
    .tone = game_tone,
    .leds = game_leds,
    .random = get_rand_32,
    .records_changed = records_changed,
    .player_name = social_name,
    .badge_id = social_id,
};

/* Radio test mode: a message every radio_test_s seconds, the flanks change the pause (held: faster and faster) */
#define RADIO_TEST_DEFAULT_S 5
#define RADIO_TEST_MAX_S (99 * 3600)  /* Practically unlimited */
#define RADIO_TEST_REPEAT_DELAY_MS 400
#define RADIO_TEST_REPEAT_MS 100
static uint32_t radio_test_s = RADIO_TEST_DEFAULT_S;
static bool radio_test_paused = false;
static unsigned radio_test_sent = 0;
static absolute_time_t radio_test_last = 0, radio_test_next = 0, radio_test_tick = 0;
static absolute_time_t radio_test_repeat_ts[2];

static void radio_test_start(absolute_time_t now) {
    radio_test_s = RADIO_TEST_DEFAULT_S;
    radio_test_paused = false;
    radio_test_sent = 0;
    radio_test_next = now;  /* The first message right away */
    radio_test_tick = now;
    display_set_periodic_full(false);  /* A redraw every second: no blinking full refresh */
    app = A_RADIO_TEST;
    printf("radio test: every %lu s\n", (unsigned long)radio_test_s);
}

/* Changes the pause, by bigger steps the longer the flank is held */
static void radio_test_change(int direction, uint32_t held_ms, bool simulated_long) {
    uint32_t step = simulated_long ? 10 : held_ms > 6000 ? 60 : held_ms > 4000 ? 10 : held_ms > 2000 ? 5 : 1;
    if (direction < 0)
        radio_test_s = radio_test_s > step ? radio_test_s - step : 1;
    else
        radio_test_s = radio_test_s + step < RADIO_TEST_MAX_S ? radio_test_s + step : RADIO_TEST_MAX_S;
    if (radio_test_sent)
        radio_test_next = delayed_by_ms(radio_test_last, radio_test_s * 1000);
    redraw = true;
}

static void radio_test_buttons(uint8_t pressed, absolute_time_t now) {
    if (pressed & BTN_CANCEL) {
        printf("radio test: stopped after %u message(s)\n", radio_test_sent);
        display_set_periodic_full(true);
        app = A_MENU;
        redraw = true;
        return;
    }
    if (pressed & BTN_OK) {
        radio_test_paused = ! radio_test_paused;
        if (! radio_test_paused)
            radio_test_next = now;  /* Resume with a message */
        redraw = true;
    }
    static const uint8_t flanks[2] = {BTN_Y, BTN_X};  /* Left flank: shorter, right flank: longer */
    for (int f = 0; f < 2; ++f) {
        if (pressed & flanks[f]) {
            radio_test_change(f ? 1 : -1, 0, btn_simulated_long & flanks[f]);
            radio_test_repeat_ts[f] = delayed_by_ms(now, RADIO_TEST_REPEAT_DELAY_MS);
        } else if (btn_held_ms(flanks[f], now) && absolute_time_diff_us(radio_test_repeat_ts[f], now) >= 0) {
            radio_test_change(f ? 1 : -1, btn_held_ms(flanks[f], now), false);
            radio_test_repeat_ts[f] = delayed_by_ms(now, RADIO_TEST_REPEAT_MS);
        }
    }
}

static void radio_test_task(absolute_time_t now) {
    if (! radio_test_paused && absolute_time_diff_us(radio_test_next, now) >= 0 && radio_tools_send()) {
        /* (radio busy, e.g. a beacon of the network: tried again at the next loop) */
        ++radio_test_sent;
        radio_test_last = now;
        radio_test_next = delayed_by_ms(now, radio_test_s * 1000);
        redraw = true;
    }
    if (absolute_time_diff_us(radio_test_tick, now) >= 1000000) {  /* The countdown */
        radio_test_tick = now;
        redraw = true;
    }
}

static void format_duration(char *buf, size_t len, uint32_t s) {
    if (s < 60)
        snprintf(buf, len, "%lu s", (unsigned long)s);
    else if (s < 3600)
        snprintf(buf, len, s % 60 ? "%lu min %02lu s" : "%lu min", (unsigned long)(s / 60), (unsigned long)(s % 60));
    else
        snprintf(buf, len, "%lu h %02lu min", (unsigned long)(s / 3600), (unsigned long)(s % 3600 / 60));
}

static void render_radio_test(absolute_time_t now) {
    char text[64], fitted[64];
    gfx_clear(fb, GFX_WHITE);
    draw_title("Test radio");
    gfx_text(fb, GFX_WIDTH/2, TITLE_H + 6, &gfx_font_small, "Un message toutes les", GFX_BLACK, GFX_ALIGN_CENTER);
    format_duration(text, sizeof(text), radio_test_s);
    gfx_text(fb, GFX_WIDTH/2, TITLE_H + 24, &gfx_font_large, text, GFX_BLACK, GFX_ALIGN_CENTER);
    gfx_text(fb, GFX_WIDTH/2, TITLE_H + 54, &gfx_font_small, "Flancs : - / +  (maintenir : vite)", GFX_BLACK,
             GFX_ALIGN_CENTER);
    snprintf(text, sizeof(text), "Messages envoyés : %u", radio_test_sent);
    gfx_text(fb, GFX_WIDTH/2, TITLE_H + 80, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
    /* The last message on two lines: "SecSea <name>" and "coucou #<counter>" */
    const char *msg = radio_test_sent ? radio_tools_last_text() : "-";
    const char *second = strstr(msg, " coucou");
    snprintf(text, sizeof(text), "%.*s", second ? (int)(second - msg) : (int)strlen(msg), msg);
    fit_text(fitted, sizeof(fitted), text, GFX_WIDTH - 8);
    gfx_text(fb, GFX_WIDTH/2, TITLE_H + 96, &gfx_font_small, fitted, GFX_BLACK, GFX_ALIGN_CENTER);
    if (second)
        gfx_text(fb, GFX_WIDTH/2, TITLE_H + 112, &gfx_font_small, second + 1, GFX_BLACK, GFX_ALIGN_CENTER);
    if (radio_test_paused) {
        snprintf(text, sizeof(text), "En pause");
    } else {
        int64_t left = absolute_time_diff_us(now, radio_test_next) / 1000000;
        char d[24];
        format_duration(d, sizeof(d), left > 0 ? (uint32_t)left : 0);
        snprintf(text, sizeof(text), "Prochain dans %s", d);
    }
    gfx_text(fb, GFX_WIDTH/2, TITLE_H + 132, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
    draw_footer(radio_test_paused ? "G : quitter  D : reprendre" : "G : quitter  D : pause");
}

/* Menu: right wing released = the usual action, held = another one (games: the record as a signed QR code,
 * radio message: the test mode) */
#define LONG_PRESS_MS 800
static void validate(void);
static bool long_ok_pending = false;

static bool long_item_selected(void) {
    menu_item_t item = SUBMENUS[top_selected].items[sub_selected];
    return app == A_MENU && menu_level == 1 && ((item >= M_TICTACTOE && item <= M_SNAKE) || item == M_RADIO_MSG);
}

static void long_item_ok(uint8_t pressed, absolute_time_t now) {
    if (pressed & BTN_OK)
        long_ok_pending = true;
    if (! long_ok_pending)
        return;
    menu_item_t item = SUBMENUS[top_selected].items[sub_selected];
    if ((btn_simulated_long & BTN_OK) || btn_held_ms(BTN_OK, now) >= LONG_PRESS_MS) {
        long_ok_pending = false;
        if (item == M_RADIO_MSG) {
            radio_test_start(now);
        } else if (games_show_record(item - M_TICTACTOE)) {
            ui_trace("Record");
            app = A_GAME;
        } else {
            set_status("Pas encore de record");
        }
        redraw = true;
    } else if (btn_released & BTN_OK) {
        long_ok_pending = false;
        validate();
        redraw = true;
    }
}

static void change_volume(int delta) {
    int v = audio_get_volume() + delta;
    if (v < 0)
        v = 0;
    audio_set_volume(v);
    redraw = true;
}

static void validate(void) {
    if (menu_level == 0) {
        /* Open the theme */
        menu_level = 1;
        sub_selected = 0;
        redraw = true;
        return;
    }
    selected = SUBMENUS[top_selected].items[sub_selected];
    if (selected >= M_APP(0) && selected < M_APP(APP_COUNT)) {
        app_open(APPS[selected - M_APP(0)]);
        return;
    }
    switch (selected) {
    case M_SOUND:
        set_sound(! sound_on);
        break;
    case M_LEDS:
        set_leds(led_mode + 1);
        break;
    case M_SCREEN_DEMO:
        app = A_START_SCREEN_DEMO;
        break;
    case M_VIDEO:
        open_browser(B_VIDEO);
        break;
    case M_MUSIC:
        open_browser(B_MUSIC);
        break;
    case M_BLIND_TEST:
        open_blind_test();
        break;
    case M_SOCIAL:
        page_refresh_ts = get_absolute_time();
        app = A_SOCIAL;
        break;
    case M_IR:
        ir_selected = 0;
        status[0] = 0;
        app = A_IR;
        break;
    case M_OLED:
        if (! oled_present() && ! oled_init()) {
            snprintf(page_title, sizeof(page_title), "Écran OLED");
            snprintf(page_text, sizeof(page_text),
                     "Aucun écran détecté.\n\nSSD1306 128x64 I2C,\nport gauche (J3) :\nSDA broche 4, SCL broche 3,\n3,3 V broche 1, GND 11.");
            app = A_PAGE;
        } else {
            app = A_OLED;
        }
        break;
    case M_CTF:
        ctf_selected = 0;
        app = A_CTF;
        break;
    case M_RSVP:
        open_browser(B_TEXT);
        break;
    case M_IMAGES:
        open_browser(B_IMAGE);
        break;
    case M_SETTINGS:
        settings_selected = 0;
        app = A_SETTINGS;
        break;
    case M_VOLUME:
        audio_set_volume((audio_get_volume() + 1) % (AUDIO_VOLUME_MAX + 1));
        play_chime();
        break;
    case M_TICTACTOE:
    case M_CONNECT4:
    case M_SIMON:
    case M_REFLEX:
    case M_SNAKE:
        games_start(selected - M_TICTACTOE, get_absolute_time());
        ui_trace(games_name(selected - M_TICTACTOE));
        app = A_GAME;
        break;
    case M_REMOTE_TOGGLE:
        remote_set_enabled(! remote_enabled());
        break;
    case M_MUTE_TOGGLE:
        remote_set_muted(! remote_muted());
        break;
    case M_ADMIN_OFF:
        set_admin(false);
        break;
    case M_CREDITS:
        credits_page = 0;
        credits_back = A_MENU;
        app = A_CREDITS;
        break;
    case M_RADIO_MSG:
        if (! radio_tools_send())
            set_status("Radio occupée");
        break;
    case M_RADIO_CARRIER:
        radio_tools_carrier_start(CARRIER_MAX_MS);
        app = A_CARRIER;
        break;
    case M_INFO:
        sd_init();  /* Refresh the card status (short when there is no card) */
        radio_tools_measure_xosc();
        app = A_INFO;
        break;
    default:
        break;
    }
    redraw = true;
}

static void cancel(void) {
    switch (app) {
    case A_MENU:
        if (menu_level == 1) {
            menu_level = 0;  /* Back to the themes */
        } else {
            set_sound(false);
            set_leds(0);
            set_status("Son et LEDs coupés");
        }
        break;
    case A_START_IMAGE:
    case A_IMAGE:
        display_invalidate();
        app = A_BROWSE;
        break;
    case A_SCREEN_DEMO:
        screen_demo_stop();
        break;
    case A_VIDEO:
        video_stop();
        break;
    case A_MUSIC:
        wav_stop();
        printf("music: stopped at %lus of %lus\n", (unsigned long)wav_position_s(), (unsigned long)wav_duration_s());
        app = A_BROWSE;
        break;
    case A_CARRIER:
        radio_tools_carrier_stop();
        app = A_MENU;
        break;
    case A_START_VIDEO:
        app = A_BROWSE;
        break;
    case A_BROWSE:
        if (! browser_up())
            app = A_MENU;
        break;
    case A_BLIND_TEST:
        wav_stop();
        app = A_MENU;
        break;
    case A_IR_RECORD:
        ir_record_stop();
        app = A_IR;
        break;
    case A_OLED:
        oled_demo_stop();
        oled_running = -1;
        oled_power(false);
        app = A_MENU;
        break;
    case A_CTF_CODE:  /* Not reached: in this page the buttons are the code */
        app = A_CTF;
        break;
    case A_NAME_EDIT:  /* Not reached: the editor handles its buttons */
        app = A_SOCIAL;
        break;
    case A_START_RSVP:
        app = A_BROWSE;
        break;
    case A_SETTINGS:
        app = A_MENU;
        break;
    case A_SAVER_IMAGES:
        app = A_SETTINGS;
        break;
    case A_START_SAVER:
    case A_SAVER:  /* Not reached: any button wakes up */
        app = saver_return;
        break;
    case A_RSVP:
        rsvp_stop();
        break;
    case A_SOCIAL:
    case A_IR:
    case A_CTF:
    case A_START_SCREEN_DEMO:
    case A_BT_FOLDERS:
    case A_INFO:
        app = A_MENU;
        break;
    case A_CREDITS:
        app = credits_back;
        break;
    case A_PAGE:
        app = page_back;
        page_back = A_MENU;
        break;
    }
    redraw = true;
}

/* ------ Mode démo (Admin > Mode démo, demo.c): the features one after the other, in a loop, for a stand;
 * any button stops it. The pages of the SD card are skipped when there is no card. ------ */

typedef enum { D_NAMETAG, D_ACHIEVEMENTS, D_IMAGES, D_VIDEO, D_SKILLS, D_MUSIC, D_SCREEN_DEMO, D_PROGRAM, D_RADAR,
               D_GAMEBOOK, D_CREDITS, D_INFO, D_STEPS } demo_step_t;
static const struct {
    const char *name;
    uint16_t seconds;
} DEMO_STEPS[D_STEPS] = {
    [D_NAMETAG] = {"badge nominatif", 8}, [D_ACHIEVEMENTS] = {"succès", 6}, [D_IMAGES] = {"images", 18},
    [D_VIDEO] = {"vidéo", 20}, [D_SKILLS] = {"compétences", 5}, [D_MUSIC] = {"musique", 10},
    [D_SCREEN_DEMO] = {"démo écran", 15}, [D_PROGRAM] = {"programme", 6}, [D_RADAR] = {"radar", 6},
    [D_GAMEBOOK] = {"livres-jeux", 5}, [D_CREDITS] = {"crédits", 6}, [D_INFO] = {"infos", 5},
};
#define DEMO_IMAGE_MS 6000

static bool demo_on = false, demo_leaving = false, demo_stopping = false;
static int demo_step = -1;
static absolute_time_t demo_next = 0, demo_sub = 0;
static unsigned demo_saved_leds = 0;

/* One step back towards the menu. \return true while not there yet (a video stops in several loops) */
static bool demo_back(void) {
    switch (app) {
    case A_MENU:
        demo_stopping = false;
        return false;
    case A_APP:
        if (cur_app && cur_app->stop)
            cur_app->stop();
        cur_app = NULL;
        app = A_MENU;
        break;
    case A_SAVER:
    case A_START_SAVER:
        still_render = NULL;
        app = A_MENU;
        break;
    case A_VIDEO:
    case A_SCREEN_DEMO:
    case A_RSVP:
        if (! demo_stopping)  /* Asked once, then wait for the end */
            cancel();
        demo_stopping = true;
        return true;
    default:
        cancel();
        break;
    }
    display_invalidate();
    redraw = true;
    return app != A_MENU;
}

/* Plays the first file of a browser (or of its first folder) */
static bool demo_media(browse_kind_t kind) {
    open_browser(kind);
    for (int depth = 0; depth < 2 && app == A_BROWSE; ++depth) {
        for (size_t i = 0; i < n_files; ++i)
            if (! file_is_dir[i]) {
                file_selected = i;
                play_selected();
                return true;
            }
        if (! n_files)
            break;
        file_selected = 0;  /* Only folders: the first one */
        play_selected();
    }
    return false;
}

static void demo_start_step(absolute_time_t now) {
    demo_step = (demo_step + 1) % D_STEPS;
    demo_next = delayed_by_ms(now, DEMO_STEPS[demo_step].seconds * 1000);
    demo_sub = delayed_by_ms(now, DEMO_IMAGE_MS);
    printf("demo: %s\n", DEMO_STEPS[demo_step].name);
    bool shown = true;
    switch (demo_step) {
    case D_NAMETAG: app_open(APPS[APP_NAMETAG]); break;
    case D_ACHIEVEMENTS: app_open(APPS[APP_ACHIEVEMENTS]); break;
    case D_SKILLS: app_open(APPS[APP_SKILLS]); break;
    case D_PROGRAM: app_open(APPS[APP_PROGRAM]); break;
    case D_RADAR: app_open(APPS[APP_RADAR]); break;
    case D_GAMEBOOK: app_open(APPS[APP_GAMEBOOK]); break;
    case D_IMAGES: shown = demo_media(B_IMAGE); break;
    case D_VIDEO: shown = demo_media(B_VIDEO); break;
    case D_MUSIC: shown = demo_media(B_MUSIC); break;
    case D_SCREEN_DEMO: app = A_START_SCREEN_DEMO; break;
    case D_CREDITS:
        credits_page = 0;
        credits_back = A_MENU;
        app = A_CREDITS;
        break;
    default:
        sd_init();
        app = A_INFO;
        break;
    }
    if (! shown) {  /* No SD card, no file: the next step at once */
        printf("demo: %s skipped\n", DEMO_STEPS[demo_step].name);
        demo_back();
        demo_next = now;
    }
    redraw = true;
}

void demo_start(void) {
    demo_on = true;
    demo_leaving = false;
    demo_step = -1;
    demo_next = get_absolute_time();
    demo_saved_leds = led_mode;
    set_leds(1);  /* Rainbow */
    printf("demo: on\n");
}

static void demo_task(absolute_time_t now, uint8_t pressed) {
    if (demo_leaving) {
        if (! demo_back()) {
            demo_leaving = false;
            set_leds(demo_saved_leds);
            set_status("Mode démo arrêté");
            printf("demo: off\n");
        }
        return;
    }
    if (! demo_on)
        return;
    if (pressed) {
        demo_on = false;
        demo_leaving = true;
        return;
    }
    last_activity = now;  /* No screensaver in between */
    if (demo_step == D_IMAGES && app == A_IMAGE && absolute_time_diff_us(demo_sub, now) >= 0) {
        demo_sub = delayed_by_ms(now, DEMO_IMAGE_MS);
        image_next(1);
    }
    if (absolute_time_diff_us(demo_next, now) < 0)
        return;
    if (demo_back())
        return;  /* The current page is not closed yet */
    demo_start_step(now);
}


int main() {
    stdio_init_all();
    log_set_level(LOG_LEVEL_WARNING);

    btns_init();
    leds_init(NULL);
    leds_cancel_anim(true);
    noise_gen_init_play();
    noise_gen_set_enabled(false);  /* init_play starts the sound, we start silent */
    display_init();
    store_init();  /* Before the services: they read their settings (mute, infection, contacts...) */
    radio_tools_init();
    net_init();
    remote_init();
    infection_init();
    messages_init();
    chorus_init();
    contacts_init();
    duel_init();
    smuggler_init();
    image_radio_init();
    ledcast_init();
    announce_init();
    party_init();
    achievements_init();
    social_init();
    games_init(&GAME_HOOKS, store_get()->game_records);
    puzzles_init(&GAME_HOOKS, store_get()->puzzle_records);
    battery_init();
    ir_init();
    oled_init();
    printf("version: " BADGE_VERSION " (" BADGE_BUILD ")\n");
    printf("badge menu ready\n");
    if (radio_tune_needed())
        app_open(APPS[APP_RADIO_TUNE]);  /* First start (or an update that brings the tuning): tune the radio */

    bool was_measuring = false;
    uint32_t sent_shot = 0;
    last_activity = get_absolute_time();
    while (true) {
        absolute_time_t now = get_absolute_time();
        uint8_t pressed = buttons_pressed(now);
        if (pressed)
            printf("buttons pressed: 0x%02x\n", pressed);
        if (demo_on || demo_leaving) {
            demo_task(now, pressed);
            pressed = 0;  /* The press stops the demo, it is not for the page shown */
        }

        /* Buttons */
        if (pressed)
            last_activity = now;
        if ((app == A_SAVER || app == A_START_SAVER) && pressed) {
            /* The press only wakes up */
            printf("saver: off\n");
            still_render = NULL;
            display_invalidate();
            display_settle_soon();  /* The still image leaves a ghost under the next page: cleaned soon */
            app = saver_return;
            redraw = true;
            pressed = 0;
        }
        app_buttons_t app_ev;
        app_buttons_event(&app_ev, pressed, now);
        if (app_pending) {
            /* Open an application (from the menu or from a service: wakes up from the screensaver) */
            if (app == A_SAVER || app == A_START_SAVER) {
                printf("saver: off\n");
                still_render = NULL;
                display_invalidate();
                display_settle_soon();
            }
            if (app == A_APP && cur_app && cur_app->stop)
                cur_app->stop();
            cur_app = app_pending;
            app_pending = NULL;
            ui_trace(cur_app->name);
            app = A_APP;
            cur_app->start(now);
            redraw = true;
            if (still_pending) {
                /* The application only shows a page, like the screensaver (app_show_still()) */
                still_pending = false;
                if (cur_app->stop)
                    cur_app->stop();
                cur_app = NULL;
                start_saver(A_MENU);
            }
        } else if (app == A_APP) {
            bool typed = ui_edit_typed_pending();
            if ((pressed || app_ev.released_short || app_ev.long_pressed || app_ev.held || typed)
                    && ! cur_app->buttons(&app_ev, now)) {
                if (cur_app->stop)
                    cur_app->stop();
                cur_app = NULL;
                display_set_periodic_full(true);
                set_leds(led_mode);
                app = A_MENU;
            }
            /* A held flank repeats (text editors, lamp...): the page changes while it is held */
            if (pressed || app_ev.released_short || app_ev.long_pressed || typed || (app_ev.held & (UI_BTN_X | UI_BTN_Y)))
                redraw = true;
            ui_edit_typed_clear();  /* Not used by the page: dropped */
        } else if (app == A_GAME) {
            if (! games_buttons(pressed, now)) {
                set_leds(led_mode);  /* The games used the LEDs */
                display_set_periodic_full(true);
                app = A_MENU;
            }
            redraw = true;
        } else if (app == A_RADIO_TEST) {
            radio_test_buttons(pressed, now);
        } else if (app == A_NAME_EDIT) {
            name_edit_buttons(pressed, now);
        } else if (app == A_RSVP) {
            rsvp_buttons(pressed, now);
        } else if (app == A_CTF_CODE && pressed) {
            /* Typing a code: every button is an input (one per press) */
            for (uint8_t b = 1; b <= BTN_Y; b <<= 1)
                if (pressed & b)
                    ctf_code_press(b);
            ctf_last_input = now;
            redraw = true;
            if (ctf_code_press(0) >= CTF_CODE_LEN) {
                char flag[64];
                snprintf(page_title, sizeof(page_title), "CTF");
                bool right = ctf_code_check();
                printf("ctf: code %s\n", right ? "right" : "wrong");
                if (right && ctf_flag(0, flag, sizeof(flag))) {
                    snprintf(page_text, sizeof(page_text), "Bravo, code Konami !\n\nFlag :\n%s", flag);
                    set_leds(1);
                    play_chime();
                } else {
                    snprintf(page_text, sizeof(page_text), "Code incorrect.\n\nIndice : un code célèbre\ndes jeux vidéo...");
                }
                page_back = A_CTF;
                app = A_PAGE;
            }
        } else if (long_item_selected() && ((pressed & BTN_OK) || long_ok_pending)) {
            long_item_ok(pressed, now);
        } else if (pressed & BTN_CANCEL) {
            cancel();
        } else if (pressed & BTN_OK) {
            if (app == A_MENU)
                validate();
            else if (app == A_BROWSE && n_files)
                play_selected();
            else if (app == A_BT_FOLDERS)
                start_blind_test();
            else if (app == A_BLIND_TEST) {
                bt_revealed = ! bt_revealed;
                redraw = true;
            }
            else if (app == A_SOCIAL) {
                name_edit_start();
                redraw = true;
            } else if (app == A_IR) {
                if (ir_selected == 0) {
                    ir_record_start();
                    app = A_IR_RECORD;
                } else if (store_get()->ir[ir_selected - 1].n) {
                    ir_send(&store_get()->ir[ir_selected - 1]);
                    printf("ir: sending slot %d\n", ir_selected);
                }
                redraw = true;
            } else if (app == A_IMAGE) {
                join_path(path, sizeof(path), dir, files[image_index]);
                snprintf(store_get()->saver_image, STORE_SAVER_IMAGE_LEN, "%s", path);
                store_changed();
                play_chime();
                printf("image: %s is now the screensaver image\n", path);
            } else if (app == A_SETTINGS) {
                settings_validate();
            } else if (app == A_SAVER_IMAGES) {
                choose_saver_image();
            } else if (app == A_OLED) {
                oled_power(true);
                oled_demo_start(oled_selected);
                oled_running = oled_selected;
                redraw = true;
            } else if (app == A_CTF) {
                if (ctf_selected == 0) {
                    ctf_code_start();
                    ctf_last_input = now;
                    app = A_CTF_CODE;
                } else {
                    char flag[64];
                    snprintf(page_title, sizeof(page_title), "Flags trouvés");
                    snprintf(page_text, sizeof(page_text), "%s", ctf_flag(0, flag, sizeof(flag)) ? flag : "Aucun pour l'instant.");
                    page_back = A_CTF;
                    app = A_PAGE;
                }
                redraw = true;
            }
            else if (app == A_VIDEO)
                video_toggle_pause();
            else if (app == A_MUSIC) {
                wav_toggle_pause();
                redraw = true;
            } else if (app == A_INFO) {
                credits_page = 0;
                credits_back = A_INFO;
                app = A_CREDITS;
            } else if (app == A_PAGE || app == A_CREDITS)
                cancel();
            /* Every validation changes the page (start of the blind test, sub-directory, music player...) */
            redraw = true;
        } else if (pressed & (BTN_UP | BTN_DOWN)) {
            int delta = (pressed & BTN_UP) ? -1 : 1;
            if (app == A_MENU) {
                if (menu_level == 0 && admin_sequence(pressed & (BTN_UP | BTN_DOWN), now)) {
                    set_admin(true);
                } else if (menu_level == 0)
                    top_selected = (top_selected + delta + N_SUBMENUS) % N_SUBMENUS;
                else
                    sub_selected = (sub_selected + delta + SUBMENUS[top_selected].n) % SUBMENUS[top_selected].n;
                redraw = true;
            } else if (app == A_IMAGE || app == A_START_IMAGE) {
                image_next(delta);
            } else if (app == A_BROWSE && n_files) {
                file_selected = (file_selected + delta + n_files) % n_files;
                redraw = true;
            } else if (app == A_BT_FOLDERS) {
                bt_dir_selected = (bt_dir_selected + delta + n_bt_dirs + 1) % (n_bt_dirs + 1);
                redraw = true;
            } else if (app == A_BLIND_TEST) {
                /* Right flank: next track, left flank: pause */
                if (delta > 0)
                    bt_next();
                else if (! bt_finished)
                    wav_toggle_pause();
                redraw = true;
            } else if (app == A_SOCIAL && delta > 0) {
                /* Right flank: beacons on/off */
                social_set_enabled(! social_enabled());
                redraw = true;
            } else if (app == A_IR) {
                ir_selected = (ir_selected + delta + 1 + STORE_IR_SLOTS) % (1 + STORE_IR_SLOTS);
                redraw = true;
            } else if (app == A_OLED) {
                oled_selected = (oled_selected + delta + OLED_DEMO_COUNT) % OLED_DEMO_COUNT;
                redraw = true;
            } else if (app == A_CREDITS) {
                credits_page = (credits_page + delta + credits_count()) % credits_count();
                printf("credits: %s\n", credits_name(credits_page));
                redraw = true;
            } else if (app == A_CTF) {
                ctf_selected = (ctf_selected + delta + 2) % 2;
                redraw = true;
            } else if (app == A_SETTINGS) {
                settings_selected = (settings_selected + delta + 3) % 3;
                redraw = true;
            } else if (app == A_SAVER_IMAGES) {
                saver_selected = (saver_selected + delta + n_saver_files + 1) % (n_saver_files + 1);
                redraw = true;
            } else if (app == A_MUSIC || app == A_VIDEO) {
                change_volume(delta);  /* Left flank = quieter, right flank = louder */
            }
        }

        /* Features */
        if (chime_playing && audio_queued() == 0) {
            audio_close();
            chime_playing = false;
        }
        radio_tools_task(now);
        if (strcmp(last_radio_msg, radio_tools_message())) {
            snprintf(last_radio_msg, sizeof(last_radio_msg), "%s", radio_tools_message());
            if (app == A_MENU)
                set_status(last_radio_msg);
        }
        net_task(now);
        remote_task(now);
        ook_rx_task(now);
        if (ledcast_changed() && ! (app == A_APP && cur_app->owns_leds) && app != A_GAME)
            set_leds(led_mode);  /* An admin badge set the LEDs */
        static bool was_muted = false;
        if (remote_muted() != was_muted) {
            was_muted = remote_muted();
            if (! (app == A_APP && cur_app->owns_leds))
                set_leds(led_mode);
            set_sound(sound_on);
            redraw = true;
        }
        char remote_msg[40];
        if (remote_event(remote_msg, sizeof(remote_msg)))
            set_status(remote_msg);
        vote_task(now);
        infection_task(now);
        chorus_task(now);
        /* Notifications of the social features: the page opens from the menus, a status otherwise */
        char notif[64];
        if (assassin_event(notif, sizeof(notif)))
            notify(APPS[APP_ASSASSIN], notif);  /* "Éliminé par X", a new target, the victory */
        if (werewolf_event(notif, sizeof(notif)))
            notify(APPS[APP_WEREWOLF], notif);  /* "Loup-garou : Nuit 2 : Loups" */
        messages_task(now);
        if (messages_new(notif, sizeof(notif)))
            notify(NULL, notif);
        announce_task(now);
        if (announce_new(notif, sizeof(notif))) {
            announce_open_newest();  /* The page shows it right away */
            notify(APPS[APP_ANNOUNCES], notif);
        }
        if (vote_new())
            notify(APPS[APP_VOTE], "Vote ouvert : Social > Vote");
        if (app != A_APP && app != A_NAME_EDIT)
            ui_edit_typed_clear();  /* Typed on the PC keyboard, but no text editor on the screen */
        if (admin_request >= 0) {
            set_admin(admin_request);
            admin_request = -1;
        }
        if (redraw_menu) {
            redraw_menu = false;
            if (app == A_MENU)
                redraw = true;
        }
        if (infection_coughed()) {
            set_status("Kof kof ! (virus des cigales)");
            printf("infection: cough\n");
        }
        if (infection_event())
            notify(APPS[APP_INFECTION], "Vous êtes infecté !");
        if (duel_invited(notif, sizeof(notif)))
            notify(APPS[APP_DUEL], notif);
        if (smuggler_invited(notif, sizeof(notif)))
            notify(APPS[APP_SMUGGLER], notif);
        if (battle_invited(notif, sizeof(notif)))
            notify(APPS[APP_BATTLE], notif);
        social_task(now);
        party_task(now);
        smuggler_task(now);
        tug_service(now);
        assassin_service(now);
        werewolf_service(now);
        battery_task(now);
        static int shown_bars = -2;
        static bool shown_charging = false;
        if (app == A_MENU && (battery_bars() != shown_bars || (battery_bars() >= 0 && battery_charging() != shown_charging))) {
            shown_bars = battery_bars();  /* Only when the icon changes: the e-Paper refresh is visible */
            shown_charging = battery_charging();
            redraw = true;
        }
        char event[48];
        static absolute_time_t achv_ts = 0;
        if (absolute_time_diff_us(achv_ts, now) >= 0) {
            achv_ts = delayed_by_ms(now, 1000);
            achv_task();
        }
        if (achv_event(event, sizeof(event))) {
            set_status(event);
            if (! audio_is_open())
                play_chime();
            if (app == A_MENU)
                redraw = true;
        } else if (smuggler_event(event, sizeof(event))) {
            set_status(event);  /* Discreet: no sound */
        } else if (social_event(event, sizeof(event))) {
            set_status(event);
            if (! audio_is_open())
                play_chime();
            if (app == A_SOCIAL || app == A_MENU)
                redraw = true;
        }
        store_task(now);
        oled_demo_task(now);

        /* Pages that change by themselves */
        if (app == A_SOCIAL && absolute_time_diff_us(page_refresh_ts, now) > SOCIAL_REFRESH_MS*1000ll) {
            page_refresh_ts = now;
            redraw = true;
        }
        if (app == A_IR_RECORD) {
            ir_signal_t *slot = &store_get()->ir[ir_next_slot];
            if (ir_record_task(slot)) {
                char label[40];
                store_changed();
                ir_label(ir_next_slot + 1, label, sizeof(label));
                printf("ir: recorded %u durations in slot %d\n", slot->n, ir_next_slot + 1);
                ir_selected = ir_next_slot + 1;
                ir_next_slot = (ir_next_slot + 1) % STORE_IR_SLOTS;
                set_status(label);
                app = A_IR;
            }
        }
        if (app == A_CTF_CODE && absolute_time_diff_us(ctf_last_input, now) > CTF_CODE_TIMEOUT_MS*1000ll) {
            app = A_CTF;
            redraw = true;
        }
        static bool ir_was_sending = false;
        if (app == A_IR && ir_sending() != ir_was_sending)
            redraw = true;  /* The footer shows "Emission..." while sending */
        ir_was_sending = ir_sending();

        /* Screensaver after a while without button. While it is not allowed (video, music, game...) the time counts
         * from the end of it: back in the list after a whole video, the saver must not start right away */
        if (! saver_allowed(app))
            last_activity = now;
        else if (saver_minutes() && absolute_time_diff_us(last_activity, now) > saver_minutes() * 60000000ll)
            start_saver(app);

        switch (app) {
        case A_START_SAVER:
            /* The display must have finished its updates before giving the screen */
            display_task(now);
            if (display_is_idle() && screen_boot() && ! screen_busy()) {
                if (saver_clean_step < 2) {
                    /* A clean image: the fast refreshes of the menus leave ghosts that the short custom waveforms
                     * don't erase (they come back a while after the image). Full refreshes with the waveform
                     * of the screen (OTP) in black, then in white, one per loop (non blocking, ~3s each) */
                    screen_clean(saver_clean_step == 1);
                    ++saver_clean_step;
                } else {
                    show_saver();
                    saver_shown = false;
                    app = A_SAVER;
                }
            }
            break;
        case A_SAVER:
            /* Once drawn, the screen sleeps (the image stays without power) */
            if (! saver_shown && screen_boot() && ! screen_busy()) {
                screen_deep_sleep();
                saver_shown = true;
            }
            break;
        case A_START_IMAGE:
            display_task(now);
            if (display_is_idle() && screen_boot() && ! screen_busy()) {
                join_path(path, sizeof(path), dir, files[image_index]);
                int bpp = load_saver_image(path);
                if (bpp == 2)
                    screen_show_image_4g(saver_planes[0], saver_planes[1]);
                else if (bpp == 1)
                    screen_show_image_bw(saver_planes[0]);
                printf("image: %s (%d bit(s) per pixel)\n", path, bpp);
                if (bpp) {
                    image_asleep = false;
                    app = A_IMAGE;
                } else {
                    snprintf(page_title, sizeof(page_title), "Images");
                    snprintf(page_text, sizeof(page_text), "%s\n\nImage .EPI invalide\n(voir image2epi.py).", files[image_index]);
                    page_back = A_BROWSE;
                    app = A_PAGE;
                    redraw = true;
                }
            }
            break;
        case A_IMAGE:
            /* The image stays without power: the screen sleeps */
            if (! image_asleep && screen_boot() && ! screen_busy()) {
                screen_deep_sleep();
                image_asleep = true;
            }
            break;
        case A_START_SCREEN_DEMO:
            /* The display must have finished its updates before giving the screen */
            display_task(now);
            if (display_is_idle()) {
                screen_demo_start();
                app = A_SCREEN_DEMO;
            }
            break;
        case A_SCREEN_DEMO:
            if (! screen_demo_task(now)) {
                display_invalidate();
                app = A_MENU;
                redraw = true;
            }
            break;
        case A_START_VIDEO:
            display_task(now);
            if (display_is_idle()) {
                if (video_start(path)) {
                    app = A_VIDEO;
                } else {
                    snprintf(page_title, sizeof(page_title), "Vidéo");
                    snprintf(page_text, sizeof(page_text), "%s\n\n%s", playing_name, video_message());
                    page_back = A_BROWSE;
                    app = A_PAGE;
                    redraw = true;
                }
            }
            break;
        case A_START_RSVP:
            display_task(now);
            if (display_is_idle()) {
                if (rsvp_start(path)) {
                    app = A_RSVP;
                } else {
                    snprintf(page_title, sizeof(page_title), "Lecture rapide");
                    snprintf(page_text, sizeof(page_text), "%s\n\n%s", playing_name, rsvp_message());
                    page_back = A_BROWSE;
                    app = A_PAGE;
                    redraw = true;
                }
            }
            break;
        case A_RSVP:
            if (! rsvp_task(now)) {
                display_invalidate();
                app = A_BROWSE;
                redraw = true;
            }
            break;
        case A_VIDEO:
            if (! video_task(now)) {
                if (video_completed())
                    achv_unlock(ACHV_VIDEO);
                display_invalidate();
                set_status(video_message());
                app = A_BROWSE;
                redraw = true;
            }
            break;
        case A_MUSIC:
            if (! wav_task()) {
                /* The end (or a read error: the position is then before the end) */
                printf("music: end at %lus of %lus\n", (unsigned long)wav_position_s(), (unsigned long)wav_duration_s());
                app = A_BROWSE;
                redraw = true;
            } else if (absolute_time_diff_us(music_refresh_ts, now) > MUSIC_REFRESH_MS*1000ll) {
                music_refresh_ts = now;
                redraw = true;
            }
            /* fall through */
        case A_BLIND_TEST:
            if (app == A_BLIND_TEST && ! bt_finished && ! wav_task()) {
                /* End of the track: show the answer */
                bt_finished = true;
                bt_revealed = true;
                redraw = true;
            }
            /* fall through */
        default:
            if (app == A_CARRIER && ! radio_tools_carrier_on()) {
                app = A_MENU;
                redraw = true;
            }
            if (app == A_RADIO_TEST)
                radio_test_task(now);
            if (app == A_APP) {
                if (cur_app->task && cur_app->task(now))
                    redraw = true;
                display_set_periodic_full(! cur_app->calm || cur_app->calm());
                if (still_pending) {
                    /* The application ends on a page shown like the screensaver (app_show_still()) */
                    still_pending = false;
                    if (cur_app->stop)
                        cur_app->stop();
                    cur_app = NULL;
                    start_saver(A_MENU);
                }
            }
            if (app == A_GAME) {
                if (games_task(now))
                    redraw = true;
                /* No slow full refresh in the middle of an action (Snake, Simon...), only between the rounds */
                display_set_periodic_full(games_calm());
            }
            if (app == A_INFO && was_measuring != radio_tools_measuring())
                redraw = true;
            was_measuring = radio_tools_measuring();

            /* The status in the footer expires */
            if (app == A_MENU && status[0] && absolute_time_diff_us(status_ts, now) > STATUS_MS*1000ll) {
                status[0] = 0;
                redraw = true;
            }

            if (redraw) {
                redraw = false;
                if (app == A_MENU)
                    render_menu(now);
                else if (app == A_BROWSE)
                    render_browser();
                else if (app == A_MUSIC)
                    render_music();
                else if (app == A_BT_FOLDERS)
                    render_bt_folders();
                else if (app == A_BLIND_TEST)
                    render_blind_test();
                else if (app == A_SOCIAL)
                    render_social();
                else if (app == A_NAME_EDIT)
                    render_name_edit();
                else if (app == A_IR)
                    render_ir();
                else if (app == A_IR_RECORD)
                    render_page("Infrarouge", "Enregistrement...\n\nVisez le récepteur (port\ndroit) avec la télécommande\net appuyez sur une touche.",
                                "G : annuler");
                else if (app == A_OLED)
                    render_oled();
                else if (app == A_CTF)
                    render_ctf();
                else if (app == A_CTF_CODE)
                    render_ctf_code();
                else if (app == A_SETTINGS)
                    render_settings();
                else if (app == A_SAVER_IMAGES)
                    render_saver_images();
                else if (app == A_INFO)
                    render_info();
                else if (app == A_CARRIER)
                    render_page("Porteuse", "433,92 MHz + 19 kHz\nen continu, +10 dBm\n\nVisible avec l'analyseur\nde fréquence du Flipper.\nArrêt auto après 30 s.",
                                "G : arrêter");
                else if (app == A_PAGE)
                    render_page(page_title, page_text, "G ou D : retour");
                else if (app == A_GAME)
                    games_render(fb);
                else if (app == A_RADIO_TEST)
                    render_radio_test(now);
                else if (app == A_APP) {
                    gfx_clear(fb, GFX_WHITE);
                    cur_app->render(fb, now);
                }
                else if (app == A_CREDITS) {
                    credits_render(fb, credits_page);
                    ui_trace("Crédits");
                }
                display_show(fb);
            }
            display_task(now);
            break;
        }

        /* PC application: send the screen when it drew something new */
        if (fb_send_now || (fb_stream && screen_shot_counter() != sent_shot)) {
            sent_shot = screen_shot_counter();
            fb_send_now = false;
            send_screen();
        }
    }
}
