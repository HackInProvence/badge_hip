/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */


#include <math.h>

#include "hardware/pio.h"
#include "pico/binary_info.h"
#include "pico/time.h"

#include "log.h"
#include "leds.h"
#include "pinouts.h"


/* Configured state machine which runs our program */
static PIO pio = NULL;
static uint sm = -1;
static alarm_pool_t *pool = NULL;
static repeating_timer_t timer = {};
static leds_anim_t animation = {};
static volatile uint8_t brightness = 100;  /* Percent, applied by push_led() (read from the timer IRQ) */

/* LED_SPARKLE: the brightness of each eye, drawn again when the step (diff_us / period) changes */
static uint32_t sparkle_rng = 2463534242u;  /* xorshift32 state, never 0 */
static uint64_t sparkle_step = UINT64_MAX;
static float sparkle_level[LED_N_LEDS];


/* Color order is 0xGGRRBBWW (white is not used for us).
 * Non blocking and not synchronized: if you push colors fast enough, they will be queued and passed to next leds,
 *  if you wait too long, only the first led will be programmed,
 *  if you push too fast, the PIO FIFO will overflow (drop or replace last one). */
STATIC void push_led(uint32_t grbw){
    if(! pio) {
        log_warning("led: push a color before init");
        return;
    }
    uint8_t k = brightness;
    if (k < 100) {
        uint32_t g = ((grbw >> 24) & 0xFF) * k / 100;
        uint32_t r = ((grbw >> 16) & 0xFF) * k / 100;
        uint32_t b = ((grbw >>  8) & 0xFF) * k / 100;
        grbw = (g << 24) | (r << 16) | (b << 8) | (grbw & 0xFF);
    }
    pio_sm_put(pio, sm, grbw);
}


/* Maps 0..1 to 0..255 */
uint8_t clamp2byte(float f) {
    if (f <= 0.f)
        return 0;
    if (f >= 1.f)
        return 255;
    return lroundf(f*255);
}


/* The IRQ handler that does the animations, minimal work should be done here.
 * We don't care about the rt argument, we only have one repeating timer, and it's global. */
bool change_leds(repeating_timer_t *rt) {
    /* Compute LED colors and push them (non blocking, we are in an IRQ...) */
    uint64_t diff_us = absolute_time_diff_us(animation.tref, get_absolute_time());
    float f;
    uint8_t r, g, b;
    uint64_t n, cycle;
    for(uint8_t i=0; i<LED_N_LEDS; ++i) {
        switch(animation.kind) {
        case LED_FIXED:
            push_led(animation.color);
            break;
        case LED_WHEEL:
            f = (float)(diff_us)/(float)(animation.period);
            r = clamp2byte(cosf((2*M_PI)*(f + 0.f/3.f + i/2.f)));
            g = clamp2byte(cosf((2*M_PI)*(f + 1.f/3.f + i/2.f)));
            b = clamp2byte(cosf((2*M_PI)*(f + 2.f/3.f + i/2.f)));
            push_led(LED_RGB(r,g,b));
            break;
        case LED_OOK:
            if (animation.period && (2*diff_us/animation.period) % 2)
                /* Odd number of half periods: LED OFF */
                push_led(0);
            else
                /* Even number of half periods: show color */
                push_led(animation.color);
            break;
        case LED_FLASHES:
            /* For now 2 short flashes, then a pause, ignore params */
            n = diff_us/100000;
            cycle = n%12;
            if (cycle % 2)
                push_led(0);
            else if (cycle < 4)
                push_led(animation.color);
            break;
        case LED_BREATH:
            f = (float)(diff_us)/(float)(animation.period);
            f = cosf((2*M_PI)*f)*.45f+.55f;
            r = LED_RGB_R(animation.color) * f;
            g = LED_RGB_G(animation.color) * f;
            b = LED_RGB_B(animation.color) * f;
            push_led(LED_RGB(r,g,b));
            break;
        case LED_BLINK:
            cycle = animation.period + animation.period2;
            push_led(cycle && diff_us % cycle >= animation.period ? 0 : animation.color);
            break;
        case LED_FADE:
            cycle = animation.period + animation.period2;
            n = cycle ? diff_us % cycle : 0;
            if (n < animation.period)
                f = animation.period ? (float)n / (float)animation.period : 1.f;  /* Rising */
            else
                f = animation.period2 ? 1.f - (float)(n - animation.period) / (float)animation.period2 : 0.f;
            r = LED_RGB_R(animation.color) * f;
            g = LED_RGB_G(animation.color) * f;
            b = LED_RGB_B(animation.color) * f;
            push_led(LED_RGB(r,g,b));
            break;
        case LED_ALTERNATE:
            /* Like LED_OOK with the second color instead of black, the other eye a half period later */
            n = animation.period ? 2*diff_us/animation.period : 0;
            push_led((n + i) % 2 ? animation.color2 : animation.color);
            break;
        case LED_SPARKLE:
            n = animation.period ? diff_us/animation.period : diff_us;
            if (i == 0 && n != sparkle_step) {
                sparkle_step = n;
                for (uint8_t j=0; j<LED_N_LEDS; ++j) {
                    sparkle_rng ^= sparkle_rng << 13;
                    sparkle_rng ^= sparkle_rng >> 17;
                    sparkle_rng ^= sparkle_rng << 5;
                    sparkle_level[j] = .1f + .9f * (float)(sparkle_rng & 0xFF) / 255.f;
                }
            }
            f = sparkle_level[i];
            r = LED_RGB_R(animation.color) * f;
            g = LED_RGB_G(animation.color) * f;
            b = LED_RGB_B(animation.color) * f;
            push_led(LED_RGB(r,g,b));
            break;
        default:
            /* Forgotten implementation, do nothing */
            break;
        }
    }

    return true;
}


