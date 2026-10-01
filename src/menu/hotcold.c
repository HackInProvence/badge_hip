/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Hot / cold: a master badge (admin menu) sends a beacon every second, the other badges find it with the strength
 * of its signal: text, gauge, LEDs from blue to red and beeps faster and faster (like a Geiger counter).
 * The radar (S9) follows a neighbour of the network of the cicadas the same way. */

#include <stdio.h>
#include <string.h>

#include "achievements.h"
#include "app.h"
#include "net.h"
#include "ook_rx.h"
#include "remote.h"
#include "social.h"

#define MASTER_PERIOD_MS 1000
#define LOST_MS 5000  /* No beacon for this long: lost */
#define RSSI_MIN (-100)  /* Far */
#define RSSI_MAX (-45)  /* Next to it (beacon at +10 dBm: ~-70 to -83 at 1 m) */

/* ------ The hot / cold view, shared with the radar ------ */

typedef struct {
    int16_t rssi;  /* Smoothed, dBm */
    absolute_time_t seen;
    bool valid;
    absolute_time_t beep_ts;
    uint32_t lost_ms;  /* Not heard for this long: lost (0: LOST_MS) */
} signal_t;

static void signal_add(signal_t *s, int16_t rssi, absolute_time_t now) {
    s->rssi = s->valid ? (s->rssi * 2 + rssi) / 3 : rssi;  /* Smooth the variations of the RSSI */
    s->seen = now;
    s->valid = true;
}

static bool signal_lost(const signal_t *s, absolute_time_t now) {
    return ! s->valid || absolute_time_diff_us(s->seen, now) > (s->lost_ms ? s->lost_ms : LOST_MS) * 1000ll;
}

/* 0 (far) to 100 (next to it) */
static int signal_level(const signal_t *s) {
    int l = (s->rssi - RSSI_MIN) * 100 / (RSSI_MAX - RSSI_MIN);
    return l < 0 ? 0 : l > 100 ? 100 : l;
}

static const char *signal_word(int level) {
    return level > 85 ? "BRÛLANT !" : level > 65 ? "Chaud" : level > 45 ? "Tiède" : level > 25 ? "Froid" : "Glacial";
}

/* LEDs from blue (cold) to red (hot), beeps faster when closer */
static void signal_feedback(signal_t *s, absolute_time_t now) {
    if (signal_lost(s, now)) {
        app_leds(0, 0, 0);
        return;
    }
    int l = signal_level(s);
    app_leds(l * 255 / 100, 0, (100 - l) * 255 / 100);
    if (absolute_time_diff_us(s->beep_ts, now) >= 0) {
        app_tone(800 + l * 10, 40);
        s->beep_ts = delayed_by_ms(now, 1600 - l * 14);  /* 1.6 s far, 0.2 s close */
    }
}

static void signal_render(uint8_t *fb, const signal_t *s, absolute_time_t now, const char *what) {
    char text[40];
    if (signal_lost(s, now)) {
        gfx_text(fb, GFX_WIDTH/2, 50, &gfx_font_large, "???", GFX_BLACK, GFX_ALIGN_CENTER);
        snprintf(text, sizeof(text), "%s hors de portée", what);
        ui_lines(fb, 90, &gfx_font_small, text);
        return;
    }
    int l = signal_level(s);
    gfx_text(fb, GFX_WIDTH/2, 40, &gfx_font_large, signal_word(l), GFX_BLACK, GFX_ALIGN_CENTER);
    ui_gauge(fb, 14, 80, GFX_WIDTH - 28, 22, l, 100);
    snprintf(text, sizeof(text), "%s : %d dBm", what, s->rssi);
    ui_lines(fb, 112, &gfx_font_small, text);
    ui_lines(fb, 132, &gfx_font_small, "Bougez : suivez la chaleur !");
}


/* ------ Hot / cold hunt ------ */

static bool master = false;
static absolute_time_t master_ts = 0;
static signal_t hunt;
static uint32_t hunt_master = 0;

