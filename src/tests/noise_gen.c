/* FIXME: license, for now defaults to Copyright (C) Miaou 2025 */

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
