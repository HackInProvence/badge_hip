/* FIXME: license, for now defaults to Copyright (C) Miaou 2025 */

#include <stdio.h>

#include "pico/stdlib.h"

#include "libcicada.h"


int main() {
    stdio_usb_init();
    libcicada_init_play();
    while(true) {
        sleep_ms(2000);
        libcicada_set_enabled(false);
        sleep_ms(1000);
        libcicada_set_enabled(true);
    }
}
