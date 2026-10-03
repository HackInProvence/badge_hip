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
 * the divider. The level is therefore only shown once calibrated against a multimeter (two points): an
 * uncalibrated badge shows nothing rather than a wrong value.
 *
 * Calibration, in Admin > Batterie (calibration): measure the battery with a multimeter, enter the voltage, save the
 * point (with the ADC value of that moment); once charging and once on battery after a few minutes (V_bat is
 * interpolated linearly between the two points). The points are factory settings (store_factory_t): kept through
 * the resets and the new firmwares. Without them, the build may give BATTERY_CAL_RAW1/MV1 and BATTERY_CAL_RAW2/MV2.
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
#define BATTERY_CAL_MIN_RAW 150  /* The 2 points must be this far apart (ADC steps, ~0.2 V; 4.2 V and 3.7 V: ~400) */

void battery_init(void);

/** \brief Measure from time to time, to call in the main loop. */
void battery_task(absolute_time_t now);

/** \brief Whether the measure is calibrated: otherwise the level must not be shown. */
bool battery_calibrated(void);

/** \brief Calibration: the battery measures \p mv millivolts while the ADC reads \p raw. Sets the first point, the
 * second one, or replaces the nearest one (the same within BATTERY_CAL_MIN_RAW), and saves it in the factory
 * settings. \return true when saved */
bool battery_set_point(uint16_t mv, uint16_t raw);

/** \brief Forget the calibration of this badge. \return true when saved */
bool battery_clear_points(void);

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

/* ------ Automatic calibration: the ADC at the end of the charge and when the battery runs out ------
 * Admin > Batterie (auto): started once, then 1. charged on USB until the measure stops rising (the full end),
 * 2. unplugged, the badge runs until its battery is empty (the lowest measure is saved as it goes down), 3. at the
 * next start that follows a real power off (not a reboot of the software) after a fall of at least
 * BATTERY_AUTO_MIN_SPAN, the empty end is confirmed: both are saved in the factory settings. Then battery_percent()
 * gives an estimate (the Li-ion curve stretched between the two ends) when there is no manual calibration. */

#define BATTERY_AUTO_FULL_MS (10 * 60 * 1000)  /* On USB, the measure no longer rising for this long: full */
#define BATTERY_AUTO_MIN_SPAN 400  /* ADC steps between full and empty (~0.5 V) to accept the empty end */
#define BATTERY_AUTO_STEP 6  /* The lowest measure is saved each time it goes down by this much (~8 mV) */

enum {
    BATTERY_AUTO_NONE = 0,
    BATTERY_AUTO_CHARGING = 0xA1,  /* Step 1: on USB until full */
    BATTERY_AUTO_UNPLUG = 0xA2,  /* Full: waiting to be unplugged */
    BATTERY_AUTO_DISCHARGING = 0xA3,  /* Step 2: on battery until it runs out */
};

/** \brief Call once at the start, after store_init(): concludes a discharge that ended with the battery empty. */
void battery_auto_boot(void);

/** \brief Starts (again) the automatic calibration. */
void battery_auto_start(void);

/** \brief Stops the automatic calibration in progress (the ends already saved are kept). */
void battery_auto_cancel(void);

/** \brief The step in progress (BATTERY_AUTO_*). */
uint8_t battery_auto_step(void);

/** \brief The ends saved (factory settings): true when both are known. */
bool battery_auto_ends(uint16_t *full, uint16_t *empty);

/** \brief The ADC at the end of the charge, the lowest on battery, of the calibration in progress (0: not yet). */
void battery_auto_progress(uint16_t *full, uint16_t *min);

/** \brief Forget the ends saved. \return true when saved */
bool battery_auto_clear(void);

/** \brief battery_percent() is an estimate of the automatic calibration (no manual calibration). */
bool battery_percent_estimated(void);

#endif /* _BATTERY_H */
