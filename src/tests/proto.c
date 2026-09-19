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

#include "btns.h"
#include "log.h"
#include "noise_gen.h"
#include "pinouts.h"
#include "screen.h"

#include "hip_anim.h"
#include "secsea_26_4g.h"


/* The send() macro enables writing send(CMD, ARG1, ARG2) but we can't have it in the header nor include it from here,
 * so you have to copy-paste it to use it in tests... */
#define UINT8_LIT(...) (uint8_t[]){__VA_ARGS__}
#define send(...) _send(UINT8_LIT(__VA_ARGS__), sizeof(UINT8_LIT(__VA_ARGS__))/sizeof(uint8_t))


void test_anim(const uint8_t * const * frames, size_t n_frames, const uint8_t *ws) {
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
    screen_push_ws(ws);
    screen_start_multiframe();
    while(screen_busy())
        tight_loop_contents();

    /* The first image is special because it has no prev, but we have to be sure it is drawn correctly.
     * To do so, we configure the RAM RED (which stores prev) to read as inverse */
    prev = frames[0];
    send(SSD1681_DISPLAY_CTRL1, 0x80);  /* Inverse RED, normal B/W */

    absolute_time_t t0 = get_absolute_time(), t1;
    for(size_t i=0; i<n_frames; ++i) {
        /* Show the image and swap buffer */
        next = frames[i];
        screen_push_rams(next, prev, 5000);  /* 49ms @ 2MHz, 6.1ms @ 20MHz */
        screen_draw_multiframe();
        //time_busy("frame");
        while(screen_busy()) {
            noise_gen_set_enabled(btns_get_state());
        }
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
    while(screen_busy()) {
        noise_gen_set_enabled(btns_get_state());
    }

    uint64_t diff = absolute_time_diff_us(t0, t1);
    printf("push+draw %d images took %" PRIu64 "µs, %f fps\n", n_frames, diff, n_frames * 1e6f/(float)(diff));
}


int main() {
    stdio_usb_init();
    log_set_level(LOG_LEVEL_INFO);

    btns_init();

    noise_gen_init_play();
    noise_gen_set_enabled(false);

    screen_init();
    printf("boot sequence\n");
    while(! screen_boot())
        tight_loop_contents();

    size_t len = screen_clear_image_position();

    screen_clear(1);  /* Tests showed that normal draws can occur after this one */
    while(screen_busy())
        tight_loop_contents();

    while (true) {
        test_anim(hip_anim, hip_anim_n_frames, screen_ws_10fps);
        screen_show_image_4g(secsea_26_4g_lsb, secsea_26_4g_msb);
        absolute_time_t until = make_timeout_time_ms(10000);
        while (!time_reached(until)) {
            noise_gen_set_enabled(btns_get_state());
        }
    }
}
