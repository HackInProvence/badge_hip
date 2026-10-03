/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Admin > Remise à zéro: the scores and the progress of this badge (before an event, after the tests...), each
 * with a confirmation (long press). The name, the settings, the contact card of the owner are kept. */

#include <stdio.h>
#include <string.h>

#include "app.h"
#include "i18n.h"
#include "store.h"

enum { R_SOCIAL, R_GAMES, R_CHALLENGES, R_CONTACTS, R_VIRUS, R_ACHV, R_CARGO, R_ALL, R_ANNOUNCES, N_RESETS };
static const char *NAMES[N_RESETS] = {N_("Scores sociaux"), N_("Records des jeux"), N_("Défis CTF et crypto"),
                                      N_("Contacts reçus"), N_("Virus"), N_("Succès et niveau"), N_("Contrebande"),
                                      N_("Tout"), N_("Annonces d'origine")};
static const char *DETAILS[N_RESETS] = {
    N_("Le score et les rencontres\ndu réseau des cigales."),
    N_("Les records des jeux et\ndes casse-têtes."),
    N_("Les flags du CTF et les\ndéfis crypto résolus."),
    N_("Les cartes de visite\nreçues (pas votre carte)."),
    N_("L'état du virus : en forme."),
    N_("Les succès obtenus et\nleurs compteurs (le niveau\nrepart de 1)."),
    N_("La cale de la contrebande :\nles marchandises sont\ndistribuées à nouveau."),
    N_("Tout cela à la fois (le nom,\nles réglages et votre carte\nsont gardés)."),
    N_("Les 6 annonces du menu admin\nreprennent leurs textes\nd'origine."),
};

static int sel = 0;
static bool confirming = false;
static char done[40] = "";

static void reset(int what) {
    store_t *s = store_get();
    if (what == R_SOCIAL || what == R_ALL) {
        s->score = 0;
        s->n_met = 0;
    }
    if (what == R_GAMES || what == R_ALL) {
        memset(s->game_records, 0xFF, sizeof(s->game_records));  /* 0xFFFF: no record */
        memset(s->puzzle_records, 0xFF, sizeof(s->puzzle_records));
    }
    if (what == R_CHALLENGES || what == R_ALL) {
        s->flags_found = 0;
        s->crypto_solved = 0xFFFF;  /* None */
    }
    if (what == R_VIRUS || what == R_ALL)
        s->infection = 0;
    if (what == R_ACHV || what == R_ALL) {
        s->achievements = 0;
        memset(s->achv_counters, 0, sizeof(s->achv_counters));
    }
    if (what == R_CARGO || what == R_ALL) {
        memset(s->cargo, 0, sizeof(s->cargo));
        s->cargo_seeded = 0;
    }
    if (what == R_ALL)
        s->book_hash = 0;  /* The gamebook starts again */
    store_changed();
    if (what == R_ANNOUNCES) {
        store_ext_get()->announce_magic = 0;  /* The defaults of announce.c at the next use */
        store_ext_changed();
    }
    if (what == R_CONTACTS || what == R_ALL) {
        store_ext_get()->n_contacts = 0;
        store_ext_changed();
    }
    printf("reset: %s\n", NAMES[what]);
    snprintf(done, sizeof(done), _("%s : remis à zéro"), tr(NAMES[what]));
}

static void label(int i, char *buf, size_t len) {
    snprintf(buf, len, "%s", NAMES[i]);
}

static void reset_start(absolute_time_t now) {
    (void)now;
    sel = 0;
    confirming = false;
    done[0] = 0;
}

static bool reset_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (confirming) {
        if (b->long_pressed & UI_BTN_B) {
            reset(sel);
            confirming = false;
        } else if (b->pressed & UI_BTN_A) {
            confirming = false;
        }
        return true;
    }
    if (b->pressed & UI_BTN_A)
        return false;
    if (b->pressed & UI_BTN_Y)
        sel = (sel + N_RESETS - 1) % N_RESETS;
    if (b->pressed & UI_BTN_X)
        sel = (sel + 1) % N_RESETS;
    if (b->pressed & (UI_BTN_X | UI_BTN_Y))
        done[0] = 0;
    if (b->released_short & UI_BTN_B) {
        confirming = true;
        done[0] = 0;
    }
    return true;
}

static void reset_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    ui_title(fb, N_("Remise à zéro"));
    if (confirming) {
        ui_lines(fb, UI_TITLE_H + 10, &gfx_font_medium, NAMES[sel]);
        ui_lines(fb, UI_TITLE_H + 45, &gfx_font_small, DETAILS[sel]);
        ui_lines(fb, UI_TITLE_H + 120, &gfx_font_small, N_("Remettre à zéro ?"));
        ui_footer(fb, N_("G : non  D long : oui"));
        return;
    }
    ui_list(fb, N_RESETS, sel, label);
    ui_footer(fb, done[0] ? done : N_("G : retour  D : choisir"));
}

const app_t app_reset = {
    .name = N_("Remise à zéro"),
    .start = reset_start,
    .buttons = reset_buttons,
    .render = reset_render,
};
