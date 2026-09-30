/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The pages of the cryptography challenges (crypto_ctf.c): the list, a challenge, its hint, the answer typed with
 * the 4 buttons, the Morse played with the buzzer and the LEDs, the pieces of the flag and the final flag. */

#include <stdio.h>
#include <string.h>

#include "app.h"
#include "crypto_ctf.h"
#include "store.h"

#define DOT_MS 150

enum { V_LIST, V_CHALLENGE, V_HINT, V_ANSWER, V_RESULT, V_FLAG };

static int view = V_LIST;
static int sel = 0;
static ui_edit_t edit;
static bool right = false;
/* Morse playback */
static const char *morse = NULL;
static absolute_time_t morse_ts = 0;
static bool morse_on = false;

static uint32_t solved(void) {
    uint16_t s = store_get()->crypto_solved;
    return s == 0xFFFF ? 0 : s;
}

static int n_solved(void) {
    int n = 0;
    for (int i = 0; i < crypto_ctf_count(); ++i)
        n += (solved() >> i) & 1;
    return n;
}

static void list_label(int i, char *buf, size_t len) {
    if (i == crypto_ctf_count()) {
        snprintf(buf, len, "> Le flag final");
        return;
    }
    snprintf(buf, len, "%s %d. %s", (solved() >> i) & 1 ? "[x]" : "[ ]", i + 1, crypto_ctf_title(i));
}

static void crypto_start(absolute_time_t now) {
    (void)now;
    view = V_LIST;
    morse = NULL;
}

static void crypto_stop(void) {
    morse = NULL;
    app_leds(0, 0, 0);
}

static bool crypto_buttons(const app_buttons_t *b, absolute_time_t now) {
    int n = crypto_ctf_count();
    switch (view) {
    case V_LIST:
        if (b->pressed & UI_BTN_A)
            return false;
        if (b->pressed & UI_BTN_Y)
            sel = (sel + n) % (n + 1);
        if (b->pressed & UI_BTN_X)
            sel = (sel + 1) % (n + 1);
        if (b->pressed & UI_BTN_B)
            view = sel == n ? V_FLAG : V_CHALLENGE;
        break;
    case V_CHALLENGE:
        if (b->pressed & UI_BTN_A) {
            morse = NULL;
            view = V_LIST;
        }
        if (b->long_pressed & UI_BTN_B) {
            view = V_HINT;
        } else if (b->released_short & UI_BTN_B) {
            ui_edit_start(&edit, "", 24, UI_CHARSET_UPPER);
            view = V_ANSWER;
        }
        if ((b->pressed & UI_BTN_X) && crypto_ctf_morse(sel)) {
            morse = crypto_ctf_morse(sel);  /* Play it */
            morse_on = false;
            morse_ts = now;
        }
        break;
    case V_ANSWER:
        if (b->long_pressed & UI_BTN_B) {
            char answer[UI_EDIT_MAX + 1];
            ui_edit_result(&edit, answer, sizeof(answer));
            right = crypto_ctf_check(sel, answer);
            if (right) {
                store_get()->crypto_solved = solved() | (1u << sel);
                store_changed();
                app_tone(1319, 300);
            } else {
                app_tone(262, 300);
            }
            printf("crypto: challenge %d %s\n", sel + 1, right ? "solved" : "wrong answer");
            view = V_RESULT;
            break;
        }
        if (b->released_short & UI_BTN_B)
            ui_edit_move(&edit, 1);
        if ((b->pressed & UI_BTN_A) && ! ui_edit_move(&edit, -1))
            view = V_CHALLENGE;
        for (int f = 0; f < 2; ++f) {
            uint8_t bit = f ? UI_BTN_X : UI_BTN_Y;
            static absolute_time_t repeat_ts[2];
            if (b->pressed & bit) {
                ui_edit_change(&edit, f ? 1 : -1);
                repeat_ts[f] = delayed_by_ms(now, 400);
            } else if ((b->held & bit) && absolute_time_diff_us(repeat_ts[f], now) >= 0) {
                ui_edit_change(&edit, f ? 1 : -1);
                repeat_ts[f] = delayed_by_ms(now, 90);
            }
        }
        break;
    case V_HINT:
    case V_FLAG:
        if (b->pressed & (UI_BTN_A | UI_BTN_B))
            view = view == V_HINT ? V_CHALLENGE : V_LIST;
        break;
    default:
        if (b->pressed & (UI_BTN_A | UI_BTN_B))
            view = right ? V_LIST : V_CHALLENGE;
        break;
    }
    return true;
}

