/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <string.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "pico/binary_info.h"
#include "pico/time.h"

#include "ir.h"
#include "pinouts.h"


#define END_SILENCE_US 100000  /* A signal ends after this silence */
#define MIN_PULSES 8  /* Shorter signals are noise */

/* Recording (written by the GPIO interrupt) */
static volatile bool recording = false;
static volatile uint16_t rec_n = 0;
static volatile uint32_t last_edge_us = 0;
static uint16_t rec_us[IR_MAX_PULSES];

/* Sending (alarm callbacks) */
static volatile bool sending = false;
static ir_signal_t tx;
static volatile uint16_t tx_i = 0;
static uint slice, channel;
static uint16_t carrier_level;


/* Raw handler of this pin only: the SDK has a single "callback" per core, shared with the other modules */
static void edge(void) {
    uint32_t events = gpio_get_irq_event_mask(BADGE_IR_RX);
    if (! events)
        return;
    gpio_acknowledge_irq(BADGE_IR_RX, events);
    if (! recording)
        return;
    uint32_t now = time_us_32();
    bool mark = ! gpio_get(BADGE_IR_RX);  /* The receiver output is low during a mark */
    if (rec_n == 0) {
        /* Wait for the first mark */
        if (mark) {
            last_edge_us = now;
            rec_n = 1;
        }
        return;
    }
    /* The duration that just ended: a mark when rec_n is odd... */
    uint32_t d = now - last_edge_us;
    last_edge_us = now;
    if (rec_n - 1 < IR_MAX_PULSES)
        rec_us[rec_n - 1] = d > 0xFFFF ? 0xFFFF : d;
    ++rec_n;
}


void ir_init(void) {
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_IR_RX, "IR receiver"));
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_IR_TX, "IR LED"));

    gpio_init(BADGE_IR_RX);
    gpio_pull_up(BADGE_IR_RX);
    gpio_add_raw_irq_handler(BADGE_IR_RX, edge);
    gpio_set_irq_enabled(BADGE_IR_RX, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    irq_set_enabled(IO_IRQ_BANK0, true);

    /* 38kHz carrier with a 1/3 duty cycle on the IR LED, disabled (level 0) between the marks */
    slice = pwm_gpio_to_slice_num(BADGE_IR_TX);
    channel = pwm_gpio_to_channel(BADGE_IR_TX);
    uint32_t wrap = clock_get_hz(clk_sys) / IR_CARRIER_HZ - 1;
    pwm_set_wrap(slice, wrap);
    carrier_level = (wrap + 1) / 3;
    pwm_set_chan_level(slice, channel, 0);
    pwm_set_enabled(slice, true);
    gpio_set_function(BADGE_IR_TX, GPIO_FUNC_PWM);
}


void ir_record_start(void) {
    rec_n = 0;
    recording = true;
}


void ir_record_stop(void) {
    recording = false;
}


bool ir_recording(void) {
    return recording;
}


bool ir_record_task(ir_signal_t *signal) {
    if (! recording || rec_n == 0)
        return false;
    if (time_us_32() - last_edge_us < END_SILENCE_US)
        return false;

    /* Silence: the signal is complete (the last duration is the final space, not stored) */
    recording = false;
    uint16_t n = rec_n - 1;
    if (n > IR_MAX_PULSES)
        n = IR_MAX_PULSES;
    if (n % 2 == 0)
        --n;  /* End with a mark */
    if (n < MIN_PULSES) {
        recording = true;  /* Noise: keep waiting */
        rec_n = 0;
        return false;
    }
    signal->n = n;
    memcpy(signal->us, rec_us, n * sizeof(uint16_t));
    return true;
}


static int64_t next_pulse(alarm_id_t id, void *user_data) {
    (void)id;
    (void)user_data;
    if (tx_i >= tx.n) {
        pwm_set_chan_level(slice, channel, 0);
        sending = false;
        return 0;
    }
    /* Even index: mark (carrier on), odd: space */
    pwm_set_chan_level(slice, channel, tx_i % 2 ? 0 : carrier_level);
    uint16_t d = tx.us[tx_i++];
    return -(int64_t)d;  /* Relative to the previous target: no drift */
}


bool ir_send(const ir_signal_t *signal) {
    if (sending || signal->n == 0)
        return false;
    recording = false;  /* Don't record our own signal */
    memcpy(&tx, signal, sizeof(tx));
    tx_i = 0;
    sending = true;
    if (add_alarm_in_us(10, next_pulse, NULL, true) < 0) {
        sending = false;
        return false;
    }
    return true;
}


bool ir_sending(void) {
    return sending;
}


/* NEC: 9ms mark, 4.5ms space, 32 bits (562µs mark, then 562µs space for 0 or 1687µs for 1), LSB first, final mark */
static bool near(uint16_t v, uint16_t target) {
    return v > target * 6 / 10 && v < target * 14 / 10;
}

bool ir_decode_nec(const ir_signal_t *signal, uint16_t *address, uint8_t *command) {
    if (signal->n < 67 || ! near(signal->us[0], 9000) || ! near(signal->us[1], 4500))
        return false;
    uint32_t bits = 0;
    for (int i = 0; i < 32; ++i) {
        uint16_t mark = signal->us[2 + 2*i], space = signal->us[3 + 2*i];
        if (! near(mark, 562))
            return false;
        if (near(space, 1687))
            bits |= 1u << i;
        else if (! near(space, 562))
            return false;
    }
    uint8_t addr = bits, naddr = bits >> 8, cmd = bits >> 16, ncmd = bits >> 24;
    if ((uint8_t)(cmd ^ ncmd) != 0xFF)
        return false;
    /* Extended NEC uses 16 bits of address (no inverted copy) */
    *address = (uint8_t)(addr ^ naddr) == 0xFF ? addr : (uint16_t)(addr | naddr << 8);
    *command = cmd;
    return true;
}
