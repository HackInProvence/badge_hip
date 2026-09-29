/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file battery.h
 *
 * \brief Level of the Li-ion battery.
 *
 * On the badge, the battery (connector VBATT) goes through a 100k / 200k divider buffered by an LM321 (U7)
 * to GPIO29 (ADC3): V_adc = 2/3 V_bat. The LM321 is powered by the battery and its output can't reach its supply:
 * the measure is right up to ~V_bat - 1.3V, enough for 4.2V (2.8V needed).
 * The TP4056 charger status only drives the LEDs (red: charging, green: charged), the firmware can't read it:
 * the USB cable is detected by the USB connection with a computer or a charger that enumerates... or not at all
 * with a plain charger, then a voltage above 4.1V is shown as charging.
 *
 * The percentage follows the discharge curve of a Li-ion cell under a light load, it is an estimate.
 * */

#ifndef _BATTERY_H
#define _BATTERY_H

#include <stdbool.h>
#include <stdint.h>

#include "pico/time.h"

#define BATTERY_PERIOD_MS 2000
#define BATTERY_DIVIDER_NUM 3  /* V_bat = V_adc * 3 / 2 */
#define BATTERY_DIVIDER_DEN 2

void battery_init(void);

/** \brief Measure from time to time, to call in the main loop. */
void battery_task(absolute_time_t now);

/** \brief Battery voltage in millivolts (filtered), 0 before the first measure. */
uint16_t battery_mv(void);

/** \brief Last raw value of the ADC (0..4095), for the calibration. */
uint16_t battery_raw(void);

/** \brief Estimated charge, 0..100 %. */
int battery_percent(void);

/** \brief Charge estimate from a voltage. */
int battery_percent_of_mv(uint16_t mv);

/** \brief Probably charging: connected to a computer on USB, or above the voltage of a charged battery at rest. */
bool battery_charging(void);

#endif /* _BATTERY_H */
