/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The battery by radio, for the tests of the battery life (a badge left on its battery, away from the USB):
 * - Réglages > Batterie par radio: the battery of this badge, and whether its beacons carry it (social.c: the raw
 *   value of the ADC, the voltage when the badge is calibrated, plugged in USB or not; every ~2 s with its name);
 * - Admin > Batteries des cigales: the batteries of the cicadas heard (and tools/battery_log.py on the serial port:
 *   the "battery: ..." lines of every beacon received).
 * A badge not calibrated (Admin > Batterie (calibration)) gives only its raw ADC value: shown as such, never as a
 * voltage or a percentage it does not know. */

#include <stdio.h>

#include "app.h"
#include "battery.h"
#include "i18n.h"
#include "social.h"

/* ------ Réglages > Batterie par radio ------ */

static absolute_time_t share_ts = 0;

static void share_start(absolute_time_t now) {
    share_ts = now;
}

static bool share_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (b->pressed & UI_BTN_A)
        return false;
    if (b->pressed & UI_BTN_B)
        social_share_battery(! social_battery_shared());
    return true;
}

static bool share_task(absolute_time_t now) {
    if (absolute_time_diff_us(share_ts, now) < 5000000)
        return false;
    share_ts = now;  /* The measure, again every 5 s */
    return true;
}

/* This badge: "3,92 V  81 %" when calibrated, else the raw value of the ADC */
static void own_battery(char *buf, size_t len) {
    if (battery_calibrated() && battery_mv())
        snprintf(buf, len, "%u,%02u V  %d %%", battery_mv() / 1000, battery_mv() % 1000 / 10, battery_percent());
    else if (battery_percent() >= 0)
        snprintf(buf, len, "ADC %u  ~%d %%", battery_raw(), battery_percent());  /* Automatic calibration */
    else
        snprintf(buf, len, _("ADC %u (non calibrée)"), battery_raw());
}

static void share_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    char text[48];
    ui_title(fb, N_("Batterie par radio"));
    snprintf(text, sizeof(text), _("Diffusion : %s"), social_battery_shared() ? _("oui") : _("non"));
    int y = ui_lines(fb, UI_TITLE_H + 8, &gfx_font_medium, text) + 6;
    own_battery(text, sizeof(text));
    y = ui_lines(fb, y, &gfx_font_small, text);
    if (battery_charging())
        y = ui_lines(fb, y, &gfx_font_small, N_("USB : en charge"));
    ui_lines(fb, y + 8, &gfx_font_small, N_("Le nom et la batterie\npartent avec les balises\n(toutes les 2 s)."));
    ui_footer(fb, N_("G : retour  D : oui / non"));
}

static void share_label(char *buf, int len) {
    snprintf(buf, len, _("Batterie radio : %s"), social_battery_shared() ? _("oui") : _("non"));
}

const app_t app_batt_share = {
    .name = N_("Batterie par radio"),
    .label = share_label,
    .start = share_start,
    .buttons = share_buttons,
    .task = share_task,
    .render = share_render,
};


/* ------ Admin > Batteries des cigales ------ */

static social_neighbour_t near[SOCIAL_MAX_NEIGHBOURS];
static int n_near = 0, sel = 0;
static absolute_time_t view_ts = 0;

static int with_battery(void) {
    social_neighbour_t all[SOCIAL_MAX_NEIGHBOURS];
    int n = social_neighbours(all, SOCIAL_MAX_NEIGHBOURS), k = 0;
    for (int i = 0; i < n; ++i)
        if (all[i].batt)
            near[k++] = all[i];
    return k;
}

static void view_start(absolute_time_t now) {
    view_ts = now;
    sel = 0;
    n_near = with_battery();
}

static bool view_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (b->pressed & UI_BTN_A)
        return false;
    if (n_near && (b->pressed & UI_BTN_X))
        sel = (sel + 1) % n_near;
    if (n_near && (b->pressed & UI_BTN_Y))
        sel = (sel + n_near - 1) % n_near;
    return true;
}

static bool view_task(absolute_time_t now) {
    if (absolute_time_diff_us(view_ts, now) < 2000000)
        return false;
    view_ts = now;
    n_near = with_battery();
    if (sel >= n_near)
        sel = n_near ? n_near - 1 : 0;
    return true;
}

/* A cicada: "Rackham  81 %" (calibrated) or "Rackham  ADC 2533", and USB when plugged */
void battradio_text(const social_neighbour_t *c, char *buf, size_t len) {
    if (c->batt_mv)
        snprintf(buf, len, "%d %%%s", battery_percent_of_mv(c->batt_mv), c->batt_usb ? " USB" : "");
    else if (c->batt_est >= 0)
        snprintf(buf, len, "~%d %%%s", c->batt_est, c->batt_usb ? " USB" : "");
    else
        snprintf(buf, len, "ADC %u%s", c->batt_raw, c->batt_usb ? " USB" : "");
}

static void view_label(int i, char *buf, size_t len) {
    char batt[20];
    battradio_text(&near[i], batt, sizeof(batt));
    snprintf(buf, len, "%s  %s", near[i].name, batt);
}

static void view_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    ui_title(fb, N_("Batteries des cigales"));
    if (! n_near)
        ui_lines(fb, 60, &gfx_font_small, N_("Aucune batterie reçue.\nSur le badge à suivre :\nRéglages > Batterie\npar radio : oui."));
    else
        ui_list(fb, n_near, sel, view_label);
    ui_footer(fb, N_("G : retour"));
}

const app_t app_batt_view = {
    .name = N_("Batteries des cigales"),
    .start = view_start,
    .buttons = view_buttons,
    .task = view_task,
    .render = view_render,
};
