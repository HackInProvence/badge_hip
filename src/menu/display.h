/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file display.h
 *
 * \brief Non blocking display of frame buffers (see gfx.h) for user interfaces.
 *
 * Updates use a fast refresh (multiframe mode with the 10 fps waveform, ~0.3s) that only changes what differs,
 * and a full refresh every DISPLAY_FULL_EVERY updates (or when requested) to remove the ghosting.
 * The screen goes to deep sleep after DISPLAY_SLEEP_MS without update, as recommended by the datasheet.
 *
 * Other code can use the screen when display_is_idle() (e.g. the video player), then must call display_invalidate()
 * so that the next update is a full refresh.
 * */

#ifndef _DISPLAY_H
#define _DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "pico/time.h"

#define DISPLAY_FULL_EVERY 15
#define DISPLAY_SLEEP_MS 20000

/** \brief Initialize the screen (calls screen_init()). */
void display_init(void);

/** \brief Request to show this frame buffer (copied, GFX_FB_SIZE bytes). */
void display_show(const uint8_t *fb);

/** \brief The screen content is unknown (someone else used the screen): next update is a full refresh. */
void display_invalidate(void);

/** \brief Run the display, to call in the main loop. */
void display_task(absolute_time_t now);

/** \brief Allow the full refresh every DISPLAY_FULL_EVERY updates (default), or delay it while false
 * (games: the full refresh blinks for ~2s). */
void display_set_periodic_full(bool allowed);

/** \brief Whether all updates are done and the screen can be used by someone else. */
bool display_is_idle(void);

#endif /* _DISPLAY_H */
