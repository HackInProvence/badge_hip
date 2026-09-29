/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file video.h
 *
 * \brief Video API: plays a .epv video (see video2epaper.py) from the micro SD card (board V1.1) on the e-Paper screen.
 *
 * Uses the multiframe mode of the screen: each frame only draws the differences with the previous one,
 * with the waveform matching the frame rate of the video (10, 20 or 30 fps, 10 fps has the best contrast).
 * The next frame is read from the SD card while the screen is drawing the current one (the SPI bus is free then).
 * At the end, the last frame is redrawn with a full refresh to remove the ghosting.
 *
 * The sound of the video (if any) is played on the buzzer (see audio.h), and is the clock of the video:
 * each frame is shown when the sound reaches it, frames are skipped if the screen is late.
 *
 * Usage: screen_init(), then video_start(), and call video_task() in the main loop while it returns true.
 * The video owns the screen and the buzzer while it plays. Nothing blocks more than a frame read (~5ms).
 * */

#ifndef _VIDEO_H
#define _VIDEO_H

#include <stdbool.h>

#include "pico/time.h"

/** \brief Mount the SD card, open the video and start playing.
 *
 * \param path The .EPV file, or NULL for VIDEO.EPV or the first .EPV file at the root of the card.
 * \return false when there is no card or no video, see \ref video_message */
bool video_start(const char *path);

/** \brief Run the player, to call in the main loop.
 *
 * \return true while the video plays (or is paused, or is finishing) */
bool video_task(absolute_time_t now);

/** \brief Stop the video (the last frame is redrawn cleanly, keep calling video_task() until it returns false). */
void video_stop(void);

/** \brief Pause or resume the video. */
void video_toggle_pause(void);

bool video_is_paused(void);

/** \brief Human readable status: the error, or the statistics of the last playback. */
const char *video_message(void);

#endif /* _VIDEO_H */
