/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */


#include <stdio.h>  /* This uses printf because of the print_cc_ function */

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/binary_info.h"

#include "log.h"
#include "radio.h"


/** \brief SPI read/write pulling CSn down for the whole transaction, \p response can be NULL
 *
 * We chose to block until the \p len bytes are written, as the communication is fast (~1MHz) */
STATIC void ccsend(const uint8_t *data, uint8_t *response, size_t len) {
    gpio_put(BADGE_SPI1_CSn_RADIO, 0);
    if (response)
        spi_write_read_blocking(spi1, data, response, len);
    else
        spi_write_blocking(spi1, data, len);
    gpio_put(BADGE_SPI1_CSn_RADIO, 1);
}

/** \brief Helper to burst read registers */
STATIC void ccread_burst(uint8_t reg, uint8_t *response, size_t len) {
    uint8_t cmd = CC1101_BURST(CC1101_READ(reg));
    gpio_put(BADGE_SPI1_CSn_RADIO, 0);
    spi_write_blocking(spi1, &cmd, 1);
    spi_read_blocking(spi1, 0x00, response, len);
    gpio_put(BADGE_SPI1_CSn_RADIO, 1);
}


void radio_init(void) {
    // Declare our GPIO usages
    bi_decl_if_func_used(bi_4pins_with_func(BADGE_SPI1_TX_MOSI_RADIO_SI, BADGE_SPI1_RX_MISO_RADIO_SO, BADGE_SPI1_SCK_RADIO, BADGE_SPI1_CSn_RADIO, GPIO_FUNC_SPI));
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_RADIO_GDO0, "CC1101 GDO0"));
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_RADIO_GDO2, "CC1101 GDO2"));

    // Init SPI
    spi_init(spi1, 1000*1000);  /* Should go up to 10MHz */
    gpio_set_function(BADGE_SPI1_TX_MOSI_RADIO_SI, GPIO_FUNC_SPI);
    gpio_set_function(BADGE_SPI1_RX_MISO_RADIO_SO, GPIO_FUNC_SPI);
    gpio_set_function(BADGE_SPI1_SCK_RADIO, GPIO_FUNC_SPI);
    /* CSn with SPI module does not span multiple bytes, so we have to control it manually */
    gpio_set_function(BADGE_SPI1_CSn_RADIO, GPIO_FUNC_SIO);
    spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    // Init other pins
    gpio_init(BADGE_SPI1_CSn_RADIO);
    gpio_put(BADGE_SPI1_CSn_RADIO, 1);
    gpio_set_dir(BADGE_SPI1_CSn_RADIO, GPIO_OUT);
    gpio_init(BADGE_RADIO_GDO0);
    gpio_init(BADGE_RADIO_GDO2);
}


void radio_boot(void) {
    /* (automatic) boot procedure:
     * - set CSn to low,
     * - wait for SO to go high -> takes 3µs, is this length measurable ? */
    gpio_put(BADGE_SPI1_CSn_RADIO, 0);
    while(gpio_get(BADGE_SPI1_RX_MISO_RADIO_SO))  /* Works fine, even though the pin has the SPI function */
        tight_loop_contents();
    gpio_put(BADGE_SPI1_CSn_RADIO, 1);
}


const char *STATE_NAMES[] = {
    "IDLE",
    "RX",
    "TX",
    "FSTXON",
    "CALIBRATE",
    "SETTLING",
    "RXFIFO_OVERFLOW",
    "TXFIFO_UNDERFLOW",
};

#define status_nrdy(status) (status >> 7)
#define status_state(status) ((status >> 4) & 0x7)
#define status_fifo_bytes(status) (status & 0xf)

static void _log_status(uint8_t status) {
    log_info(
        "status = 0x%02x: %sready, state 0b%03b (%s), %d TX FIFO bytes avail",
        status,
        status_nrdy(status) ? "NOT " : "",
        status_state(status), STATE_NAMES[status_state(status)],
        status_fifo_bytes(status)
    );
}

STATIC void log_cc_status(void) {
    radio_state_t status = radio_state();
    _log_status(status);
}

void print_cc_configuration(void) {
    uint8_t cfg[0x30];
    size_t i,j;

    printf("current configuration:\n");
    ccread_burst(0x00, cfg, 0x30);

    /* Print table header */
    printf("    ");
    for (i=0; i<16; ++i)
        printf("% 2x ", i);
    printf("\n");

    /* Print memory content with first column for current line */
    for (j=0; j<3; ++j) {
        printf("%02x: ", j*16);
        for(size_t i=0; i<16; ++i)
            printf("%02x ", cfg[j*16+i]);
        printf("\n");
    }

    printf("PATABLE:\n    ");
    ccread_burst(0x3E, cfg, 8);  /* PATABLE */
    for(size_t i=0; i<8; ++i)
        printf("%02x ", cfg[i]);
    printf("\n");

    log_cc_status();
}


void radio_set_frequency(uint32_t freq_hz) {
    /* setting = freq_hz * 2**16/fXOSC */
    uint64_t setting = freq_hz;
    setting <<= 16;
    setting /= CC1101_fXOSC;
    setting &= 0x003FFFFF;  /* Can only write the upper 22 bits, which gives 1.664GHz max */

    uint8_t cmd[4] = {
        CC1101_BURST(CC1101_FREQ2),
        (setting >> 16) & 0xFF, /* FREQ2 */
        (setting >> 8) & 0xFF,  /* FREQ1 */
         setting & 0xFF,        /* FREQ0 */
    };
    ccsend(cmd, NULL, 4);
}


