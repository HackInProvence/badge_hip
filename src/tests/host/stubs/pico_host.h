/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Minimal stand-in of the Pico SDK to compile modules of the badge on a PC (see run_tests.py).
 * The hardware functions do nothing, except what the tests need to observe (SPI writes, time). */

#ifndef PICO_HOST_H
#define PICO_HOST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef unsigned int uint;
typedef uint64_t absolute_time_t;
typedef int32_t alarm_id_t;
typedef int64_t (*alarm_callback_t)(alarm_id_t, void *);
typedef void (*irq_handler_t)(void);
typedef struct spi_inst spi_inst_t;

#define spi0 ((spi_inst_t *)0)
#define spi1 ((spi_inst_t *)1)
#define GPIO_OUT 1
#define GPIO_IN 0
#define GPIO_FUNC_SPI 1
#define GPIO_FUNC_PWM 4
#define GPIO_FUNC_SIO 5
#define GPIO_IRQ_EDGE_FALL 4
#define GPIO_IRQ_EDGE_RISE 8
#define IO_IRQ_BANK0 13
#define clk_sys 5
#define bi_decl_if_func_used(x)
#define tight_loop_contents() do {} while (0)

/* Time: advanced by the tests */
extern uint64_t host_time_us;
static inline absolute_time_t get_absolute_time(void) { return host_time_us; }
static inline int64_t absolute_time_diff_us(absolute_time_t a, absolute_time_t b) { return (int64_t)(b - a); }
static inline absolute_time_t delayed_by_ms(absolute_time_t t, uint32_t ms) { return t + ms * 1000ull; }
static inline absolute_time_t delayed_by_us(absolute_time_t t, uint64_t us) { return t + us; }
static inline absolute_time_t make_timeout_time_ms(uint32_t ms) { return host_time_us + ms * 1000ull; }
static inline bool time_reached(absolute_time_t t) { return host_time_us >= t; }
static inline uint32_t time_us_32(void) { return (uint32_t)host_time_us; }
static inline alarm_id_t add_alarm_in_us(uint64_t us, alarm_callback_t cb, void *data, bool fire) { (void)us; (void)cb; (void)data; (void)fire; return 1; }

/* GPIO */
extern bool host_gpio[32];
static inline void gpio_init(uint p) { (void)p; }
static inline void gpio_put(uint p, bool v) { host_gpio[p] = v; }
static inline bool gpio_get(uint p) { return host_gpio[p]; }
static inline void gpio_set_dir(uint p, bool out) { (void)p; (void)out; }
static inline void gpio_set_function(uint p, int f) { (void)p; (void)f; }
static inline void gpio_pull_up(uint p) { (void)p; }
static inline void gpio_set_irq_enabled(uint p, uint32_t e, bool on) { (void)p; (void)e; (void)on; }
static inline void gpio_add_raw_irq_handler(uint p, irq_handler_t h) { (void)p; (void)h; }
static inline uint32_t gpio_get_irq_event_mask(uint p) { (void)p; return 0; }
static inline void gpio_acknowledge_irq(uint p, uint32_t e) { (void)p; (void)e; }
static inline void irq_set_enabled(uint n, bool on) { (void)n; (void)on; }

/* SPI: the writes are recorded */
extern uint8_t host_spi_log[65536];
extern size_t host_spi_len;
static inline uint spi_init(spi_inst_t *s, uint baud) { (void)s; return baud; }
static inline int spi_write_blocking(spi_inst_t *s, const uint8_t *d, size_t n) {
    (void)s;
    for (size_t i = 0; i < n && host_spi_len < sizeof(host_spi_log); ++i)
        host_spi_log[host_spi_len++] = d[i];
    return (int)n;
}

/* PWM, clocks */
static inline uint32_t clock_get_hz(int c) { (void)c; return 125000000; }
static inline uint pwm_gpio_to_slice_num(uint p) { return (p >> 1) & 7; }
static inline uint pwm_gpio_to_channel(uint p) { return p & 1; }
static inline void pwm_set_wrap(uint s, uint16_t w) { (void)s; (void)w; }
static inline void pwm_set_chan_level(uint s, uint c, uint16_t l) { (void)s; (void)c; (void)l; }
static inline void pwm_set_enabled(uint s, bool on) { (void)s; (void)on; }

#endif
