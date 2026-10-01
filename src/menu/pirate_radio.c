/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Pirate radio (Admin > Radio pirate): the cicada transmits sound in narrow FM on 433 MHz, for a Portapack, an SDR
 * (NFM) or another cicada (app_pirate_listen, "Écouter la radio pirate"). See docs/fr/radio_pirate.md.
 *
 * Transmitter: the CC1101 in 2-FSK, asynchronous serial mode: it sends f0 + deviation while GDO0 is high,
 * f0 - deviation while it is low, and samples GDO0 at 8 times its data rate (500 kBaud: 4 MHz). The RP2040 drives
 * GDO0 with a PWM of ~122 kHz whose duty cycle follows the samples (audio.c, AUDIO_OUT_RADIO): a receiver with a
 * narrow channel filter (12.5 kHz) only sees the average frequency f0 + deviation * (2 * duty - 1), i.e. the sound
 * in FM. The products of the PWM fall at +-122 kHz (and multiples), far from the channel, low (~-30 dBc).
 * Sources: a melody synthesized here ("Au clair de la lune", no SD card needed), a 1 kHz test tone, a WAV file of
 * the SD card (wav.c). The network of the cicadas is paused while transmitting; the radio is given back when the
 * transmission stops (end of the track, button, page left, safety timeout of 10 minutes).
 *
 * Receiver: the CC1101 of another badge in 2-FSK asynchronous RX (406 kHz channel, data rate 500 kBaud): its
 * demodulator gives on GDO2 a 1 when the frequency is above f0, a 0 below. The PWM of the transmitter is inside this
 * channel, so the bit stream follows it: its duty cycle over each sample period is the sound (compressed by ~2).
 * A PWM slice of the RP2040 counts the clock cycles while GDO2 is high (PWM_DIV_B_HIGH, GDO2 is a "B" pin) and a
 * DMA channel paced at 16 kHz copies its counter to a ring: the difference between two copies is the time high,
 * exact to the cycle, without any CPU. The main loop plays it on the buzzer, measures it (frequency by the zero
 * crossings, purity of 1 kHz by Goertzel, level), corrects the frequency offset between the two crystals (AFC: the
 * average duty cycle must be 50 %) and prints "pirate: rx ..." each second on the USB serial port. */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

#include "app.h"
#include "audio.h"
#include "net.h"
#include "ook_rx.h"
#include "ook_tx.h"
#include "pinouts.h"
#include "radio.h"
#include "radio_tools.h"
#include "sd.h"
#include "wav.h"

#define DATA_RATE 500000  /* The highest of 2-FSK: GDO0 sampled at 4 MHz, ~33 samples per period of the PWM */
#define TX_MAX_MS (10 * 60 * 1000)  /* Safety: the transmission stops after 10 minutes */
#define NET_WAIT_MS 1000  /* A packet of the network on air: wait for its end before taking the radio */
#define SYNTH_RATE 16000
#define TONE_HZ 1000
#define TONE_AMPLITUDE 100  /* Of 127: 78 % of the deviation */
#define MELODY_AMPLITUDE 110
#define MELODY_QUARTER_MS 360
#define MUSIC_DIR "MUSIQUE"
#define MAX_FILES 32
#define TX_REDRAW_MS 5000

/* Channels in the 433.05-434.79 MHz band, with room for the products of the PWM (+-122 kHz) */
static const uint32_t FREQS[] = {433300000, 433650000, 433920000, 434200000, 434500000};
#define N_FREQS ((int)(sizeof(FREQS) / sizeof(FREQS[0])))
#define FREQ_DEFAULT 2

/* PATABLE[0] at 433 MHz (CC1101 datasheet) */
static const struct { int8_t dbm; uint8_t pa; } POWERS[] = {{-20, 0x0E}, {-10, 0x34}, {0, 0x60}, {5, 0x84}, {10, 0xC0}};
#define N_POWERS ((int)(sizeof(POWERS) / sizeof(POWERS[0])))
#define POWER_DEFAULT 1  /* -10 dBm */

static const uint32_t DEVIATIONS[] = {2500, 5000};
#define N_DEVIATIONS 2
#define DEVIATION_DEFAULT 1

/* 2-FSK, asynchronous serial mode (registers over the GFSK profile of radio_tools.c) */
static const uint8_t FM_REGS[] = {
    CC1101_IOCFG0, 0x2E,  /* TX: GDO0 in high impedance, the data input driven by the RP2040 (RX: changed below) */
    CC1101_PKTCTRL0, 0x32,  /* Asynchronous serial mode, infinite length */
    CC1101_FSCTRL1, 0x0C,  /* IF 305 kHz, for the wide channel of the receiver (SmartRF, 500 kBaud) */
    CC1101_MDMCFG4, 0x40,  /* Channel 406 kHz (the data rate exponent is set by radio_set_baud_rate()) */
    CC1101_MDMCFG2, 0x00,  /* 2-FSK, no Manchester, no sync word */
    CC1101_MDMCFG1, 0x00,
    CC1101_FOCCFG, 0x00,  /* No frequency offset compensation: it would follow the sound (AFC done here) */
    CC1101_MCSM0, 0x18,  /* Calibration when leaving IDLE */
    CC1101_FREND1, 0xB6,
    CC1101_FREND0, 0x10,  /* PATABLE[0] */
};

