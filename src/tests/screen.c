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
#include "notif_4g.h"
#include "companion.h"


int main() {
    stdio_usb_init();

    screen_init();
    printf("Boot sequence\n");
    while(! screen_boot())
        tight_loop_contents();
    printf("Ready\n");

    //read_all(); /* TODO */

    size_t len = screen_set_image_position(0, 0, 200, 200);
    printf("image should be of size %d\n", len);

    printf("push b/w image\n");
    //screen_push_ws(ws_1681_times);
    screen_show_image_bw(text_bw);
    //screen_show_image_bw(hip_bw);
    //screen_show_image_bw(secsea_bw);
    while(screen_busy())
        tight_loop_contents();
    printf("done\n");

    sleep_ms(2000);
    printf("push 4g image\n");
    screen_push_ws(screen_ws_1681_4grays);
    //screen_push_rams(text_bw, squares_bw); /* LSB, MSB */
    //screen_push_rams(grad_4g_lsb, grad_4g_msb);
    screen_push_rams(secsea_4g_lsb, secsea_4g_msb, 5000);
    screen_show_rams();
    while(screen_busy())
        tight_loop_contents();
    printf("done\n");

    sleep_ms(2000);
    printf("push subimage\n");
    screen_push_ws(screen_ws_1681_4grays);
    //len = screen_set_image_position(48, 68, 168, 168);
    //screen_push_rams(notif_4g_lsb, notif_4g_msb, len);
    len = screen_set_image_position(32, 25, 32+companion_width*8, 25+companion_height);
    screen_push_rams(companion_lsb, companion_msb, len);
    screen_show_rams();
    while(screen_busy())
        tight_loop_contents();
    printf("done\n");

    /* It was tested that the screen is cleared then something else can be drawn */
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
