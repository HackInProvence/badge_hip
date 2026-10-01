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
#include "pico/stdio_usb.h"
#include "pinouts.h"

#define ADC_SAMPLES 16

static uint16_t filtered_raw = 0;
static absolute_time_t next_ts = 0;

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
    return stdio_usb_connected();
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
}

uint16_t battery_raw(void) {
    return filtered_raw;
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
    return mv ? battery_percent_of_mv(mv) : -1;
}
