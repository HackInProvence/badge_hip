/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <string.h>

#include "display.h"
#include "gfx.h"
#include "screen.h"


typedef enum {
    D_IDLE,
    D_FAST_DRAW,  /* Multiframe started, push and draw the new image */
    D_FAST_END,  /* Leave the multiframe mode */
} display_state_t;

static display_state_t state = D_IDLE;
static uint8_t wanted[GFX_FB_SIZE];  /* Last requested image */
static uint8_t shown[GFX_FB_SIZE];  /* Image on the screen, reference for the fast refresh */
static bool dirty = false;  /* wanted != shown */
static bool full_needed = true;
static unsigned fast_count = 0;
static bool asleep = true;
static absolute_time_t last_update = 0;


void display_init(void) {
    screen_init();
}


void display_show(const uint8_t *fb) {
    memcpy(wanted, fb, GFX_FB_SIZE);
    dirty = true;
}


void display_invalidate(void) {
    full_needed = true;
    asleep = false;  /* The other user may have left the screen awake, or asleep: sleep again to be sure */
    last_update = get_absolute_time();
}


bool display_is_idle(void) {
    /* Only our own updates: the screen may be busy or asleep because of another user (image viewer, screensaver...),
     * the next user checks screen_boot() and screen_busy() itself */
    return state == D_IDLE && ! dirty;
}


void display_task(absolute_time_t now) {
    if (state == D_IDLE && ! dirty) {
        /* Deep sleep when unused for a while */
        if (! asleep && ! screen_busy() && absolute_time_diff_us(last_update, now) > DISPLAY_SLEEP_MS*1000ll) {
            if (screen_boot())  /* Only if booted, otherwise it is already asleep */
                screen_deep_sleep();
            asleep = true;
        }
        return;
    }

    /* Boots or wakes the screen when needed */
    if (! screen_boot() || screen_busy())
        return;
    asleep = false;
    last_update = now;

    switch (state) {
    case D_IDLE:
        if (full_needed || fast_count >= DISPLAY_FULL_EVERY) {
            screen_show_image_bw(wanted);
            memcpy(shown, wanted, GFX_FB_SIZE);
            dirty = false;
            full_needed = false;
            fast_count = 0;
        } else {
            screen_clear_image_position();
            screen_push_ws(screen_ws_10fps);
            screen_start_multiframe();
            state = D_FAST_DRAW;
        }
        break;
    case D_FAST_DRAW:
        /* lsb = new image, msb = image on the screen */
        screen_push_rams(wanted, shown, GFX_FB_SIZE);
        screen_draw_multiframe();
        memcpy(shown, wanted, GFX_FB_SIZE);
        dirty = false;
        state = D_FAST_END;
        break;
    case D_FAST_END:
        screen_end_multiframe();
        ++fast_count;
        state = D_IDLE;
        break;
    }
}
