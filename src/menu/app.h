/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file app.h
 *
 * \brief The features of the menu written as "applications": main.c opens them from the menus,
 * gives them the buttons, runs them in the main loop and draws their page when they ask for it.
 *
 * Like everything else, an application never blocks: its task is called at each loop.
 * */

#ifndef _APP_H
#define _APP_H

#include <stdbool.h>
#include <stdint.h>

#include "pico/time.h"

#include "ui.h"

#define APP_LONG_PRESS_MS 800

/* The buttons, bits UI_BTN_A (left wing), UI_BTN_B (right wing), UI_BTN_X (right flank), UI_BTN_Y (left flank) */
typedef struct {
    uint8_t pressed;  /* Pressed since the last call */
    uint8_t released_short;  /* Released without a long press: use it for buttons that also have a long press */
    uint8_t long_pressed;  /* Held for APP_LONG_PRESS_MS (reported once per press) */
    uint8_t held;  /* Currently down */
    uint32_t held_ms[4];  /* How long each button has been held (A, B, X, Y), 0 when up */
} app_buttons_t;

static inline uint32_t app_held_ms(const app_buttons_t *b, uint8_t bit) {
    return b->held_ms[bit == UI_BTN_A ? 0 : bit == UI_BTN_B ? 1 : bit == UI_BTN_X ? 2 : 3];
}

/** \brief The buttons (and the PC keyboard) in the text editor of ui.h: flanks = character (held: repeated),
 * right wing = next position, left wing = previous one, long press on the right wing = done.
 * Returns UI_EDIT_DONE, UI_EDIT_CANCEL (left wing on the first position) or UI_EDIT_EDITING. */
int app_edit_buttons(ui_edit_t *e, const app_buttons_t *b);

typedef struct {
    const char *name;  /* In the menus */
    void (*label)(char *buf, int len);  /* Optional: dynamic text in the menus (e.g. "Lampe : 50 %") */
    void (*start)(absolute_time_t now);
    bool (*buttons)(const app_buttons_t *b, absolute_time_t now);  /* false: back to the menu */
    bool (*task)(absolute_time_t now);  /* Optional: true when the page must be redrawn */
    void (*render)(uint8_t *fb, absolute_time_t now);
    bool (*calm)(void);  /* Optional: false while the page changes often (no slow full refresh then) */
    void (*stop)(void);  /* Optional: when leaving (LEDs, sound...) */
    bool no_saver;  /* The screensaver must not start (the page shows something alive) */
    bool owns_leds;  /* The application drives the LEDs itself, even in mute mode (talk badge) */
} app_t;

/** \brief Opens an application (from a service: e.g. a message received shows the program), implemented in main.c. */
void app_open(const app_t *app);
/** \brief Leaves the application and shows the page drawn by \p render like the screensaver: full waveform of the
 * screen (no ghost), kept without power; any button goes back to the menu. */
void app_show_still(void (*render)(uint8_t *fb));

/** \brief The application shown, NULL in the menus. */
const app_t *app_current(void);

/* Sound and LEDs for the applications, implemented in main.c: they respect the mute mode (remote.h) */
void app_tone(uint16_t hz, uint16_t ms);
void app_cough(void);  /* The cough of the virus (infection.c) */
void app_leds(uint8_t r, uint8_t g, uint8_t b);  /* All black: back to the LED animation of the badge */

#endif /* _APP_H */
