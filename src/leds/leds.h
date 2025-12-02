/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file leds.h
 *
 * \brief LEDs API: handle programming colors to the LEDs and animate them.
 *
 * */

#ifndef _LEDS_H
#define _LEDS_H


#include <stddef.h>

#include "pico/time.h"

#include "ws2812.pio.h"


/** The number of LEDs is expected to be less than 8 (we only have 2 eyes), as it would change the structure of the library */
#define LED_N_LEDS 2

#ifndef LED_REFRESH_RATE
/** For now the refresh rate is a constant, in Hz */
#define LED_REFRESH_RATE 25
#endif

/** Color mixing/de-mixing defines */
#define LED_RGB(r,g,b) (((g) << 24) | ((r) << 16) | ((b) << 8))
#define LED_RGB_R(color) (((color) >> 16) & 0xFF)
#define LED_RGB_G(color) (((color) >> 24) & 0xFF)
#define LED_RGB_B(color) (((color) >>  8) & 0xFF)


typedef enum {
    LED_FIXED,
    LED_WHEEL,
    LED_OOK,
    LED_FLASHES,
    LED_BREATH,
} leds_anim_kind_t;

typedef struct {
    leds_anim_kind_t kind;
    //uint8_t level;
    uint32_t color;  /* BRGW in big endian where white is unused */
    absolute_time_t tref;
    uint64_t period;  /* in µs */
    //uint16_t param;
    //uint64_t phase;  /* in µs between eyes */
} leds_anim_t;


/** \brief Initialize the LED library.
 *
 * \param pool  Can be NULL to use the default alarm pool.
 * \return false when the application could not initialize (no program space in PIO) */
bool leds_init(alarm_pool_t *pool);

/** \brief Program an animation on the LEDs.
 *
 * \param anim  Configuration is copied by this function, so it's safe to point to the stack. */
void leds_set_anim(leds_anim_t *anim);

/** \brief Set a fixed color on the leds. */
void leds_anim_fixed(uint32_t color);

/** \brief Go through the color wheel on the leds, period in µs. */
void leds_anim_wheel(uint64_t period);

/** \brief Set an ON/OFF pattern on the leds, period in µs. */
void leds_anim_ook(uint32_t color, uint64_t period);

/** \brief Set a double flash pattern (~ heartbeat). */
void leds_anim_flashes(uint32_t color);

/** \brief Sets a continuously changing color, period in µs. */
void leds_anim_breath(uint32_t color, uint64_t period);

/** \brief Cancels previous animations on the LEDs, optionally shuts them down */
void leds_cancel_anim(bool leds_off);

/** Internal util made accessible to tests */
STATIC void push_led(uint32_t grbw);
uint8_t clamp2byte(float f);  /* TODO: move to utils or something, or prefix with leds_ */


#endif /* _LEDS_H */
