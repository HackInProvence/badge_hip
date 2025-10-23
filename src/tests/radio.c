/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

// Include sys/types.h before inttypes.h to work around issue with
// certain versions of GCC and newlib which causes omission of PRIu64
#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include "radio.h"


#define status_nrdy(status) (status >> 7)
#define status_state(status) ((status >> 4) & 0x7)
#define status_fifo_bytes(status) (status & 0xf)

/* Test configuration found on https://github.com/jamisonderek/flipper-zero-tutorials/wiki/Sub-GHz
 * This uses asynch serial mode/operation, and downgrades features (no FIFO, no whitening, no interleave, no FEC, no Manchester, no MSK) */
const uint8_t conf_am270_async[] = {
    0x02, 0x0D, /* GD0 conf: async serial mode */
    //0x01, 0x2E, /* GD1 */
    //0x00, 0x29, /* GD2 */
    /* ADC_RETENTION to be able to filter RX bandwidth < 325kHz on wakeup
     * 0 dB RX attenuation,
     * 33/32 FIFO threshold */
    0x03, 0x47,
    0x08, 0x32, /* PKTCTRL0: no whitening, use asynch serial on GDOx, infinite packet length */
    0x0B, 0x06, /* FSCTRL1: IF frequency (selectivity?), 152kHz */
    0x0D, 0x10, /* FREQ2 */
    0x0E, 0xB0, /* FREQ1 */
    0x0F, 0x71, /* FREQ0 433.92 */
    0x14, 0x00, /* MDMCFG0: channel spacing, TODO kHz */
    0x13, 0x00, /* MDMCFG1: no FEC, no preamble bits */
    0x12, 0x30, /* MDMCFG2: enable DC filter, ASK/OOK, Manchester disabled, no preamble/sync */
    0x11, 0x32, /* MDMCFG3: data rate mantissa */
    0x10, 0x67, /* MDMCFG4: channel bandwidth (271kHz) + data rate exponent 3.8 kHz */
    0x18, 0x18, /* MCSM0: ... + pin radio control option */
    0x19, 0x18, /* FOCCFG: frequency offset compensation */
    0x1D, 0x40, /* AGCCTRL0: small dead zone*/
    0x1C, 0x00, /* AGCCTRL1: carrier sense -8 dB */
    0x1B, 0x03, /* AGCCTRL2: 33 dB target on digital filter channel */
    0x20, 0xFB, /* WORCTRL: WakeOnRadio, event timeout and max timeout */
    0x22, 0x11, /* FREND0: use index PATABLE[1] when ASK encodes a '1' */
    0x21, 0xB6,  /* FREND1: RX current configuration */
    /* This does not make sense, it does not access the PATABLE: "0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00"
     * 0x3E writes PATABLE[0]
     * 0x7E writes PATABLE[0:8] in burst mode */
};


uint8_t print_status(void) {
    uint8_t status;
    gpio_put(BADGE_SPI1_CSn_RADIO, 0);
    spi_write_read_blocking(spi1, "\x3D", &status, 1);  /* NOOP (in write mode, so FIFO is the TX one) */
    gpio_put(BADGE_SPI1_CSn_RADIO, 1);
    printf("status = 0x%02x: %sready, state 0b%03b, %d FIFO bytes avail\n",
            status,
            status_nrdy(status) ? "NOT " : "",
            status_state(status),
            status_fifo_bytes(status)
    );
    return status;
}


