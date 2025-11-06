/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/time.h"

#include "log.h"
#include "leds.h"


#define ORANGE LED_RGB(255, 64, 0)


void leds_test(void) {
    /*         GGRRBB.. */
    push_led(0xFF000000);
    push_led(0x00FF0000);
    push_led(0x0000FF00);  /* There are 3 LEDs on the prototype */
    sleep_ms(500);
}


int main() {
    stdio_usb_init();
    log_set_level(LOG_LEVEL_INFO);
    leds_init(NULL);

    leds_test();

    printf("fixed\n");
    leds_anim_fixed(ORANGE);
    sleep_ms(500);

    printf("rainbow/wheel\n");
    leds_anim_wheel(500000);
    sleep_ms(3000);

    printf("OOK\n");
    leds_anim_ook(ORANGE, 500000);
    sleep_ms(5000);

    printf("flashes\n");
    leds_anim_flashes(ORANGE);
    sleep_ms(5000);

    printf("breath\n");
    leds_anim_breath(ORANGE, 2000000);
    sleep_ms(10000);

    printf("cancel with leds on\n");
    leds_cancel_anim(false);
    sleep_ms(2000);
    printf("cancel with leds off\n");
    leds_cancel_anim(true);
    sleep_ms(2000);

    printf("disable all LEDs, incl. prototype\n");
    /* Shutdown */
    push_led(0);
    push_led(0);
    push_led(0);  /* There are 3 LEDs on the prototype */
}
