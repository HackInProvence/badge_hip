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
    0x00, 0x0E, /* GD2: carrier sense */
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
    //0x1C, 0x00, /* AGCCTRL1: carrier sense absolute at MAGN_TARGET */
    0x1C, 0x38, /* AGCCTRL1: carrier sense relative 6db, absolute disabled */
    0x1B, 0x03, /* AGCCTRL2: 33 dB target on digital filter channel */
    0x20, 0xFB, /* WORCTRL: WakeOnRadio, event timeout and max timeout */
    0x22, 0x11, /* FREND0: use index PATABLE[1] when ASK encodes a '1' */
    0x21, 0xB6,  /* FREND1: RX current configuration */
    /* This does not make sense, it does not access the PATABLE: "0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00"
     * 0x3E writes PATABLE[0]
     * 0x7E writes PATABLE[0:8] in burst mode */
};


/** \brief SPI read/write pulling CSn down for the whole transaction, \p response can be NULL
 *
 * We chose to block until the \p len bytes are written, as the communication is fast (~1MHz) */
void send(const uint8_t *data, uint8_t *response, size_t len) {
    gpio_put(BADGE_SPI1_CSn_RADIO, 0);
    if (response)
        spi_write_read_blocking(spi1, data, response, len);
    else
        spi_write_blocking(spi1, data, len);
    gpio_put(BADGE_SPI1_CSn_RADIO, 1);
}

#define READ(reg) ((reg) | 0x80)
#define BURST(reg) ((reg) | 0x40)


/** \brief Helper to burst read registers */
void burst_read(uint8_t reg, uint8_t *response, size_t len) {
    uint8_t cmd = BURST(READ(reg));
    gpio_put(BADGE_SPI1_CSn_RADIO, 0);
    spi_write_blocking(spi1, &cmd, 1);
    spi_read_blocking(spi1, 0x00, response, len);
    gpio_put(BADGE_SPI1_CSn_RADIO, 1);
}


const char *states[] = {
    "IDLE",
    "RX",
    "TX",
    "FSTXON",
    "CALIBRATE",
    "SETTLING",
    "RXFIFO_OVERFLOW",
    "TXFIFO_UNDERFLOW",
};

static size_t _printf_status(uint8_t status) {
    return printf(
        "status = 0x%02x: %sready, state 0b%03b (%s), %d TX FIFO bytes avail\n",
        status,
        status_nrdy(status) ? "NOT " : "",
        status_state(status), states[status_state(status)],
        status_fifo_bytes(status)
    );
}

uint8_t print_status(void) {
    uint8_t status;
    send("\x3D", &status, 1);  /* NOOP (in write mode, so FIFO is the TX one) */
    _printf_status(status);
    return status;
}

void wait_state(uint8_t tgt) {
    uint8_t old_status = 0, status = 0;
    do {
        send("\x3D", &status, 1);
        if (status != old_status) {
            _printf_status(status);
            old_status = status;
        }
    } while (status_state(status) != tgt);
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
    gpio_init(BADGE_RADIO_GDO2);

    print_status();

    // Try asynch serial mode: GDO0 becomes our TX line (usually an Output of the radio, but here becomes an input)
    send(conf_am270_async, NULL, sizeof(conf_am270_async));
    //send("\x3E\x50", 2); /* PATABLE: PWR 0db */

    printf("configured, read configuration back\n");
    print_status();
    uint8_t cfg[0x30];
    burst_read(0x00, cfg, 0x30);
    for(size_t i=0; i<0x30; ++i) {
        printf("%02x ", cfg[i]);
        if ((i+1)%8 == 0) {
            printf("\n");
        }
    }
    burst_read(0x3E, cfg, 8);  /* PATABLE */
    for(size_t i=0; i<8; ++i)
        printf("%02x ", cfg[i]);
    printf("\n");
    print_status();

    //// Put the CC1101 in TX mode (asynch serial) then emit 5ms pulses 10 times per sec
    //send("\x35", NULL, 1);
    //wait_state(0b010);  /* FIXME: replace magic numbers by name */

    //gpio_init(BADGE_RADIO_GDO0);
    //gpio_put(BADGE_RADIO_GDO0, 1);
    //gpio_set_dir(BADGE_RADIO_GDO0, GPIO_OUT);

    //printf("start\n");
    //for(uint i=0; i<30; ++i) {
    //    gpio_put(BADGE_RADIO_GDO0, 0);
    //    sleep_ms(5);
    //    gpio_put(BADGE_RADIO_GDO0, 1);
    //    sleep_ms(95);
    //    print_status();
    //}

    // Try to read
    gpio_init(BADGE_RADIO_GDO0);
    send("\x34", NULL, 1);
    wait_state(0b001);  /* FIXME: replace magic numbers by name */

    printf("start\n");
    int8_t prev_sig = -1;
    absolute_time_t prev_ts = get_absolute_time();
    uint64_t lengths[64];
    size_t cur_len = 0;
    //for(int i=0; i<5; ++i) {
    while(cur_len < sizeof(lengths)/sizeof(lengths[0])) {
        /* Wait for carrier sense */
        bool cs = gpio_get(BADGE_RADIO_GDO2);
        if (!cs) {
            prev_sig = -1;
            continue;
        }

        /* We have signal */
        bool sig = gpio_get(BADGE_RADIO_GDO0);
        if(prev_sig != sig) {
            absolute_time_t now = get_absolute_time();
            if (prev_sig != -1) {
                lengths[cur_len] = absolute_time_diff_us(prev_ts, now) | ((uint64_t)(prev_sig) << 63);
                ++cur_len;
            }
            prev_sig = sig;
            prev_ts = now;
        }
    }
    // Show a trace
    for (size_t i=0; i<sizeof(lengths)/sizeof(lengths[0]); ++i) {
        printf("%d for % 7" PRIu64 "\n", (uint8_t)(lengths[i] >> 63), lengths[i] & 0x7fFFffFF);
    }

    printf("wait\n");
    sleep_ms(3000);
    printf("stop\n");
    send("\x36\x39", NULL, 2); /* Return to IDLE, then power down */
    sleep_us(100);  /* Have to wait ~100µ before we see the chip powers down */
    print_status();
}
