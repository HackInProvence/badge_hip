/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "badge_pinout.h"
#include "btns.h"


int main() {
    stdio_usb_init();

    btns_init();

    printf("\nBtns pressed: ");
    while(true) {
        char s[8] = {};
        uint8_t btns = btns_get_state();
        if (! btns)
            strncpy(s, "None", sizeof(s));
        if (btns & 1)
            strlcat(s, "A", sizeof(s));
        if (btns & 2)
            strlcat(s, "B", sizeof(s));
        if (btns & 4)
            strlcat(s, "X", sizeof(s));
        if (btns & 8)
            strlcat(s, "Y", sizeof(s));
        printf(s);
        for(size_t i=0; i<sizeof(s) && s[i]>0; ++i)
            s[i] = '\b';
        printf(s);
        sleep_ms(20);
        for(size_t i=0; i<sizeof(s) && s[i]>0; ++i)
            s[i] = ' ';
        printf(s);
        for(size_t i=0; i<sizeof(s) && s[i]>0; ++i)
            s[i] = '\b';
        printf(s);
    }
}
