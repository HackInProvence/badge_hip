/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file ir.h
 *
 * \brief Infrared API: record and replay remote control signals, with the modules of the right extension port J2
 * (see pinouts.h):
 * - a 38kHz receiver (VS1838B, TSOP38238...): its output is low while it receives the carrier,
 * - an IR LED (940nm), driven by a transistor (or a resistor), lit when the pin is high.
 *
 * A signal is stored raw: the durations (in µs) of the alternating marks (carrier on) and spaces (carrier off),
 * starting with a mark. It can be replayed whatever the protocol. The NEC protocol (the most common) is also decoded.
 * Recording and sending use interrupts and alarms: nothing blocks.
 * */

#ifndef _IR_H
#define _IR_H

#include <stdbool.h>
#include <stdint.h>

#define IR_MAX_PULSES 256
#define IR_CARRIER_HZ 38000

typedef struct {
    uint16_t n;  /* Number of durations, odd (ends with a mark) */
    uint16_t us[IR_MAX_PULSES];  /* Mark, space, mark, ... */
} ir_signal_t;

void ir_init(void);

/** \brief Start waiting for a signal. */
void ir_record_start(void);

/** \brief Stop recording (the partial signal is lost). */
void ir_record_stop(void);

/** \brief To call in the main loop while recording.
 * \return true when a complete signal was received (it ends after 100ms without edge), copied in \p signal */
bool ir_record_task(ir_signal_t *signal);

bool ir_recording(void);

/** \brief Send a signal (non blocking). \return false when already sending */
bool ir_send(const ir_signal_t *signal);

bool ir_sending(void);

/** \brief Decode a NEC frame. \return true with the address and command */
bool ir_decode_nec(const ir_signal_t *signal, uint16_t *address, uint8_t *command);

#endif /* _IR_H */
