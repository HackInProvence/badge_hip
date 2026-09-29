/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* 433 MHz receivers (receive only): the decoder lists what is on the air (remotes, sensors),
 * the weather station shows the last measure of each sensor. Both listen with ook_rx.c. */

#include <stdio.h>
#include <string.h>

#include "app.h"
#include "ook_rx.h"

#define HISTORY 8
#define SENSORS 4

typedef struct {
    ookdec_result_t r;
    absolute_time_t at;
    uint16_t repeats;  /* Same frame received again */
} entry_t;

static entry_t history[HISTORY];  /* The newest first */
static int n_history = 0;
static entry_t sensors[SENSORS];  /* One per sensor (protocol, channel, id) */
static int n_sensors = 0;
static uint32_t seen = 0;
static absolute_time_t tick = 0;

static bool same_sensor(const ookdec_result_t *a, const ookdec_result_t *b) {
    return ! strcmp(a->protocol, b->protocol) && a->channel == b->channel && a->id == b->id;
}

static void add(const ookdec_result_t *r, absolute_time_t now) {
    /* The history: the same frame again only counts */
    if (n_history && ! strcmp(history[0].r.text, r->text)) {
        ++history[0].repeats;
        history[0].at = now;
    } else {
        memmove(&history[1], &history[0], sizeof(history[0]) * (HISTORY - 1));
        history[0] = (entry_t){*r, now, 0};
        if (n_history < HISTORY)
            ++n_history;
    }
    if (! r->weather)
        return;
    for (int i = 0; i < n_sensors; ++i)
        if (same_sensor(&sensors[i].r, r)) {
            sensors[i].r = *r;
            sensors[i].at = now;
            return;
        }
    if (n_sensors < SENSORS)
        ++n_sensors;
    memmove(&sensors[1], &sensors[0], sizeof(sensors[0]) * (SENSORS - 1));
    sensors[0] = (entry_t){*r, now, 0};
}

static void rx_start(absolute_time_t now) {
    ook_rx_start();
    tick = now;
}

static void rx_stop(void) {
    ook_rx_stop();
}

static bool rx_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (b->pressed & UI_BTN_A)
        return false;
    if (b->pressed & UI_BTN_B) {
        n_history = 0;  /* Clear */
        n_sensors = 0;
    }
    return true;
}

static bool rx_task(absolute_time_t now) {
    ookdec_result_t r;
    bool redraw = false;
    while (ook_rx_get(&seen, &r)) {
        add(&r, now);
        redraw = true;
    }
    if (absolute_time_diff_us(tick, now) > 5000000) {  /* The ages of the frames */
        tick = now;
        redraw = true;
    }
    return redraw;
}

static void age(char *buf, size_t len, absolute_time_t at, absolute_time_t now) {
    uint32_t s = absolute_time_diff_us(at, now) / 1000000;
    if (s < 60)
        snprintf(buf, len, "%lus", (unsigned long)s);
    else
        snprintf(buf, len, "%lumin", (unsigned long)(s / 60));
}

static void decoder_render(uint8_t *fb, absolute_time_t now) {
    char text[64], a[12];
    ui_title(fb, "Décodeur 433 MHz");
    if (! n_history) {
        ui_lines(fb, 50, &gfx_font_small, "En écoute (réception seule)\ntélécommandes Princeton,\nCAME, Nice FLO, sondes\nmétéo...");
        snprintf(text, sizeof(text), "Impulsions : %lu", (unsigned long)ook_rx_pulses());
        ui_lines(fb, 130, &gfx_font_small, text);
    }
    for (int i = 0; i < n_history; ++i) {
        int y = UI_TITLE_H + 3 + i * 18;
        age(a, sizeof(a), history[i].at, now);
        snprintf(text, sizeof(text), history[i].repeats ? "%s x%u" : "%s", history[i].r.text, history[i].repeats + 1);
        char fitted[64];
        ui_fit(&gfx_font_small, fitted, sizeof(fitted), text, GFX_WIDTH - 40);
        gfx_text(fb, 3, y, &gfx_font_small, fitted, GFX_BLACK, GFX_ALIGN_LEFT);
        gfx_text(fb, GFX_WIDTH - 3, y, &gfx_font_small, a, GFX_BLACK, GFX_ALIGN_RIGHT);
    }
    ui_footer(fb, "G : retour  D : effacer");
}

static void weather_render(uint8_t *fb, absolute_time_t now) {
    char text[48], a[12];
    ui_title(fb, "Station météo");
    if (! n_sensors) {
        ui_lines(fb, 45, &gfx_font_small, "En attente d'une sonde\n433 MHz (Nexus, inFactory,\nThermoPRO, GT-WT02,\nLaCrosse, Acurite...)");
        ui_lines(fb, 125, &gfx_font_small, "Les sondes émettent\ntoutes les 30 à 60 s.");
    }
    /* The newest sensor in big, the others below */
    for (int i = 0; i < n_sensors; ++i) {
        const ookdec_result_t *r = &sensors[i].r;
        int t = r->temp_c10 < 0 ? -r->temp_c10 : r->temp_c10;
        age(a, sizeof(a), sensors[i].at, now);
        if (i == 0) {
            snprintf(text, sizeof(text), "%s%d.%d°C", r->temp_c10 < 0 ? "-" : "", t / 10, t % 10);
            gfx_text(fb, GFX_WIDTH/2, UI_TITLE_H + 6, &gfx_font_large, text, GFX_BLACK, GFX_ALIGN_CENTER);
            if (r->humidity != OOKDEC_NO_HUMIDITY) {
                snprintf(text, sizeof(text), "Humidité %u %%", r->humidity);
                gfx_text(fb, GFX_WIDTH/2, UI_TITLE_H + 36, &gfx_font_medium, text, GFX_BLACK, GFX_ALIGN_CENTER);
            }
            snprintf(text, sizeof(text), "%s ch%u  il y a %s%s", r->protocol, r->channel, a, r->battery_low ? "  pile !" : "");
            char fitted[48];
            ui_fit(&gfx_font_small, fitted, sizeof(fitted), text, GFX_WIDTH - 6);
            gfx_text(fb, GFX_WIDTH/2, UI_TITLE_H + 60, &gfx_font_small, fitted, GFX_BLACK, GFX_ALIGN_CENTER);
            gfx_fill_rect(fb, 10, UI_TITLE_H + 80, GFX_WIDTH - 20, 1, GFX_BLACK);
        } else {
            char fitted[48];
            snprintf(text, sizeof(text), "%s (%s)", r->text, a);
            ui_fit(&gfx_font_small, fitted, sizeof(fitted), text, GFX_WIDTH - 6);
            gfx_text(fb, 3, UI_TITLE_H + 66 + i * 20, &gfx_font_small, fitted, GFX_BLACK, GFX_ALIGN_LEFT);
        }
    }
    ui_footer(fb, "G : retour  D : effacer");
}

const app_t app_decoder = {
    .name = "Décodeur 433 MHz",
    .start = rx_start,
    .buttons = rx_buttons,
    .task = rx_task,
    .render = decoder_render,
    .stop = rx_stop,
    .no_saver = true,
};

const app_t app_weather = {
    .name = "Station météo",
    .start = rx_start,
    .buttons = rx_buttons,
    .task = rx_task,
    .render = weather_render,
    .stop = rx_stop,
    .no_saver = true,
};
