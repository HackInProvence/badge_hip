/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Calibration of the battery measure (Admin > Batterie (calibration)): the voltage measured with a multimeter is
 * entered with the wings and saved as a point with the ADC value of that moment, once charging and once on battery
 * (battery.h). The points are factory settings: kept through the resets and the new firmwares. */

#include <stdio.h>

#include "app.h"
#include "battery.h"
#include "i18n.h"
#include "store.h"

#define MV_MIN 2500
#define MV_MAX 4500
#define MV_STEP 10

enum { ROW_MV, ROW_SAVE, ROW_CLEAR, N_ROWS };

static int row = 0;
static uint16_t entered_mv = 0;
static uint16_t shown_raw = 0;
static bool confirm = false;  /* "Effacer" pressed once: a second press clears */
static absolute_time_t repeat_ts[2] = {0, 0};
static char status[40] = "";

static void battcal_start(absolute_time_t now) {
    (void)now;
    row = 0;
    confirm = false;
    status[0] = 0;
    entered_mv = battery_mv() ? battery_mv() / MV_STEP * MV_STEP : battery_charging() ? 4200 : 3800;
    shown_raw = battery_raw();
}

/* +1 / -1, faster while held */
static int change(const app_buttons_t *b, absolute_time_t now) {
    for (int w = 0; w < 2; ++w) {
        uint8_t bit = w ? UI_BTN_B : UI_BTN_A;
        int sign = w ? 1 : -1;
        if (b->pressed & bit) {
            repeat_ts[w] = delayed_by_ms(now, 400);
            return sign;
        }
        if ((b->held & bit) && absolute_time_diff_us(repeat_ts[w], now) >= 0) {
            repeat_ts[w] = delayed_by_ms(now, 80);
            return sign * (app_held_ms(b, bit) > 2000 ? 10 : 1);
        }
    }
    return 0;
}

static bool battcal_buttons(const app_buttons_t *b, absolute_time_t now) {
    if (b->pressed & (UI_BTN_X | UI_BTN_Y)) {
        row = (row + ((b->pressed & UI_BTN_X) ? 1 : N_ROWS - 1)) % N_ROWS;
        confirm = false;
        status[0] = 0;
    }
    if (b->long_pressed & UI_BTN_A)
        return false;
    if (row == ROW_MV) {
        int c = change(b, now);
        if (c) {
            int mv = entered_mv + c * MV_STEP;
            entered_mv = mv < MV_MIN ? MV_MIN : mv > MV_MAX ? MV_MAX : mv;
        }
        return true;
    }
    if (b->pressed & UI_BTN_A)
        return false;
    if (! (b->pressed & UI_BTN_B))
        return true;
    if (row == ROW_SAVE) {
        uint16_t raw = battery_raw();
        if (! raw)
            snprintf(status, sizeof(status), _("Pas encore de mesure"));
        else if (battery_set_point(entered_mv, raw))
            snprintf(status, sizeof(status), battery_calibrated() ? _("Point enregistré") : _("Il faut un 2e point"));
        else
            snprintf(status, sizeof(status), _("Erreur d'écriture"));
    } else if (! confirm) {
        confirm = true;
        snprintf(status, sizeof(status), _("D encore : effacer"));
    } else {
        confirm = false;
        snprintf(status, sizeof(status), battery_clear_points() ? _("Calibration effacée") : _("Erreur d'écriture"));
    }
    return true;
}

static bool battcal_task(absolute_time_t now) {
    (void)now;
    if (battery_raw() == shown_raw)
        return false;
    shown_raw = battery_raw();  /* Live: a new measure every 2 s */
    return true;
}

static void volts(char *buf, size_t len, uint16_t mv) {
    snprintf(buf, len, "%u,%02u V", mv / 1000, mv % 1000 / 10);
}

static void battcal_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    char text[48], v[16];
    const store_factory_t *f = store_factory_get();
    ui_title(fb, N_("Batterie"));
    int y = UI_TITLE_H + 3;
    snprintf(text, sizeof(text), "ADC : %u%s", shown_raw, battery_charging() ? " (USB)" : "");
    gfx_text(fb, GFX_WIDTH/2, y, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
    y += 18;
    if (battery_mv()) {
        volts(v, sizeof(v), battery_mv());
        snprintf(text, sizeof(text), _("Mesure : %s  %d %%"), v, battery_percent());
    } else {
        snprintf(text, sizeof(text), _("Mesure : non calibrée"));
    }
    gfx_text(fb, GFX_WIDTH/2, y, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
    for (int i = 0; i < 2; ++i) {
        y += 18;
        if (f->battery_mv[i]) {
            volts(v, sizeof(v), f->battery_mv[i]);
            snprintf(text, sizeof(text), _("Point %d : %s à %u"), i + 1, v, f->battery_raw[i]);
        } else {
            snprintf(text, sizeof(text), _("Point %d : -"), i + 1);
        }
        gfx_text(fb, GFX_WIDTH/2, y, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
    }
    y += 24;
    for (int i = 0; i < N_ROWS; ++i, y += 22) {
        volts(v, sizeof(v), entered_mv);
        if (i == ROW_MV)
            snprintf(text, sizeof(text), _("Multimètre : %s"), v);
        else
            snprintf(text, sizeof(text), i == ROW_SAVE ? _("> Enregistrer le point") : _("> Effacer"));
        if (i == row) {
            gfx_fill_rect(fb, 4, y - 2, GFX_WIDTH - 8, 21, GFX_BLACK);
            gfx_text(fb, GFX_WIDTH/2, y, &gfx_font_small, text, GFX_WHITE, GFX_ALIGN_CENTER);
        } else {
            gfx_text(fb, GFX_WIDTH/2, y, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
        }
    }
    ui_footer(fb, status[0] ? status : row == ROW_MV ? N_("Ailes : -  +") : N_("G : retour  D : valider"));
}

const app_t app_battcal = {
    .name = N_("Batterie (calibration)"),
    .start = battcal_start,
    .buttons = battcal_buttons,
    .task = battcal_task,
    .render = battcal_render,
};
