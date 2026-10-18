/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdio.h>
#include <string.h>

#include "display.h"
#include "gfx.h"
#include "screen.h"


typedef enum {
    D_IDLE,
    D_FAST_DRAW,  /* Multiframe started, push and draw the new image */
    D_FAST_END,  /* Leave the multiframe mode */
    D_SETTLE_BLACK,  /* Cleaning of a page that stays: OTP waveform in black, */
    D_SETTLE_WHITE,  /* in white, */
    D_SETTLE_SHOW,  /* then the page */
} display_state_t;

static display_state_t state = D_IDLE;
static uint8_t wanted[GFX_FB_SIZE];  /* Last requested image */
static uint8_t shown[GFX_FB_SIZE];  /* Image on the screen, reference for the fast refresh */
static bool dirty = false;  /* wanted != shown */
static bool full_needed = true;
static unsigned fast_count = 0;
static bool asleep = true;
static bool periodic_full = true;
static display_clean_t clean_mode = DISPLAY_CLEAN_DISCREET;
static absolute_time_t last_update = 0;
static bool settled = true;  /* The image on the screen was cleaned (or nothing to clean) */
static bool settle_soon = false;  /* The image shown is a page made to stay */
static absolute_time_t soon_until = 0;  /* The frames shown until then are (display_settle_soon()) */
#define SOON_WINDOW_MS 1500  /* A page is often drawn twice (a status, the battery icon...) */
static bool settle_stop = false;


void display_init(void) {
    screen_init();
}


void display_show(const uint8_t *fb) {
    memcpy(wanted, fb, GFX_FB_SIZE);
    dirty = true;
    settle_soon = absolute_time_diff_us(get_absolute_time(), soon_until) > 0;
}


void display_settle_soon(void) {
    soon_until = delayed_by_ms(get_absolute_time(), SOON_WINDOW_MS);
}


void display_invalidate(void) {
    full_needed = true;
    settled = true;  /* Someone else drew: nothing of ours to clean */
    asleep = false;  /* The other user may have left the screen awake, or asleep: sleep again to be sure */
    last_update = get_absolute_time();
}


void display_set_periodic_full(bool allowed) {
    periodic_full = allowed;
}


void display_set_clean(display_clean_t mode) {
    clean_mode = mode;
}


bool display_is_idle(void) {
    /* Only our own updates: the screen may be busy or asleep because of another user (image viewer, screensaver...),
     * the next user checks screen_boot() and screen_busy() itself */
    if (state >= D_SETTLE_BLACK)
        settle_stop = true;  /* Someone wants the screen: the cleaning stops after its current step */
    if (state != D_IDLE || dirty)
        return false;
    settled = true;  /* The screen is given away: never clean our old page over the image of its new user */
    return true;
}


void display_task(absolute_time_t now) {
    if (state >= D_SETTLE_BLACK) {
        if (! screen_boot() || screen_busy())
            return;
        if (dirty || settle_stop) {
            /* Stopped: the screen holds a step of the cleaning, the next image is a full refresh */
            printf("display: cleaning stopped\n");
            state = D_IDLE;
            settle_stop = false;
            settled = true;
            full_needed = true;
            return;
        }
        last_update = now;
        if (state == D_SETTLE_SHOW) {
            screen_show_image_bw_otp(shown);
            printf("display: page cleaned\n");
            settled = true;
            fast_count = 0;
            state = D_IDLE;
        } else {
            screen_clean(state == D_SETTLE_WHITE);
            ++state;
        }
        return;
    }
    if (state == D_IDLE && ! dirty) {
        /* The full refresh against the ghosts of the fast ones: only once the screen did not change for
         * DISPLAY_FULL_IDLE_MS (the user reads, nothing is pressed), as the games do it between the rounds */
        unsigned every = clean_mode == DISPLAY_CLEAN_FULL ? DISPLAY_FULL_EVERY : DISPLAY_FULL_EVERY_DISCREET;
        if (periodic_full && fast_count >= every && ! screen_busy()
                && absolute_time_diff_us(last_update, now) > DISPLAY_FULL_IDLE_MS * 1000ll) {
            if (! screen_boot())
                return;
            screen_show_image_bw(shown);
            fast_count = 0;
            last_update = now;
            if (clean_mode != DISPLAY_CLEAN_FULL)
                settled = true;  /* Just redrawn in full: no second cleaning of this page */
            printf("display: full refresh (idle)\n");
            return;
        }
        /* A page that stays: cleaned once, like the screensaver (only after our own drawings, and not in the games
         * that refuse the periodic full refresh) */
        if (! settled && periodic_full && clean_mode != DISPLAY_CLEAN_OFF && ! screen_busy()
                && absolute_time_diff_us(last_update, now) > (settle_soon ? DISPLAY_SETTLE_SOON_MS : DISPLAY_SETTLE_MS) * 1000ll) {
            if (! screen_boot())
                return;
            settle_stop = false;
            /* Full: black, white, then the page (~9 s); discreet: the page once (~3 s) */
            state = clean_mode == DISPLAY_CLEAN_FULL ? D_SETTLE_BLACK : D_SETTLE_SHOW;
            printf("display: cleaning the page that stays\n");
            return;
        }
        if (! settled && clean_mode == DISPLAY_CLEAN_OFF)
            settled = true;
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
        /* Never the periodic full refresh while the badge is used: it blocks the screen ~2 s (below, once idle) */
        if (full_needed) {
            screen_show_image_bw(wanted);
            memcpy(shown, wanted, GFX_FB_SIZE);
            dirty = false;
            full_needed = false;
            fast_count = 0;
            settled = false;
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
        settled = false;
        state = D_FAST_END;
        break;
    case D_FAST_END:
        screen_end_multiframe();
        ++fast_count;
        state = D_IDLE;
        break;
    }
}
