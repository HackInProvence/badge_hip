/* FIXME: license, for now defaults to Copyright (C) Miaou 2025 */

#ifndef _NOISE_GEN_H
#define _NOISE_GEN_H

// TODO: move me to the board definition
#ifndef NOISE_GEN_BUZZ_PIN
#warning "Set NOISE_GEN_BUZZ_PIN with cmake -D, defaults to 28"
#define NOISE_GEN_BUZZ_PIN 28
#endif

#include "noise_gen.pio.h"


/* \brief Auto choose a PIO and play sound effects. */
void noise_gen_init_play(void);

/* \brief Enable/suspend sound effects.
 *
 * You can use the GPIO for other purposes when not \p enabled,
 * and this call will re-assign the GPIO to the PIO when \p enabled.*/
void noise_gen_set_enabled(bool enabled);

/* \brief Configure the sound engine to tell which PIO and sound generation function to use and setups IRQ.
 *
 * This has the side effect of immediately run sound_gen until the TX FIFO of the PIO is full.
 * The \p sound_gen should be fast enough to not stall the other tasks of the RP2040.
 *
 * \param sound_gen Use noise_gen_cicada if in doubt. */
void noise_gen_setup(PIO pio, uint sm, irq_handler_t sound_gen);


/* \brief Interrupt handler that fills the FIFO of the PIO with cicada sounds.
 *
 * Use it as the \ref irq_handler_t of \ref noise_gen_setup.
 * Not to be called directly. */
void noise_gen_cicada(void);

// See the .pio for documentation of the sound engine, and libcicacda_fill_fifo on how to use this
#define CICA_NOTE(note, len)  (uint8_t)(((len & 0xF) << 4) | (note & 0xF))
#define CICA_SILENCE(len) (uint8_t)((len & 0xF) << 4)

#define _shift_u8(x, n) ((x&0xff) << n)
// Plays a then b then c then d
#define CICA_WORD(a, b, c, d) (uint32_t)(_shift_u8(d, 24) | _shift_u8(c, 16) | _shift_u8(b, 8) | _shift_u8(a, 0))


#endif /* _NOISE_GEN_H */
