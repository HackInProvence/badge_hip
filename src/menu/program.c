/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Program of the conference, hard-coded (no SD card needed): the list, the details with a QR code.
 * The remote command 0x20 + n (Flipper: Princeton key 0xC16A2n) shows the talk n of the program of the badge.
 * Admin > Annoncer un talk sends the content of the talk as an announcement (announce.c): the badges show what
 * they received. */

#include <stdio.h>
#include <string.h>

#include "announce.h"
#include "app.h"
#include "remote.h"
#include "score_code.h"

typedef struct {
    const char *time;
    const char *title;  /* Lines separated by '\n' (2 at most, ~26 characters each) */
    const char *speaker;
    const char *url;  /* For the QR code */
} talk_t;

/* PROGRAMME À COMPLÉTER : the program of SecSea 2026 is not published yet, these are placeholders */
static const talk_t TALKS[] = {
    {"09:00", "Accueil et café", "Equipe Hack In Provence", "https://www.hackinprovence.fr/"},
    {"09:30", "Ouverture de SecSea 2026", "Hack In Provence", "https://www.hackinprovence.fr/"},
    {"10:00", "Talk 1\n(titre à compléter)", "Orateur à confirmer", "https://www.hackinprovence.fr/"},
    {"11:00", "Talk 2\n(titre à compléter)", "Orateur à confirmer", "https://www.hackinprovence.fr/"},
    {"12:00", "Pause déjeuner", "", "https://www.hackinprovence.fr/"},
    {"14:00", "Talk 3\n(titre à compléter)", "Orateur à confirmer", "https://www.hackinprovence.fr/"},
    {"15:00", "Atelier badge :\nhackez votre cigale !", "Equipe du badge", "https://github.com/HackInProvence/badge_hip"},
    {"16:00", "Talk 4\n(titre à compléter)", "Orateur à confirmer", "https://www.hackinprovence.fr/"},
    {"17:00", "CTF : remise des prix", "Hack In Provence", "https://www.hackinprovence.fr/"},
};
#define N_TALKS ((int)(sizeof(TALKS) / sizeof(TALKS[0])))

static int selected = 0;
static bool details = false;
static bool announced = false;  /* Opened by a remote command */
static int pending = -1;  /* Talk announced, to show */

static void talk_label(int i, char *buf, size_t len) {
    char title[48];
    snprintf(title, sizeof(title), "%s", TALKS[i].title);
    char *nl = strchr(title, '\n');
    if (nl)
        *nl = ' ';
    snprintf(buf, len, "%s %s", TALKS[i].time, title);
}

static void program_start(absolute_time_t now) {
    (void)now;
    if (pending >= 0) {
        selected = pending;
        details = true;
        announced = true;
        pending = -1;
    } else {
        details = false;
        announced = false;
    }
}

static bool program_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (b->pressed & UI_BTN_A) {
        if (details && ! announced) {
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
        ui_title(fb, "Programme");
        ui_list(fb, N_TALKS, selected, talk_label);
        ui_footer(fb, "G : retour  D : détails");
        return;
    }
    const talk_t *t = &TALKS[selected];
    char title[32];
    snprintf(title, sizeof(title), announced ? "Prochain : %s" : "Programme : %s", t->time);
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
    ui_footer(fb, announced ? "G : fermer" : "G : liste  Flancs : autre talk");
}

/* Remote command 0x20 + n: show the talk n */
static void program_remote(uint8_t arg) {
    if (arg >= N_TALKS)
        return;
    pending = arg;
    printf("program: talk %d announced\n", arg);
}

void program_init(void) {
    remote_subscribe(REMOTE_PROGRAM, program_remote);
}

/* A talk was announced: the main loop opens the program (or shows a status when busy) */
bool program_announced(char *buf, int len) {
    if (pending < 0)
        return false;
    char title[48];
    snprintf(title, sizeof(title), "%s", TALKS[pending].title);
    char *nl = strchr(title, '\n');
    if (nl)
        *nl = ' ';
    snprintf(buf, len, "%s %s", TALKS[pending].time, title);
    return true;
}

void program_forget(void) {
    pending = -1;
}

const app_t app_program = {
    .name = "Programme",
    .start = program_start,
    .buttons = program_buttons,
    .render = program_render,
};


/* ------ Admin: announce a talk to all the badges ------ */

static void announce_start(absolute_time_t now) {
    (void)now;
}

static bool announce_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (b->pressed & UI_BTN_A)
        return false;
    if (b->pressed & UI_BTN_Y)
        selected = (selected + N_TALKS - 1) % N_TALKS;
    if (b->pressed & UI_BTN_X)
        selected = (selected + 1) % N_TALKS;
    if (b->pressed & UI_BTN_B) {
        /* The content of the talk is sent (the cicadas build the screen from it): an announcement */
        const talk_t *t = &TALKS[selected];
        store_announce_t a = {.qr_type = ANNOUNCE_QR_URL};
        snprintf(a.time, sizeof(a.time), "%s", t->time);
        snprintf(a.text, sizeof(a.text), "%s%s%s", t->title, t->speaker[0] ? " - " : "", t->speaker);
        for (char *c = a.text; *c; ++c)
            if (*c == '\n')
                *c = ' ';
        snprintf(a.qr, sizeof(a.qr), "%s", t->url);
        announce_send(&a);
        printf("program: talk %d announced\n", selected);
    }
    return true;
}

static void announce_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    ui_title(fb, "Annoncer un talk");
    ui_list(fb, N_TALKS, selected, talk_label);
    ui_footer(fb, "G : retour  D : annoncer à tous");
}

const app_t app_program_announce = {
    .name = "Annoncer un talk",
    .start = announce_start,
    .buttons = announce_buttons,
    .render = announce_render,
};