static void handle_hotcold(const net_packet_t *p) {
    if (hunt_master && p->src != hunt_master)
        return;  /* Another hunt: keep the first master heard */
    hunt_master = p->src;
    signal_add(&hunt, p->rssi, p->at);
    if (signal_level(&hunt) > 85)
        achv_unlock(ACHV_HOTCOLD);  /* "BRÛLANT !": found */
    static absolute_time_t logged = 0;
    if (absolute_time_diff_us(logged, p->at) > 5000000) {
        logged = p->at;
        printf("hotcold: master %08lX at %d dBm\n", (unsigned long)p->src, p->rssi);
    }
}

static void hotcold_init(void) {
    static bool done = false;
    if (! done) {
        net_subscribe(NET_HOTCOLD, handle_hotcold);
        done = true;
    }
}

static void hunt_start(absolute_time_t now) {
    (void)now;
    hotcold_init();
    memset(&hunt, 0, sizeof(hunt));
    hunt_master = 0;
}

static bool hunt_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (b->pressed & UI_BTN_B) {
        memset(&hunt, 0, sizeof(hunt));  /* Look for another master */
        hunt_master = 0;
    }
    return ! (b->pressed & UI_BTN_A);
}

static absolute_time_t hunt_redraw_ts = 0;

static bool hunt_task(absolute_time_t now) {
    signal_feedback(&hunt, now);
    if (absolute_time_diff_us(hunt_redraw_ts, now) < 500000)
        return false;
    hunt_redraw_ts = now;  /* The level changes all the time: 2 pages per second */
    return true;
}

static void hunt_render(uint8_t *fb, absolute_time_t now) {
    ui_title(fb, "Chaud - froid");
    signal_render(fb, &hunt, now, "Balise");
    ui_footer(fb, "G : quitter  D : autre balise");
}

static void hunt_stop(void) {
    app_leds(0, 0, 0);
}

static bool never_calm(void) {
    return false;
}

const app_t app_hotcold = {
    .name = "Chaud - froid",
    .start = hunt_start,
    .buttons = hunt_buttons,
    .task = hunt_task,
    .render = hunt_render,
    .calm = never_calm,
    .stop = hunt_stop,
    .no_saver = true,
};


/* ------ Master beacon (admin menu) ------ */

static void master_start(absolute_time_t now) {
    hotcold_init();
    master = true;
    master_ts = now;
    printf("hotcold: master beacon on\n");
}

static bool master_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (b->pressed & UI_BTN_B) {
        master = ! master;
        printf("hotcold: master beacon %s\n", master ? "on" : "off");
    }
    return ! (b->pressed & UI_BTN_A);
}

static bool master_task(absolute_time_t now) {
    if (master && absolute_time_diff_us(master_ts, now) >= 0) {
        uint8_t data[1] = {1};
        net_send(NET_HOTCOLD, data, sizeof(data), NET_LOUD);
        master_ts = delayed_by_ms(now, MASTER_PERIOD_MS);
    }
    return false;
}

static void master_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    ui_title(fb, "Balise chaud-froid");
    gfx_text(fb, GFX_WIDTH/2, 50, &gfx_font_large, master ? "Emission" : "Arrêtée", GFX_BLACK, GFX_ALIGN_CENTER);
    ui_lines(fb, 95, &gfx_font_small, "Une balise par seconde\n(+10 dBm). Cachez ce badge :\nles autres le cherchent avec\nSocial > Chaud - froid.");
    ui_footer(fb, master ? "G : quitter  D : arrêter" : "G : quitter  D : émettre");
}

static void master_stop(void) {
    master = false;
    printf("hotcold: master beacon off\n");
}

const app_t app_hotcold_master = {
    .name = "Balise chaud-froid",
    .start = master_start,
    .buttons = master_buttons,
    .task = master_task,
    .render = master_render,
    .stop = master_stop,
    .no_saver = true,
};


/* ------ Radar of the cicadas: the neighbours by signal strength, and follow one ------ */

static social_neighbour_t near[SOCIAL_MAX_NEIGHBOURS];
static int n_near = 0, radar_sel = 0;
static uint32_t followed = 0;
static char followed_name[9];
static signal_t follow;
static absolute_time_t radar_ts = 0;

static void radar_label(int i, char *buf, size_t len) {
    char level[8] = "";
    if (near[i].level)  /* Sent by the newer firmwares only */
        snprintf(level, sizeof(level), " N%u", near[i].level);
    snprintf(buf, len, "%s%s %d dBm%s", near[i].name, level, near[i].rssi, near[i].met ? " *" : "");
}