void radio_set_baud_rate(uint32_t rate_bauds) {
    /* Recover the part of MDMCFG4 that would be overwritten */
    uint8_t pad[3] = {CC1101_READ(CC1101_MDMCFG4)};
    ccread_burst(CC1101_READ(CC1101_MDMCFG4), pad+1, 1);

    /* Now prepare the new settings (current MDMCFG4 is already inplace in pad[1]) */
    pad[0] = CC1101_BURST(CC1101_MDMCFG4);
    pad[1] &= 0xF0;
    /* Rdata = (256+mantissa)*2^exp * fXOSC/2^28
     * Rdata*2^28/fXOSC = (256+mantissa)*2^exp */
    uint64_t conf = rate_bauds;
    conf <<= 28;
    conf /= CC1101_fXOSC;
    size_t mag = 63-__builtin_clzll(conf);
    //log_info("conf=%llu, 0x%016llx, mag=%d\n", conf, conf, mag);
    pad[1] |= (mag-8) & 0x0F;
    conf >>= mag-8;
    pad[2] = conf-256;
    //log_info("mantissa=%d, exp=%d\n", mantissa, exp);
    log_info("radio: closest baud rate = %lld bps", ((uint64_t)((256+pad[2])*(1<<(pad[1]&0x0F)))*CC1101_fXOSC)>>28);
    ccsend(pad, NULL, 3);
}


radio_state_t radio_state(void) {
    uint8_t status;
    uint8_t cmd = CC1101_SNOP;
    ccsend(&cmd, &status, 1);
    return status;
}


void radio_wait_state(radio_state_t target_state, bool do_change) {
    uint8_t old_status = 0, status = 0;
    uint8_t cmd;

    /* Some states can't be reached "on demand" */
    if (do_change && target_state <= CC1101_STATE_FSTXON) {
        switch(target_state) {
        case CC1101_STATE_IDLE:
            cmd = CC1101_SIDLE;
            break;
        case CC1101_STATE_RX:
            cmd = CC1101_SRX;
            break;
        case CC1101_STATE_TX:
            cmd = CC1101_STX;
            break;
        case CC1101_STATE_FSTXON:
            cmd = CC1101_SFSTXON;
            break;
        }
        ccsend(&cmd, NULL, 1);
    }

    /* Now wait...
     * FIXME: add a timeout
     * TODO: measure the times it takes to calibrate and settle
     * FIXME: some state are not waitable (SFRX) */
    cmd = CC1101_SNOP;
    do {
        ccsend(&cmd, &status, 1);
        if (status != old_status) {
            _log_status(status);
            old_status = status;
        }
    } while (status_state(status) != target_state);
}


const uint8_t radio_preset_gfsk[] = {
    CC1101_IOCFG0, 0x06, /* GDO0 = packet being transmitted or received */
    CC1101_FIFOTHR, 0x47, /* ADC retention, no RX attenuation, 33/32 TX/RX FIFO thresholds */
    CC1101_SYNC1, 0x46, /* Sync word MSB */
    CC1101_SYNC0, 0x4C, /* Sync work LSB */
    //CC1101_PKTLEN, 0x00, /* The doc says that the value must be different from 0... */
    /*CC1101_PKTCTRL1 -> default value **includes** the 2 status bytes... so the max received FIFO size is 62 */
    CC1101_PKTCTRL0, 0x05, /* no whitening, use FIFOs, with CRC, variable packet length (first byte after sync word) */
    CC1101_ADDR, 0x00, /* no packet filtration */
    CC1101_FSCTRL1, 0x06, /* IF frequency */
    CC1101_MDMCFG4, 0xC0, /* Channel bandwidth: 203kHz */
    //CC1101_MDMCFG4, 0xC8, /* Channel bandwidth: 203kHz */
    //CC1101_MDMCFG3, 0x93, /* Data rate: 9.992kbps */
    CC1101_MDMCFG2, 0x12, /* Modulation: GSK, no manchester, 16/16 sync word bits */
    CC1101_DEVIATN, 0x34, /* Deviation = 19.04kHz */
    CC1101_MCSM0, 0x18, /* Autocalibration on RX or TX, 64 ripples, no pin radio control */
    CC1101_FOCCFG, 0x16, /* FOC: 3K, K/2 after sync word, limited to BW_chan/4 */
    CC1101_AGCCTRL2, 0x43, /* Reduce DVGA gain, maximum LNA gain, target averaged amplitude to 33 dB */
    CC1101_AGCCTRL1, 0x40, /* Relative carrier sense disabled, but absolute carrier sense to 33 dB (??) */
    CC1101_AGCCTRL0, 0x91,
    CC1101_WORCTRL, 0xFB, /* WakeOnRadio: power down RC, 48 cycles for Event 1 (43ms), calibrate RC, maximum Event 0 timeout: 17h */
    /* Note: as MCSM2.RX_TIME is kept to its default value (7), RX will never timeout and WOR should have its auto-sleep disabled */
};
const size_t radio_preset_gfsk_len = sizeof(radio_preset_gfsk);
