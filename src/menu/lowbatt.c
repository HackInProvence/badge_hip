/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The low battery (battery.h, the degraded mode of main.c):
 * - the warning page, opened when the battery becomes low;
 * - Admin > Vider la batterie: everything that draws current, in a loop (the LEDs white at full power, a radio ping
 *   every RADIO_MS, the screen redrawn every SCREEN_MS, the processor busy), to empty the battery as fast as possible
 *   and test the low battery mode; it stops by itself when the battery is low. */

#include <stdio.h>

#include "pico/time.h"

#include "app.h"
#include "battery.h"
#include "i18n.h"
#include "leds.h"
#include "net.h"

/* ------ The warning ------ */

static void warning_start(absolute_time_t now) {
    (void)now;  /* Nothing to set up, but main.c always calls start() */
}

static bool warning_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    return ! (b->pressed & (UI_BTN_A | UI_BTN_B));
}

static void warning_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    ui_title(fb, N_("Batterie faible"));
    ui_lines(fb, UI_TITLE_H + 12, &gfx_font_medium, N_("Batterie faible !"));
    ui_lines(fb, UI_TITLE_H + 44, &gfx_font_small,
             N_("Mode économie : muet,\nLEDs éteintes,\ntélécommande coupée.\nRecharger le badge\n(USB) pour en sortir."));
    ui_footer(fb, N_("G : retour"));
}

const app_t app_low_battery = {
    .name = N_("Batterie faible"),
    .start = warning_start,
    .buttons = warning_buttons,
    .render = warning_render,
};


/* ------ Admin > Vider la batterie ------ */

#define RADIO_MS 500
#define SCREEN_MS 2000
#define BUSY_US 8000  /* The processor busy at each turn of the main loop */

static bool draining = false;
static absolute_time_t start_ts = 0, radio_ts = 0, screen_ts = 0;
static uint32_t pings = 0;

static void drain_on(bool on) {
    draining = on;
    if (on)
        leds_anim_fixed(LED_RGB(255, 255, 255));  /* Full power, whatever the mute mode */
    else
        app_leds(0, 0, 0);  /* Back to the animation of the badge (none in the low battery mode) */
    printf("drain: %s\n", on ? "on" : "off");
}

static void drain_start(absolute_time_t now) {
    start_ts = radio_ts = screen_ts = now;
    pings = 0;
    drain_on(! battery_low());
}

static bool drain_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (b->pressed & UI_BTN_A)
        return false;
    if ((b->pressed & UI_BTN_B) && ! battery_low())
        drain_on(! draining);
    return true;
}

static bool drain_task(absolute_time_t now) {
    if (draining && battery_low()) {
        drain_on(false);  /* The goal is reached: the degraded mode takes over */
        printf("drain: low battery after %lu min\n", (unsigned long)(absolute_time_diff_us(start_ts, now) / 60000000));
        return true;
    }
    if (! draining)
        return false;
    if (absolute_time_diff_us(radio_ts, now) >= RADIO_MS * 1000ll) {
        radio_ts = now;
        net_ping();
        ++pings;
    }
    busy_wait_us(BUSY_US);
    if (absolute_time_diff_us(screen_ts, now) >= SCREEN_MS * 1000ll) {
        screen_ts = now;
        return true;  /* The screen redrawn: the e-paper draws current too */
    }
    return false;
}

static void drain_render(uint8_t *fb, absolute_time_t now) {
    char text[120];
    ui_title(fb, N_("Vider la batterie"));
    uint32_t s = absolute_time_diff_us(start_ts, now) / 1000000;
    if (draining)
        snprintf(text, sizeof(text), _("LEDs blanches, radio,\nécran, processeur.\nDepuis %lu min %02lu s\n"
                                       "ADC %u, %lu pings"), (unsigned long)(s / 60), (unsigned long)(s % 60),
                 battery_raw(), (unsigned long)pings);
    else if (battery_low())
        snprintf(text, sizeof(text), "%s", _("Batterie faible :\narrêté, le mode\néconomie a pris\nle relais."));
    else
        snprintf(text, sizeof(text), "%s", _("Arrêté.\nD : relancer."));
    ui_lines(fb, UI_TITLE_H + 10, &gfx_font_small, text);
    /* A pattern that changes at each redraw: the whole screen moves */
    if (draining)
        for (int y = 150; y < UI_FOOTER_Y - 4; y += 4)
            gfx_fill_rect(fb, (int)((s * 7 + y) % 40), y, GFX_WIDTH - 40, 2, GFX_BLACK);
    ui_footer(fb, battery_low() ? N_("G : retour") : N_("G : retour  D : marche / arrêt"));
}

static void drain_stop(void) {
    drain_on(false);
}

const app_t app_battery_drain = {
    .name = N_("Vider la batterie"),
    .start = drain_start,
    .buttons = drain_buttons,
    .task = drain_task,
    .render = drain_render,
    .stop = drain_stop,
    .no_saver = true,
    .owns_leds = true,
};
