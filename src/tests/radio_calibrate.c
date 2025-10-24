/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \brief Program to "calibrate" the CC1101 crystal from the CPU crystal */


// Include sys/types.h before inttypes.h to work around issue with
// certain versions of GCC and newlib which causes omission of PRIu64
#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>

#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include "radio.h"

/* TODO: when it is decided whether radio_send functions are static or not, change that (non static -> in .h, static -> find another solution...) */
void radio_send(const uint8_t *data, uint8_t *response, size_t len);


static uint64_t rises = 0;

void counter(uint gpio, uint32_t events) {
    ++rises;
}


#define MACHINE

int main() {
    stdio_usb_init();
    radio_init();
    radio_boot();

    printf("Use GDO0 to measure it with this MCU's crystal...\n");

    /* Set up the CC1101 to output some divider of fXOSC */
    uint16_t cmd = CC1101_IOCFG0 | (0x3D<<8);  /* 96 times divider (the CPU does not handle 64 or lower dividers) */
    radio_send((uint8_t *)&cmd, NULL, 2);

    /* Set up the interrupt on rising edges of GDO0 to count and start counting */
    absolute_time_t t0 = get_absolute_time(), last = t0, now;
    gpio_set_irq_enabled_with_callback(BADGE_RADIO_GDO0, GPIO_IRQ_EDGE_RISE, true, &counter);
    while(true) {
        absolute_time_t now = get_absolute_time();
        uint64_t dt = absolute_time_diff_us(last, now);
        if (dt > 300000) {
            uint64_t rate = 96*rises*1000000/absolute_time_diff_us(t0, now);
#ifndef MACHINE
            /* Human readable format */
            printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");  /* 58 should be enough */
            printf("current estimate: %" PRIu64 " Hz", rate);
#else
            /* To be read by radio_calibrate.py */
            printf("%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n", to_us_since_boot(now), rises, rate);
#endif
            last = now;
        }
    }
}