int main() {
    stdio_usb_init();

    /* (automatic) boot procedure:
     * - set CSn to low,
     * - wait for SO to go high -> takes 3µs, is this length noise? */
    gpio_init(BADGE_SPI1_RX_MISO_RADIO_SO);
    gpio_init(BADGE_SPI1_CSn_RADIO);

    absolute_time_t t0 = get_absolute_time();
    gpio_put(BADGE_SPI1_CSn_RADIO, 0);
    gpio_set_dir(BADGE_SPI1_CSn_RADIO, GPIO_OUT);
    while(gpio_get(BADGE_SPI1_RX_MISO_RADIO_SO))
        tight_loop_contents();
    absolute_time_t t1 = get_absolute_time();
    printf("\nCC1101 booted in %" PRIu64 "µs\n", absolute_time_diff_us(t0, t1));
    gpio_put(BADGE_SPI1_CSn_RADIO, 1);

    radio_init();
    gpio_init(BADGE_RADIO_GDO0);
    //gpio_set_input_enabled(BADGE_RADIO_GDO0, true);
    gpio_init(BADGE_RADIO_GDO2);
    //gpio_set_input_enabled(BADGE_RADIO_GDO2, true);
    printf("inputs %d, %d\n", BADGE_RADIO_GDO0, BADGE_RADIO_GDO2);

    print_status();

    // Try asynch serial mode: GDO0 becomes our TX line (usually an Output of the radio, but here becomes an input)
    gpio_put(BADGE_SPI1_CSn_RADIO, 0);
    spi_write_blocking(spi1, conf_am270_async, sizeof(conf_am270_async));
    gpio_put(BADGE_SPI1_CSn_RADIO, 1);
    sleep_ms(1);
    gpio_put(BADGE_SPI1_CSn_RADIO, 0);
    //spi_write_blocking(spi1, "\x3E\x50", 2); /* PATABLE: PWR 0db */
    gpio_put(BADGE_SPI1_CSn_RADIO, 1);
    sleep_ms(1);

    print_status();
    printf("configured, read configuration back\n");
    uint8_t cfg[0x30];
    gpio_put(BADGE_SPI1_CSn_RADIO, 0);
    spi_write_blocking(spi1, "\xC0", 1); // Burst + Read register
    spi_read_blocking(spi1, 0x00, (uint8_t *)&cfg, 0x30);
    gpio_put(BADGE_SPI1_CSn_RADIO, 1);
    for(size_t i=0; i<0x30; ++i) {
        printf("%02x ", cfg[i]);
        if ((i+1)%8 == 0) {
            printf("\n");
        }
    }
    print_status();

    gpio_put(BADGE_SPI1_CSn_RADIO, 0);
    spi_write_blocking(spi1, "\xFE", 1); // Burst + Read PATABLE
    spi_read_blocking(spi1, 0x00, (uint8_t *)&cfg, 0x08);
    gpio_put(BADGE_SPI1_CSn_RADIO, 1);
    for(size_t i=0; i<0x08; ++i)
        printf("%02x ", cfg[i]);
    printf("\n");
    print_status();

    // TODO: putting in TX mode fails, but why ? Could we still read something? Did the GDOx configuration succeeded?
    // Emit 5ms pulses 10 times per sec
    sleep_ms(1);
    gpio_put(BADGE_SPI1_CSn_RADIO, 0);
    spi_write_blocking(spi1, "\x35", 1);  /* Put in TX mode */
    gpio_put(BADGE_SPI1_CSn_RADIO, 1);
    sleep_ms(1);
    //uint8_t old_status = print_status();
    //uint8_t status = old_status;
    //while (status == old_status && status_state(status) != 0b010) {
    //    gpio_put(BADGE_SPI1_CSn_RADIO, 0);
    //    spi_write_read_blocking(spi1, "\x3D", &status, 1);
    //    gpio_put(BADGE_SPI1_CSn_RADIO, 1);
    //    if (status != old_status)
    //        print_status();
    //}

    gpio_init(BADGE_RADIO_GDO0);
    gpio_put(BADGE_RADIO_GDO0, 1);
    gpio_set_dir(BADGE_RADIO_GDO0, GPIO_OUT);

    printf("start\n");
    for(uint i=0; i<30; ++i) {
        gpio_put(BADGE_RADIO_GDO0, 0);
        sleep_ms(5);
        gpio_put(BADGE_RADIO_GDO0, 1);
        sleep_ms(100);
        print_status();
    }

    //// Try to read
    //gpio_init(BADGE_RADIO_GDO0);
    //spi_write_blocking(spi1, "\x34", 1);
    //uint8_t old_status = print_status();
    //uint8_t status = old_status;
    //while (status == old_status && status_state(status) != 0b001) {
    //    spi_write_read_blocking(spi1, "\x3D", &status, 1);
    //    if (status != old_status)
    //        print_status();
    //}

    //printf("start\n");
    ////for(int i=0; i<100; ++i) {
    //while(true) {
    //    while(! gpio_get(BADGE_RADIO_GDO0));
    //    while(gpio_get(BADGE_RADIO_GDO0));
    //    //printf("01");
    //}

    printf("wait\n");
    sleep_ms(3000);
    printf("stop\n");
    gpio_put(BADGE_SPI1_CSn_RADIO, 0);
    spi_write_blocking(spi1, "\x36", 1);  /* Return to IDLE */
    spi_write_blocking(spi1, "\x39", 1);  /* Power down mode */
    gpio_put(BADGE_SPI1_CSn_RADIO, 1);
    sleep_us(100);  /* Have to wait ~100µ before we see the chip powered down */
    print_status();
}
