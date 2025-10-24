/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

// Include sys/types.h before inttypes.h to work around issue with
// certain versions of GCC and newlib which causes omission of PRIu64
#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include "radio.h"


/* TODO: when it is decided whether radio_send functions are static or not, change that (non static -> in .h, static -> find another solution...) */
void radio_send(const uint8_t *data, uint8_t *response, size_t len);
void radio_burst_read(uint8_t reg, uint8_t *response, size_t len);


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
    //0x0D, 0x10, /* FREQ2 */
    //0x0E, 0xB0, /* FREQ1 */
    //0x0F, 0x71, /* FREQ0 433.92 */
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
     * 0x7E writes PATABLE[0:8] in burst mode
     * I checked that my CC1101 *does not* accept setting the PATABLE with 0x00, 0x00 then the PATABLE
     * but it does work with 0x3E and 0x7E... */
};


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
    radio_send("\x3D", &status, 1);  /* NOOP (in write mode, so FIFO is the TX one) */
    _printf_status(status);
    return status;
}

void wait_state(uint8_t tgt) {
    uint8_t old_status = 0, status = 0;
    do {
        radio_send("\x3D", &status, 1);
        if (status != old_status) {
            _printf_status(status);
            old_status = status;
        }
    } while (status_state(status) != tgt);
}


void print_configuration(void) {
    uint8_t cfg[0x30];
    size_t i,j;

    //print_status();
    printf("current configuration:\n");
    radio_burst_read(0x00, cfg, 0x30);

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
    radio_burst_read(0x3E, cfg, 8);  /* PATABLE */
    for(size_t i=0; i<8; ++i)
        printf("%02x ", cfg[i]);
    printf("\n");

    print_status();
}


/** Pulses a TX on 933.92 with OOK (PWM 5% duty on 100ms cycle)
 * Uses the asynch serial mode, which is the usual mode for the Sub-GHz apps on the flipper (RAW read, RAW send) */
void tx_pulses(void) {
    radio_send(conf_am270_async, NULL, sizeof(conf_am270_async));
    //radio_send("\x00\x00\xC0\x00\x00\x00\x00\x00\x00\x00", NULL, 10);  /* Done by flipper but does not work */
    //radio_send("\x3E\x50", NULL 2);  /* PATABLE: PWR 0db (C0 for maximal power, C6 by default, which is less power) */
    radio_set_frequency(433920000);
    print_configuration();

    // Put the CC1101 in TX mode (asynch serial) then emit 5ms pulses 10 times per sec
    radio_send("\x35", NULL, 1);
    wait_state(0b010);  /* FIXME: replace magic numbers by name */

    gpio_init(BADGE_RADIO_GDO0);
    gpio_put(BADGE_RADIO_GDO0, 1);
    gpio_set_dir(BADGE_RADIO_GDO0, GPIO_OUT);

    printf("start\n");
    for(uint i=0; i<30; ++i) {
        gpio_put(BADGE_RADIO_GDO0, 0);
        sleep_ms(5);
        gpio_put(BADGE_RADIO_GDO0, 1);
        sleep_ms(95);
        print_status();
    }
}


/** Put the CC1101 in RX mode and print the first bits received with high enough RSSI.
 * Uses the asynch serial mode, which is the usual mode for the Sub-GHz apps on the flipper (RAW read, RAW send) */
void rx_times(void) {
    radio_send(conf_am270_async, NULL, sizeof(conf_am270_async));
    print_configuration();

    gpio_init(BADGE_RADIO_GDO0);
    radio_send("\x34", NULL, 1);  /* Go to RX mode */
    wait_state(0b001);  /* FIXME: replace magic numbers by name */

    printf("start\n");

    int8_t prev_sig = -1;
    absolute_time_t prev_ts = get_absolute_time();
    uint64_t lengths[64];  /* The first bit of each int is used to store the signal value */
    size_t cur_len = 0;

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

    /* Show a trace */
    for (size_t i=0; i<sizeof(lengths)/sizeof(lengths[0]); ++i) {
        printf("%d for % 7" PRIu64 "\n", (uint8_t)(lengths[i] >> 63), lengths[i] & 0x7fFFffFF);
    }
}


/** \brief Default configuration for the flipper chat app.
 *
 * There is no specific configuration found in https://github.com/twisted-pear/esubghz_chat/blob/main/esubghz_chat.c,
 * but the enter_chat function calls subghz_tx_rx_worker, which sets up a GFSK by default,
 * see subghz_device_cc1101_preset_gfsk_9_99kb_async_regs in https://github.com/flipperdevices/flipperzero-firmware/blob/dev/lib/subghz/devices/cc1101_configs.c
 *
 * This uses the packet mode of the CC1101
 *
 * 999 = 9.99kbps */
