/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file oled_demo.h
 *
 * \brief Demos for the OLED screen (see oled.h): starfield, 3D cube, cicada animation, scrolling text,
 * and the videos of the SD card (.EPV, cropped to 128x64, without sound).
 * */

#ifndef _OLED_DEMO_H
#define _OLED_DEMO_H

#include <stdbool.h>

#include "pico/time.h"

#define OLED_DEMO_COUNT 5

void oled_demo_start(int demo);
void oled_demo_stop(void);

/** \brief Name of the demo, for the menu. */
const char *oled_demo_name(int demo);

/** \brief Run the demo, to call in the main loop. */
void oled_demo_task(absolute_time_t now);

#endif /* _OLED_DEMO_H */
