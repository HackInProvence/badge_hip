/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */


#include "hardware/gpio.h"
#include "pico/binary_info.h"

#include "badge_pinout.h"
#include "btns.h"


void btns_init(void) {
    /* GPIO usages */
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_BUTTON_A, "button: A"));
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_BUTTON_B, "button: B"));
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_BUTTON_X, "button: X"));
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_BUTTON_Y, "button: Y"));

    /* Put the GPIO of the buttons in input mode */
    gpio_init(BADGE_BUTTON_A);
    gpio_init(BADGE_BUTTON_B);
    gpio_init(BADGE_BUTTON_X);
    gpio_init(BADGE_BUTTON_Y);
}

uint8_t btns_get_state(void)
{
    uint8_t res = 0;
    if (gpio_get(BADGE_BUTTON_A))
        res |= 1;
    if (gpio_get(BADGE_BUTTON_B))
        res |= 2;
    if (gpio_get(BADGE_BUTTON_X))
        res |= 4;
    if (gpio_get(BADGE_BUTTON_Y))
        res |= 8;
    return res;
}
