/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

// Include sys/types.h before inttypes.h to work around issue with
// certain versions of GCC and newlib which causes omission of PRIu64
#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include "badge_pinout.h"
#include "log.h"
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



void test_read_all(void) {
    /* TODO: read everything that can be read from the screen memory */
}


void time_busy(const char *msg) {
    absolute_time_t t0 = get_absolute_time(), t1;
    while(screen_busy())
        tight_loop_contents();
    t1 = get_absolute_time();
    printf("%s done, took %" PRIu64 "µs\n", msg, absolute_time_diff_us(t0, t1));
}


void test_show_bw_images(void) {
    printf("push b/w images\n");

#define _show_bw(ptr) { \
    printf("- " #ptr ":"); \
    screen_show_image_bw(ptr); \
    time_busy(NULL); \
    sleep_ms(1000); \
}
    _show_bw(text_bw)
    _show_bw(hip_bw)
    _show_bw(secsea_bw)
}

void test_show_4g_images(void) {
    printf("push 4g image\n");

#define _show_4g(lsb, msb) { \
    printf("- " #lsb ", " #msb ":"); \
    screen_show_image_4g(lsb, msb); \
    time_busy(NULL); \
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


/* Test that we can skip clock and analog enable+disable for each frame, which takes 230ms */
void test_enable_once(void) {
    /* There seems to be interactions with the previous LUT and/or image:
     * the image lightens a little when enabling clocks, but that depends on the previous image */
    send("\x3F\x07", 2);

    /* Time how long to enable + disable */
    send("\x22\xC3", 2);
    send("\x20", 1);
    time_busy("enable+disable clocks");
    sleep_ms(1000);

    /* Tested: we can, and the effective frame rate is now guided by chosen FR and repetitions */
    send("\x22\xC0", 2);  /* Enable clock and analog -> should be disabled at some point in the near future */
    send("\x20", 1);
    time_busy("enable clocks");

    /* Here we can do other things like draw an image,
     * and this time it respects the FR[n] of the successive groups... */

    send("\x22\x03", 2); /* Disable clock and analog */
    send("\x20", 1);
    time_busy("disable clocks");
}


/* makes it busy, lasts 90.5ms */
void screen_start_multiframe(void) {
    if (screen_busy()) {
        log_warning("screen_start_multiframe() called but screen is busy");
        return;
    }
    /* if (state FIXME
    state = ; */

    uint8_t cmd[2];
    cmd[0] = SSD1681_DISPLAY_CTRL2;
    cmd[1] = 0xC0;  /* enable clock and analog */
    send(cmd, 2);
    cmd[0] = SSD1681_ACTIVATE;
    send(cmd, 1);
}

/* makes it busy, lasts 140ms */
void screen_end_multiframe(void) {
    if (screen_busy()) {
        log_warning("screen_end_multiframe() called but screen is busy");
        return;
    }
    /* if (state FIXME
    state = ; */

    uint8_t cmd[2];
    cmd[0] = SSD1681_DISPLAY_CTRL2;
    cmd[1] = 0x03;  /* disable clock and analog */
    send(cmd, 2);
    cmd[0] = SSD1681_ACTIVATE;
    send(cmd, 1);
}

const uint8_t ws_roll[159] = \
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* 00 = no touch */ \
    "\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* 01 = lighter */ \
    "\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* 10 = darker */ \
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* 11 = no touch */ \
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* VCOM = DVCOM */ \
    "\x01\x00\x00\x00\x00\x00\x00" /* TP[0A], TP[0B], SR[0AB], TP[0C], TP[0D], SR[0CD], RP[0] */ \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" /* 1 TP anywhere = 20ms with FR=2, hence FR = k*25Hz */ \
    "\x82\x22\x22\x22\x22\x22" "\x00\x00\x00" \
    "\x07"  /* EOPT, 0x22 = normal */         \
    "\x17"  /*  VGH, 0x17 == 0x00 == 20V */   \
    "\x41"  /* VSH1, 0x41 == 15V */           \
    /* This LUT never uses VSH2 */            \
    "\xA8"  /* VSH2, 0x00 == ???, POR is 5V */\
    "\x32"  /*  VSL, 0x32 == -15V */          \
    "\x20"; /* VCOM, 0x20 == -0.8V */

void test_roll(void) {
    uint8_t A[5000], B[5000], *prev = A, *next = B, *o;

    screen_clear_image_position();
    screen_push_ws(ws_roll);

    screen_start_multiframe();
    while(screen_busy())
        tight_loop_contents();

    /* Make a rolling shutter */
    memset(prev, 0xFF, 5000);
    absolute_time_t t0 = get_absolute_time(), t1;
    for(size_t t=0; t<200; ++t) {
        /* For each timestep, draw the image */
        for(size_t j=0; j<200; ++j) {
            for(size_t i=0; i<25; ++i) {
                size_t k = i/5+1;
                next[j*SCREEN_WIDTH/8 + i] = (j/k == t%(200/k)) ? 0x00:0xFF;
                //next[j*SCREEN_WIDTH/8 + i] = (j/10==t-1 || j/10==t || j/10==t+1) ? 0x00:0xFF;
            }
        }

        /* Show the image and swap buffer */
        screen_push_rams(next, prev, 5000);  /* 49ms @ 2MHz, 6.1ms @ 20MHz */
        //screen_show_rams();
        uint8_t cmd[2] = {SSD1681_DISPLAY_CTRL2, 0x04};
        send(cmd, 2);
        cmd[0] = SSD1681_ACTIVATE;
        send(cmd, 1);
        time_busy("frame"); /* 13s for the 200 frames, but each frame lasts 5.7ms, so it should be 1.2s */
        while(screen_busy())
            tight_loop_contents();
        o = prev;
        prev = next;
        next = o;
        //sleep_ms(1000);
    }
    t1 = get_absolute_time();
    /* With SPI @20MHz, and only 1 FR @175Hz, takes 3.3s for 200 images, hence 61fps! */
    printf("push+draw 200 images took %" PRIu64 "µs\n", absolute_time_diff_us(t0, t1));

    screen_end_multiframe();
    while(screen_busy())
        tight_loop_contents();
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
    //test_subimage();
    //test_enable_once();

    test_clear(); sleep_ms(1000);
    //uint8_t buf[5000];
    //memset(buf, 0xFF, 2500);
    //memset(buf+2500, 0x00, 2500);
    //screen_show_image_bw(buf);
    ////screen_push_rams(buf, buf, 5000);
    ////screen_show_rams();
    //while(screen_busy())
    //    tight_loop_contents();
    //sleep_ms(1000);
    test_roll();

    /* Clear to white before going to sleep */
    //test_clear();

    screen_deep_sleep();
    sleep_ms(1000);
    printf("Screen sleeping, after 1s BUSY = %d\n\n", gpio_get(BADGE_SCREEN_BUSY));
}
