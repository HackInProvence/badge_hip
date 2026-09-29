/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file screen_demo.h
 *
 * \brief Non blocking demo of the screen: B/W images, 4 grays images, sub-images, 10 fps animation.
 * */

#ifndef _SCREEN_DEMO_H
#define _SCREEN_DEMO_H

#include <stdbool.h>

#include "pico/time.h"

void screen_demo_start(void);

/** \brief Ends the demo after the current step. */
void screen_demo_stop(void);

/** \brief Run the demo, returns true while running (the demo owns the screen). */
bool screen_demo_task(absolute_time_t now);

/** \brief The frames of the cicada animation (200x200, 1 bit), shared with the OLED demos:
 * the generated image headers can only be included once. */
const uint8_t *const *screen_demo_cicada_frames(size_t *n_frames);

/** \brief The SecSea image in 4 grays (200x200), the default image of the screensaver. */
void screen_demo_secsea_4g(const uint8_t **lsb, const uint8_t **msb);

#endif /* _SCREEN_DEMO_H */
