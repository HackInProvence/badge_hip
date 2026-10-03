/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Automatic tuning of the radio: at the first start of the badge (and after an update that brings it), and from
 * Réglages > Réglage radio. Three measures, saved in the store:
 * 1. the crystal of the CC1101 (26 or 27 MHz: the frequency and the data rate depend on it);
 * 2. the noise floor (median of the RSSI when no packet is received): the trigger of the listening to the remotes
 *    is the noise + 15 dB (remote_trigger_dbm()), more sensitive in a quiet place, not fooled in a noisy one;
 * 3. the frequency offset with the other badges heard (FREQEST of their packets): half of the mean offset is
 *    corrected (FSCTRL0, for sending and receiving), so that two badges tuned at the same time converge. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "i18n.h"
#include "net.h"
#include "radio.h"
#include "radio_tools.h"
#include "remote.h"
#include "store.h"

#define NOISE_MS 3000
#define NOISE_POLL_MS 20
#define NOISE_SAMPLES (NOISE_MS / NOISE_POLL_MS)
#define FREQ_MS 10000  /* The beacons of the other badges: one every 2 s */
#define FREQ_MIN_PACKETS 2
#define FREQ_MAX_STEPS 60  /* ~95 kHz: beyond, a wrong measure */

enum { T_XOSC, T_NOISE, T_FREQ, T_DONE };

static int step = T_DONE;
static absolute_time_t step_ts = 0, poll_ts = 0;
static int8_t samples[NOISE_SAMPLES];
static int n_samples = 0;
static bool xosc_started = false;
static int noise_dbm = 0, freq_packets = 0, freq_mean = 0;
static int8_t old_offset = 0, new_offset = 0;
static bool changed = false;
static bool paused = false;  /* The listening windows of the remotes are paused by this page */
static bool measured = false;  /* A tuning was done since the page was opened: its details are shown */

static int cmp_i8(const void *a, const void *b) {
    return *(const int8_t *)a - *(const int8_t *)b;
}

bool radio_tune_needed(void) {
    return store_get()->radio_tuned != STORE_RADIO_TUNED;
}

static void tune_start(absolute_time_t now) {
    step = T_XOSC;
    step_ts = now;
    xosc_started = false;
    n_samples = 0;
    freq_packets = 0;
    if (! paused)
        remote_pause_windows(true);  /* The network listens all the time during the measures */
    paused = true;
    measured = false;
    printf("tune: start\n");
    changed = true;
}

static void tune_stop(void) {
    if (step != T_DONE) {
        printf("tune: stopped\n");
        step = T_DONE;
    }
    if (paused)
        remote_pause_windows(false);
    paused = false;
}

static void finish(void) {
    store_t *s = store_get();
    s->radio_tuned = STORE_RADIO_TUNED;
    s->radio_noise_dbm = noise_dbm;
    s->radio_freq_offset = new_offset;
    store_changed();
    radio_tools_set_freq_offset(new_offset);
    net_reconfigure();
    if (paused)
        remote_pause_windows(false);
    paused = false;
    measured = true;
    step = T_DONE;
    printf("tune: crystal %lu Hz, noise %d dBm (remotes above %d dBm), frequency offset %d -> %d (%d packets, mean %d)\n",
           (unsigned long)radio_get_xosc(), noise_dbm, remote_trigger_dbm(), old_offset, new_offset, freq_packets,
           freq_mean);
}