const uint8_t conf_gfsk999[] = {
    CC1101_IOCFG0, 0x06, /* GDO0 = packet being received */
    CC1101_FIFOTHR, 0x47, /* ADC retention, no RX attenuation, 33/32 TX/RX FIFO thresholds */
    CC1101_SYNC1, 0x46, /* Sync word MSB */
    CC1101_SYNC0, 0x4C, /* Sync work LSB */
    //CC1101_PKTLEN, 0x00, /* The doc says that the value must be different from 0... */
    CC1101_PKTCTRL0, 0x05, /* no whitening, use FIFOs, with CRC, variable packet length (first byte after sync word) */
    CC1101_ADDR, 0x00, /* no packet filtration */
    CC1101_FSCTRL1, 0x06, /* IF frequency */
    CC1101_MDMCFG4, 0xC8, /* Channel bandwidth: 203kHz */
    CC1101_MDMCFG3, 0x93, /* Data rate: 9.992kbps */
    CC1101_MDMCFG2, 0x12, /* Modulation: GSK, no manchester, 16/16 sync word bits */
    CC1101_DEVIATN, 0x34, /* Deviation = 19.04kHz */
    CC1101_MCSM0, 0x18, /* Autocalibration on RX or TX, 64 ripples, no pin radio control */
    CC1101_FOCCFG, 0x16, /* FOC: 3K, K/2 after sync word, limited to BW_chan/4 */
    CC1101_AGCCTRL2, 0x43,
    CC1101_AGCCTRL1, 0x40, /* Relative carrier sense disabled, but absolute carrier sense */
    CC1101_AGCCTRL0, 0x91,
    CC1101_WORCTRL, 0xFB, /* WakeOnRadio: power down RC, 48 cycles for Event 1 (43ms), calibrate RC, maximum Event 0 timeout: 17h */
    /* Note: as MCSM2.RX_TIME is kept to its default value (7), RX will never timeout and WOR should have its auto-sleep disabled */
};


/** \brief msg must be \0 terminated */
void tx_chat_flipper(const uint8_t *msg) {
    /* 800µs per byte */
    radio_send(conf_gfsk999, NULL, sizeof(conf_gfsk999));
    radio_set_frequency(433920000);
    print_configuration();

    /* we send data in 63 bytes blocks to simplify the transmission (no interrupt, use GD0 to follow the current packet status) */
    size_t len = strlen(msg);
    size_t block_len;
    uint8_t tx[66];  /* Group the flush + burst write FIFO in a single SPI write */
    while (len) {
        /* Prepare the block */
        block_len = len > 63 ? 63 : len;  /* 64-1 for the length */
        memcpy(tx+3, msg, block_len);
        printf("send block (len %d)\n", block_len);
        print_status();

        tx[0] = CC1101_SFTX;  /* Flush the TX FIFO to be sure that OUR message is sent */
        tx[1] = CC1101_BURST(CC1101_TXFIFO);
        tx[2] = block_len;  /* We are in variable length: the first byte in the FIFO must be the length */
        radio_send(tx, NULL, block_len+3);

        print_status();
        tx[0] = CC1101_STX;
        radio_send(tx, NULL, 1);
        print_status();

        /* Wait for GD0 to go high (preamble+sync has been sent) */
        while(! gpio_get(BADGE_RADIO_GDO0))  /* FIXME: timeout */
            tight_loop_contents();
        print_status();

        /* Wait for GD0 to go low (packet has been sent) */
        while(gpio_get(BADGE_RADIO_GDO0))
            tight_loop_contents();
        print_status();

        len -= block_len;
    }
}


int main() {
    stdio_usb_init();

    printf("init\n");
    radio_init();
    printf("boot\n");
    radio_boot();
    gpio_init(BADGE_RADIO_GDO0);
    gpio_init(BADGE_RADIO_GDO2);

    print_status();

    //tx_pulses();
    //rx_times();
    tx_chat_flipper("Badge SecSea joined chat.\n");
    sleep_ms(3000);
    tx_chat_flipper("Badge SecSea: Hey, how are you?\n");
    sleep_ms(2000);
    tx_chat_flipper("Badge SecSea: Ouais ?\n");
    sleep_ms(2000);
    tx_chat_flipper("Badge SecSea: pas tres locace dis donc...\n");
    sleep_ms(2000);
    tx_chat_flipper("Badge SecSea: ...\n");
    sleep_ms(2000);
    tx_chat_flipper("Badge SecSea: Never\n");
    sleep_ms(300);
    tx_chat_flipper("Badge SecSea: gonna\n");
    sleep_ms(300);
    tx_chat_flipper("Badge SecSea: let\n");
    sleep_ms(500);
    tx_chat_flipper("Badge SecSea: you\n");
    sleep_ms(500);
    tx_chat_flipper("Badge SecSea: doooown!\n");
    sleep_ms(3000);
    tx_chat_flipper("Badge SecSea left chat.\n");

    /* Shutdown */
    printf("wait\n");
    sleep_ms(2000);  /* When out of TX mode, it still emits around the frequency here, but it's okay we are not in IDLE */

    printf("stop\n");
    radio_send("\x36\x39", NULL, 2); /* Return to IDLE, then power down */
    sleep_us(100);  /* Have to wait ~100µ before we see the chip powers down */
    print_status();
    /* Bringing CSn to 0 again will wake up the chip */
    sleep_ms(1000);
    printf("reboot\n");
    radio_boot();
    print_status();
    sleep_ms(1000);
    printf("restop\n");
    uint8_t cmd = CC1101_SPWD;
    radio_send(&cmd, NULL, 1);
}
