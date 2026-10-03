/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* La cigale bavarde (épreuve CTF) : la cigale chante un code secret en Morse (buzzer + LEDs). Le joueur le décode
 * puis le rejoue avec les 4 boutons A / B / X / Y (aucun texte à taper). Bon code -> trophée (succès "Cigale
 * bavarde") et le flag SECSEA{...} en récompense.
 *
 * Le code et son Morse sont dans le firmware (il doit bien chanter le code) : l'épreuve est de le DÉCODER à
 * l'oreille / à l'œil, pas de le lire dans un dump. Le flag est obfusqué (invisible avec "strings"), comme ctf.c. */

#include <stdio.h>
#include <string.h>

#include "achievements.h"
#include "app.h"
#include "i18n.h"
#include "store.h"

/* The secret code, as buttons A X Y B A, and its Morse (letters A=.- B=-... X=-..- Y=-.--, space = letter gap).
 * The buttons are labeled like a gamepad: A = left wing, B = right wing, X = right flank, Y = left flank. */
static const uint8_t SECRET[] = {UI_BTN_A, UI_BTN_X, UI_BTN_Y, UI_BTN_B, UI_BTN_A};
#define SECRET_LEN ((int)(sizeof(SECRET) / sizeof(SECRET[0])))
static const char SECRET_MORSE[] = ".- -..- -.-- -... .-";

/* SECSEA{...} XORed with "cigale" and 7*i (nothing readable with "strings"), shown once solved */
static const uint8_t FLAG[] = {
    0x30, 0x2B, 0x2A, 0x27, 0x35, 0x07, 0x32, 0x14, 0x6B, 0x01, 0x69, 0x19,
    0x70, 0x06, 0x49, 0x3B, 0x43, 0x50, 0x29, 0xBA, 0xDF, 0xA0, 0xB2, 0xF7,
    0xB6,
};

#define DOT_MS 150

enum { V_LISTEN, V_ENTER, V_DONE };
static int view;
static uint8_t typed[SECRET_LEN];
static int n_typed;
static bool success;

/* Morse playback, same idea as crypto_app.c */
static const char *morse;
static absolute_time_t morse_ts;
static bool morse_on;

static char flag_buf[sizeof(FLAG) + 1];
static const char *flag_text(void) {
    static const char key[] = "cigale";
    for (size_t i = 0; i < sizeof(FLAG); ++i)
        flag_buf[i] = FLAG[i] ^ key[i % 6] ^ (uint8_t)(i * 7);
    flag_buf[sizeof(FLAG)] = 0;
    return flag_buf;
}

static char letter(uint8_t btn) {
    return btn == UI_BTN_A ? 'A' : btn == UI_BTN_B ? 'B' : btn == UI_BTN_X ? 'X' : 'Y';
}

static void play_from_start(absolute_time_t now) {
    morse = SECRET_MORSE;
    morse_on = false;
    morse_ts = now;
}

static void stop_morse(void) {
    morse = NULL;
    morse_on = false;
    app_leds(0, 0, 0);
}

static void babble_start(absolute_time_t now) {
    n_typed = 0;
    success = achv_unlocked(ACHV_BABBLE);
    if (success) {
        view = V_DONE;  /* Already solved: show the flag */
    } else {
        view = V_LISTEN;
        play_from_start(now);  /* The cicada starts chanting right away */
    }
}

static void babble_stop(void) {
    stop_morse();
}

static bool babble_buttons(const app_buttons_t *b, absolute_time_t now) {
    if (view == V_DONE) {
        if (b->pressed & UI_BTN_A)
            return false;  /* back to the menu */
        return true;
    }
    if (view == V_LISTEN) {
        if (b->pressed & UI_BTN_A)
            return false;  /* left wing: back to the menu */
        if (b->pressed & UI_BTN_X)
            play_from_start(now);  /* right flank: (re)play */
        if (b->pressed & UI_BTN_B) {  /* right wing: enter the code */
            view = V_ENTER;
            n_typed = 0;
            stop_morse();
        }
        return true;
    }
    /* V_ENTER: the 4 buttons are the code; a long press cancels back to the listening */
    if (b->long_pressed) {
        view = V_LISTEN;
        play_from_start(now);
        return true;
    }
    for (uint8_t bit = 0x01; bit <= 0x08; bit <<= 1) {
        if ((b->pressed & bit) && n_typed < SECRET_LEN)
            typed[n_typed++] = bit;
    }
    if (n_typed >= SECRET_LEN) {
        success = !memcmp(typed, SECRET, SECRET_LEN);
        if (success) {
            achv_unlock(ACHV_BABBLE);
            store_save_now();  /* A trophy is kept at once, even if the badge reboots or sleeps right after */
            printf("babble: solved, flag %s\n", flag_text());
            view = V_DONE;
        } else {
            printf("babble: wrong code\n");
            n_typed = 0;
            view = V_LISTEN;
            play_from_start(now);
        }
    }
    return true;
}

static bool babble_task(absolute_time_t now) {
    if (!morse || absolute_time_diff_us(morse_ts, now) < 0)
        return false;
    if (morse_on) {
        app_leds(0, 0, 0);
        morse_on = false;
        morse_ts = delayed_by_ms(now, DOT_MS);
        return false;
    }
    char c = *morse++;
    if (!c) {
        morse = SECRET_MORSE;  /* loop after a long silence */
        morse_ts = delayed_by_ms(now, 10 * DOT_MS);
        return false;
    }
    if (c == '.' || c == '-') {
        uint32_t ms = c == '.' ? DOT_MS : 3 * DOT_MS;
        app_tone(880, ms);
        app_leds(0, 80, 255);
        morse_on = true;
        morse_ts = delayed_by_ms(now, ms);
    } else {
        morse_ts = delayed_by_ms(now, 2 * DOT_MS);  /* letter gap (the sign gap is already done) */
    }
    return false;
}

static void babble_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    char line[40];
    if (view == V_DONE) {
        ui_title(fb, N_("Cigale bavarde"));
        ui_lines(fb, UI_TITLE_H + 10, &gfx_font_small, N_("Bravo !\nCode déchiffré."));
        /* The flag is wider than the screen: show it on two lines, split after the '{' */
        char fl[sizeof(flag_buf) + 2];
        const char *f = flag_text(), *brace = strchr(f, '{');
        if (brace)
            snprintf(fl, sizeof(fl), "%.*s\n%s", (int)(brace - f + 1), f, brace + 1);
        else
            snprintf(fl, sizeof(fl), "%s", f);
        ui_lines(fb, 96, &gfx_font_small, fl);
        ui_footer(fb, N_("G : retour"));
        return;
    }
    if (view == V_LISTEN) {
        ui_title(fb, N_("Cigale bavarde"));
        ui_lines(fb, UI_TITLE_H + 6, &gfx_font_small,
                 N_("La cigale chante un code\nen Morse (bips + yeux).\nDécode-le, puis rejoue-le."));
        ui_footer(fb, N_("Flanc D : réécouter  D : rejouer"));
        return;
    }
    /* V_ENTER */
    ui_title(fb, N_("Rejoue le code"));
    ui_lines(fb, UI_TITLE_H + 4, &gfx_font_small, N_("A: aile G  B: aile D\nX: flanc D  Y: flanc G"));
    int pos = 0;
    for (int i = 0; i < n_typed && pos < (int)sizeof(line) - 3; ++i)
        pos += snprintf(line + pos, sizeof(line) - pos, "%c ", letter(typed[i]));
    gfx_text(fb, GFX_WIDTH / 2, 112, &gfx_font_medium, n_typed ? line : "_", GFX_BLACK, GFX_ALIGN_CENTER);
    ui_footer(fb, N_("Appui long : réécouter"));
}

const app_t app_babble = {
    .name = N_("Cigale bavarde"),
    .start = babble_start,
    .buttons = babble_buttons,
    .task = babble_task,
    .render = babble_render,
    .stop = babble_stop,
    .no_saver = true,
};
