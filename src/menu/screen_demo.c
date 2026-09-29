/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdio.h>

#include "screen.h"
#include "screen_demo.h"

#include "text_bw.h"
#include "hip_bw.h"
#include "secsea_bw.h"
#include "grad_4g.h"
#include "secsea_4g.h"
#include "hip_4g.h"
#include "notif_4g.h"
#include "companion.h"
#include "hip_anim.h"


#define PAUSE_MS 1000

typedef enum {
    SCR_IDLE = 0,
    SCR_TEXT_BW, SCR_HIP_BW, SCR_SECSEA_BW,
    SCR_GRAD_4G, SCR_SECSEA_4G, SCR_HIP_4G,
    SCR_SUBIMAGE,
    SCR_CLEAR,
    SCR_ANIM_START, SCR_ANIM, SCR_ANIM_END,
    SCR_DONE,
} screen_step_t;

static screen_step_t step = SCR_IDLE;
static bool pause_pending = false;  /* Pause once the current drawing is done */
static absolute_time_t next_ts = 0;
static size_t anim_i = 0;


const uint8_t *const *screen_demo_cicada_frames(size_t *n_frames) {
    *n_frames = hip_anim_n_frames;
    return hip_anim;
}


void screen_demo_secsea_4g(const uint8_t **lsb, const uint8_t **msb) {
    *lsb = secsea_4g_lsb;
    *msb = secsea_4g_msb;
}


void screen_demo_start(void) {
    if (step != SCR_IDLE)
        return;
    step = SCR_TEXT_BW;
    pause_pending = false;
    next_ts = get_absolute_time();
}


void screen_demo_stop(void) {
    if (step == SCR_ANIM_START || step == SCR_ANIM)
        step = SCR_ANIM_END;  /* Must leave the multiframe mode */
    else if (step != SCR_IDLE && step != SCR_ANIM_END)
        step = SCR_DONE;
}


bool screen_demo_task(absolute_time_t now) {
    if (step == SCR_IDLE)
        return false;
    /* Boots (or wakes from deep sleep) the screen, a few calls are needed */
    if (! screen_boot() || screen_busy())
        return true;
    if (pause_pending) {
        next_ts = delayed_by_ms(now, PAUSE_MS);
        pause_pending = false;
    }
    if (absolute_time_diff_us(now, next_ts) > 0)
        return true;

    size_t len;
    switch (step) {
    case SCR_TEXT_BW:
        screen_show_image_bw(text_bw);
        break;
    case SCR_HIP_BW:
        screen_show_image_bw(hip_bw);
        break;
    case SCR_SECSEA_BW:
        screen_show_image_bw(secsea_bw);
        break;
    case SCR_GRAD_4G:
        screen_show_image_4g(grad_4g_lsb, grad_4g_msb);
        break;
    case SCR_SECSEA_4G:
        screen_show_image_4g(secsea_4g_lsb, secsea_4g_msb);
        break;
    case SCR_HIP_4G:
        screen_show_image_4g(hip_4g_lsb, hip_4g_msb);
        break;
    case SCR_SUBIMAGE:
        /* Draw on top of the previous image */
        screen_push_ws(screen_ws_1681_4grays);
        len = screen_set_image_position(48, 68, 168, 168);
        screen_push_rams(notif_4g_lsb, notif_4g_msb, len);
        len = screen_set_image_position(32, 18, 32+companion_width*8, 18+companion_height);
        screen_push_rams(companion_lsb, companion_msb, len);
        screen_show_rams();
        break;
    case SCR_CLEAR:
        screen_clear(1);
        break;
    case SCR_ANIM_START:
        screen_clear_image_position();
        screen_push_ws(screen_ws_10fps);
        screen_start_multiframe();
        anim_i = 0;
        step = SCR_ANIM;
        return true;  /* No pause between frames */
    case SCR_ANIM:
        /* lsb = next image, msb = previous image */
        screen_push_rams(hip_anim[anim_i], hip_anim[anim_i ? anim_i-1 : 0], (SCREEN_WIDTH*SCREEN_HEIGHT)/8);
        screen_draw_multiframe();
        if (++anim_i >= hip_anim_n_frames)
            step = SCR_ANIM_END;
        return true;
    case SCR_ANIM_END:
        screen_end_multiframe();
        step = SCR_DONE;
        return true;
    case SCR_DONE:
    default:
        step = SCR_IDLE;
        return false;
    }
    printf("screen demo: step %d\n", step);
    step++;
    pause_pending = true;
    return true;
}