/* The radio: taken from the network and radio_tools.c, given back */
static bool radio_taken = false;

static bool take_radio(void) {
    if (! radio_tools_idle() || ook_rx_active() || ook_tx_busy())
        return false;
    net_pause(true);
    if (! radio_tools_claim()) {
        net_pause(false);
        return false;
    }
    radio_taken = true;
    return true;
}

static void give_radio_back(void) {
    if (! radio_taken)
        return;
    radio_taken = false;
    radio_tools_release();  /* GDO0 and GDO2 as inputs, the GFSK profile */
    net_pause(false);  /* The network configures the radio again and listens */
}

/* DEVIATN for \p hz: fXOSC / 2^17 * (8 + M) * 2^E */
static uint8_t deviatn(uint32_t hz, uint32_t *actual) {
    uint32_t xosc = radio_get_xosc();
    uint32_t best = 0, best_err = UINT32_MAX;
    uint8_t reg = 0;
    for (int e = 0; e < 8; ++e) {
        for (int m = 0; m < 8; ++m) {
            uint32_t f = (uint32_t)(((uint64_t)xosc * (8 + m) << e) >> 17);
            uint32_t err = f > hz ? f - hz : hz - f;
            if (err < best_err) {
                best_err = err;
                best = f;
                reg = (uint8_t)(e << 4 | m);
            }
        }
    }
    *actual = best;
    return reg;
}

/* FREQ2..0 for \p freq_hz, plus \p steps of fXOSC / 2^16 (~400 Hz, the AFC of the receiver) */
static void set_frequency(uint32_t freq_hz, int steps) {
    uint32_t word = (uint32_t)(((uint64_t)freq_hz << 16) / radio_get_xosc()) + steps;
    radio_write_registers((const uint8_t[]){CC1101_FREQ2, (word >> 16) & 0x3F, CC1101_FREQ1, (word >> 8) & 0xFF,
                                            CC1101_FREQ0, word & 0xFF}, 6);
}

/* The common configuration, in IDLE: the GFSK profile (with the correction of radio_tune.c), then the FM */
static uint32_t fm_configure(uint32_t freq_hz, uint32_t deviation_hz) {
    radio_wait_state(CC1101_STATE_IDLE, true);
    radio_tools_reconfigure();
    radio_write_registers(FM_REGS, sizeof(FM_REGS));
    set_frequency(freq_hz, 0);
    radio_set_baud_rate(DATA_RATE);
    uint32_t actual = 0;
    radio_write_registers((const uint8_t[]){CC1101_DEVIATN, deviatn(deviation_hz, &actual)}, 2);
    return actual;
}

static void mhz(char *buf, size_t len, uint32_t hz) {
    snprintf(buf, len, "%lu,%03lu MHz", (unsigned long)(hz / 1000000), (unsigned long)(hz / 1000 % 1000));
}

/* DMA timer fraction X/Y of clk_sys closest to \p rate (same search as audio.c: the same rate exactly) */
static void timer_fraction(uint32_t rate, uint16_t *x, uint16_t *y) {
    uint32_t clk = clock_get_hz(clk_sys);
    uint64_t best_err = UINT64_MAX;
    for (uint32_t i = 1; i < 0x10000; ++i) {
        uint64_t j = ((uint64_t)clk * i + rate / 2) / rate;
        if (j > 0xFFFF)
            break;
        uint64_t f = (uint64_t)clk * i / j;
        uint64_t err = f > rate ? f - rate : rate - f;
        if (err < best_err) {
            best_err = err;
            *x = i;
            *y = j;
        }
        if (err == 0)
            break;
    }
}


/* ====================================== Transmitter ====================================== */

enum { SRC_MELODY, SRC_TONE, SRC_SD, N_SOURCES };
static const char *SOURCES[N_SOURCES] = {"Mélodie", "Tonalité 1 kHz", "Fichier SD"};

enum { ROW_SOURCE, ROW_FREQ, ROW_POWER, ROW_DEVIATION, ROW_MONITOR, ROW_START, N_ROWS };

typedef enum { T_SETUP, T_FILES, T_WAIT, T_ON_AIR } tx_page_t;

static tx_page_t page = T_SETUP;
static int row = ROW_START;
static int source = SRC_MELODY;
static int freq_i = FREQ_DEFAULT;
static int power_i = POWER_DEFAULT;
static int deviation_i = DEVIATION_DEFAULT;
static bool monitor = false;  /* The buzzer also plays the sound */
static char status[48] = "";
static absolute_time_t on_air_ts = 0, wait_ts = 0, redraw_ts = 0;

static char files[MAX_FILES][SD_NAME_MAX];
static int n_files = 0, file_sel = 0;
static char dir[16] = "";
static char path[SD_NAME_MAX + 20] = "";

/* The synthesizer: a sine with an envelope */
static int8_t sine[256];
static uint32_t phase = 0, phase_inc = 0;
static int note = 0;
static uint32_t note_pos = 0, note_len = 0;
static bool synth_done = false;

