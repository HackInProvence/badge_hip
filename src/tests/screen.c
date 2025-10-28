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
#include "log.h"
#include "screen.h"
#include "tests.h"


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



void test_read_all(void) {
    /* TODO: read everything that can be read from the screen memory */
}


void test_show_bw_images(void) {
    printf("push b/w images\n");

#define _show_bw(ptr) { \
    absolute_time_t t0, now; \
    printf("- " #ptr ":"); \
    t0 = get_absolute_time(); \
    screen_show_image_bw(ptr); \
    while(screen_busy()) \
        tight_loop_contents(); \
    now = get_absolute_time(); \
    printf(" done, took %" PRIu64 "µs\n", absolute_time_diff_us(t0, now)); \
    sleep_ms(1000); \
}
    _show_bw(text_bw)
    _show_bw(hip_bw)
    _show_bw(secsea_bw)
}

void test_show_4g_images(void) {
    printf("push 4g image\n");

#define _show_4g(lsb, msb) { \
    absolute_time_t t0, now; \
    printf("- " #lsb ", " #msb ":"); \
    t0 = get_absolute_time(); \
    screen_show_image_4g(lsb, msb); \
    while(screen_busy()) \
        tight_loop_contents(); \
    now = get_absolute_time(); \
    printf(" done, took %" PRIu64 "µs\n", absolute_time_diff_us(t0, now)); \
    sleep_ms(1000); \
}
    _show_4g(text_bw, squares_bw)
    _show_4g(grad_4g_lsb, grad_4g_msb)
    _show_4g(secsea_4g_lsb, secsea_4g_msb)
}


void test_subimage(void) {
    /* We see the previous image, even if the screen was cleared.
     * This is EXPECTED. */
    size_t len;
    printf("push subimage:");
    screen_push_ws(screen_ws_1681_4grays);
    len = screen_set_image_position(48, 68, 168, 168);
    screen_push_rams(notif_4g_lsb, notif_4g_msb, len);
    len = screen_set_image_position(32, 18, 32+companion_width*8, 18+companion_height);
    screen_push_rams(companion_lsb, companion_msb, len);
    screen_show_rams();
    while(screen_busy())
        tight_loop_contents();
    printf(" done\n");
    sleep_ms(2000);
}


void test_clear(void) {
    printf("clear to white\n");
    screen_clear(1);  /* Tests showed that normal draws can occur after this one */
    while(screen_busy())
        tight_loop_contents();
    printf("done\n");
}


int main() {
    stdio_usb_init();
    log_set_level(LOG_LEVEL_INFO);

    screen_init();
    printf("boot sequence\n");
    while(! screen_boot())
        tight_loop_contents();

    size_t len = screen_clear_image_position();
    printf("fullscreen images should be of size %d\n", len);

    //test_read_all();
    //test_show_bw_images();
    //test_show_4g_images();
    test_subimage();

    /* Clear to white before going to sleep */
    test_clear();

    screen_deep_sleep();
    sleep_ms(1000);
    printf("Screen sleeping, after 1s BUSY = %d\n\n", gpio_get(BADGE_SCREEN_BUSY));
}
