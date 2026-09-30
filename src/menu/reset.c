/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Admin > Remise à zéro: the scores and the progress of this badge (before an event, after the tests...), each
 * with a confirmation (long press). The name, the settings, the contact card of the owner are kept. */

#include <stdio.h>
#include <string.h>

#include "app.h"
#include "store.h"

enum { R_SOCIAL, R_GAMES, R_CHALLENGES, R_CONTACTS, R_VIRUS, R_ALL, N_RESETS };
static const char *NAMES[N_RESETS] = {"Scores sociaux", "Records des jeux", "Défis CTF et crypto", "Contacts reçus",
                                      "Virus", "Tout"};
static const char *DETAILS[N_RESETS] = {
    "Le score et les rencontres\ndu réseau des cigales.",
    "Les records des jeux et\ndes casse-têtes.",
    "Les flags du CTF et les\ndéfis crypto résolus.",
    "Les cartes de visite\nreçues (pas votre carte).",
    "L'état du virus : en forme.",
    "Tout cela à la fois (le nom,\nles réglages et votre carte\nsont gardés).",
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
    store_changed();
    if (what == R_CONTACTS || what == R_ALL) {
        store_ext_get()->n_contacts = 0;
        store_ext_changed();
    }
    printf("reset: %s\n", NAMES[what]);
    snprintf(done, sizeof(done), "%s : remis à zéro", NAMES[what]);
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
    ui_title(fb, "Remise à zéro");
    if (confirming) {
        ui_lines(fb, UI_TITLE_H + 10, &gfx_font_medium, NAMES[sel]);
        ui_lines(fb, UI_TITLE_H + 45, &gfx_font_small, DETAILS[sel]);
        ui_lines(fb, UI_TITLE_H + 120, &gfx_font_small, "Remettre à zéro ?");
        ui_footer(fb, "G : non  D long : oui");
        return;
    }
    ui_list(fb, N_RESETS, sel, label);
    ui_footer(fb, done[0] ? done : "G : retour  D : choisir");
}

const app_t app_reset = {
    .name = "Remise à zéro",
    .start = reset_start,
    .buttons = reset_buttons,
    .render = reset_render,
};
