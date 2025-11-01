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


/* The send() macro enables writing send(CMD, ARG1, ARG2) but we can't have it in the header nor include it from here,
 * so you have to copy-paste it to use it in tests... */
#define UINT8_LIT(...) (uint8_t[]){__VA_ARGS__}
#define send(...) _send(UINT8_LIT(__VA_ARGS__), sizeof(UINT8_LIT(__VA_ARGS__))/sizeof(uint8_t))


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
    send(SSD1681_EOPT_CTRL, 0x07);

    /* Time how long to enable + disable */
    send(SSD1681_DISPLAY_CTRL2, 0xC3);
    send(SSD1681_ACTIVATE);
    time_busy("enable+disable clocks");
    sleep_ms(1000);

    /* Tested: we can, and the effective frame rate is now guided by chosen FR and repetitions */
    send(SSD1681_DISPLAY_CTRL2, 0xC0);
    send(SSD1681_ACTIVATE);
    time_busy("enable clocks");

    /* Here we can do other things like draw an image,
     * and this time it respects the FR[n] of the successive groups... */

    send(SSD1681_DISPLAY_CTRL2, 0x03);
    send(SSD1681_ACTIVATE);
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

    send(SSD1681_DISPLAY_CTRL2, 0xC0);
    send(SSD1681_ACTIVATE);
}

/* makes it busy, lasts 140ms */
void screen_end_multiframe(void) {
    if (screen_busy()) {
        log_warning("screen_end_multiframe() called but screen is busy");
        return;
    }
    /* if (state FIXME
    state = ; */

    send(SSD1681_DISPLAY_CTRL2, 0x03);
    send(SSD1681_ACTIVATE);
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
        /* Naive loop took 4.5ms !!! -> divisions are not cheap
         * Now takes 1.7ms */
        for(size_t j=0; j<200; ++j) {
            size_t j8 = j*SCREEN_WIDTH/8;
            for(size_t k=0; k<5; ++k) {
                size_t v = (j/k == t%(200/k)) ? 0x00:0xFF;
                size_t k5 = j8 + 5*k;
                for(size_t i=0; i<5; ++i) {
                    next[k5 + i] = v;
                    //next[j*SCREEN_WIDTH/8 + i] = (j/10==t-1 || j/10==t || j/10==t+1) ? 0x00:0xFF;
                }
            }
        }

        /* Show the image and swap buffer */
        screen_push_rams(next, prev, 5000);  /* 49ms @ 2MHz, 6.1ms @ 20MHz */
        //screen_show_rams();
        send(SSD1681_DISPLAY_CTRL2, 0x04);
        send(SSD1681_ACTIVATE);
        //time_busy("frame");  /* 5.0ms per FR @200Hz, as expected */
        while(screen_busy())
            tight_loop_contents();
        o = prev;
        prev = next;
        next = o;
        //sleep_ms(1000);
    }
    t1 = get_absolute_time();
    /* With SPI @20MHz, and only 1 FR @175Hz, takes 3.3s for 200 images, hence 61fps! */
    /* With SPI @20MHz, and only 1 FR @200Hz, takes 3.1s for 200 images, hence 64fps! */
    printf("push+draw 200 images took %" PRIu64 "µs\n", absolute_time_diff_us(t0, t1));

    screen_end_multiframe();
    while(screen_busy())
        tight_loop_contents();
}


/* 30 fps means a budget of 33.3ms - 6.1ms to send an image, leaving 27.2ms hence 5.4 ticks @200Hz or 4.1 @150Hz (better ON time) */
const uint8_t ws_30fps[159] = \
    "\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* 00 = no touch */ \
    "\x02\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* 01 = lighter */ \
    "\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* 10 = darker */ \
    "\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* 11 = relight white */ \
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* VCOM = DVCOM */ \
    "\x00\x00\x00\x00\x03\x00\x00" /* TP[0A], TP[0B], SR[0AB], TP[0C], TP[0D], SR[0CD], RP[0] */ \
    "\x00\x00\x00\x00\x01\x00\x00" \
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
    "\x66\x22\x22\x22\x22\x22" "\x00\x00\x00" \
    "\x07"  /* EOPT, 0x22 = normal */         \
    "\x17"  /*  VGH, 0x17 == 0x00 == 20V */   \
    "\x41"  /* VSH1, 0x41 == 15V */           \
    /* This LUT never uses VSH2 */            \
    "\xA8"  /* VSH2, 0x00 == ???, POR is 5V */\
    "\x32"  /*  VSL, 0x32 == -15V */          \
    "\x20"; /* VCOM, 0x20 == -0.8V */