/* "Au clair de la lune" (traditional, public domain): Hz, quarters */
static const struct { uint16_t hz; uint8_t quarters; } MELODY[] = {
    {523, 1}, {523, 1}, {523, 1}, {587, 1}, {659, 2}, {587, 2}, {523, 1}, {659, 1}, {587, 1}, {587, 1}, {523, 4},
    {523, 1}, {523, 1}, {523, 1}, {587, 1}, {659, 2}, {587, 2}, {523, 1}, {659, 1}, {587, 1}, {587, 1}, {523, 4},
    {587, 1}, {587, 1}, {587, 1}, {587, 1}, {440, 2}, {440, 2}, {587, 1}, {523, 1}, {494, 1}, {440, 1}, {392, 4},
    {523, 1}, {523, 1}, {523, 1}, {587, 1}, {659, 2}, {587, 2}, {523, 1}, {659, 1}, {587, 1}, {587, 1}, {523, 4},
};
#define N_NOTES ((int)(sizeof(MELODY) / sizeof(MELODY[0])))

static void synth_note(int i) {
    note = i;
    note_pos = 0;
    note_len = MELODY[i].quarters * MELODY_QUARTER_MS * (SYNTH_RATE / 1000);
    phase_inc = (uint32_t)(((uint64_t)MELODY[i].hz << 32) / SYNTH_RATE);
}

static void synth_start(void) {
    if (! sine[64])
        for (int i = 0; i < 256; ++i)
            sine[i] = (int8_t)lrintf(127.0f * sinf(6.2831853f * i / 256));
    phase = 0;
    synth_done = false;
    if (source == SRC_TONE) {
        phase_inc = (uint32_t)(((uint64_t)TONE_HZ << 32) / SYNTH_RATE);
        note_len = 0;
    } else {
        synth_note(0);
    }
}

/* Writes the next samples, returns false at the end of the melody */
static bool synth_fill(void) {
    uint8_t buf[256];
    for (int k = 0; k < 4 && ! synth_done; ++k) {
        size_t n = audio_free();
        if (n < 64)
            break;
        if (n > sizeof(buf))
            n = sizeof(buf);
        for (size_t i = 0; i < n; ++i) {
            int amplitude = TONE_AMPLITUDE;
            if (source != SRC_TONE) {
                if (note_pos >= note_len) {
                    if (note + 1 >= N_NOTES) {
                        synth_done = true;
                        n = i;
                        break;
                    }
                    synth_note(note + 1);
                }
                /* Attack 10 ms, release 40 ms: the repeated notes are heard */
                uint32_t attack = SYNTH_RATE / 100, release = SYNTH_RATE / 25, left = note_len - note_pos;
                amplitude = MELODY_AMPLITUDE;
                if (note_pos < attack)
                    amplitude = amplitude * (int)note_pos / (int)attack;
                else if (left < release)
                    amplitude = amplitude * (int)left / (int)release;
                ++note_pos;
            }
            buf[i] = (uint8_t)(128 + sine[phase >> 24] * amplitude / 127);
            phase += phase_inc;
        }
        audio_write(buf, n);
    }
    return ! synth_done;
}

static const char *source_name(void) {
    return source == SRC_SD ? path : SOURCES[source];
}

static void tx_stop(const char *reason) {
    if (page != T_ON_AIR)
        return;
    if (source == SRC_SD)
        wav_stop();
    else
        audio_close();  /* GDO0 stays at 50 % until the radio is in IDLE */
    radio_write_registers((const uint8_t[]){CC1101_SIDLE}, 1);
    audio_set_outputs(AUDIO_OUT_SPEAKER);  /* GDO0 is an input again */
    give_radio_back();
    page = T_SETUP;
    snprintf(status, sizeof(status), "%s", reason);
    printf("pirate: stop (%s) after %lu s\n", reason,
           (unsigned long)(absolute_time_diff_us(on_air_ts, get_absolute_time()) / 1000000));
}

static void tx_begin(absolute_time_t now) {
    if (! take_radio()) {
        page = T_SETUP;
        snprintf(status, sizeof(status), "Radio occupée");
        printf("pirate: radio busy (OOK receiver or another radio feature)\n");
        return;
    }
    uint32_t dev = fm_configure(FREQS[freq_i], DEVIATIONS[deviation_i]);
    radio_set_power(POWERS[power_i].pa);
    audio_set_outputs(AUDIO_OUT_RADIO | (monitor ? AUDIO_OUT_SPEAKER : 0));  /* GDO0: PWM at 50 % */
    bool ok;
    if (source == SRC_SD) {
        ok = wav_start(path);
        if (! ok)
            snprintf(status, sizeof(status), "%s", wav_message());
    } else {
        synth_start();
        ok = audio_open(SYNTH_RATE);
        if (! ok)
            snprintf(status, sizeof(status), "Audio indisponible");
    }
    if (! ok) {
        audio_set_outputs(AUDIO_OUT_SPEAKER);
        give_radio_back();
        page = T_SETUP;
        printf("pirate: error, %s\n", status);
        return;
    }
    radio_write_registers((const uint8_t[]){CC1101_STX}, 1);
    page = T_ON_AIR;
    on_air_ts = now;
    redraw_ts = delayed_by_ms(now, TX_REDRAW_MS);
    status[0] = 0;
    printf("pirate: start on %lu Hz, deviation %lu Hz (DEVIATN for %lu Hz), power %d dBm (PATABLE 0x%02x), "
           "source %s, PWM %lu Hz, data rate %u\n", (unsigned long)FREQS[freq_i], (unsigned long)dev,
           (unsigned long)DEVIATIONS[deviation_i], POWERS[power_i].dbm, POWERS[power_i].pa, source_name(),
           (unsigned long)(clock_get_hz(clk_sys) / (AUDIO_RADIO_WRAP + 1)), DATA_RATE);
}

