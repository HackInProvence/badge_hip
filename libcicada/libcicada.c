/* FIXME: license, for now defaults to Copyright (C) Miaou 2025 */

#include <stdio.h>

#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "pico/rand.h"

#include "libcicada.h"


// Configured state machine which runs our program
static PIO pio = NULL;
static uint sm = -1;
// Sound effect state
static uint next_silence = 0;
static uint64_t cur_rand = 0;
static uint rem_rand_bits = 0;


// Refills the program's FIFO when needed
// Called by interrupts
//typedef void(* irq_handler_t) (void)
void libcicada_fill_fifo() {
    // We can't be blocking here, but we should have time to do a get_rand (<20µs according to doc)

    // while or if is the same here, as exiting this without filling the FIFO will call this method again...
    while(! pio_sm_is_tx_fifo_full(pio, sm)) {
        // First check if we have enough rand bits (this is only to speed up the IRQ when possible)
        if (rem_rand_bits < 12)
        {
            cur_rand = get_rand_64();  // _32 calls _64 but only keep the low 32
            rem_rand_bits = 64;
        }
        // Mostly, noise
        if (next_silence) {
            // With len 5, notes are around 1ms long each, so a word covers around 4ms...
            //  So we need to do this 250 times per second
            // And filling the FIFO (4 words) covers us for 12 more ms so we "only" need to do this @ 63 fps
            pio_sm_put(pio, sm, CICA_WORD(
                CICA_NOTE( 8+((cur_rand>>0)%8), 4),
                CICA_NOTE(11+((cur_rand>>3)%4), 4), /* always a lower pitch */
                CICA_NOTE( 8+((cur_rand>>6)%3), 4), /* always a higher pitch */
                CICA_NOTE(10+((cur_rand>>9)%6), 4)
            ));

            cur_rand >>= 12;
            rem_rand_bits -= 12;
            --next_silence;
        }
        // Other times, silence
        else {
            pio_sm_put(pio, sm, CICA_WORD(
                CICA_SILENCE(15),
                CICA_SILENCE(15),
                CICA_SILENCE(15),
                CICA_SILENCE(0)
            ));

            next_silence = 40;
        }
    }
}


void libcicada_setup(PIO pio, uint sm, irq_handler_t sound_gen) {
    int8_t irq_pio;
    pio = pio;
    sm = sm;

    // Interrupts / IRQs
    // We use the "interrupt when not full" to be sure to feed the PIO with enough data
    //  and avoid having to regularly check whether it is full or not.
    // There are 2 sides for the interrupt:
    // - the rp2040 cores have 26 configurable interrupt, which are shared between the cores (see doc),
    //   and four are dedicated to the PIOs: PIOx_IRQ_y (x is 0 or 1, y is 0 or 1),
    // - the PIO is able to fire 3 types of IRQs:
    //   1. with the "irq" instruction in the PIO program,
    //   2. when the TX FIFO is not full (the cores can send more data),
    //   3. when the RX FIFO is not empty (the cores should read data).
    // We have to choose which IRQ each PIO is able to fire and link it to the cores IRQs.
    // This is done by choosing the correct IRQx_INTE register.

    // Setup the CPU side first, to be ready to be interrupted as soon as the PIO IRQ is fired
    // Use PIOx_IRQ_0 for our "not full TX FIFO"
    irq_pio = pio_get_irq_num(pio, 0);  // Returns the correct PIOx_IRQ_0
    // Assert that it is not already used, which would indicate that another program is running and that was not anticipated...
    if (irq_has_handler(irq_pio))
        panic("PIOx_IRQ_0 already used");
    irq_set_exclusive_handler(irq_pio, sound_gen);
    irq_set_enabled(irq_pio, true);

    // Setup the PIO side: pioX_hw->inte0 = SMz_TXNFULL
    // This has the side effect of immediately run sound_gen()
    pio_set_irqn_source_enabled(
        pio, 0 /* PIOx_IRQ0 */,
        pio_get_tx_fifo_not_full_interrupt_source(sm),
        true /* enabled */
    );
}


void libcicada_init_play(void)
{
    uint offset;

    // This will find a free pio and state machine for our program and load it for us
    // We use pio_claim_free_sm_and_add_program_for_gpio_range so we can address gpios >= 32 if needed and supported by the hardware
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(
        &libcicada_program,
        &pio, &sm, &offset,
        LIBCICADA_BUZZ_PIN, 1 /* count */,
        true  /* bool set_gpio_base */
    );
    hard_assert(success);

    // Run the random number generator once to initialize it (slower than latter calls)
    (void) get_rand_64();

    // Setup the state machine now because we need pio and sm for interrupts
    libcicada_program_init(pio, sm, offset, LIBCICADA_BUZZ_PIN);

    libcicada_setup(pio, sm, libcicada_fill_fifo);
    libcicada_set_enabled(true);
}


void libcicada_set_enabled(bool enabled) {
    if (enabled)
        pio_gpio_init(pio, LIBCICADA_BUZZ_PIN);  /* In case the GPIO was set to PWM for other uses */
    pio_sm_set_enabled(pio, sm, enabled);
}
