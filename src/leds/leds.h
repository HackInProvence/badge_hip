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

#include "badge_defs.h"
#include "ws2812.pio.h"


/** The number of LEDs is expected to be less than 8 (we only have 2 eyes), as it would change the structure of the library */
#define LED_N_LEDS 2

#ifndef LED_REFRESH_RATE
/** For now the refresh rate is a constant, in Hz */
#define LED_REFRESH_RATE 25
#endif

/** Color mixing/de-mixing defines */
#define LED_RGB(r,g,b) (((uint32_t)(g) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(b) << 8))
#define LED_RGB_R(color) (((color) >> 16) & 0xFF)
#define LED_RGB_G(color) (((color) >> 24) & 0xFF)
#define LED_RGB_B(color) (((color) >>  8) & 0xFF)


typedef enum {
    LED_FIXED,
    LED_WHEEL,
    LED_OOK,
    LED_FLASHES,
    LED_BREATH,
    LED_BLINK,  /* period: time on, period2: time off */
    LED_FADE,  /* period: fade in (black -> color), period2: fade out (color -> black) */
    LED_ALTERNATE,  /* color and color2 in turn, the eyes in opposition; period: a whole cycle */
    LED_SPARKLE,  /* the color at a random brightness per eye, a new one every period (0: every frame) */
} leds_anim_kind_t;

typedef struct {
    leds_anim_kind_t kind;
    //uint8_t level;
    uint32_t color;  /* BRGW in big endian where white is unused */
    uint32_t color2;  /* LED_ALTERNATE: the second color */
    absolute_time_t tref;
    uint64_t period;  /* in µs */
    uint64_t period2;  /* in µs, LED_BLINK and LED_FADE */
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

/** \brief Blinks: \p on_us with the color, \p off_us off. */
void leds_anim_blink(uint32_t color, uint64_t on_us, uint64_t off_us);

/** \brief Fades: from black to the color in \p in_us, then back to black in \p out_us, again and again. */
void leds_anim_fade(uint32_t color, uint64_t in_us, uint64_t out_us);

/** \brief Alternates \p color and \p color2, one eye with each; \p period in µs is a whole cycle. */
void leds_anim_alternate(uint32_t color, uint32_t color2, uint64_t period);

/** \brief Sparkles: the color at a random brightness on each eye, changed every \p period µs (0: every frame). */
void leds_anim_sparkle(uint32_t color, uint64_t period);

/** \brief Brightness of all the LEDs, in percent (clamped to 100), applied to every color pushed: the animations
 * keep their colors and get dimmer. 100 at boot. */
void leds_set_brightness(uint8_t percent);
uint8_t leds_get_brightness(void);

/** \brief Cancels previous animations on the LEDs, optionally shuts them down */
void leds_cancel_anim(bool leds_off);

/** Internal util made accessible to tests */
STATIC void push_led(uint32_t grbw);
uint8_t clamp2byte(float f);  /* TODO: move to utils or something, or prefix with leds_ */


#endif /* _LEDS_H */
