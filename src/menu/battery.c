/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "battery.h"
#include "store.h"

#include "hardware/adc.h"
#include "hardware/watchdog.h"
#include "pico/stdio_usb.h"
#include "tusb.h"
#include "pinouts.h"

#define ADC_SAMPLES 16

static uint16_t filtered_raw = 0;
static absolute_time_t next_ts = 0;
static void auto_task(absolute_time_t now);
static uint16_t auto_max = 0;  /* Automatic calibration, on USB: the highest measure, and since when */
static absolute_time_t auto_max_ts = 0;

/* Voltage (mV) -> charge (%) of a Li-ion cell under a light load, linear between the points */
static const uint16_t CURVE[][2] = {
    {3300, 0}, {3500, 5}, {3600, 10}, {3700, 25}, {3750, 35}, {3800, 45},
    {3850, 55}, {3900, 65}, {3950, 72}, {4000, 80}, {4100, 92}, {4180, 100},
};
#define CURVE_N (sizeof(CURVE) / sizeof(CURVE[0]))

int battery_percent_of_mv(uint16_t mv) {
    if (mv <= CURVE[0][0])
        return 0;
    for (unsigned i = 1; i < CURVE_N; ++i)
        if (mv < CURVE[i][0])
            return CURVE[i-1][1] + (mv - CURVE[i-1][0]) * (CURVE[i][1] - CURVE[i-1][1]) / (CURVE[i][0] - CURVE[i-1][0]);
    return 100;
}

void battery_init(void) {
    adc_init();
    adc_gpio_init(BADGE_VBAT);
}

/* The calibration of this badge (factory settings, admin menu), otherwise the one of the build */
static void points(int32_t raw[2], int32_t mv[2]) {
    const store_factory_t *f = store_factory_get();
    bool factory = f->battery_mv[0] && f->battery_mv[1];
    for (int i = 0; i < 2; ++i) {
        raw[i] = factory ? f->battery_raw[i] : i ? BATTERY_CAL_RAW2 : BATTERY_CAL_RAW1;
        mv[i] = factory ? f->battery_mv[i] : i ? BATTERY_CAL_MV2 : BATTERY_CAL_MV1;
    }
}

bool battery_calibrated(void) {
    int32_t raw[2], mv[2];
    points(raw, mv);
    return abs(raw[1] - raw[0]) >= BATTERY_CAL_MIN_RAW && mv[0] && mv[1];
}

bool battery_set_point(uint16_t mv, uint16_t raw) {
    store_factory_t *f = store_factory_get();
    int i;
    if (! f->battery_mv[0] || abs(raw - f->battery_raw[0]) < BATTERY_CAL_MIN_RAW)
        i = 0;  /* First point, or the same one measured again */
    else if (! f->battery_mv[1] || abs(raw - f->battery_raw[1]) < BATTERY_CAL_MIN_RAW)
        i = 1;
    else
        i = abs(raw - f->battery_raw[0]) < abs(raw - f->battery_raw[1]) ? 0 : 1;  /* Replace the nearest one */
    f->battery_raw[i] = raw;
    f->battery_mv[i] = mv;
    printf("battery: point %d set, ADC raw %u = %u mV\n", i + 1, raw, mv);
    return store_factory_save();
}

bool battery_clear_points(void) {
    store_factory_t *f = store_factory_get();
    memset(f->battery_raw, 0, sizeof(f->battery_raw));
    memset(f->battery_mv, 0, sizeof(f->battery_mv));
    printf("battery: calibration cleared\n");
    return store_factory_save();
}

bool battery_charging(void) {
    /* Enumerated by a computer (a terminal open or not: stdio_usb_connected() was only the terminal); the board
     * has no VBUS sense: a charger alone is not seen */
    return tud_mounted();
}

void battery_task(absolute_time_t now) {
    if (absolute_time_diff_us(next_ts, now) < 0)
        return;
    next_ts = delayed_by_ms(now, BATTERY_PERIOD_MS);
    adc_select_input(BADGE_VBAT - 26);  /* GPIO26..29 are ADC0..3 */
    uint32_t sum = 0;
    for (int i = 0; i < ADC_SAMPLES; ++i)  /* 2us each */
        sum += adc_read();
    uint16_t raw = sum / ADC_SAMPLES;
    /* The voltage drops while the buzzer or the radio draw current: slow filter (1/4 of each new measure) */
    filtered_raw = filtered_raw ? (filtered_raw * 3 + raw) / 4 : raw;
    auto_task(now);
}

uint16_t battery_raw(void) {
    return filtered_raw;
}


/* ------ Automatic calibration ------ */

static bool valid(uint16_t v) {
    return v && v != 0xFFFF;
}

bool battery_auto_ends(uint16_t *full, uint16_t *empty) {
    const store_factory_t *f = store_factory_get();
    bool ok = valid(f->battery_auto_full) && valid(f->battery_auto_empty)
              && f->battery_auto_full > f->battery_auto_empty + BATTERY_AUTO_MIN_SPAN;
    if (full)
        *full = ok ? f->battery_auto_full : 0;
    if (empty)
        *empty = ok ? f->battery_auto_empty : 0;
    return ok;
}

uint8_t battery_auto_step(void) {
    uint8_t s = store_get()->batt_auto_step;
    return s == BATTERY_AUTO_CHARGING || s == BATTERY_AUTO_UNPLUG || s == BATTERY_AUTO_DISCHARGING ? s
                                                                                                  : BATTERY_AUTO_NONE;
}

