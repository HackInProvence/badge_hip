/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */


#include "hardware/pio.h"
#include "pico/binary_info.h"

#include "badge_pinout.h"
#include "leds.h"


/* Configured state machine which runs our program */
static PIO pio = NULL;
static uint sm = -1;


void leds_init(void) {
    /* GPIO usages */
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_LED, "LEDs WS2812B"));

    // This will find a free pio and state machine for our program and load it for us
    // We use pio_claim_free_sm_and_add_program_for_gpio_range so we can address gpios >= 32 if needed and supported by the hardware
    uint offset;
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(
        &ws2812_program,
        &pio, &sm, &offset,
        BADGE_LED, 1 /* count */,
        true  /* bool set_gpio_base */
    );
    hard_assert(success);  /* FIXME: I don't know how to debug that, so is this the right thing to do? */

    /* Setup the state machine now that we have allocated the pio and sm */
    ws2812_program_init(pio, sm, offset, BADGE_LED, 800000, false);
}


/* This is a solution to be able to see the static variables, but makes it import other things */
#include "pico/time.h"
void leds_test(void) {
    leds_init();
    pio_gpio_init(pio, BADGE_LED);
    pio_sm_set_enabled(pio, sm, true);

    /*                    GGRRBB.. */
    pio_sm_put(pio, sm, 0xFF000000);
    pio_sm_put(pio, sm, 0x00FF0000);
    pio_sm_put(pio, sm, 0x0000FF00);
    sleep_ms(3000);
    pio_sm_put(pio, sm, 0x00000000);
    pio_sm_put(pio, sm, 0x00000000);
    pio_sm_put(pio, sm, 0x00000000);
}
