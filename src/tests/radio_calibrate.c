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
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include "log.h"
#include "radio.h"


static uint64_t rises = 0;
static absolute_time_t t0 = 0;
static uint slice = 0;

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


void __not_in_flash_func(counter)(void) {
    pwm_clear_irq(slice);
    ++rises;
}

void setup_pwm_slice(uint pin) {
    /* Use the PWM edge counter on B port of slice 6 to count the rising edges of the CC1101 clock */
    if (pwm_gpio_to_channel(pin) != PWM_CHAN_B) {
        log_warning("can't use pin %u, not on PWM channel B", pin);
        return;
    }

    slice = pwm_gpio_to_slice_num(pin);

    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv_mode(&cfg, PWM_DIV_B_RISING);

    /* Here we can diverge:
     * 1. use another slice to count time, and use DMA to stop the counting PWM (gives a fixed period in hardware),
     *    -> correct accuracy on counter, best accuracy on the period (max period = 125MHz/(65536*256*2 with phase correct) = 268ms)
     * 2. use an interrupt on wrap and count the number of wraps
     *    -> count as long as we want, lower precision on period measurement (did not know exactly when it wrapped).
     *    With 65536*256 wraps (can be *512 with phase correct) = 16M (32M), we can have around 1 pulse per second on CLK_XOSC/1 configuration.
     * -> choose 2 for now, because we can have longer measurements
     */
    /* Finally use a 2.6k wrap so the interrupt is called at ~ 10kHz, which is slow enough to not be glitched by the UART activity */
    pwm_config_set_wrap(&cfg, 2599);
    pwm_config_set_clkdiv_int(&cfg, 1);  /* 0 means 256, so only counts 1 for 256 rising edges -> in fact, we don't need that */
    pwm_init(slice, &cfg, /*start=*/false);
    gpio_set_function(pin, GPIO_FUNC_PWM);
    pwm_set_counter(slice, 0);
    pwm_clear_irq(slice);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, counter);
    irq_set_enabled(PWM_IRQ_WRAP, true);
    pwm_set_irq_enabled(slice, true);
    pwm_set_enabled(slice, true);
    t0 = get_absolute_time();
}


/* To be read by radio_calibrate.py */
#define PYTHON_READ

int main() {
    stdio_usb_init();
    radio_init();
    radio_boot();
    log_set_level(LOG_LEVEL_INFO);

    //printf("Use GDO0 to measure it with this MCU's crystal...\n");
    printf("\n");
    log_info("Use GDO2 to measure it with this MCU's crystal...");
    log_info("cpu_freq = %d", clock_get_hz(clk_sys));

    /* Set up the CC1101 to output some divider of fXOSC */
    //uint16_t cmd = CC1101_IOCFG0 | (0x3D<<8);  /* 96 times divider (the CPU does not handle 64 or lower dividers) */
    /* On GDO2 because GDO2 is on GPIO23, which is on a port B of the PWM slice 3,
     *  whereas GDO0 is on GPIO22 which is on port A, not available as input,
     *  and the clock can be output on 1 of the pins only, so disable it on IOCFG0 */

    /* With PWM slice IRQ, we can count XOSC/1 (PWM can count up to sys_clk/2) */
    ccsend((uint8_t[]){CC1101_BURST(CC1101_IOCFG2), 0x30, 0x2E, 0x29}, NULL, 4);

    ///* Set up the interrupt on the other core, so that the UART does not interrupt the interrupt */
    //multicore_launch_core1(setup_counter);
    //while(0 == t0)
    //    tight_loop_contents();  /* The other core may take time to boot */
    //printf("core 1 started\n");
    //
    setup_pwm_slice(BADGE_RADIO_GDO2);

    absolute_time_t last = t0;
    while(true) {
        absolute_time_t now = get_absolute_time();
        uint64_t dt = absolute_time_diff_us(last, now);
        if (dt > 300000) {
            //uint64_t rate = 96*rises*1000000/absolute_time_diff_us(t0, now);
            uint64_t diff = absolute_time_diff_us(t0, now);
            uint64_t rate = rises*1000000/diff;  /* Rate of the IRQ */
            uint64_t xosc = 2600*rises*1000000/diff;
            uint64_t xosc_ = 2600*(rises+1)*1000000/diff;
#ifndef PYTHON_READ
            /* Human readable format */
            //printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");  /* 58 should be enough */
            //printf("current estimate: %" PRIu64 " Hz", rate);
            rate = rises*1000000/diff;
            printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");  /* 104 should be enough */
            printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
            printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
            printf("rises=% 9" PRIu64 ", diff=% 9" PRIu64 "ms, rate=% 6d.%03d kHz, % 5d.%03d <= xosc <= % 5d.%03d kHz", rises, diff/1000,
                   (uint32_t)(rate/1000), (uint32_t)(rate%1000),
                   (uint32_t)(xosc/1000), (uint32_t)(xosc%1000),
                   (uint32_t)(xosc_/1000), (uint32_t)(xosc_%1000));
            // rises=   713996, diff=   714002ms, rate=     0.999 kHz,  25999.767 <= xosc <=  25999.804 kHz
#else
            /* To be read by radio_calibrate.py */
            printf("%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n", to_us_since_boot(now), rises, rate, xosc);
#endif
            last = now;
        }
    }
}