void battery_auto_progress(uint16_t *full, uint16_t *min) {
    store_t *s = store_get();
    *full = battery_auto_step() >= BATTERY_AUTO_UNPLUG && valid(s->batt_auto_full) ? s->batt_auto_full : 0;
    *min = battery_auto_step() == BATTERY_AUTO_DISCHARGING && valid(s->batt_auto_min) ? s->batt_auto_min : 0;
}

static void set_step(uint8_t step) {
    store_get()->batt_auto_step = step;
    store_changed();
}

void battery_auto_start(void) {
    store_t *s = store_get();
    s->batt_auto_full = 0;
    s->batt_auto_min = 0xFFFF;
    auto_max = 0;
    auto_max_ts = get_absolute_time();
    set_step(BATTERY_AUTO_CHARGING);
    printf("battery: automatic calibration started (charge on USB until full)\n");
}

void battery_auto_cancel(void) {
    set_step(BATTERY_AUTO_NONE);
    printf("battery: automatic calibration stopped\n");
}

bool battery_auto_clear(void) {
    store_factory_t *f = store_factory_get();
    f->battery_auto_full = f->battery_auto_empty = 0;
    printf("battery: automatic calibration cleared\n");
    return store_factory_save();
}

void battery_auto_boot(void) {
    store_t *s = store_get();
    if (battery_auto_step() != BATTERY_AUTO_DISCHARGING)
        return;
    /* A reboot of the software (watchdog: a new firmware, a command, the RESET of the menu) is not the battery */
    if (watchdog_caused_reboot()) {
        printf("battery: automatic calibration: discharging goes on (software reboot)\n");
        return;
    }
    if (! valid(s->batt_auto_full) || ! valid(s->batt_auto_min) || s->batt_auto_full < s->batt_auto_min
        || s->batt_auto_full - s->batt_auto_min < BATTERY_AUTO_MIN_SPAN) {
        printf("battery: automatic calibration: discharge too short (%u -> %u), goes on\n", s->batt_auto_full,
               s->batt_auto_min);
        return;  /* Switched off too early: the discharge goes on */
    }
    store_factory_t *f = store_factory_get();
    f->battery_auto_full = s->batt_auto_full;
    f->battery_auto_empty = s->batt_auto_min;
    bool saved = store_factory_save();
    printf("battery: automatic calibration done, full ADC %u, empty ADC %u (%s)\n", f->battery_auto_full,
           f->battery_auto_empty, saved ? "saved" : "NOT saved");
    set_step(BATTERY_AUTO_NONE);
}

/* The steps, at each measure */
static void auto_task(absolute_time_t now) {
    store_t *s = store_get();
    switch (battery_auto_step()) {
    case BATTERY_AUTO_CHARGING:
        if (! battery_charging()) {
            auto_max = 0;  /* Unplugged before the end of the charge: waits for the USB again */
            break;
        }
        if (filtered_raw > auto_max + 1 || ! auto_max) {
            auto_max = filtered_raw;
            auto_max_ts = now;
        } else if (absolute_time_diff_us(auto_max_ts, now) >= BATTERY_AUTO_FULL_MS * 1000ll) {
            s->batt_auto_full = auto_max;
            printf("battery: automatic calibration: full, ADC %u (unplug the badge now)\n", auto_max);
            set_step(BATTERY_AUTO_UNPLUG);
        }
        break;
    case BATTERY_AUTO_UNPLUG:
        if (! battery_charging()) {
            s->batt_auto_min = filtered_raw;
            printf("battery: automatic calibration: on battery, ADC %u (let it run out)\n", filtered_raw);
            set_step(BATTERY_AUTO_DISCHARGING);
        }
        break;
    case BATTERY_AUTO_DISCHARGING:
        if (battery_charging()) {
            /* Plugged in before the battery ran out: the discharge is lost, back to the charge */
            printf("battery: automatic calibration: plugged in before empty, back to the charge\n");
            auto_max = 0;
            set_step(BATTERY_AUTO_CHARGING);
        } else if (filtered_raw + BATTERY_AUTO_STEP <= s->batt_auto_min) {
            s->batt_auto_min = filtered_raw;
            store_changed();  /* Saved as it goes down: the last one before the end is the empty end */
        }
        break;
    default:
        break;
    }
}

bool battery_percent_estimated(void) {
    return ! battery_calibrated() && battery_auto_ends(NULL, NULL);
}

uint16_t battery_mv(void) {
    if (! battery_calibrated() || ! filtered_raw)
        return 0;  /* Not calibrated: no value rather than a wrong one */
    int32_t raw[2], mv[2];
    points(raw, mv);
    int32_t v = mv[0] + ((int32_t)filtered_raw - raw[0]) * (mv[1] - mv[0]) / (raw[1] - raw[0]);
    return v < 0 ? 0 : v > 5000 ? 5000 : v;
}

int battery_percent(void) {
    uint16_t mv = battery_mv();
    if (mv)
        return battery_percent_of_mv(mv);
    uint16_t full, empty;
    if (! filtered_raw || ! battery_auto_ends(&full, &empty))
        return -1;  /* Not calibrated: no value rather than a wrong one */
    /* Automatic calibration: the curve stretched between its empty end (CURVE 0 %) and its full end (100 %) */
    int32_t lo = CURVE[0][0], hi = CURVE[CURVE_N - 1][0];
    int32_t v = lo + ((int32_t)filtered_raw - empty) * (hi - lo) / (full - empty);
    return battery_percent_of_mv(v < lo ? lo : v > hi ? hi : v);
}
