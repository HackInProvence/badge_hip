/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/binary_info.h"
#include "pico/time.h"

#include "badge_pinout.h"
#include "music.h"


static uint slice_num = -1;
static const Note *cur_notes = NULL;
static float cur_beat = 0.f;
static alarm_id_t aid = 0;


int64_t next_note(alarm_id_t id, void *user_data) {
    if (!cur_notes) {
        aid = 0;
        return 0;
    }

    ++cur_notes;
    if(cur_notes->pitch == 0 && cur_notes->duration == 0.f) {
        cur_notes = NULL;
        aid = 0;
        return 0;
    }

    // Handle either pitch or silence (pitch=0 will automatically produce a 0% PWM output)
    uint16_t wrap = cur_notes->pitch;
    pwm_set_wrap(slice_num, wrap);
    // As we don't know the channel of the configured pin in its slice, set both
    pwm_set_both_levels(slice_num, wrap>>1, wrap>>1);

    // Compute how long this silence or note lasts
    // (a 0 duration also cancels the timer)
    return -(int64_t)(1e6f * cur_notes->duration * 60.f/cur_beat);
}


void music_init(void) {
    if (slice_num != -1)
        return; // Already initialized

    // Declare our GPIO usages
    bi_decl_if_func_used(bi_1pin_with_func(BADGE_BUZZER, GPIO_FUNC_PWM));

    // Get the slice and configure it
    slice_num = pwm_gpio_to_slice_num(BADGE_BUZZER);
    float div = clock_get_hz(clk_sys) / TARGET_PWM_HZ;
    //printf("div=%f\n", div);
    pwm_set_clkdiv(slice_num, div);
}

/* Handles the alarms to resume or pause the player, returns whether the alarm was set */
bool _resume(bool enabled) {
    if (enabled) {
        if (! cur_notes)
            return false;  /* Nothging to play, next_note(NULL) would fail */
        if (aid)
            cancel_alarm(aid);
        aid = add_alarm_in_us(0, next_note, NULL, true);
    } else {
        if (aid)
            cancel_alarm(aid);
        aid = 0;
    }
    return aid > 0;
}

bool music_set_enabled(bool enabled){
    if (enabled)
        gpio_set_function(BADGE_BUZZER, GPIO_FUNC_PWM);
    bool res = _resume(enabled);
    pwm_set_enabled(slice_num, enabled);
    return res;
}

void music_set_melody(const Note *notes, float beat) {
    if (! notes)
        return;

    cur_beat = beat;
    cur_notes = notes;
    /* FIXME: add an option to enable or not */
    if (! aid) {
        music_set_enabled(true);  /* TODO: test the returned value? */
    }
}

bool music_is_playing(void) {
    return cur_notes != NULL && aid != 0;
}
