/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdio.h>

#include "pico/stdlib.h"

#include "noise_gen.h"


int main() {
    stdio_usb_init();
    noise_gen_init_play();
    while(true) {
        sleep_ms(2000);
        noise_gen_set_enabled(false);
        sleep_ms(1000);
        noise_gen_set_enabled(true);
    }
}