static void radar_start(absolute_time_t now) {
    (void)now;
    followed = 0;
    radar_sel = 0;
    n_near = social_neighbours(near, SOCIAL_MAX_NEIGHBOURS);
    printf("radar: %d cicada(s)%s%s\n", n_near, n_near ? ", the first " : "", n_near ? near[0].name : "");
}

static bool radar_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (b->pressed & UI_BTN_A) {
        if (! followed)
            return false;
        followed = 0;  /* Back to the list */
        app_leds(0, 0, 0);
        return true;
    }
    if (followed)
        return true;
    if (n_near && (b->pressed & UI_BTN_Y))
        radar_sel = (radar_sel + n_near - 1) % n_near;
    if (n_near && (b->pressed & UI_BTN_X))
        radar_sel = (radar_sel + 1) % n_near;
    if (n_near && (b->pressed & UI_BTN_B)) {
        followed = near[radar_sel].id;
        snprintf(followed_name, sizeof(followed_name), "%s", near[radar_sel].name);
        memset(&follow, 0, sizeof(follow));
        signal_add(&follow, near[radar_sel].rssi, now);
    }
    return true;
}

static bool radar_task(absolute_time_t now) {
    if (absolute_time_diff_us(radar_ts, now) < 1000000)
        return false;
    radar_ts = now;
    int before = n_near;
    n_near = social_neighbours(near, SOCIAL_MAX_NEIGHBOURS);
    if (n_near != before)
        printf("radar: %d cicada(s)%s%s\n", n_near, n_near ? ", the first " : "", n_near ? near[0].name : "");
    if (radar_sel >= n_near)
        radar_sel = n_near ? n_near - 1 : 0;
    if (followed) {
        for (int i = 0; i < n_near; ++i)
            if (near[i].id == followed && absolute_time_diff_us(follow.seen, now) > 500000)
                signal_add(&follow, near[i].rssi, now);
        signal_feedback(&follow, now);
    }
    return true;
}

static void radar_render(uint8_t *fb, absolute_time_t now) {
    if (followed) {
        char title[24];
        snprintf(title, sizeof(title), "Radar : %s", followed_name);
        ui_title(fb, title);
        signal_render(fb, &follow, now, followed_name);
        ui_footer(fb, "G : retour à la liste");
        return;
    }
    ui_title(fb, "Radar des cigales");
    if (! n_near)
        ui_lines(fb, 60, &gfx_font_small, "Aucune cigale entendue.\nLes badges proches\napparaissent ici (balises\ntoutes les 2 s).");
    else
        ui_list(fb, n_near, radar_sel, radar_label);
    ui_footer(fb, "G : quitter  D : suivre");
}

static void radar_stop(void) {
    app_leds(0, 0, 0);
}

static bool radar_calm(void) {
    return ! followed;
}

const app_t app_radar = {
    .name = "Radar des cigales",
    .start = radar_start,
    .buttons = radar_buttons,
    .task = radar_task,
    .render = radar_render,
    .calm = radar_calm,
    .stop = radar_stop,
    .no_saver = true,
};


/* ------ Hunt for a 433 MHz transmitter (a remote, a sensor, a jammer that repeats its code) ------
 * The OOK receiver listens all the time: the list of the codes heard, then the hot / cold of the chosen one
 * (the power of its frames, ook_rx_last_rssi()). */

#define TARGETS 8
#define TARGET_LOST_MS 15000  /* A transmitter repeats less often than a badge */

typedef struct {
    char protocol[20];
    uint64_t code;
    char text[32];
    int16_t rssi;  /* Last frame */
    uint16_t frames;
    absolute_time_t seen;
} target_t;

static target_t targets[TARGETS];
static int n_targets = 0, target_sel = 0, hunted = -1;
static uint32_t frames_seen = 0;
static signal_t target_signal;
static absolute_time_t list_ts = 0;

static void hunt433_start(absolute_time_t now) {
    (void)now;
    n_targets = 0;
    target_sel = 0;
    hunted = -1;
    ook_rx_start();  /* All the time, like the decoder page */
    ookdec_result_t r;
    ook_rx_get(&frames_seen, &r);  /* Only the frames from now on */
    printf("hunt433: listening\n");
}