/* Starts now, or after the packet of the network on air */
static void tx_request(absolute_time_t now) {
    if (net_transmitting()) {
        page = T_WAIT;
        wait_ts = delayed_by_ms(now, NET_WAIT_MS);
        return;
    }
    tx_begin(now);
}

static void load_files(void) {
    n_files = (int)sd_list_files(MUSIC_DIR, ".WAV", files, MAX_FILES);
    snprintf(dir, sizeof(dir), "%s", MUSIC_DIR);
    if (! n_files) {
        n_files = (int)sd_list_files("", ".WAV", files, MAX_FILES);  /* The root, like the music player */
        dir[0] = 0;
    }
    file_sel = 0;
}

static void tx_start(absolute_time_t now) {
    (void)now;
    page = T_SETUP;
    row = ROW_START;
    status[0] = 0;
}

static void tx_app_stop(void) {
    if (page == T_ON_AIR)
        tx_stop("page quittée");
    page = T_SETUP;
}

static void change(int delta) {
    switch (row) {
    case ROW_SOURCE: source = (source + N_SOURCES + delta) % N_SOURCES; break;
    case ROW_FREQ: freq_i = (freq_i + N_FREQS + delta) % N_FREQS; break;
    case ROW_POWER: power_i = (power_i + N_POWERS + delta) % N_POWERS; break;
    case ROW_DEVIATION: deviation_i = (deviation_i + N_DEVIATIONS + delta) % N_DEVIATIONS; break;
    case ROW_MONITOR: monitor = ! monitor; break;
    default: break;
    }
}

static bool tx_buttons(const app_buttons_t *b, absolute_time_t now) {
    switch (page) {
    case T_ON_AIR:
        if (b->long_pressed & UI_BTN_A) {
            tx_stop("arrêt manuel");
            return false;
        }
        if (b->pressed & (UI_BTN_A | UI_BTN_B))
            tx_stop("arrêt manuel");
        return true;
    case T_WAIT:
        if (b->pressed & UI_BTN_A)
            page = T_SETUP;
        return true;
    case T_FILES:
        if (b->pressed & UI_BTN_A) {
            page = T_SETUP;
        } else if (n_files && (b->pressed & (UI_BTN_X | UI_BTN_Y))) {
            file_sel = (file_sel + ((b->pressed & UI_BTN_X) ? 1 : n_files - 1)) % n_files;
        } else if (n_files && (b->pressed & UI_BTN_B)) {
            if (dir[0])
                snprintf(path, sizeof(path), "%s/%s", dir, files[file_sel]);
            else
                snprintf(path, sizeof(path), "%s", files[file_sel]);
            tx_request(now);
        }
        return true;
    default:
        break;
    }
    if (b->pressed & (UI_BTN_X | UI_BTN_Y)) {
        row = (row + ((b->pressed & UI_BTN_X) ? 1 : N_ROWS - 1)) % N_ROWS;
        status[0] = 0;
    }
    if (b->long_pressed & UI_BTN_A)
        return false;
    if (row != ROW_START) {
        if (b->released_short & UI_BTN_A)
            change(-1);
        if (b->pressed & UI_BTN_B)
            change(1);
        return true;
    }
    if (b->pressed & UI_BTN_A)
        return false;
    if (b->pressed & UI_BTN_B) {
        if (source == SRC_SD) {
            load_files();
            page = T_FILES;
        } else {
            tx_request(now);
        }
    }
    return true;
}

static bool tx_task(absolute_time_t now) {
    if (page == T_WAIT) {
        if (! net_transmitting() || absolute_time_diff_us(wait_ts, now) >= 0) {
            tx_begin(now);
            return true;
        }
        return false;
    }
    if (page != T_ON_AIR)
        return false;
    if (source == SRC_SD) {
        if (! wav_task()) {
            tx_stop("fin du morceau");
            return true;
        }
    } else if (! synth_fill() && audio_queued() == 0) {
        tx_stop(source == SRC_TONE ? "fin" : "fin de la mélodie");
        return true;
    }
    if (absolute_time_diff_us(on_air_ts, now) >= TX_MAX_MS * 1000ll) {
        tx_stop("sécurité : 10 min max");
        return true;
    }
    if (absolute_time_diff_us(redraw_ts, now) >= 0) {
        redraw_ts = delayed_by_ms(now, TX_REDRAW_MS);
        radio_state_t st = radio_state();
        if (st != CC1101_STATE_TX)
            printf("pirate: error, the radio left TX (state %d)\n", st);
        return true;  /* The elapsed time */
    }
    return false;
}

static void row_text(int i, char *buf, size_t len) {
    char f[24];
    switch (i) {
    case ROW_SOURCE: snprintf(buf, len, "Source : %s", SOURCES[source]); break;
    case ROW_FREQ: mhz(f, sizeof(f), FREQS[freq_i]); snprintf(buf, len, "Fréquence : %s", f); break;
    case ROW_POWER: snprintf(buf, len, "Puissance : %d dBm", POWERS[power_i].dbm); break;
    case ROW_DEVIATION:
        snprintf(buf, len, "Excursion : %s kHz", DEVIATIONS[deviation_i] == 2500 ? "2,5" : "5");
        break;
    case ROW_MONITOR: snprintf(buf, len, "Haut-parleur : %s", monitor ? "oui" : "non"); break;
    default: snprintf(buf, len, source == SRC_SD ? "> Choisir le fichier" : "> Émettre"); break;
    }
}

