/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Program of the conference, hard-coded (no SD card needed): the list, the details with a QR code. */

#include <stdio.h>
#include <string.h>

#include "app.h"
#include "display.h"
#include "i18n.h"
#include "score_code.h"

typedef struct {
    const char *time;
    const char *title;  /* Lines separated by '\n' (2 at most, ~26 characters each) */
    const char *speaker;
    const char *url;  /* For the QR code */
} talk_t;

/* PROGRAMME À COMPLÉTER : the program of SecSea 2026 is not published yet, these are placeholders */
static const talk_t TALKS[] = {
    {"09:00", N_("Accueil et café"), N_("Equipe Hack In Provence"), "https://www.hackinprovence.fr/"},
    {"09:30", N_("Ouverture de SecSea 2026"), "Hack In Provence", "https://www.hackinprovence.fr/"},
    {"10:00", N_("Talk 1\n(titre à compléter)"), N_("Orateur à confirmer"), "https://www.hackinprovence.fr/"},
    {"11:00", N_("Talk 2\n(titre à compléter)"), N_("Orateur à confirmer"), "https://www.hackinprovence.fr/"},
    {"12:00", N_("Pause déjeuner"), "", "https://www.hackinprovence.fr/"},
    {"14:00", N_("Talk 3\n(titre à compléter)"), N_("Orateur à confirmer"), "https://www.hackinprovence.fr/"},
    {"15:00", N_("Atelier badge :\nhackez votre cigale !"), N_("Equipe du badge"),
     "https://github.com/HackInProvence/badge_hip"},
    {"16:00", N_("Talk 4\n(titre à compléter)"), N_("Orateur à confirmer"), "https://www.hackinprovence.fr/"},
    {"17:00", N_("CTF : remise des prix"), "Hack In Provence", "https://www.hackinprovence.fr/"},
};
#define N_TALKS ((int)(sizeof(TALKS) / sizeof(TALKS[0])))

static int selected = 0;
static bool details = false;

static void talk_label(int i, char *buf, size_t len) {
    char title[48];
    snprintf(title, sizeof(title), "%s", tr(TALKS[i].title));
    char *nl = strchr(title, '\n');
    if (nl)
        *nl = ' ';
    snprintf(buf, len, "%s %s", TALKS[i].time, title);
}

static void program_start(absolute_time_t now) {
    (void)now;
    details = false;
}

static bool program_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (b->pressed & UI_BTN_A) {
        if (details) {
            details = false;
            return true;
        }
        return false;
    }
    if (b->pressed & UI_BTN_Y)
        selected = (selected + N_TALKS - 1) % N_TALKS;
    if (b->pressed & UI_BTN_X)
        selected = (selected + 1) % N_TALKS;
    if (b->pressed & UI_BTN_B)
        details = ! details;
    return true;
}

static void program_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    if (! details) {
        ui_title(fb, N_("Programme"));
        ui_list(fb, N_TALKS, selected, talk_label);
        ui_footer(fb, N_("G : retour  D : détails"));
        return;
    }
    const talk_t *t = &TALKS[selected];
    display_settle_soon();  /* A QR code, read for a while: cleaned like the screensaver */
    char title[32];
    snprintf(title, sizeof(title), _("Programme : %s"), t->time);
    ui_title(fb, title);
    int y = ui_lines(fb, UI_TITLE_H + 4, &gfx_font_medium, t->title);
    if (t->speaker[0])
        y = ui_lines(fb, y, &gfx_font_small, t->speaker);
    /* The QR code of the link, as big as the room left allows */
    int room = UI_FOOTER_Y - 4 - y;
    int size = score_code_draw(NULL, t->url, 0, 1);
    int scale = size ? room / size : 0;
    if (scale > 3)
        scale = 3;
    if (scale >= 2)
        score_code_draw(fb, t->url, y + 2, scale);
    ui_footer(fb, N_("G : liste  Flancs : autre talk"));
}

const app_t app_program = {
    .name = N_("Programme"),
    .start = program_start,
    .buttons = program_buttons,
    .render = program_render,
};

