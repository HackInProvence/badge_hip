/* FIXME: license, for now defaults to Copyright (C) Miaou 2025 */

#include <stdio.h>
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

    libcicada_program_init(pio, sm, offset, LIBCICADA_BUZZ_PIN);
    printf("offset: %d\n", offset);
    float div = clock_get_hz(clk_sys) / 200000;  /* Not used, just for debug */
    printf("CPU %d Hz, div SM %f\n", clock_get_hz(clk_sys), div);

    // Set the state machine running
    printf("PC: %d\n", pio_sm_get_pc(pio, sm));
    pio_sm_set_enabled(pio, sm, true);

    printf("PC: %d\n", pio_sm_get_pc(pio, sm));
    //pio_sm_put/*_blocking*/(pio, sm, 0x80A0C0FF);  // Placed in the OSR as is represented here
    //pio_sm_put_blocking(pio, sm, 0x80A0C0FF);
    printf("put\n");
    printf("PC: %d\n", pio_sm_get_pc(pio, sm));
    sleep_ms(500);
    printf("PC: %d\n", pio_sm_get_pc(pio, sm));

    //bool pio_sm_is_tx_fifo_full (PIO pio, uint sm);
    //bool pio_sm_is_exec_stalled (PIO pio, uint sm);
    //uint pio_sm_get_tx_fifo_level (PIO pio, uint sm);
    //uint8_t pio_sm_get_pc (PIO pio, uint sm)

    // 8bit blob (space harrier), but at 9600 (debug)
    while(true) {
        //pio_sm_put_blocking(pio, sm, cica_word(cica_note(11, 255), cica_note(20, 255)));
        //pio_sm_put_blocking(pio, sm, 23 | (2000<<16));
        pio_sm_put_blocking(pio, sm, 12 | (13<<16));
        //sleep_ms(100);
        pio_sm_put_blocking(pio, sm, 32 | (6<<16));
    }

    // Pseudo mario ring
    while(false) {
        pio_sm_put_blocking(pio, sm, 59 | (200<<16));
        pio_sm_put_blocking(pio, sm, 39 | (900<<16));
        sleep_ms(1000);
    }

#define LEN 5
    for (int i=0; ; ++i) {
        // Silence is max 255*8 * 5µs = 10ms
        pio_sm_put_blocking(pio, sm, cica_word(cica_note(23, LEN), cica_note(22, LEN)));
        pio_sm_put_blocking(pio, sm, cica_word(cica_note(19, LEN), cica_note(19, LEN)));
        //pio_sm_put_blocking(pio, sm, cica_work(cica_silence(255), cica_silence(255)));
        pio_sm_put_blocking(pio, sm, cica_word(cica_silence(0), cica_silence(30)));

        if ((i%30) == 0) {
            for (int j=0; j<6; ++j)
                pio_sm_put_blocking(pio, sm, cica_word(cica_silence(255), cica_silence(255)));
        }
    }
}
