/* FIXME: license, for now defaults to Copyright (C) Miaou 2025 */

#include <stdio.h>
#include <stdlib.h> // rand
#include "pico/stdlib.h"

#include "libcicada.h"
#include "libcicada.pio.h"


int main() {
    PIO pio;
    uint sm;
    uint offset;

    stdio_usb_init();

    // This will find a free pio and state machine for our program and load it for us
    // We use pio_claim_free_sm_and_add_program_for_gpio_range so we can address gpios >= 32 if needed and supported by the hardware
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(
        &libcicada_program,
        &pio, &sm, &offset,
        LIBCICADA_BUZZ_PIN, 1 /* count */,
        true  /* bool set_gpio_base */
    );
    hard_assert(success);

    // Setup the state machine and run it
    libcicada_program_init(pio, sm, offset, LIBCICADA_BUZZ_PIN);
    pio_sm_set_enabled(pio, sm, true);

#define LEN 5
    /*for (int i=0; ; ++i) {
        // Silence is max 256*16 * 5µs = 20ms
        pio_sm_put_blocking(pio, sm, cica_word(cica_note(10, LEN), cica_note(13, LEN)));
        pio_sm_put_blocking(pio, sm, cica_word(cica_note(11, LEN), cica_note(12, LEN)));
        //pio_sm_put_blocking(pio, sm, cica_work(cica_silence(255), cica_silence(255)));
        //pio_sm_put_blocking(pio, sm, cica_word(cica_silence(0), cica_silence(30)));

        if ((i%20) == 0) {
            for (int j=0; j<6; ++j)
                pio_sm_put_blocking(pio, sm, cica_word(cica_silence(255), cica_silence(255)));
        }
    }*/
    for (int i=0;;++i) {
        // With len 5, notes are around 1ms long each, so a word covers around 4ms...
        //  So we need to do this 250 times per second
        // And filling the FIFO (4 words) covers us for 12 more ms so we "only" need to do this @ 63 fps
        pio_sm_put_blocking(pio, sm, cica_word(
            cica_note(9+rand()%6, LEN),
            cica_note(11+rand()%4, LEN),
            cica_note(8+rand()%2, LEN),
            cica_note(10+rand()%6, LEN)
        ));

        if ((i%35) == 0) {
            pio_sm_put_blocking(pio, sm, cica_word(
                cica_silence(15),
                cica_silence(15),
                cica_silence(15),
                cica_silence(15)
            ));
        }
    }
}
