/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* "Drapeaux" : the CTF flag gallery. Lists every flag ([x] found, [ ] locked); opening a found one shows its
 * SECSEA{...} string to type into the CTFd. The flags are revealed by ctf.c (Konami code, games won, records,
 * multiplayer victories). */

#include <stdio.h>
#include <string.h>

#include "app.h"
#include "ctf.h"
#include "i18n.h"

enum { V_LIST, V_DETAIL };
static int sel, view;

static void list_label(int i, char *buf, size_t len) {
    snprintf(buf, len, ctf_flag_is_found(i) ? "[x] %s" : "[ ] %s", tr(ctf_flag_name(i)));
}

static void flags_start(absolute_time_t now) {
    (void)now;
    view = V_LIST;
}

static bool flags_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    int n = ctf_count();
    if (view == V_LIST) {
        if (b->pressed & UI_BTN_A)
            return false;  /* back to the menu */
        if (b->pressed & UI_BTN_Y)
            sel = (sel + n - 1) % n;
        if (b->pressed & UI_BTN_X)
            sel = (sel + 1) % n;
        if (b->pressed & UI_BTN_B)
            view = V_DETAIL;
    } else if (b->pressed & (UI_BTN_A | UI_BTN_B)) {
        view = V_LIST;
    }
    return true;
}

static void flags_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    char buf[48];
    if (view == V_LIST) {
        snprintf(buf, sizeof(buf), _("Drapeaux : %d / %d"), ctf_found_count(), ctf_count());
        ui_title(fb, buf);
        ui_list(fb, ctf_count(), sel, list_label);
        ui_footer(fb, N_("Flancs : choix  D : voir  G : retour"));
        return;
    }
    ui_title(fb, tr(ctf_flag_name(sel)));
    if (ctf_flag_is_found(sel) && ctf_flag(sel, buf, sizeof(buf))) {
        /* The flag is wider than the screen: show it on two lines, split after the '{' */
        char fl[56];
        const char *brace = strchr(buf, '{');
        if (brace)
            snprintf(fl, sizeof(fl), "%.*s\n%s", (int)(brace - buf + 1), buf, brace + 1);
        else
            snprintf(fl, sizeof(fl), "%s", buf);
        ui_lines(fb, UI_TITLE_H + 12, &gfx_font_small, N_("Flag (à saisir dans CTFd) :"));
        ui_lines(fb, 82, &gfx_font_small, fl);
    } else {
        ui_lines(fb, UI_TITLE_H + 20, &gfx_font_small, N_("Pas encore trouvé.\nGagne ce défi !"));
    }
    ui_footer(fb, N_("G : retour"));
}

const app_t app_flags = {
    .name = N_("Drapeaux"),
    .start = flags_start,
    .buttons = flags_buttons,
    .render = flags_render,
};
