/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \brief Program to "calibrate" the CC1101 crystal from the CPU crystal
 *
 * See radio_calibrate.py */


// Include sys/types.h before inttypes.h to work around issue with
// certain versions of GCC and newlib which causes omission of PRIu64
#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>

#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "log.h"
#include "radio.h"


///* This exception, even when in RAM, may not be called frequently enough, when watching rising edges of GD0 and using UART.
// * Even when it is set on core 1 and UART is used on core 0. */
//void __not_in_flash_func(counter)(uint gpio, uint32_t events) {
//    ++rises;  /* We may be called late, so don't clock here */
//}


//void setup_counter(void) {
//    /* Set up the IRQ on this core so that it is never "interrupted" by anything else */
//
//    /* Set up the interrupt on rising edges of GDO0 to count and start counting */
//    t0 = get_absolute_time();
//    gpio_set_irq_enabled_with_callback(BADGE_RADIO_GDO0, GPIO_IRQ_EDGE_RISE, true, &counter);
//
//    /* What happens if we exit this function? */
//    while(true)
//        tight_loop_contents();
//}


int main() {
    stdio_usb_init();
    radio_init();
    radio_boot();
    log_set_level(LOG_LEVEL_INFO);

    log_info("Use GDO2 to calibrate CC1101's crystal against this MCU's crystal...");
    log_info("cpu_freq = %d", clock_get_hz(clk_sys));

    ///* Set up the interrupt on the other core, so that the UART does not interrupt the interrupt */
    //multicore_launch_core1(setup_counter);
    //while(0 == t0)
    //    tight_loop_contents();  /* The other core may take time to boot */
    //printf("core 1 started\n");
    //

    /* Should converge to 25999785 on my current badge */
    radio_calibrate(30000);

    /* Let the UART finish */
    while(1)
        tight_loop_contents();
}
