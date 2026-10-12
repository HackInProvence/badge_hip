/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */


// Include sys/types.h before inttypes.h to work around issue with
// certain versions of GCC and newlib which causes omission of PRIu64
#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>  /* This uses printf because of the print_cc_ function */

#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "pico/binary_info.h"
#include "pico/time.h"

#include "flash_write.h"
#include "log.h"
#include "radio.h"


/* Used for calibration */
static uint64_t rises = 0;
static absolute_time_t t0 = 0;
static uint slice = 0;


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

/** \brief Helper to burst read registers
 *
 * \warning Status bytes (strobes+burst) cannot be accessed in burst mode */
STATIC void ccread_burst(uint8_t reg, uint8_t *response, size_t len) {
    reg = CC1101_BURST(CC1101_READ(reg));
    gpio_put(BADGE_SPI1_CSn_RADIO, 0);
    spi_write_blocking(spi1, &reg, 1);
    spi_read_blocking(spi1, 0x00, response, len);
    gpio_put(BADGE_SPI1_CSn_RADIO, 1);
}

/** \brief Reads a status register: cannot be read in bursts */
STATIC uint8_t ccread_status_reg(uint8_t reg) {
    uint8_t response;
    gpio_put(BADGE_SPI1_CSn_RADIO, 0);
    spi_write_blocking(spi1, &reg, 1);
    spi_read_blocking(spi1, 0x00, &response, 1);
    gpio_put(BADGE_SPI1_CSn_RADIO, 1);
    return response;
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
    gpio_set_dir(BADGE_RADIO_GDO0, GPIO_IN);
    gpio_set_dir(BADGE_RADIO_GDO2, GPIO_IN);
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
    ccread_burst(CC1101_PATABLE, cfg, 8);  /* PATABLE */
    for(size_t i=0; i<8; ++i)
        printf("%02x ", cfg[i]);
    printf("\n");

    log_cc_status();
}


void radio_set_frequency(uint32_t freq_hz) {
    /* setting = freq_hz * 2**16/fXOSC */
    uint32_t fXOSC = flash_write_get_ro_ptr()->radio_crystal_frequency;
    uint64_t setting = freq_hz;
    setting <<= 16;
    setting /= fXOSC;
    setting &= 0x003FFFFF;  /* Can only write the upper 22 bits, which gives 1.664GHz max */

    uint8_t cmd[4] = {
        CC1101_BURST(CC1101_FREQ2),
        (setting >> 16) & 0xFF, /* FREQ2 */
        (setting >> 8) & 0xFF,  /* FREQ1 */
         setting & 0xFF,        /* FREQ0 */
    };
    log_info("radio: closest frequency = %lldHz", (setting*fXOSC)>>16);
    ccsend(cmd, NULL, 4);
}


