/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file leds.h
 *
 * \brief LEDs API:
 *
 * TODO:
 * - maybe it's time to decide how to handle debug... take inspiration on flipper?
 * */

#ifndef _LEDS_H
#define _LEDS_H

#include "ws2812.pio.h"


void leds_init(void);
void leds_test(void);


#endif /* _LEDS_H */
