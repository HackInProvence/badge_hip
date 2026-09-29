/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include "battery.h"

#include "hardware/adc.h"
#include "pico/stdio_usb.h"
#include "pinouts.h"

#define ADC_VREF_MV 3300
#define ADC_SAMPLES 16
#define CHARGING_MV 4120  /* A Li-ion cell at rest stays below, a charger goes above */

static uint16_t filtered_mv = 0;
static uint16_t last_raw = 0;
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

static uint16_t measure_mv(void) {
    adc_select_input(BADGE_VBAT - 26);  /* GPIO26..29 are ADC0..3 */
    uint32_t sum = 0;
    for (int i = 0; i < ADC_SAMPLES; ++i)  /* 2us each */
        sum += adc_read();
    last_raw = sum / ADC_SAMPLES;
    return (uint32_t)last_raw * ADC_VREF_MV * BATTERY_DIVIDER_NUM / (4095u * BATTERY_DIVIDER_DEN);
}

bool battery_charging(void) {
    return stdio_usb_connected() || filtered_mv >= CHARGING_MV;
}

void battery_task(absolute_time_t now) {
    if (absolute_time_diff_us(next_ts, now) < 0)
        return;
    next_ts = delayed_by_ms(now, BATTERY_PERIOD_MS);
    uint16_t mv = measure_mv();
    /* The voltage drops while the buzzer or the radio draw current: slow filter (1/4 of each new measure) */
    filtered_mv = filtered_mv ? (filtered_mv * 3 + mv) / 4 : mv;
}

uint16_t battery_mv(void) {
    return filtered_mv;
}

uint16_t battery_raw(void) {
    return last_raw;
}

int battery_percent(void) {
    return battery_percent_of_mv(filtered_mv);
}