void radio_set_baud_rate(uint32_t rate_bauds) {
    /* Recover the part of MDMCFG4 that would be overwritten */
    uint8_t pad[3] = {CC1101_READ(CC1101_MDMCFG4)};
    ccread_burst(CC1101_MDMCFG4, pad+1, 1);

    /* Now prepare the new settings (current MDMCFG4 is already inplace in pad[1]) */
    pad[0] = CC1101_BURST(CC1101_MDMCFG4);
    pad[1] &= 0xF0;
    /* Rdata = (256+mantissa)*2^exp * fXOSC/2^28
     * Rdata*2^28/fXOSC = (256+mantissa)*2^exp */
    uint64_t conf = rate_bauds;
    conf <<= 28;
    uint32_t fXOSC = flash_write_get_ro_ptr()->radio_crystal_frequency;
    conf /= fXOSC;
    size_t mag = 63-__builtin_clzll(conf);
    //log_info("conf=%llu, 0x%016llx, mag=%d\n", conf, conf, mag);
    pad[1] |= (mag-8) & 0x0F;
    conf >>= mag-8;
    pad[2] = conf-256;
    //log_info("mantissa=%d, exp=%d\n", mantissa, exp);
    log_info("radio: closest baud rate = %lld bps", ((uint64_t)((256+pad[2])*(1<<(pad[1]&0x0F)))*fXOSC)>>28);
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
    } else if (do_change) {
        log_warning("cannot do_change to state %s, would dead lock, ignoring command", STATE_NAMES[status_state(status)]);
        return;
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


/* The counter callback is not in flash to avoid the XIP cache and reduce its footprint */
void __not_in_flash_func(counter)(void) {
    pwm_clear_irq(slice);
    ++rises;
}

/* Global strategy: use the RP2040 crystal to calibrate the CC1101 crystal.
 * It's hard to conclude with great precision, because one crystal is not supposed to be better than the other.
 * TODO: implement a mini-NTP equivalent to calibrate "both"...
 *  (FREQEST would be less accurate because it is somewhat coarse I don't have a calibrated SDR for reference)
 *
 * Use the PWM edge-sensitivity to count GDO2 rising edges and interrupt on wraps.
 * Target around 10kHz interrupts to be sure to not miss an interrupt because of UART (seems to fail at 100kHz).
 * Accuracy (relative to the RP2040 crystal) is 2600/(length_ms/1000) in Hz = 100000/length_ms in ppm,
 *  and we only need less than 1000 ppm to not skip channels, hence 10 to 100 seconds should be ok.
 *
 * There would be (at least) another way to measure a delay and stop the count by hardware using another slice to count time,
 *  and make it DMA on wrap to stop the counting slice.
 * The time measurement would be more accurate (because here we use get_absolute_time which is a call),
 *  but we would have more work to make it count times longer than 268ms
 *  ((65536*256*2)/125MHz, *256 with clock divider, *2 with phase correct) */
void radio_calibrate(uint32_t length_ms){
    /* Use the PWM edge counter on B port of slice 6 to count the rising edges of the CC1101 clock
     *  (GDO0 is on port A and cannot count edges) */
    if (pwm_gpio_to_channel(BADGE_RADIO_GDO2) != PWM_CHAN_B) {
        log_warning("can't use pin %u, not on PWM channel B", BADGE_RADIO_GDO2);
        return;
    }
    slice = pwm_gpio_to_slice_num(BADGE_RADIO_GDO2);

    /* With PWM slice IRQ, we can count XOSC/1 (PWM can count up to sys_clk/2),
     *  but only one of GDOx should output the clock at a time */
    ccsend((uint8_t[]){CC1101_BURST(CC1101_IOCFG2), 0x30, 0x2E, 0x29}, NULL, 4);

    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv_mode(&cfg, PWM_DIV_B_RISING);

    /* Use a 2.6k wrap so the interrupt is called at ~ 10kHz, which is slow enough to not be glitched by the UART activity */
    pwm_config_set_wrap(&cfg, 2599);
    pwm_config_set_clkdiv_int(&cfg, 1);  /* 0 means 256, so only counts 1 for 256 rising edges -> in fact, we don't need that */
    pwm_init(slice, &cfg, /*start=*/false);

    gpio_set_function(BADGE_RADIO_GDO2, GPIO_FUNC_PWM);
    pwm_set_counter(slice, 0);
    pwm_clear_irq(slice);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, counter);
    irq_set_enabled(PWM_IRQ_WRAP, true);
    pwm_set_irq_enabled(slice, true);

    /* Get time before enabling because we will get time before disabling */
    t0 = get_absolute_time();
    pwm_set_enabled(slice, true);

    /* Wait for length_ms */
    absolute_time_t until = delayed_by_ms(t0, length_ms);
    absolute_time_t log_until = delayed_by_ms(t0, length_ms > 100 ? length_ms-100 : 0);
    absolute_time_t now = t0;
    absolute_time_t last_log = t0;
    while (absolute_time_diff_us(log_until, now) < 0) {
        uint64_t dt_log = absolute_time_diff_us(last_log, now);
        if (dt_log > 250000) {
            last_log = now;
            uint64_t diff_us = absolute_time_diff_us(t0, now);
            uint64_t xosc_l = 2600*rises*1000000/diff_us;  /* lower bound, in Hz */
            uint64_t xosc_u = 2600*(rises+1)*1000000/diff_us;  /* upper bound, in Hz */

            log_printf(LOG_LEVEL_INFO, /*add_newline=*/false, "\x1b[2K\r");
            log_printf(LOG_LEVEL_INFO, /*add_newline=*/false,
                 "rises=% 9" PRIu64 ", %5d.%03d <= xosc <= %5d.%03d kHz (%3d.%03d/%3d.%03d s)", rises,
                 (uint32_t)(xosc_l/1000), (uint32_t)(xosc_l%1000),
                 (uint32_t)(xosc_u/1000), (uint32_t)(xosc_u%1000),
                 (uint32_t)(diff_us/1000000), (uint32_t)((diff_us/1000)%1000),
                 length_ms/1000, length_ms%1000
            );
        }
        now = get_absolute_time();
    }
    while (absolute_time_diff_us(until, now) < 0)
        now = get_absolute_time();
    pwm_set_enabled(slice, false);

    /* TODO: we can have a more reliable measurement having counter recording the time, but this requires:
     * - disabling other interrupts,
     * - getting time in the interrupt. */
    /* "now" is the closest time that is before disabling the PWM */
    uint64_t diff_us = absolute_time_diff_us(t0, now);
    //uint64_t xosc_l = 2600*rises*1000000/diff_us;  /* lower bound, in Hz */
    /* A closer upper bound is reached with pwm_get_counter */
    uint64_t xosc = (2600*rises+pwm_get_counter(slice))*1000000/diff_us;  /* upper bound, in Hz */

    /* Take a point in between and save it */
    //uint64_t xosc = (xosc_l+xosc_u)/2ull;
    Persistence *pers = flash_write_get_rw_copy();
    pers->radio_crystal_frequency = xosc;
    pers->flags_configured |= FLASH_CONFIGURED_CC1101;
    flash_write_commit();

    printf("\n");  /* Leave that like that because we don't have means to prefix a \n in a log_ */
    log_info("CC1101 calibrated to fXOSC = %u Hz", pers->radio_crystal_frequency);
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
    CC1101_MDMCFG1, 0x22, /* 4 preamble bytes by default */
    CC1101_DEVIATN, 0x34, /* Deviation = 19.04kHz for 26.000000MHz */
    CC1101_MCSM0, 0x18, /* Autocalibration on RX or TX, 64 ripples, no pin radio control */
    CC1101_FOCCFG, 0x16, /* FOC: 3K, K/2 after sync word, limited to BW_chan/4 */
    CC1101_AGCCTRL2, 0x43, /* Reduce DVGA gain, maximum LNA gain, target averaged amplitude to 33 dB */
    CC1101_AGCCTRL1, 0x40, /* Relative carrier sense disabled, but absolute carrier sense to 33 dB (??) */
    CC1101_AGCCTRL0, 0x91,
    CC1101_WORCTRL, 0xFB, /* WakeOnRadio: power down RC, 48 cycles for Event 1 (43ms), calibrate RC, maximum Event 0 timeout: 17h */
    /* Note: as MCSM2.RX_TIME is kept to its default value (7), RX will never timeout and WOR should have its auto-sleep disabled */
};
const size_t radio_preset_gfsk_len = sizeof(radio_preset_gfsk);
