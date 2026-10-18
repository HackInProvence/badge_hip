/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Social > Détecteur de cigales: on, every cigale heard (whatever the distance) is notified (social.c, main.c);
 * the page turns it on or off and lists the last detections (name, signal, how long ago). */

#include <stdio.h>

#include "app.h"
#include "i18n.h"
#include "social.h"

static social_detection_t list[SOCIAL_DETECTIONS];
static int n_list = 0, n_shown = -1;

static void det_start(absolute_time_t now) {
    (void)now;
    n_shown = -1;
}

static bool det_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (b->pressed & UI_BTN_A)
        return false;
    if (b->pressed & UI_BTN_B)
        social_set_detector(! social_detector());
    return true;
}

static bool det_task(absolute_time_t now) {
    static absolute_time_t ts = 0;
    n_list = social_detections(list, SOCIAL_DETECTIONS);
    if (n_list != n_shown || absolute_time_diff_us(ts, now) > 10000000) {  /* "how long ago" every 10 s */
        n_shown = n_list;
        ts = now;
        return true;
    }
    return false;
}

static void det_render(uint8_t *fb, absolute_time_t now) {
    char text[48];
    ui_title(fb, N_("Détecteur de cigales"));
    bool on = social_detector();
    ui_lines(fb, UI_TITLE_H + 4, &gfx_font_medium, on ? N_("Activé") : N_("Désactivé"));
    if (! n_list) {
        ui_wrapped(fb, UI_TITLE_H + 34, &gfx_font_small, on ? N_("Chaque cigale captée, même loin, est signalée "
                   "(son, LEDs, message).") : N_("B : activer. Chaque cigale captée, même loin, sera signalée."), 5);
    } else {
        int y = UI_TITLE_H + 30;
        for (int i = 0; i < n_list && y < UI_FOOTER_Y - 18; ++i, y += 18) {
            int64_t s = absolute_time_diff_us(list[i].at, now) / 1000000;
            if (s < 60)
                snprintf(text, sizeof(text), _("%s %d dBm, %d s"), list[i].name, list[i].rssi, (int)s);
            else
                snprintf(text, sizeof(text), _("%s %d dBm, %d min"), list[i].name, list[i].rssi, (int)(s / 60));
            ui_scroll_text(fb, 6, y, GFX_WIDTH - 12, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_LEFT);
        }
    }
    ui_footer(fb, on ? N_("A : retour  B : désactiver") : N_("A : retour  B : activer"));
}

const app_t app_detector = {
    .name = N_("Détecteur de cigales"),
    .start = det_start,
    .buttons = det_buttons,
    .task = det_task,
    .render = det_render,
};
