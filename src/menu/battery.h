/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file battery.h
 *
 * \brief Level of the Li-ion battery.
 *
 * On the badge, the battery (connector VBATT) goes through a 100k / 200k divider buffered by an LM321 (U7)
 * to GPIO29 (ADC3). In theory V_adc = 2/3 V_bat, but the LM321 is powered by the battery and its input and output
 * can't go above ~V_bat - 1.5V: it saturates, and the first measures (2.05V on the ADC while charging) don't follow
 * the divider. The level is therefore only shown once calibrated against a multimeter (two points,
 * BATTERY_CAL_* below): an uncalibrated badge shows nothing rather than a wrong value.
 *
 * Calibration: note the "ADC raw" of the "!" diagnostic (USB serial) and the voltage of the battery measured
 * with a multimeter at the same time, once charging and once on battery after a few minutes, then set
 * BATTERY_CAL_RAW1/MV1 and BATTERY_CAL_RAW2/MV2 (V_bat is interpolated linearly between them).
 *
 * The TP4056 charger status only drives the LEDs (red: charging, green: charged), the firmware can't read it:
 * charging is guessed from the USB connection to a computer.
 * The percentage follows the discharge curve of a Li-ion cell under a light load, it is an estimate.
 * */

#ifndef _BATTERY_H
#define _BATTERY_H

#include <stdbool.h>
#include <stdint.h>

#include "pico/time.h"

#define BATTERY_PERIOD_MS 2000

/* Calibration (ADC raw value, battery millivolts), 0 = not calibrated */
#ifndef BATTERY_CAL_RAW1
#define BATTERY_CAL_RAW1 0
#define BATTERY_CAL_MV1 0
#define BATTERY_CAL_RAW2 0
#define BATTERY_CAL_MV2 0
#endif

void battery_init(void);

/** \brief Measure from time to time, to call in the main loop. */
void battery_task(absolute_time_t now);

/** \brief Whether the measure is calibrated: otherwise the level must not be shown. */
bool battery_calibrated(void);

/** \brief Battery voltage in millivolts (filtered), 0 when unknown (not calibrated or not measured yet). */
uint16_t battery_mv(void);

/** \brief Raw value of the ADC (0..4095, filtered), for the calibration. */
uint16_t battery_raw(void);

/** \brief Estimated charge, 0..100 %, -1 when unknown. */
int battery_percent(void);

/** \brief Charge estimate from a voltage. */
int battery_percent_of_mv(uint16_t mv);

/** \brief Connected to a computer on USB (then charging, or charged). */
bool battery_charging(void);

#endif /* _BATTERY_H */