static bool tune_task(absolute_time_t now) {
    switch (step) {
    case T_XOSC:
        /* The crystal: the measure of radio_tools.c (the radio must be free) */
        if (! xosc_started) {
            if (radio_tools_idle()) {
                radio_tools_measure_xosc();
                xosc_started = true;
            }
        } else if (! radio_tools_measuring() && radio_tools_idle()) {
            step = T_NOISE;
            step_ts = now;
            changed = true;
        }
        break;
    case T_NOISE:
        /* The noise: the RSSI when no packet is being received (the network listens) */
        if (absolute_time_diff_us(poll_ts, now) >= 0 && radio_tools_idle() && ! net_transmitting()
                && absolute_time_diff_us(step_ts, now) > 300000) {  /* The network back in reception first */
            poll_ts = delayed_by_ms(now, NOISE_POLL_MS);
            uint8_t raw = 0;
            radio_read_registers(CC1101_RSSI, &raw, 1);
            if (n_samples < NOISE_SAMPLES)
                samples[n_samples++] = (int8_t)raw / 2 - 74;
        }
        if (n_samples == NOISE_SAMPLES || absolute_time_diff_us(step_ts, now) > 2 * NOISE_MS * 1000ll) {
            qsort(samples, n_samples, 1, cmp_i8);
            noise_dbm = n_samples ? samples[n_samples / 2] : -100;
            int32_t sum;
            net_freq_offsets(&sum, true);  /* The next measure from now */
            step = T_FREQ;
            step_ts = now;
            changed = true;
        }
        break;
    case T_FREQ:
        if (absolute_time_diff_us(step_ts, now) > FREQ_MS * 1000ll) {
            int32_t sum;
            freq_packets = net_freq_offsets(&sum, true);
            old_offset = new_offset = radio_tools_freq_offset();
            freq_mean = 0;
            if (freq_packets >= FREQ_MIN_PACKETS) {
                freq_mean = (sum + (sum >= 0 ? freq_packets / 2 : -freq_packets / 2)) / freq_packets;
                int o = old_offset + freq_mean / 2;  /* Half: the other badges correct too */
                if (abs(freq_mean) <= FREQ_MAX_STEPS)
                    new_offset = o < -FREQ_MAX_STEPS ? -FREQ_MAX_STEPS : o > FREQ_MAX_STEPS ? FREQ_MAX_STEPS : o;
            }
            finish();
            changed = true;
        } else if (absolute_time_diff_us(poll_ts, now) >= 0) {
            poll_ts = delayed_by_ms(now, 1000);
            changed = true;  /* The progress */
        }
        break;
    }
    bool c = changed;
    changed = false;
    return c;
}

static bool tune_buttons(const app_buttons_t *b, absolute_time_t now) {
    if (b->pressed & UI_BTN_A)
        return false;
    if ((b->pressed & UI_BTN_B) && step == T_DONE)
        tune_start(now);
    return true;
}

static void tune_render(uint8_t *fb, absolute_time_t now) {
    ui_title(fb, N_("Réglage radio"));
    char text[200];
    const store_t *s = store_get();
    if (step == T_DONE) {
        if (s->radio_tuned != STORE_RADIO_TUNED) {
            ui_lines(fb, 50, &gfx_font_small, N_("Pas encore réglée."));
        } else {
            snprintf(text, sizeof(text), _("Quartz : %.4f MHz\nBruit : %d dBm\nTélécommandes : > %d dBm\n"
                     "Fréquence : %+d"), radio_get_xosc() / 1e6, s->radio_noise_dbm, remote_trigger_dbm(),
                     s->radio_freq_offset);
            int y = ui_text(fb, 4, UI_TITLE_H + 6, &gfx_font_small, text);
            if (measured) {
                snprintf(text, sizeof(text), freq_packets >= FREQ_MIN_PACKETS ? _("(%d paquets d'autres cigales)")
                         : _("(pas d'autre cigale)"), freq_packets);
                ui_text(fb, 4, y, &gfx_font_small, text);
            }
        }
        ui_footer(fb, N_("D : régler  G : retour"));
        return;
    }
    static const char *STEPS[] = {N_("1. Quartz"), N_("2. Bruit radio"), N_("3. Fréquence")};
    int y = UI_TITLE_H + 8;
    for (int i = 0; i < 3; ++i) {
        snprintf(text, sizeof(text), "%s%s", tr(STEPS[i]), i < step ? _(" : ok") : i == step ? "..." : "");
        y = ui_text(fb, 10, y, &gfx_font_small, text) + 4;
    }
    int total = 1500 + NOISE_MS + FREQ_MS, done = step == T_XOSC ? 0 : step == T_NOISE ? 1500 : 1500 + NOISE_MS;
    if (step == T_FREQ)
        done += (int)(absolute_time_diff_us(step_ts, now) / 1000);
    ui_gauge(fb, 14, y + 6, GFX_WIDTH - 28, 16, done, total);
    ui_lines(fb, y + 30, &gfx_font_small, N_("Restez près d'autres cigales."));
    ui_footer(fb, N_("G : arrêter"));
}

const app_t app_radio_tune = {
    .name = N_("Réglage radio"),
    .start = tune_start,
    .buttons = tune_buttons,
    .task = tune_task,
    .render = tune_render,
    .stop = tune_stop,
    .no_saver = true,
};
