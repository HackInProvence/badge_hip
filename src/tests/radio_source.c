/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdio.h>

#include "pico/stdlib.h"

#include "leds.h"
#include "radio.h"

#define ORANGE LED_RGB(255, 64, 0)
#define GREEN LED_RGB(0, 64, 0)

typedef enum {
    BOOT,
    RECEIVING,
    WAITING,
    WAITING_LONG,
} state_t;

static state_t state = BOOT;
absolute_time_t state_ts = 0;

int main() {
    stdio_usb_init();
    radio_init();
    radio_boot();
    leds_init(NULL);
    leds_anim_fixed(GREEN);

    /* First version will be an hex echoer which blinks */
    /* You can also connect to the pico through /dev/serial/by-id/usb-Raspberry_Pi_Pico_E66164084315472C-if00 */
    /* You can also connect to the pico through /dev/serial/by-id/usb-Raspberry_Pi_Pico_5044340588A7511C-if00 */
    state = RECEIVING;
    state_ts = get_absolute_time();
    while(true) {
        int ret = stdio_getchar_timeout_us(50000);
        absolute_time_t now = get_absolute_time();
        if (ret >= 0) {
            if (state != RECEIVING) {
                state = RECEIVING;
                state_ts = now;
                leds_anim_ook(ORANGE, 50000);
            }
            printf("%02X", ret);
        } else {
            if (state == RECEIVING && absolute_time_diff_us(state_ts, now) > 50000) {
                state = WAITING;
                state_ts = now;
                leds_anim_fixed(0);
            } else if (state == WAITING && absolute_time_diff_us(state_ts, now) > 1000000) {
                state = WAITING_LONG;
                state_ts = now;
                leds_anim_fixed(GREEN);
            }
        }
    }
}