/* 20 fps means a budget of 50.0ms - 6.1ms to send an image, leaving 43.9ms hence 8.8 ticks @200Hz (any other freq has the same ON time) */
/* TODO -> currently this targets 14ms */
const uint8_t ws_20fps[159] = \
    "\x00\x00\x00\x00\x00\x00\x02\x10\x00\x00\x00\x00" /* 00 = redark */ \
    "\x12\x02\x00\x00\x00\x00\x12\x12\x00\x00\x00\x00" /* 01 = lighter */ \
    "\x21\x01\x00\x00\x00\x00\x21\x21\x00\x00\x00\x00" /* 10 = darker */ \
    "\x00\x00\x00\x00\x00\x00\x01\x20\x00\x00\x00\x00" /* 11 = relight white */ \
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" /* VCOM = DVCOM */ \
    "\x00\x00\x00\x00\x00\x00\x00" /* TP[0A], TP[0B], SR[0AB], TP[0C], TP[0D], SR[0CD], RP[0] */ \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x02\x00\x00\x01\x00\x00" /* group6 */ \
    "\x00\x02\x00\x00\x08\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00" \
    "\x01\x00\x00\x00\x00\x00\x00" /* 11A=1 !! */
    "\x88\x88\x88\x88\x88\x88" "\x00\x00\x00" \
    "\x07"  /* EOPT, 0x22 = normal */         \
    "\x17"  /*  VGH, 0x17 == 0x00 == 20V */   \
    "\x41"  /* VSH1, 0x41 == 15V */           \
    /* This LUT never uses VSH2 */            \
    "\xA8"  /* VSH2, 0x00 == ???, POR is 5V */\
    "\x32"  /*  VSL, 0x32 == -15V */          \
    "\x20"; /* VCOM, 0x20 == -0.8V */

void test_anim(const uint8_t * const * frames, size_t n_frames) {
    const uint8_t *prev, *next, *o;

    /* Booster Soft Start Control
     * -> should only impact phase 1 to 3 (are these groups 0 to 2 ? or just 3 phases of the booster?)
     * Default values: 8B 9C 96 0F
     * - phase 1: 8B -> strength 1, min off time 8.4
     * - phase 2: 9C -> strength 2, min off time 9.8
     * - phase 3: 96 -> strength 2, min off time 3.9
     * - duration: 0F -> phase 1: 40ms, phase 2: 40ms, phase 3: 10ms */
    //send((uint8_t[]){SSD1681_BOOSTER_CTRL, 0xF4, 0x9C, 0x96, 0x0F}, 5);

    /* Now go to animation mode */
    screen_clear_image_position();
    screen_push_ws(ws_20fps);
    screen_start_multiframe();
    while(screen_busy())
        tight_loop_contents();

    /* We always want to "just draw" TODO move to screen API */
    send(SSD1681_DISPLAY_CTRL2, 0x04);

    /* The first image is special because it has no prev, but we have to be sure it is drawn correctly.
     * To do so, we configure the RAM RED (which stores prev) to read as inverse */
    prev = frames[0];
    send(SSD1681_DISPLAY_CTRL1, 0x80);  /* Inverse RED, normal B/W */

    absolute_time_t t0 = get_absolute_time(), t1;
    for(size_t i=0; i<n_frames; ++i) {
        /* Show the image and swap buffer */
        next = frames[i];
        screen_push_rams(next, prev, 5000);  /* 49ms @ 2MHz, 6.1ms @ 20MHz */
        send(SSD1681_ACTIVATE);
        //time_busy("frame");
        while(screen_busy())
            tight_loop_contents();
        o = prev;
        prev = next;
        next = o;
        /* FIXME: this shows:
         * - with EOPT = 0x07, the colors are still controlled after the frame is finished,
         * - except when there is a phase that set them to VSS.
         * -> TODO: test other values of EOPT
         * -> TODO: test with other booster control values, or putting the 00 group BEFORE the color groups */
        if ((i%10) == 7)
            ;//sleep_ms(100);

        /* Reset the trick used for the first image to obtain the inverse */
        if(i == 0)
            send(SSD1681_DISPLAY_CTRL1, 0x00);
    }
    t1 = get_absolute_time();
    screen_end_multiframe();
    while(screen_busy())
        tight_loop_contents();

    uint64_t diff = absolute_time_diff_us(t0, t1);
    printf("push+draw %d images took %" PRIu64 "µs, %f fps\n", n_frames, diff, n_frames * 1e6f/(float)(diff));
}

#include "hip_anim.h"


int main() {
    stdio_usb_init();
    log_set_level(LOG_LEVEL_INFO);

    ///* Test that the screen also work with CSn always low */
    //gpio_init(BADGE_SPI0_CSn);
    //gpio_put(BADGE_SPI0_CSn, 0);
    //gpio_set_dir(BADGE_SPI0_CSn, true);

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

    //test_clear(); sleep_ms(1000);
    ////uint8_t buf[5000];
    ////memset(buf, 0xFF, 2500);
    ////memset(buf+2500, 0x00, 2500);
    ////screen_show_image_bw(buf);
    //////screen_push_rams(buf, buf, 5000);
    //////screen_show_rams();
    ////while(screen_busy())
    ////    tight_loop_contents();
    ////sleep_ms(1000);
    //test_roll();

    test_clear();
    test_anim(hip_anim, hip_anim_n_frames);

    /* Clear to white before going to sleep */
    //test_clear();

    screen_deep_sleep();
    sleep_ms(1000);
    printf("Screen sleeping, after 1s BUSY = %d\n\n", gpio_get(BADGE_SCREEN_BUSY));
}