static void hunt433_stop(void) {
    ook_rx_stop();
    app_leds(0, 0, 0);
}

static bool hunt433_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (hunted >= 0) {
        if (b->pressed & UI_BTN_A) {
            hunted = -1;  /* Back to the list */
            app_leds(0, 0, 0);
        }
        return true;
    }
    if (b->pressed & UI_BTN_A)
        return false;
    if (! n_targets)
        return true;
    if (b->pressed & UI_BTN_Y)
        target_sel = (target_sel + n_targets - 1) % n_targets;
    if (b->pressed & UI_BTN_X)
        target_sel = (target_sel + 1) % n_targets;
    if (b->pressed & UI_BTN_B) {
        hunted = target_sel;
        memset(&target_signal, 0, sizeof(target_signal));
        target_signal.lost_ms = TARGET_LOST_MS;
        if (targets[hunted].rssi > -128)
            signal_add(&target_signal, targets[hunted].rssi, now);
        printf("hunt433: hunting %s\n", targets[hunted].text);
    }
    return true;
}

static bool hunt433_task(absolute_time_t now) {
    bool changed = false;
    ookdec_result_t r;
    while (ook_rx_get(&frames_seen, &r)) {
        int rssi = ook_rx_last_rssi();
        int i = 0;
        while (i < n_targets && ! (targets[i].code == r.code && ! strcmp(targets[i].protocol, r.protocol)))
            ++i;
        if (i == n_targets) {
            if (n_targets == TARGETS) {
                /* The list is full: the code heard the longest ago leaves (not the one hunted) */
                int old = -1;
                for (int k = 0; k < n_targets; ++k)
                    if (k != hunted && (old < 0 || absolute_time_diff_us(targets[k].seen, targets[old].seen) > 0))
                        old = k;
                if (old < 0)
                    continue;
                i = old;
            } else {
                ++n_targets;
            }
            memset(&targets[i], 0, sizeof(targets[i]));
            snprintf(targets[i].protocol, sizeof(targets[i].protocol), "%s", r.protocol);
            targets[i].code = r.code;
            /* A short name: the protocol and the code (the text of the decoder also has the te, that varies) */
            snprintf(targets[i].text, sizeof(targets[i].text), "%.12s %0*llX", r.protocol, (r.bits + 3) / 4,
                     (unsigned long long)r.code);
        }
        targets[i].rssi = rssi;
        targets[i].seen = now;
        ++targets[i].frames;
        if (i == hunted && rssi > -128)
            signal_add(&target_signal, rssi, now);
        printf("hunt433: %s at %d dBm\n", r.text, rssi);
        achv_unlock(ACHV_HUNT433);
        changed = true;
    }
    if (hunted >= 0) {
        signal_feedback(&target_signal, now);
        return changed || absolute_time_diff_us(list_ts, now) > 1000000 ? (list_ts = now, true) : false;
    }
    return changed;
}

static void target_label(int i, char *buf, size_t len) {
    snprintf(buf, len, "%s %ddBm x%u", targets[i].text, targets[i].rssi, targets[i].frames);
}

static void hunt433_render(uint8_t *fb, absolute_time_t now) {
    if (hunted >= 0) {
        ui_title(fb, "Chasse 433 MHz");
        signal_render(fb, &target_signal, now, targets[hunted].text);
        ui_footer(fb, "G : retour à la liste");
        return;
    }
    ui_title(fb, "Chasse 433 MHz");
    if (! n_targets) {
        ui_lines(fb, 50, &gfx_font_small, "Écoute des émetteurs\n433 MHz (télécommandes,\ncapteurs, brouilleurs)...");
        ui_footer(fb, "G : retour");
        return;
    }
    ui_list(fb, n_targets, target_sel, target_label);
    ui_footer(fb, "G : retour  D : le chasser");
}

const app_t app_hunt433 = {
    .name = "Chasse 433 MHz",
    .start = hunt433_start,
    .buttons = hunt433_buttons,
    .task = hunt433_task,
    .render = hunt433_render,
    .calm = never_calm,
    .stop = hunt433_stop,
    .no_saver = true,
};
