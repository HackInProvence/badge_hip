/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Talk badge: a badge in front of the speaker shows the time left with its LEDs (and its screen), changed with
 * a remote (a Flipper Zero, Princeton key 0xC16A10 to 0xC16A14, or an admin badge): off, green (all right),
 * orange breathing (5 minutes left), red blinking (time is over), angry red (faster, with the buzzer).
 * It ignores the mute mode: it is made for the talks. It listens to the OOK remotes all the time (not only a moment
 * every second like the other pages): the network of the cicadas is paused meanwhile. */

#include <stdio.h>

#include "app.h"
#include "audio.h"
#include "noise_gen.h"
#include "leds.h"
#include "ook_rx.h"
#include "remote.h"


enum { TALK_OFF, TALK_GREEN, TALK_ORANGE, TALK_RED, TALK_ANGRY, TALK_STATES };

static const char *NAMES[TALK_STATES] = {"Eteint", "OK", "5 min", "FINI", "STOP !"};
static const char *DETAILS[TALK_STATES] = {"LEDs éteintes", "Vert : tout va bien", "Orange : il reste 5 min",
                                           "Rouge : temps écoulé", "Rouge énervé : on conclut !"};
static int state = TALK_OFF;
static bool active = false;
static bool changed = false;

static void apply(void) {
    switch (state) {
    case TALK_GREEN: leds_anim_fixed(LED_RGB(0, 255, 0)); break;
    case TALK_ORANGE: leds_anim_breath(LED_RGB(255, 80, 0), 3000000); break;  /* Soft fading */
    case TALK_RED: leds_anim_ook(LED_RGB(255, 0, 0), 1000000); break;
    case TALK_ANGRY: leds_anim_ook(LED_RGB(255, 0, 0), 250000); break;
    default: leds_cancel_anim(true); break;
    }
    /* Angry: the cicada sings (the noise generator drives the buzzer at full swing), even in mute mode */
    if (state == TALK_ANGRY)
        audio_close();  /* The buzzer to the noise generator */
    noise_gen_set_enabled(state == TALK_ANGRY);
    printf("talk: %s\n", NAMES[state]);
}

static void set_state(int s) {
    state = s;
    apply();
    changed = true;
}

/* Remote commands 0x10 + state: only when the badge is in talk mode */
static void talk_remote(uint8_t arg) {
    if (active && arg < TALK_STATES)
        set_state(arg);
}

static void talk_start(absolute_time_t now) {
    (void)now;
    static bool subscribed = false;
    if (! subscribed) {
        remote_subscribe(REMOTE_TALK, talk_remote);
        subscribed = true;
    }
    active = true;
    ook_rx_start();  /* The remote, all the time */
    audio_set_mute(false);  /* The cicada of the angry state, even in mute mode */
    set_state(TALK_OFF);
}

static void talk_stop(void) {
    active = false;
    ook_rx_stop();
    state = TALK_OFF;
    noise_gen_set_enabled(false);
    leds_cancel_anim(true);
    audio_set_mute(remote_muted());
}

static bool talk_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (b->pressed & UI_BTN_A)
        return false;
    /* The buttons also change the state (without remote) */
    if (b->pressed & UI_BTN_Y)
        set_state((state + TALK_STATES - 1) % TALK_STATES);
    if (b->pressed & (UI_BTN_X | UI_BTN_B))
        set_state((state + 1) % TALK_STATES);
    return true;
}

static bool talk_task(absolute_time_t now) {
    (void)now;
    bool c = changed;
    changed = false;
    return c;
}

static void talk_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    ui_title(fb, "Badge de talk");
    /* The state in big, inverted when time is over */
    bool alert = state >= TALK_RED;
    if (alert)
        gfx_fill_rect(fb, 0, 40, GFX_WIDTH, 60, GFX_BLACK);
    gfx_text(fb, GFX_WIDTH/2, 57, &gfx_font_large, NAMES[state], alert ? GFX_WHITE : GFX_BLACK, GFX_ALIGN_CENTER);
    ui_lines(fb, 108, &gfx_font_small, DETAILS[state]);
    ui_lines(fb, 130, &gfx_font_small, "Télécommande : Princeton\n0xC16A10 à 0xC16A14");
    ui_footer(fb, "G : quitter  Flancs/D : état");
}

const app_t app_talk = {
    .name = "Badge de talk",
    .start = talk_start,
    .buttons = talk_buttons,
    .task = talk_task,
    .render = talk_render,
    .stop = talk_stop,
    .no_saver = true,
    .owns_leds = true,
};