bool leds_init(alarm_pool_t *pool_init) {
    if (pio) {
        log_warning("leds second init");
        return true;
    }

    pool = pool_init;
    if (! pool) {
        pool = alarm_pool_get_default();
        log_info("led: use default alarm pool (core %d, pool %p)", alarm_pool_core_num(pool), pool);
    }

    /* GPIO usages */
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_LED, "LEDs WS2812B"));

    // This will find a free pio and state machine for our program and load it for us
    // We use pio_claim_free_sm_and_add_program_for_gpio_range so we can address gpios >= 32 if needed and supported by the hardware
    uint offset;
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(
        &ws2812_program,
        &pio, &sm, &offset,
        BADGE_LED, 1 /* count */,
        true  /* bool set_gpio_base */
    );
    if (! success) {
        log_warning("leds failed to init, could not allocate PIO program");
        pio = NULL;
        return false;
    }

    /* Setup the state machine now that we have allocated the pio and sm */
    ws2812_program_init(pio, sm, offset, BADGE_LED, 800000, false);

    /* FIXME: put this somewhere else? */
    pio_gpio_init(pio, BADGE_LED);
    pio_sm_set_enabled(pio, sm, true);

    return true;
}


void leds_set_anim(leds_anim_t *anim) {
    if (! pio) {
        log_warning("leds_set_anim() called but lib not initialized");
        return;
    }

    /* Only create the timer if it does not exist beforehand */
    if (timer.alarm_id == 0) {
        /* The minus in the delay is important: it specifies that this is the time between 2 triggers */
        bool success = alarm_pool_add_repeating_timer_us(pool, -1000000/LED_REFRESH_RATE, change_leds, NULL, &timer);
        if (! success) {
            log_warning("led: could not set_anim, no more timer available");
            timer.alarm_id = 0;
        }
    }

    if (timer.alarm_id != 0) {
        /* There is a small risk that the timer (a previously setup timer) interrupts this copy */
        animation = *anim;
    }
}


void leds_anim_fixed(uint32_t color) {
    /* TODO: we could debate whether these helpers are useful or not */
    leds_anim_t anim = {
        .kind = LED_FIXED,
        .color = color,
        .tref = get_absolute_time(),
    };
    leds_set_anim(&anim);
}

void leds_anim_wheel(uint64_t period) {
    /* TODO: we could debate whether these helpers are useful or not */
    leds_anim_t anim = {
        .kind = LED_WHEEL,
        .period = period,
        .tref = get_absolute_time(),
    };
    leds_set_anim(&anim);
}

void leds_anim_ook(uint32_t color, uint64_t period) {
    /* TODO: we could debate whether these helpers are useful or not */
    leds_anim_t anim = {
        .kind = LED_OOK,
        .color = color,
        .period = period,
        .tref = get_absolute_time(),
    };
    leds_set_anim(&anim);
}

void leds_anim_flashes(uint32_t color) {
    /* TODO: we could debate whether these helpers are useful or not */
    leds_anim_t anim = {
        .kind = LED_FLASHES,
        .color = color,
        .tref = get_absolute_time(),
    };
    leds_set_anim(&anim);
}

void leds_anim_breath(uint32_t color, uint64_t period) {
    /* TODO: we could debate whether these helpers are useful or not */
    leds_anim_t anim = {
        .kind = LED_BREATH,
        .color = color,
        .period = period,
        .tref = get_absolute_time(),
    };
    leds_set_anim(&anim);
}


void leds_anim_blink(uint32_t color, uint64_t on_us, uint64_t off_us) {
    leds_anim_t anim = {
        .kind = LED_BLINK,
        .color = color,
        .period = on_us,
        .period2 = off_us,
        .tref = get_absolute_time(),
    };
    leds_set_anim(&anim);
}

void leds_anim_fade(uint32_t color, uint64_t in_us, uint64_t out_us) {
    leds_anim_t anim = {
        .kind = LED_FADE,
        .color = color,
        .period = in_us,
        .period2 = out_us,
        .tref = get_absolute_time(),
    };
    leds_set_anim(&anim);
}

void leds_anim_alternate(uint32_t color, uint32_t color2, uint64_t period) {
    leds_anim_t anim = {
        .kind = LED_ALTERNATE,
        .color = color,
        .color2 = color2,
        .period = period,
        .tref = get_absolute_time(),
    };
    leds_set_anim(&anim);
}

void leds_anim_sparkle(uint32_t color, uint64_t period) {
    leds_anim_t anim = {
        .kind = LED_SPARKLE,
        .color = color,
        .period = period,
        .tref = get_absolute_time(),
    };
    leds_set_anim(&anim);
}


void leds_set_brightness(uint8_t percent) {
    brightness = percent > 100 ? 100 : percent;
}

uint8_t leds_get_brightness(void) {
    return brightness;
}


void leds_cancel_anim(bool leds_off) {
    if (! pio) {
        log_warning("leds_cancel_anim() called but lib not initialized");
        return;
    }

    if (timer.alarm_id == 0) {
        log_info("leds_cancel_anim() no current animation");
    } else {
        cancel_repeating_timer(&timer);
        timer.alarm_id = 0;
    }

    for(uint8_t i=0; leds_off && i<LED_N_LEDS; ++i) {
        push_led(0);
    }
}