static void file_label(int i, char *buf, size_t len) {
    ui_fit_preview(&gfx_font_small, buf, len, files[i], GFX_WIDTH - 16);
}

static void tx_render(uint8_t *fb, absolute_time_t now) {
    char text[64], f[24];
    if (page == T_FILES) {
        ui_title(fb, dir[0] ? "MUSIQUE" : "Carte SD");
        if (n_files)
            ui_list(fb, n_files, file_sel, file_label);
        else
            ui_wrapped(fb, 70, &gfx_font_small, "Aucun fichier .WAV (dossier MUSIQUE)", 3);
        ui_footer(fb, n_files ? "G : retour  D : émettre" : "G : retour");
        return;
    }
    if (page == T_ON_AIR) {
        ui_title(fb, "Radio pirate");
        gfx_fill_rect(fb, 6, UI_TITLE_H + 4, GFX_WIDTH - 12, 40, GFX_BLACK);
        gfx_text(fb, GFX_WIDTH/2, UI_TITLE_H + 8, &gfx_font_large, "ÉMISSION", GFX_WHITE, GFX_ALIGN_CENTER);
        int y = UI_TITLE_H + 50;
        mhz(f, sizeof(f), FREQS[freq_i]);
        snprintf(text, sizeof(text), "%s  NFM", f);
        gfx_text(fb, GFX_WIDTH/2, y, &gfx_font_medium, text, GFX_BLACK, GFX_ALIGN_CENTER);
        y += 24;
        snprintf(text, sizeof(text), "%d dBm, excursion %s kHz", POWERS[power_i].dbm,
                 DEVIATIONS[deviation_i] == 2500 ? "2,5" : "5");
        gfx_text(fb, GFX_WIDTH/2, y, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
        y += 18;
        ui_fit_preview(&gfx_font_small, text, sizeof(text), source == SRC_SD ? files[file_sel] : SOURCES[source],
                       GFX_WIDTH - 8);
        gfx_text(fb, GFX_WIDTH/2, y, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
        y += 18;
        uint32_t s = (uint32_t)(absolute_time_diff_us(on_air_ts, now) / 1000000);
        if (source == SRC_SD && wav_duration_s())
            snprintf(text, sizeof(text), "%lu:%02lu / %lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60),
                     (unsigned long)(wav_duration_s() / 60), (unsigned long)(wav_duration_s() % 60));
        else
            snprintf(text, sizeof(text), "%lu:%02lu (10 min max)", (unsigned long)(s / 60), (unsigned long)(s % 60));
        gfx_text(fb, GFX_WIDTH/2, y, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
        ui_footer(fb, "G : arrêter");
        return;
    }
    ui_title(fb, "Radio pirate");
    int y = UI_TITLE_H + 2;
    for (int i = 0; i < N_ROWS; ++i, y += 18) {
        row_text(i, text, sizeof(text));
        if (i == row) {
            gfx_fill_rect(fb, 2, y, GFX_WIDTH - 4, 18, GFX_BLACK);
            gfx_text(fb, GFX_WIDTH/2, y, &gfx_font_small, text, GFX_WHITE, GFX_ALIGN_CENTER);
        } else {
            gfx_text(fb, GFX_WIDTH/2, y, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
        }
    }
    y += 3;
    if (page == T_WAIT)
        gfx_text(fb, GFX_WIDTH/2, y, &gfx_font_small, "Attente du réseau...", GFX_BLACK, GFX_ALIGN_CENTER);
    else if (status[0])
        ui_wrapped(fb, y, &gfx_font_small, status, 2);
    else
        ui_wrapped(fb, y, &gfx_font_small, "Bande ISM 433 MHz : faible puissance, essais courts", 2);
    ui_footer(fb, row == ROW_START ? "G : retour  D : valider" : "Ailes : -  +");
}

static bool tx_calm(void) {
    return page != T_ON_AIR;
}

const app_t app_pirate_radio = {
    .name = "Radio pirate",
    .start = tx_start,
    .buttons = tx_buttons,
    .task = tx_task,
    .render = tx_render,
    .calm = tx_calm,
    .stop = tx_app_stop,
    .no_saver = true,
};


/* ====================================== Receiver ====================================== */

#define RX_RATE 16000
#define CAP_BITS 12  /* 4096 copies of the counter: 256 ms */
#define CAP_SIZE (1u << CAP_BITS)
#define RX_DEVIATION 5000  /* Only the transmitter uses it (the channel of the receiver is wide) */
#define SQUELCH_DBM (-92)  /* Below: no carrier, the demodulator only gives noise */
#define RSSI_MS 100
#define AFC_BLOCK (RX_RATE / 4)  /* Correction of the frequency every 250 ms */
#define AFC_LIMIT 80  /* Steps of fXOSC / 2^16 (~400 Hz): +-32 kHz, two crystals of +-20 ppm and more */
#define STATS_BLOCK RX_RATE  /* Measures every second */
#define RX_REDRAW_MS 1000

static uint16_t cap_ring[CAP_SIZE] __attribute__((aligned(CAP_SIZE * sizeof(uint16_t))));
static int cap_chan = -1, cap_timer = -1;
static uint cap_slice = 0;
static uint32_t cap_rd = 0;
static uint16_t cap_prev = 0;
static uint32_t period_cycles = 1;

static bool listening = false;
static bool rx_busy = false;  /* The radio was busy at the start */
static bool rx_waiting = false;
static int rx_freq_i = FREQ_DEFAULT;
static bool squelch = true;
static int rssi = -128;
static absolute_time_t rssi_ts = 0, rx_redraw_ts = 0, rx_wait_ts = 0;

static int32_t mean_acc = 0;  /* Average duty cycle (Q16 << 9): the sound is the difference */
static int afc = 0, afc_sign = 1;
static uint32_t afc_n = 0;
static int64_t afc_sum = 0;

/* The measures of the last second (shown) and the sums of the current one */
static struct {
    bool valid;
    bool carrier;
    uint32_t tone_hz;
    uint32_t level_pct;  /* Amplitude of the sound, % of the half range of the duty cycle */
    uint32_t purity_pct;  /* Part of the power at 1 kHz */
    uint32_t duty_pct;
} shown;
static uint32_t st_n = 0, st_crossings = 0;
static int64_t st_sum = 0, st_sum2 = 0;
static float g_s1 = 0, g_s2 = 0, g_coeff = 0;
static bool st_positive = false;

static void rx_reset_stats(void) {
    st_n = st_crossings = 0;
    st_sum = st_sum2 = 0;
    g_s1 = g_s2 = 0;
    afc_n = 0;
    afc_sum = 0;
}

static void rx_retune(void) {
    radio_wait_state(CC1101_STATE_IDLE, true);
    set_frequency(FREQS[rx_freq_i], afc);
    radio_write_registers((const uint8_t[]){CC1101_SRX}, 1);
}

static void rx_end(void) {
    if (! listening)
        return;
    listening = false;
    audio_close();
    if (cap_chan >= 0) {
        dma_channel_abort(cap_chan);
        dma_channel_unclaim(cap_chan);
        cap_chan = -1;
    }
    if (cap_timer >= 0) {
        dma_timer_unclaim(cap_timer);
        cap_timer = -1;
    }
    pwm_set_enabled(cap_slice, false);
    radio_write_registers((const uint8_t[]){CC1101_SIDLE}, 1);
    give_radio_back();
    printf("pirate: rx stop\n");
}

static void rx_begin(absolute_time_t now) {
    rx_waiting = false;
    if (! take_radio()) {
        rx_busy = true;
        printf("pirate: radio busy (OOK receiver or another radio feature)\n");
        return;
    }
    rx_busy = false;
    fm_configure(FREQS[rx_freq_i], RX_DEVIATION);
    /* The demodulated data on GDO2 (the counter of the PWM) and GDO0 (for a scope) */
    radio_write_registers((const uint8_t[]){CC1101_IOCFG2, 0x0D, CC1101_IOCFG0, 0x0D}, 4);

    /* The counter: clk_sys cycles while GDO2 is high */
    cap_slice = pwm_gpio_to_slice_num(BADGE_RADIO_GDO2);
    pwm_config pc = pwm_get_default_config();
    pwm_config_set_clkdiv_mode(&pc, PWM_DIV_B_HIGH);
    pwm_config_set_clkdiv_int(&pc, 1);
    pwm_config_set_wrap(&pc, 0xFFFF);
    pwm_init(cap_slice, &pc, true);
    gpio_set_function(BADGE_RADIO_GDO2, GPIO_FUNC_PWM);
    period_cycles = clock_get_hz(clk_sys) / RX_RATE;

    cap_chan = dma_claim_unused_channel(false);
    cap_timer = dma_claim_unused_timer(false);
    listening = true;
    if (cap_chan < 0 || cap_timer < 0 || pwm_gpio_to_channel(BADGE_RADIO_GDO2) != PWM_CHAN_B) {
        printf("pirate: error, no DMA channel or timer for the receiver\n");
        rx_end();
        rx_busy = true;
        return;
    }
    uint16_t x = 1, y = 0xFFFF;
    timer_fraction(RX_RATE, &x, &y);
    dma_timer_set_fraction(cap_timer, x, y);
    dma_channel_config dc = dma_channel_get_default_config(cap_chan);
    channel_config_set_transfer_data_size(&dc, DMA_SIZE_16);
    channel_config_set_read_increment(&dc, false);
    channel_config_set_write_increment(&dc, true);
    channel_config_set_ring(&dc, true, CAP_BITS + 1);  /* Wrap the write address */
    channel_config_set_dreq(&dc, dma_get_timer_dreq(cap_timer));
    dma_channel_configure(cap_chan, &dc, cap_ring, &pwm_hw->slice[cap_slice].ctr, 0xFFFFFFFF, true);
    cap_rd = 0;
    cap_prev = (uint16_t)pwm_hw->slice[cap_slice].ctr;

    audio_set_outputs(AUDIO_OUT_SPEAKER);
    audio_open(RX_RATE);
    afc = 0;
    afc_sign = 1;
    mean_acc = 32768 << 9;
    g_coeff = 2.0f * cosf(6.2831853f * TONE_HZ / RX_RATE);
    memset(&shown, 0, sizeof(shown));
    rx_reset_stats();
    radio_write_registers((const uint8_t[]){CC1101_SRX}, 1);
    rssi_ts = now;
    printf("pirate: rx start on %lu Hz, channel 406 kHz, data rate %u, sampling %u Hz\n",
           (unsigned long)FREQS[rx_freq_i], DATA_RATE, RX_RATE);
}

static void rx_request(absolute_time_t now) {
    if (net_transmitting()) {
        rx_waiting = true;
        rx_wait_ts = delayed_by_ms(now, NET_WAIT_MS);
        return;
    }
    rx_begin(now);
}

/* The end of a second: the measures */
static void rx_stats(void) {
    float n = (float)st_n;
    float mean = (float)st_sum / n / 65536.0f;
    float var = (float)st_sum2 / n / (65536.0f * 65536.0f);  /* Of the sound, the duty cycle in 0..1 */
    float power = g_s1 * g_s1 + g_s2 * g_s2 - g_coeff * g_s1 * g_s2;
    float purity = var > 0 ? 2.0f * power / (n * n * var) : 0;
    shown.valid = true;
    shown.carrier = rssi >= SQUELCH_DBM;
    shown.tone_hz = (uint32_t)(st_crossings * (float)RX_RATE / n + 0.5f);
    shown.level_pct = (uint32_t)(sqrtf(2.0f * var) * 200.0f + 0.5f);
    shown.purity_pct = (uint32_t)(purity > 1 ? 100 : purity * 100.0f + 0.5f);
    shown.duty_pct = (uint32_t)(mean * 100.0f + 0.5f);
    long afc_hz = (long)((int64_t)afc * radio_get_xosc() >> 16);
    printf("pirate: rx tone %lu Hz, level %lu %%, purity 1 kHz %lu %%, duty %lu %%, rssi %d dBm, afc %+ld Hz%s\n",
           (unsigned long)shown.tone_hz, (unsigned long)shown.level_pct, (unsigned long)shown.purity_pct,
           (unsigned long)shown.duty_pct, rssi, afc_hz, shown.carrier ? "" : " (no carrier)");
    st_n = st_crossings = 0;
    st_sum = st_sum2 = 0;
    g_s1 = g_s2 = 0;
}

/* Every 250 ms with a carrier: the average duty cycle must be 50 %, else the transmitter is off our frequency */
static void rx_afc(void) {
    int32_t err = (int32_t)(afc_sum / (int64_t)afc_n) - 32768;
    afc_n = 0;
    afc_sum = 0;
    if (rssi < SQUELCH_DBM || (err > -2000 && err < 2000))
        return;  /* No carrier, or within 3 % */
    int step = (err > 29500 || err < -29500) ? 5 : 1;  /* Saturated (> 95 %): far away, faster */
    afc += (err > 0 ? step : -step) * afc_sign;
    if (afc > AFC_LIMIT || afc < -AFC_LIMIT) {
        /* Never found: the other polarity of the demodulator */
        afc = 0;
        afc_sign = -afc_sign;
        printf("pirate: rx afc at its limit, polarity %+d\n", afc_sign);
    }
    rx_retune();
}

static void rx_process(void) {
    uint32_t wr = ((uint32_t)dma_hw->ch[cap_chan].write_addr - (uint32_t)(uintptr_t)cap_ring) / sizeof(uint16_t);
    wr %= CAP_SIZE;
    uint32_t n = (wr - cap_rd) % CAP_SIZE;
    if (n > CAP_SIZE - 256) {
        /* The main loop was too slow: start again from the latest copies */
        cap_rd = (wr + CAP_SIZE - 1) % CAP_SIZE;
        cap_prev = cap_ring[cap_rd];
        cap_rd = wr;
        return;
    }
    uint8_t out[256];
    size_t n_out = 0, room = audio_free();
    bool open = ! squelch || rssi >= SQUELCH_DBM;
    while (n--) {
        uint16_t c = cap_ring[cap_rd];
        cap_rd = (cap_rd + 1) % CAP_SIZE;
        uint16_t high = (uint16_t)(c - cap_prev);
        cap_prev = c;
        int32_t d = (int32_t)(((uint32_t)high << 16) / period_cycles);  /* Duty cycle, Q16 */
        if (d > 65536)
            d = 65536;
        mean_acc += d - (mean_acc >> 9);  /* ~5 Hz high-pass for the sound */
        int32_t x = d - (mean_acc >> 9);

        st_sum += d;
        st_sum2 += (int64_t)x * x;
        float g = g_coeff * g_s1 - g_s2 + (float)x / 65536.0f;
        g_s2 = g_s1;
        g_s1 = g;
        /* Zero crossings, with a hysteresis of 1 % */
        if (! st_positive && x > 655) {
            st_positive = true;
            ++st_crossings;
        } else if (st_positive && x < -655) {
            st_positive = false;
        }
        if (++st_n == STATS_BLOCK)
            rx_stats();
        afc_sum += d;
        if (++afc_n == AFC_BLOCK)
            rx_afc();

        if (n_out < room) {
            int s = open ? 128 + x / 128 : 128;
            out[n_out++] = (uint8_t)(s < 0 ? 0 : s > 255 ? 255 : s);
            if (n_out == sizeof(out)) {
                audio_write(out, n_out);
                room -= n_out;
                n_out = 0;
            }
        }
    }
    if (n_out)
        audio_write(out, n_out);
}

static void rx_start(absolute_time_t now) {
    rx_busy = false;
    rx_request(now);
    rx_redraw_ts = delayed_by_ms(now, RX_REDRAW_MS);
}

static void rx_app_stop(void) {
    rx_waiting = false;
    rx_end();
}

static bool rx_buttons(const app_buttons_t *b, absolute_time_t now) {
    if (b->pressed & UI_BTN_A)
        return false;
    if (! listening) {
        if ((b->pressed & UI_BTN_B) && ! rx_waiting)
            rx_request(now);  /* Try again */
        return true;
    }
    if (b->pressed & (UI_BTN_X | UI_BTN_Y)) {
        rx_freq_i = (rx_freq_i + ((b->pressed & UI_BTN_X) ? 1 : N_FREQS - 1)) % N_FREQS;
        afc = 0;
        rx_retune();
        memset(&shown, 0, sizeof(shown));
        rx_reset_stats();
        printf("pirate: rx on %lu Hz\n", (unsigned long)FREQS[rx_freq_i]);
    }
    if (b->pressed & UI_BTN_B)
        squelch = ! squelch;
    return true;
}

static bool rx_task(absolute_time_t now) {
    if (rx_waiting) {
        if (! net_transmitting() || absolute_time_diff_us(rx_wait_ts, now) >= 0) {
            rx_begin(now);
            return true;
        }
        return false;
    }
    if (! listening)
        return false;
    if (absolute_time_diff_us(rssi_ts, now) >= 0) {
        rssi_ts = delayed_by_ms(now, RSSI_MS);
        uint8_t raw = 0;
        radio_read_registers(CC1101_RSSI, &raw, 1);
        rssi = (int8_t)raw / 2 - 74;
    }
    rx_process();
    if (absolute_time_diff_us(rx_redraw_ts, now) >= 0) {
        rx_redraw_ts = delayed_by_ms(now, RX_REDRAW_MS);
        return true;
    }
    return false;
}

static void rx_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    char text[48], f[24];
    ui_title(fb, "Écoute pirate");
    if (! listening) {
        ui_wrapped(fb, 70, &gfx_font_small, rx_waiting ? "Attente du réseau..." : "Radio occupée", 2);
        ui_footer(fb, rx_waiting ? "G : retour" : "G : retour  D : réessayer");
        return;
    }
    int y = UI_TITLE_H + 4;
    mhz(f, sizeof(f), FREQS[rx_freq_i]);
    snprintf(text, sizeof(text), "%s  NFM", f);
    gfx_text(fb, GFX_WIDTH/2, y, &gfx_font_medium, text, GFX_BLACK, GFX_ALIGN_CENTER);
    y += 24;
    snprintf(text, sizeof(text), "Signal : %d dBm", rssi);
    gfx_text(fb, GFX_WIDTH/2, y, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
    y += 17;
    ui_gauge(fb, 20, y, GFX_WIDTH - 40, 8, rssi < -110 ? 0 : rssi > -30 ? 80 : rssi + 110, 80);
    y += 14;
    bool carrier = rssi >= SQUELCH_DBM;
    if (! carrier) {
        gfx_text(fb, GFX_WIDTH/2, y, &gfx_font_small, "Pas d'émission", GFX_BLACK, GFX_ALIGN_CENTER);
    } else {
        long afc_hz = (long)((int64_t)afc * radio_get_xosc() >> 16);
        snprintf(text, sizeof(text), "Correction : %+ld Hz", afc_hz);
        gfx_text(fb, GFX_WIDTH/2, y, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
        y += 18;
        /* Only a measure of a real tone is shown (a clear peak, and a level above the noise) */
        if (shown.valid && shown.carrier && shown.purity_pct >= 50 && shown.level_pct >= 5)
            snprintf(text, sizeof(text), "Tonalité : %lu Hz", (unsigned long)shown.tone_hz);
        else
            snprintf(text, sizeof(text), "Tonalité : -");
        gfx_text(fb, GFX_WIDTH/2, y, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
        y += 18;
        if (shown.valid && shown.carrier)
            snprintf(text, sizeof(text), "Niveau %lu %%  Cycle %lu %%", (unsigned long)shown.level_pct,
                     (unsigned long)shown.duty_pct);
        else
            snprintf(text, sizeof(text), "Niveau : -");
        gfx_text(fb, GFX_WIDTH/2, y, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
    }
    gfx_text(fb, GFX_WIDTH/2, UI_TITLE_H + 131, &gfx_font_small, squelch ? "Silencieux : oui" : "Silencieux : non",
             GFX_BLACK, GFX_ALIGN_CENTER);
    ui_footer(fb, "Flancs : canal  D : silencieux");
}

static bool rx_calm(void) {
    return ! listening;
}

const app_t app_pirate_listen = {
    .name = "Écouter la radio pirate",
    .start = rx_start,
    .buttons = rx_buttons,
    .task = rx_task,
    .render = rx_render,
    .calm = rx_calm,
    .stop = rx_app_stop,
    .no_saver = true,
};
