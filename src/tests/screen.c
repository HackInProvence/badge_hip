/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

// Include sys/types.h before inttypes.h to work around issue with
// certain versions of GCC and newlib which causes omission of PRIu64
#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include "badge_pinout.h"
#include "screen.h"




// Waveform settings, showing grays
const uint8_t ws_1681_times[159] = \
    "\x80\x48\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* black */ \
    "\x40\x48\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* darker */ \
    "\x80\x48\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* lighter */ \
    "\x40\x48\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* white */ \
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
    "\x0A\x00\x00\x00\x00\x00\x00" \
    "\x08\x01\x00\x08\x01\x00\x02" \
    "\x01\x40\x00\x00\x00\x00\x0E" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x22\x22\x22\x22\x22\x22" "\x00\x00\x00" \
    "\x22"  /* EOPT, 0x22 = normal */         \
    "\x17"  /*  VGH, 0x17 == 0x00 == 20V */   \
    "\x41"  /* VSH1, 0x41 == 15V */           \
    /* This LUT never uses VSH2 */            \
    "\x00"  /* VSH2, 0x00 == ???, POR is 5V */\
    "\x32"  /*  VSL, 0x32 == -15V */          \
    "\x20"; /* VCOM, 0x20 == -0.8V */


#include "hip_bw.h"
#include "secsea_bw.h"

#include "squares_bw.h"
#include "text_bw.h"
#include "grad_4g.h"
#include "secsea_4g.h"


int main() {
    stdio_usb_init();

    screen_init();
    printf("Boot sequence\n");
    while(! screen_boot())
        tight_loop_contents();
    printf("Started\n");
    while(screen_busy())
        tight_loop_contents();
    printf("Available\n");

    //read_all();
    //screen_push_ws(ws_1681_bw_full);

    //printf("clear to black\n");
    //img_const(0);  // black
    //printf("done\n");
    /* Test all 0x21 (bypass ram) */
    //for(size_t i=0; i<16; ++i) {
    //    uint8_t flag = i | (i<<4);
    //    printf("0x%02X\n", flag);
    //    img_const(0, flag);
    //    sleep_ms(5000);
    //}

    ////sleep_ms(1000);
    //printf("push image\n");
    //push_image(secsea_bw);
    //printf("done\n");
    //sleep_ms(2000);
    //printf("push image\n");
    //push_image(hip_bw);
    //printf("done\n");

    printf("push b/w image\n");
    //screen_push_ws(ws_1681_times);
    screen_set_image_1plane(text_bw, 5000, true);
    while(screen_busy())
        tight_loop_contents();
    printf("done\n");
    sleep_ms(5000);

    printf("push 4g image\n");
    screen_push_ws(screen_ws_1681_4grays);
    while(screen_busy())
        tight_loop_contents();
    //push_images(text_bw, squares_bw); /* LSB, MSB */
    //push_images(grad_4g_lsb, grad_4g_msb);
    screen_set_image_2planes(secsea_4g_lsb, secsea_4g_msb, 5000, false);
    while(screen_busy())
        tight_loop_contents();
    printf("done\n");

    sleep_ms(2000);
    printf("clear to white\n");
    screen_clear(1);  // Clear to white before sleep
    while(screen_busy())
        tight_loop_contents();
    printf("done\n");

    screen_deep_sleep();
    sleep_ms(1000);
    printf("Screen sleeping, after 1s BUSY = %d\n\n", gpio_get(BADGE_SCREEN_BUSY));
}
