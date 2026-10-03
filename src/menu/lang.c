/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Réglages > Langue: the choice of the language of the texts (i18n.c, docs/fr/traduction.md). Each language is
 * written in itself ("English", "Français"), the title in all of them: a badge left in an unknown language can be
 * set back by anyone (and by the sequence of keys of main.c, or the key 'E' of the serial port: English). */

#include <stdio.h>

#include "app.h"
#include "i18n.h"

static int sel = 0;

static void lang_start(absolute_time_t now) {
    (void)now;
    sel = i18n_current();
}

static bool lang_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (b->pressed & UI_BTN_A)
        return false;
    if (b->pressed & UI_BTN_X)
        sel = (sel + 1) % I18N_N_LANGS;
    if (b->pressed & UI_BTN_Y)
        sel = (sel + I18N_N_LANGS - 1) % I18N_N_LANGS;
    if (b->pressed & UI_BTN_B) {
        i18n_set(sel, true);
        return false;  /* Back to the menu, now in this language */
    }
    return true;
}

static void lang_label_row(int i, char *buf, size_t len) {
    /* The names are not translated: "English" stays "English" */
    snprintf(buf, len, "%s %s", i == i18n_current() ? "*" : " ", I18N_LANGS[i].name);
}

static void lang_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    ui_title(fb, "Langue / Language");
    ui_list(fb, I18N_N_LANGS, sel, lang_label_row);
    ui_footer(fb, N_("G : retour  D : choisir"));
}

static void lang_label(char *buf, int len) {
    snprintf(buf, len, _("Langue : %s"), I18N_LANGS[i18n_current()].name);
}

const app_t app_lang = {
    .name = N_("Langue"),
    .label = lang_label,
    .start = lang_start,
    .buttons = lang_buttons,
    .render = lang_render,
};
