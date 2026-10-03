/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Lamp: the LEDs in white, the flanks change the brightness (kept in the flash) */

#include <stdio.h>

#include "app.h"
#include "i18n.h"
#include "leds.h"
#include "remote.h"
#include "store.h"

#define LAMP_DEFAULT 50
#define LAMP_STEP 10
#define LAMP_REPEAT_MS 250

static bool on = true;
static absolute_time_t repeat_ts = 0;

static int percent(void) {
    uint8_t p = store_get()->lamp_percent;
    return p == 0 || p > 100 ? LAMP_DEFAULT : p;
}

static void apply(void) {
    int v = on ? percent() * 255 / 100 : 0;
    app_leds(v, v, v);
}

static void change(int delta) {
    int p = percent() + delta;
    store_get()->lamp_percent = p < LAMP_STEP ? LAMP_STEP : p > 100 ? 100 : p;
    store_changed();
    on = true;
    apply();
}

static void lamp_start(absolute_time_t now) {
    (void)now;
    on = true;
    leds_set_brightness(100);  /* The lamp has its own brightness, not the one of Badge > Luminosité LEDs */
    apply();
}

static bool lamp_buttons(const app_buttons_t *b, absolute_time_t now) {
    if (b->pressed & UI_BTN_A)
        return false;
    if (b->pressed & UI_BTN_B) {
        on = ! on;
        apply();
    }
    /* Flanks: -/+ one step, repeated while held */
    for (int f = 0; f < 2; ++f) {
        uint8_t bit = f ? UI_BTN_X : UI_BTN_Y;
        if (b->pressed & bit) {
            change(f ? LAMP_STEP : -LAMP_STEP);
            repeat_ts = delayed_by_ms(now, 2 * LAMP_REPEAT_MS);
        } else if ((b->held & bit) && absolute_time_diff_us(repeat_ts, now) >= 0) {
            change(f ? LAMP_STEP : -LAMP_STEP);
            repeat_ts = delayed_by_ms(now, LAMP_REPEAT_MS);
        }
    }
    return true;
}

static void lamp_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    char text[32];
    ui_title(fb, N_("Lampe"));
    snprintf(text, sizeof(text), on ? "%d %%" : _("Eteinte"), percent());
    gfx_text(fb, GFX_WIDTH/2, 50, &gfx_font_large, text, GFX_BLACK, GFX_ALIGN_CENTER);
    ui_gauge(fb, 20, 90, GFX_WIDTH - 40, 16, on ? percent() : 0, 100);
    if (remote_muted())
        ui_lines(fb, 118, &gfx_font_small, N_("Mode muet : LEDs coupées\n(Réglages)"));
    else
        ui_lines(fb, 118, &gfx_font_small, N_("Flancs : - / +\n(maintenir : vite)"));
    ui_footer(fb, on ? N_("G : quitter  D : éteindre") : N_("G : quitter  D : allumer"));
}

static void lamp_stop(void) {
    leds_set_brightness(led_brightness());
    app_leds(0, 0, 0);
}

static void lamp_label(char *buf, int len) {
    snprintf(buf, len, _("Lampe : %d %%"), percent());
}

const app_t app_lamp = {
    .name = N_("Lampe"),
    .label = lamp_label,
    .start = lamp_start,
    .buttons = lamp_buttons,
    .render = lamp_render,
    .stop = lamp_stop,
};
