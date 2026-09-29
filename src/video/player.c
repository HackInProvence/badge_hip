/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file player.c
 *
 * \brief Standalone video player (the video is also available in the menu application), see video.h.
 *
 * Buttons (badge seen from the front, the keys on the USB serial port do the same):
 * - right wing (B, key 'b'): play / stop,
 * - left wing (A, key 'a'): pause / resume.
 */

#include <stdio.h>

#include "pico/stdlib.h"

#include "btns.h"
#include "log.h"
#include "screen.h"
#include "video.h"


#define BTN_LEFT_WING  0x01  /* A */
#define BTN_RIGHT_WING 0x02  /* B */
#define DEBOUNCE_US 20000


static uint8_t btn_stable = 0, btn_raw = 0;
static absolute_time_t btn_ts = 0;

static uint8_t buttons_pressed(absolute_time_t now) {
    uint8_t raw = btns_get_state();
    uint8_t pressed = 0;
    if (raw != btn_raw) {
        btn_raw = raw;
        btn_ts = now;
    } else if (raw != btn_stable && absolute_time_diff_us(btn_ts, now) > DEBOUNCE_US) {
        pressed = raw & ~btn_stable;
        btn_stable = raw;
    }
    int c = getchar_timeout_us(0);
    if (c == 'a' || c == 'A')
        pressed |= BTN_LEFT_WING;
    else if (c == 'b' || c == 'B')
        pressed |= BTN_RIGHT_WING;
    return pressed;
}


int main() {
    stdio_init_all();
    log_set_level(LOG_LEVEL_WARNING);
    btns_init();
    screen_init();  /* Also initializes SPI0, shared with the SD card */

    bool playing = false;
    while (true) {
        absolute_time_t now = get_absolute_time();
        uint8_t pressed = buttons_pressed(now);

        if (pressed & BTN_RIGHT_WING) {
            if (! playing)
                playing = video_start(NULL);
            else
                video_stop();
        }
        if ((pressed & BTN_LEFT_WING) && playing)
            video_toggle_pause();

        if (playing && ! video_task(now)) {
            playing = false;
            printf("video: %s\n", video_message());
            screen_deep_sleep();  /* The datasheet recommends to deep sleep as soon as possible */
        }
    }
}
