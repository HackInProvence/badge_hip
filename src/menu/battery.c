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
static bool low = false, low_simulated = false;
static int low_measures = 0;
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

bool battery_set_point(int i, uint16_t mv, uint16_t raw) {
    store_factory_t *f = store_factory_get();
    /* The point chosen by the user: it used to be chosen here (the same one again when the ADC was within
     * BATTERY_CAL_MIN_RAW of point 1), so with an ADC almost flat the point 2 could never be saved */
    i = i ? 1 : 0;
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
    /* Enumerated by a computer (a terminal open or not: stdio_usb_connected() was only the terminal) and the bus
     * active: the board has no VBUS sense, so an unplugged badge stays "mounted" with its bus idle (suspended) - it
     * said USB for 13 h on its battery during a battery life test; a charger alone is not seen either */
    return tud_mounted() && ! tud_suspended();
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


/* ------ Automatic calibration and the time on battery ------
 * The ADC of the badges stays almost flat for most of the discharge (a battery life test: 2530 for 13 h, then the
 * fall of the last 3 h): it cannot tell the charge left. The estimate is the time on battery since the last full
 * charge, against the battery life measured by the automatic calibration; the ADC only tells the final fall. */

static bool valid(uint16_t v) {
    return v && v != 0xFFFF;
}

bool battery_auto_life(uint16_t *minutes) {
    const store_factory_t *f = store_factory_get();
    bool ok = valid(f->battery_auto_life) && f->battery_auto_life >= BATTERY_AUTO_MIN_LIFE;
    if (minutes)
        *minutes = ok ? f->battery_auto_life : 0;
    return ok;
}

uint8_t battery_auto_step(void) {
    uint8_t s = store_get()->batt_auto_step;
    return s == BATTERY_AUTO_CHARGING || s == BATTERY_AUTO_UNPLUG || s == BATTERY_AUTO_DISCHARGING ? s
                                                                                                  : BATTERY_AUTO_NONE;
}

uint16_t battery_auto_minutes(void) {
    uint16_t m = store_get()->batt_auto_minutes;
    return battery_auto_step() == BATTERY_AUTO_DISCHARGING && m != 0xFFFF ? m : 0;
}

static void set_step(uint8_t step) {
    store_get()->batt_auto_step = step;
    store_changed();
}

void battery_auto_start(void) {
    store_t *s = store_get();
    s->batt_auto_full = 0;
    s->batt_auto_min = 0xFFFF;
    s->batt_auto_minutes = 0;
    set_step(BATTERY_AUTO_CHARGING);
    printf("battery: automatic calibration started (charge on USB until full)\n");
}

void battery_auto_cancel(void) {
    set_step(BATTERY_AUTO_NONE);
    printf("battery: automatic calibration stopped\n");
}

bool battery_auto_clear(void) {
    store_factory_t *f = store_factory_get();
    f->battery_auto_full = f->battery_auto_empty = f->battery_auto_life = 0;
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
    if (s->batt_auto_minutes == 0xFFFF || s->batt_auto_minutes < BATTERY_AUTO_MIN_LIFE) {
        printf("battery: automatic calibration: %u min on battery only, goes on\n", s->batt_auto_minutes);
        return;  /* Switched off too early: the discharge goes on */
    }
    store_factory_t *f = store_factory_get();
    f->battery_auto_full = s->batt_auto_full;
    f->battery_auto_empty = s->batt_auto_min;
    /* The minutes are saved every BATTERY_SAVE_MINUTES: on average half of it was not */
    f->battery_auto_life = s->batt_auto_minutes + BATTERY_SAVE_MINUTES / 2;
    bool saved = store_factory_save();
    printf("battery: automatic calibration done, battery life %u min (ADC full %u, empty %u) (%s)\n",
           f->battery_auto_life, f->battery_auto_full, f->battery_auto_empty, saved ? "saved" : "NOT saved");
    set_step(BATTERY_AUTO_NONE);
}

/* At each measure: the full charge (the measure on USB stable for BATTERY_AUTO_FULL_MS), the minutes on battery since
 * (saved every BATTERY_SAVE_MINUTES), the steps of the automatic calibration */
static void auto_task(absolute_time_t now) {
    static bool was_usb = false, full_seen = false;
    static uint64_t battery_ms = 0;
    static absolute_time_t last_ts = 0;
    static int unsaved = 0;
    store_t *s = store_get();
    bool usb = battery_charging();
    uint64_t dt = last_ts ? absolute_time_diff_us(last_ts, now) / 1000 : 0;
    last_ts = now;
    if (usb) {
        if (! was_usb) {
            auto_max = 0;
            full_seen = false;
            s->batt_elapsed = 0xFFFF;  /* A charge: unknown until it is full (a partial charge is not counted) */
            store_changed();
        }
        if (filtered_raw > auto_max + 1 || ! auto_max) {
            auto_max = filtered_raw;
            auto_max_ts = now;
        } else if (! full_seen && absolute_time_diff_us(auto_max_ts, now) >= BATTERY_AUTO_FULL_MS * 1000ll) {
            full_seen = true;
            s->batt_elapsed = 0;  /* Full: the time on battery starts from 0 */
            printf("battery: full (ADC %u)\n", auto_max);
            if (battery_auto_step() == BATTERY_AUTO_CHARGING) {
                s->batt_auto_full = auto_max;
                printf("battery: automatic calibration: full, unplug the badge now\n");
                s->batt_auto_step = BATTERY_AUTO_UNPLUG;
            }
            store_changed();
        }
        if (battery_auto_step() == BATTERY_AUTO_DISCHARGING) {
            printf("battery: automatic calibration: plugged in before empty, back to the charge\n");
            set_step(BATTERY_AUTO_CHARGING);  /* The discharge is lost */
        }
    } else {
        if (was_usb || ! valid(s->batt_unplug_raw)) {
            s->batt_unplug_raw = filtered_raw;  /* The level on battery: the final fall is below it */
            if (battery_auto_step() == BATTERY_AUTO_UNPLUG) {
                s->batt_auto_minutes = 0;
                s->batt_auto_min = filtered_raw;
                s->batt_auto_step = BATTERY_AUTO_DISCHARGING;
                printf("battery: automatic calibration: on battery, let it run out\n");
            }
            store_changed();
        }
        if (battery_auto_step() == BATTERY_AUTO_DISCHARGING && filtered_raw < s->batt_auto_min)
            s->batt_auto_min = filtered_raw;
        battery_ms += dt;
        while (battery_ms >= 60000) {  /* One more minute on battery */
            battery_ms -= 60000;
            if (s->batt_elapsed < 0xFFFE)
                ++s->batt_elapsed;
            if (battery_auto_step() == BATTERY_AUTO_DISCHARGING && s->batt_auto_minutes < 0xFFFE)
                ++s->batt_auto_minutes;
            if (++unsaved >= BATTERY_SAVE_MINUTES) {
                unsaved = 0;
                store_changed();  /* The last save before the battery runs out gives the battery life */
            }
        }
    }
    was_usb = usb;

    /* Low battery: confirmed over several measures, kept until the USB */
    if (usb) {
        if (low)
            printf("battery: low battery over (USB)\n");
        low = false;
        low_measures = 0;
    } else if (! low) {
        int p = battery_percent();
        bool fall = valid(s->batt_unplug_raw) && filtered_raw + BATTERY_FALL_RAW < s->batt_unplug_raw;
        low_measures = (p >= 0 && p <= BATTERY_LOW_PERCENT) || fall ? low_measures + 1 : 0;
        if (low_measures >= BATTERY_LOW_MEASURES) {
            low = true;
            printf("battery: low battery (ADC %u, level when unplugged %u, estimate %d %%)\n", filtered_raw,
                   s->batt_unplug_raw, p);
        }
    }
}

bool battery_low(void) {
    return low || low_simulated;
}

void battery_simulate_low(bool on) {
    low_simulated = on;
    printf("battery: low battery %s (simulated)\n", on ? "on" : "off");
}

bool battery_percent_estimated(void) {
    return ! battery_calibrated() && battery_percent() >= 0;
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
    /* Automatic calibration: the time on battery since the last full charge, against the battery life measured */
    const store_t *s = store_get();
    uint16_t life;
    if (! filtered_raw || ! battery_auto_life(&life) || s->batt_elapsed == 0xFFFF || battery_charging())
        return -1;  /* Not calibrated, or a partial charge: no value rather than a wrong one */
    int p = 100 - (int)((uint32_t)s->batt_elapsed * 100 / life);
    if (p < 0)
        p = 0;
    /* The final fall of the ADC (the last hours): at most BATTERY_LOW_PERCENT, whatever the time says */
    if (valid(s->batt_unplug_raw) && filtered_raw + BATTERY_FALL_RAW < s->batt_unplug_raw && p > BATTERY_LOW_PERCENT)
        p = BATTERY_LOW_PERCENT;
    return p;
}
