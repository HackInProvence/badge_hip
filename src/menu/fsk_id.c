/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Badge > Identifiant sonore: the badge plays its identifier ("name#id", like the lists of the cicadas) on the
 * buzzer in 4-FSK, FSK_REPEATS times; a phone or a PC decodes it with the page docs/fsk-buzzer-link.html (Écouter).
 * The frame: fsk_link.c. The tones: the PWM of the buzzer at the frequency of the tone (duty cycle 50 %), its period
 * changed at the end of a period (the registers of the RP2040 are double-buffered: the phase stays continuous); a
 * hardware repeating timer gives the FSK_SYMBOL_MS of each symbol (the receiver only syncs at the start of the frame). */

#include <stdio.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/time.h"

#include "app.h"
#include "fsk_link.h"
#include "i18n.h"
#include "net.h"
#include "pinouts.h"
#include "remote.h"
#include "social.h"

#define GAP_SYMBOLS (FSK_GAP_MS / FSK_SYMBOL_MS)

static fsk_frame_t frame;
static int n_syms = 0;
static char text[24];
static volatile int pos = 0, copy = 0;  /* The symbol played (n_syms..: the silence after a copy), the copy */
static volatile bool sending = false;
static bool done = false, muted_refused = false;
static repeating_timer_t timer;
static uint slice;
static uint32_t clk_hz;

/* The PWM of the buzzer at \p hz, 50 % (0: silent) */
static void tone(uint16_t hz) {
    if (! hz) {
        pwm_set_gpio_level(BADGE_BUZZER, 0);
        return;
    }
    uint32_t wrap = clk_hz / hz - 1;  /* clkdiv 1: 125 MHz / 16000 Hz = 7812, within 16 bits */
    pwm_set_wrap(slice, wrap);
    pwm_set_gpio_level(BADGE_BUZZER, (wrap + 1) / 2);
}

/* Every FSK_SYMBOL_MS, in the timer interrupt: the next symbol, the silence between the copies, the end */
static bool next_symbol(repeating_timer_t *t) {
    (void)t;
    int p = ++pos;
    if (p < n_syms) {
        tone(FSK_TONES_HZ[fsk_symbol(&frame, p)]);
        return true;
    }
    tone(0);
    if (p < n_syms + GAP_SYMBOLS)
        return true;
    if (++copy >= FSK_REPEATS) {
        sending = false;
        return false;  /* The timer stops */
    }
    pos = 0;
    tone(FSK_TONES_HZ[fsk_symbol(&frame, 0)]);
    return true;
}

static void stop_sending(void) {
    if (sending)
        cancel_repeating_timer(&timer);
    sending = false;
    pwm_set_enabled(slice, false);
    gpio_init(BADGE_BUZZER);  /* No current in the buzzer */
    gpio_set_dir(BADGE_BUZZER, GPIO_OUT);
    gpio_put(BADGE_BUZZER, 0);
    app_buzzer_claim(false);
    app_leds(0, 0, 0);
}

static void start_sending(void) {
    app_buzzer_claim(true);  /* The other sounds and the cicada stop */
    clk_hz = clock_get_hz(clk_sys);
    slice = pwm_gpio_to_slice_num(BADGE_BUZZER);
    pwm_config pc = pwm_get_default_config();
    pwm_config_set_clkdiv_int(&pc, 1);
    pwm_init(slice, &pc, false);
    gpio_set_function(BADGE_BUZZER, GPIO_FUNC_PWM);
    pos = copy = 0;
    sending = true;
    done = false;
    tone(FSK_TONES_HZ[fsk_symbol(&frame, 0)]);
    pwm_set_enabled(slice, true);
    /* Negative: the period between the starts of the callbacks (no drift from the time they take) */
    add_repeating_timer_us(-FSK_SYMBOL_MS * 1000, next_symbol, NULL, &timer);
    app_leds(0, 0, 80);
    printf("fsk: sending \"%s\", %d symbols, %d ms, %d times\n", text, n_syms, n_syms * FSK_SYMBOL_MS, FSK_REPEATS);
}

static void fsk_start(absolute_time_t now) {
    (void)now;
    snprintf(text, sizeof(text), "%s#%04X", social_name(), (unsigned)(net_id() & 0xFFFF));
    n_syms = fsk_build((const uint8_t *)text, (int)strlen(text), &frame);
    done = muted_refused = false;
}

static bool fsk_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (b->pressed & UI_BTN_A) {
        if (sending) {
            stop_sending();
            printf("fsk: stopped\n");
            return true;
        }
        return false;
    }
    if ((b->pressed & UI_BTN_B) && ! sending) {
        muted_refused = remote_muted();
        if (! muted_refused)
            start_sending();
    }
    return true;
}

static bool fsk_task(absolute_time_t now) {
    (void)now;
    static bool was = false;
    if (was && ! sending && ! done) {
        done = true;  /* The timer stopped by itself: the end of the last copy */
        stop_sending();
        printf("fsk: sent\n");
    }
    bool changed = was != sending;
    static int shown_copy = -1;
    if (sending && copy != shown_copy) {
        shown_copy = copy;
        changed = true;
    }
    was = sending;
    return changed;
}

static void fsk_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    char line[48];
    ui_title(fb, N_("Identifiant sonore"));
    ui_lines(fb, UI_TITLE_H + 8, &gfx_font_medium, text);
    int y;
    if (sending) {
        snprintf(line, sizeof(line), _("Émission %d / %d..."), copy + 1, FSK_REPEATS);
        y = ui_lines(fb, UI_TITLE_H + 40, &gfx_font_small, line);
    } else if (muted_refused) {
        y = ui_lines(fb, UI_TITLE_H + 40, &gfx_font_small, N_("Mode muet : pas de son.\n(Réglages > Mode muet)"));
    } else {
        y = ui_lines(fb, UI_TITLE_H + 40, &gfx_font_small, done ? N_("Envoyé !") : N_("Prêt"));
    }
    snprintf(line, sizeof(line), _("%d tons de %d ms, %.1f s"), n_syms, FSK_SYMBOL_MS, n_syms * FSK_SYMBOL_MS / 1000.0);
    y = ui_lines(fb, y + 6, &gfx_font_small, line);
    ui_wrapped(fb, y + 6, &gfx_font_small, N_("Sur un téléphone ou un PC : la page fsk-buzzer-link.html, "
               "bouton Écouter, près du badge."), 3);  /* 3 lines above the footer: more would scroll */
    ui_footer(fb, sending ? N_("A : arrêter") : N_("A : retour  B : émettre"));
}

static void fsk_stop(void) {
    if (sending)
        stop_sending();
}

const app_t app_fsk_id = {
    .name = N_("Identifiant sonore"),
    .start = fsk_start,
    .buttons = fsk_buttons,
    .task = fsk_task,
    .render = fsk_render,
    .stop = fsk_stop,
    .no_saver = true,
};