/* The Morse: a dot = 1 unit, a dash = 3, 1 unit between the signs, 3 between the letters (space) */
static bool crypto_task(absolute_time_t now) {
    if (! morse || absolute_time_diff_us(morse_ts, now) < 0)
        return false;
    if (morse_on) {
        app_leds(0, 0, 0);
        morse_on = false;
        morse_ts = delayed_by_ms(now, DOT_MS);
        return false;
    }
    char c = *morse++;
    if (! c) {
        morse = NULL;
        return false;
    }
    if (c == '.' || c == '-') {
        uint32_t ms = c == '.' ? DOT_MS : 3 * DOT_MS;
        app_tone(880, ms);
        app_leds(0, 80, 255);
        morse_on = true;
        morse_ts = delayed_by_ms(now, ms);
    } else {
        morse_ts = delayed_by_ms(now, (c == '/' ? 6 : 2) * DOT_MS);  /* The sign gap is already done */
    }
    return false;
}

static void crypto_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    char text[64];
    switch (view) {
    case V_LIST:
        snprintf(text, sizeof(text), "Crypto : %d / %d", n_solved(), crypto_ctf_count());
        ui_title(fb, text);
        ui_list(fb, crypto_ctf_count() + 1, sel, list_label);
        ui_footer(fb, "G : retour  D : ouvrir");
        break;
    case V_CHALLENGE:
        ui_title(fb, crypto_ctf_title(sel));
        ui_text(fb, 2, UI_TITLE_H + 4, &gfx_font_small, crypto_ctf_text(sel));  /* The lines are at most 196 px */
        ui_footer(fb, crypto_ctf_morse(sel) ? "D : répondre  Flanc D : écouter" : "D : répondre  D long : indice");
        break;
    case V_HINT:
        ui_title(fb, "Indice");
        ui_text(fb, 2, UI_TITLE_H + 8, &gfx_font_small, crypto_ctf_hint(sel)[0] ? crypto_ctf_hint(sel) : "Pas d'indice !");
        ui_footer(fb, "G : retour");
        break;
    case V_ANSWER:
        ui_edit_render(fb, &edit, "Réponse", crypto_ctf_title(sel));
        break;
    case V_RESULT: {
        ui_title(fb, crypto_ctf_title(sel));
        if (right) {
            char piece[16] = "";
            crypto_ctf_piece(sel, solved(), piece, sizeof(piece));
            snprintf(text, sizeof(text), "Bravo !\nMorceau du flag :\n%s", piece);
            ui_box(fb, text);
        } else {
            ui_box(fb, "Ce n'est pas ça...");
        }
        ui_footer(fb, "D : continuer");
        break;
    }
    default: {
        ui_title(fb, "Le flag final");
        char flag[CRYPTO_CTF_FLAG_MAX];
        if (crypto_ctf_final_flag(solved(), flag, sizeof(flag))) {
            ui_lines(fb, 50, &gfx_font_small, "Tous les défis sont résolus :");
            ui_lines(fb, 80, &gfx_font_small, flag);
        } else {
            snprintf(text, sizeof(text), "Encore %d défi%s à résoudre.", crypto_ctf_count() - n_solved(),
                     crypto_ctf_count() - n_solved() > 1 ? "s" : "");
            ui_lines(fb, 60, &gfx_font_small, text);
        }
        ui_footer(fb, "G : retour");
        break;
    }
    }
}

const app_t app_crypto = {
    .name = "Défis crypto",
    .start = crypto_start,
    .buttons = crypto_buttons,
    .task = crypto_task,
    .render = crypto_render,
    .stop = crypto_stop,
};
