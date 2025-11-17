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

#include "log.h"
#include "radio.h"


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
    //0x11, 0x32, /* MDMCFG3: data rate mantissa -> use radio_set_baud_rate */
    //0x10, 0x67, /* MDMCFG4: channel bandwidth (271kHz) + data rate exponent 3793 Hz */
    0x10, 0x60, /* MDMCFG4: channel bandwidth (271kHz) */
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


/** Pulses a TX on 933.92 with OOK (PWM 5% duty on 100ms cycle)
 * Uses the asynch serial mode, which is the usual mode for the Sub-GHz apps on the flipper (RAW read, RAW send) */
void tx_pulses(void) {
    ccsend(conf_am270_async, NULL, sizeof(conf_am270_async));
    //ccsend("\x00\x00\xC0\x00\x00\x00\x00\x00\x00\x00", NULL, 10);  /* Done by flipper but does not work */
    //ccsend("\x3E\x50", NULL 2);  /* PATABLE: PWR 0db (C0 for maximal power, C6 by default, which is less power) */
    radio_set_frequency(433920000);
    radio_set_baud_rate(3795);
    print_cc_configuration();

    // Put the CC1101 in TX mode (asynch serial) then emit 5ms pulses 10 times per sec
    radio_wait_state(CC1101_STATE_TX, true);

    gpio_init(BADGE_RADIO_GDO0);
    gpio_put(BADGE_RADIO_GDO0, 1);
    gpio_set_dir(BADGE_RADIO_GDO0, GPIO_OUT);

    printf("start\n");
    for(uint i=0; i<30; ++i) {
        gpio_put(BADGE_RADIO_GDO0, 0);
        sleep_ms(5);
        gpio_put(BADGE_RADIO_GDO0, 1);
        sleep_ms(95);
        log_cc_status();
    }

    radio_wait_state(CC1101_STATE_IDLE, true);
}


/** Put the CC1101 in RX mode and print the first bits received with high enough RSSI.
 * Uses the asynch serial mode, which is the usual mode for the Sub-GHz apps on the flipper (RAW read, RAW send) */
void rx_times(void) {
    ccsend(conf_am270_async, NULL, sizeof(conf_am270_async));
    radio_set_frequency(433920000);
    radio_set_baud_rate(3795);
    print_cc_configuration();

    gpio_init(BADGE_RADIO_GDO0);
    radio_wait_state(CC1101_STATE_RX, true);

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


/** \brief msg must be \0 terminated */
void tx_chat_flipper(const uint8_t *msg) {
    /* Maybe someone else, like rx_pulses, did not reset the direction of this pin... */
    gpio_set_dir(BADGE_RADIO_GDO0, GPIO_IN);

    /* 800µs per byte (without preamble) */
    ccsend(radio_preset_gfsk, NULL, radio_preset_gfsk_len);
    radio_set_frequency(433920000);
    radio_set_baud_rate(9995);  /* Closest is 9991 or 9992 */
    print_cc_configuration();

    /* We send data in 63 bytes blocks to simplify the transmission (no interrupt, use GD0 to follow the current packet status) */
    size_t len = strlen(msg);
    size_t block_len;
    uint8_t tx[66];  /* Group the flush + burst write FIFO in a single SPI write */
    while (len) {
        /* Prepare the block */
        block_len = len > 63 ? 63 : len;  /* 64-1 for the length */
        memcpy(tx+3, msg, block_len);
        printf("send block (len %d)\n", block_len);
        log_cc_status();

        tx[0] = CC1101_SFTX;  /* Flush the TX FIFO to be sure that OUR message is sent */
        tx[1] = CC1101_BURST(CC1101_TXFIFO);
        tx[2] = block_len;  /* We are in variable length: the first byte in the FIFO must be the length */
        ccsend(tx, NULL, block_len+3);
        log_cc_status();

        radio_wait_state(CC1101_STATE_TX, true);

        /* Wait for GD0 to go high (preamble+sync has been sent) */
        while(! gpio_get(BADGE_RADIO_GDO0))  /* FIXME: timeout */
            tight_loop_contents();

        /* Wait for GD0 to go low (packet has been sent) */
        while(gpio_get(BADGE_RADIO_GDO0))
            tight_loop_contents();
        log_cc_status();

        len -= block_len;
    }
}


/* Receive FSK-transmitted data (preset radio_preset_gfsk used by radio_source) */
void rx_fsk_printf(void) {
    gpio_set_dir(BADGE_RADIO_GDO0, GPIO_IN);
    ccsend(radio_preset_gfsk, NULL, radio_preset_gfsk_len);
    radio_set_frequency(433920000);
    radio_set_baud_rate(9995);
    print_cc_configuration();

    uint8_t recv[64];  /* FIFO = len+payload+RSSI+LQI with this config */
    printf("wait for RX...\n");
    while (true) {
        if (radio_state() != CC1101_STATE_RX)
            radio_wait_state(CC1101_STATE_RX, true);

        /* Wait for GD0 to go high (preamble+sync has been received)
         * -> implements a timeout but it should be reliable:
         * - for now, don't use MCSM2.RX_TIME, the timeout is the previous loop reaching 0, so the state should always stay to RX
         * - it is recommended to have an interrupt approach and configure GDO0 to 0x06 and wait for the pin to go high */
        //uint64_t timeout = 10000000;
        while(! gpio_get(BADGE_RADIO_GDO0) /*&& --timeout*/) /* FIXME: we still have to check, some times, that we didn't overflow or something... */
            tight_loop_contents();

        printf("GDO0 high ");

        /* Wait for GD0 to go low (packet has been sent) */
        while(gpio_get(BADGE_RADIO_GDO0))
            tight_loop_contents();

        printf("low ");

        /* Maybe we timed out instead of receiving something -> ... */
        //if (timeout == 0) {
        //    printf("timed out\n");
        //    continue;
        //}

        /* We received something... */
        uint8_t n;
        ccread_burst(CC1101_RXBYTES, &n, 1);
        if (n>>7) {
            printf("RX overflow\n");
            recv[0] = CC1101_SFRX;
            ccsend(recv, NULL, 1);
            continue;
        }
        printf("received %02d bytes ", n-2);
        ccread_burst(CC1101_RXFIFO, recv, n);
        int16_t rssi = (int8_t)recv[n-2];
        rssi -= 74;  /* According to CC1101 datasheet */
        uint8_t lqi = recv[n-1];
        bool crc_ok = lqi >> 7;
        lqi = lqi & 0x7f;
        int8_t eoff;
        ccread_burst(CC1101_FREQEST, &eoff, 1);
        printf("with RSSI=%+04d dBm, LQI=%03d, CRC=%d, est. freq. %+ 7lli Hz: ", rssi, lqi, crc_ok, ((int64_t)(eoff)*CC1101_fXOSC)>>14);
        for (size_t i=0; i<n-2; ++i)
            printf("%02x ", recv[i]);
        printf("\n");
    }
}


const uint8_t fsk_full_rx[] = {
    CC1101_IOCFG0, 0x01, /* GDO0 = FIFO threshold or end of packet (not sure EOPacket reached with length mode = infinite) */
    CC1101_IOCFG2, 0x0E, /* GDO2 = carrier sense */
    CC1101_FIFOTHR, 0x47, /* ADC retention, no RX attenuation, 4 RX FIFO threshold */
    CC1101_SYNC1, 0xAA, /* Sync word MSB */
    CC1101_SYNC0, 0xAA, /* Sync work LSB */
    //CC1101_PKTLEN, 0x00, /* The doc says that the value must be different from 0... */
    /*CC1101_PKTCTRL1 -> default value **includes** the 2 status bytes... so the max received FIFO size is 62 */
    CC1101_PKTCTRL1, 0x00, /* no status bytes appended */
    CC1101_PKTCTRL0, 0x02, /* no whitening, use FIFOs, without CRC, infinite packet length (first byte after sync word) */
    CC1101_ADDR, 0x00, /* no packet filtration */
    CC1101_FSCTRL1, 0x06, /* IF frequency */
    CC1101_MDMCFG4, 0xC0, /* Channel bandwidth: 203kHz */
    CC1101_MDMCFG2, 0x04, /* Modulation: FSK, no manchester, without preamble/sync but carrier-sense */
    CC1101_MDMCFG1, 0x72, /* 24 preamble bytes, default channel spacing */
    CC1101_DEVIATN, 0x34, /* Deviation = 19.04kHz FIXME: should this be handled with data rate??? */
    CC1101_MCSM0, 0x18, /* Autocalibration on RX or TX, 64 ripples, no pin radio control */
    CC1101_FOCCFG, 0x16, /* FOC: 3K, K/2 after sync word, limited to BW_chan/4 */
    CC1101_AGCCTRL2, 0x43,
    CC1101_AGCCTRL1, 0x47, /* Relative carrier sense disabled, but absolute carrier sense, 7db above MAGN_TARGET */
    CC1101_AGCCTRL0, 0x91,
    CC1101_WORCTRL, 0xFB, /* WakeOnRadio: power down RC, 48 cycles for Event 1 (43ms), calibrate RC, maximum Event 0 timeout: 17h */
    /* Note: as MCSM2.RX_TIME is kept to its default value (7), RX will never timeout and WOR should have its auto-sleep disabled */
};

/* Receive FSK-transmitted data without CRC or length */
void rx_fsk_raw_printf(void) {
    gpio_set_dir(BADGE_RADIO_GDO0, GPIO_IN);
    gpio_set_dir(BADGE_RADIO_GDO2, GPIO_IN);
    ccsend(fsk_full_rx, NULL, sizeof(fsk_full_rx));
    radio_set_frequency(868925000);
    radio_set_baud_rate(19500);
    print_cc_configuration();

    uint8_t recv[65];
    printf("wait for RX...\n");
    uint8_t buf[1024];  /* stores multi-part packets */
    size_t buf_i = 0;
    absolute_time_t t0 = get_absolute_time(), now;
    while (true) {
        if (radio_state() != CC1101_STATE_RX)
            radio_wait_state(CC1101_STATE_RX, true);

        /* For now, don't use MCSM2.RX_TIME, the timeout is the previous loop reaching 0, so the state should always stay to RX. */
        /* GDO0 = RX FIFO threshold
         * GDO2 = carrier sense */
        //uint64_t timeout = 10000000;
        while(! (gpio_get(BADGE_RADIO_GDO2) || gpio_get(BADGE_RADIO_GDO0))) /*|| (--timeout == 0) */ {
            //tight_loop_contents();
            /* Polling degrades RX quality but we are in the debug phase */
            uint8_t n;
            sleep_us(500);
            ccread_burst(CC1101_RXBYTES, &n, 1);
            if (n) {
                printf("FIFO not empty ");
                break;
            }
            /* Delimit packet by waiting for no CS long enough, and print them */
            if(buf_i > 0) {
                now = get_absolute_time();
                if (absolute_time_diff_us(t0, now) > 10000) {  /* ~100 symbols */
                    printf("delimited packet: ");
                    for (size_t i=0; i<buf_i; ++i)
                        printf("%02x ", buf[i]);
                    printf("\n");
                    buf_i = 0;
                }
            }
        }

        t0 = get_absolute_time();  /* last received byte timestamp */
        if (gpio_get(BADGE_RADIO_GDO2))
            printf("carrier sense  ");
        else if (gpio_get(BADGE_RADIO_GDO0))
            printf("FIFO threshold ");

        /* TODO: carrier sens does not always de-asserts, so we can't rely on it...
         *  We should maybe rely on GDO0 = 0x01 -> FIFO >= threshold or EOPacket reached.
         *  Moreover, the received bits are not synced and there is always some issue so we must find a strategy:
         *  - find a sync byte/word (while we don't know if there is one, we could use SYNC1=SYNC0=0xAA,
         *    but we risk missing out the first 2 bits of the message (1 in 4),
         *  - find the length of the preamble, or the signal start indication,
         *  - find something else, like printing the packet shifted by 0 to 7 bits...
         *  - find out why carrier sense does not always de-asserts. */
        ///* Wait for GD0 to go low (packet has been sent) */
        //while(gpio_get(BADGE_RADIO_GDO2))
        //    tight_loop_contents();

        /* Maybe we timed out instead of receiving something -> ... */
        //if (timeout == 0) {
        //    printf("timed out\n");
        //    continue;
        //}

        /* We received something... */
        uint8_t n;
        ccread_burst(CC1101_RXBYTES, &n, 1);
        bool of = n>>7;
        n &= 0x7f;
        printf("received %02d bytes ", n);

        ccread_burst(CC1101_FREQEST, recv, 3);  /* FREQEST then LQI then RSSI */
        int8_t eoff = recv[0];
        bool crc_ok = recv[1] >> 7;
        uint8_t lqi = recv[1] & 0x7f;
        int16_t rssi = recv[2];
        rssi -= 74;  /* According to CC1101 datasheet */
        printf("with RSSI=%+04d dBm, LQI=%03d, CRC=%d, est. freq. %+ 7lli Hz\n", rssi, lqi, crc_ok, ((int64_t)(eoff)*CC1101_fXOSC)>>14);

        /* Add data to our current buffer, but don't overflow */
        ccread_burst(CC1101_RXFIFO, recv, n);
        for (size_t i=0; i<n && buf_i<sizeof(buf);)
            buf[buf_i++] = recv[i++];

        if (of) {
            printf(" overflow");
            //recv[0] = CC1101_SFRX;
            //ccsend(recv, NULL, 1);
        }
    }
}


int main() {
    uint8_t cmd[2];
    stdio_usb_init();
    //log_set_level(LOG_LEVEL_INFO);

    printf("init\n");
    radio_init();
    printf("boot\n");
    radio_boot();
    gpio_init(BADGE_RADIO_GDO0);
    gpio_init(BADGE_RADIO_GDO2);

    log_cc_status();

    //tx_pulses();
    ////rx_times();

    ///* We need a reset between changing modes, otherwise some of the conf makes it never go out of calibrating */
    //cmd[0] = CC1101_SRES;
    //printf("reset\n");
    //ccsend(cmd, NULL, 1);
    //radio_wait_state(CC1101_STATE_IDLE, false);

    //tx_chat_flipper("Badge SecSea joined chat.\n");
    //sleep_ms(3000);
    ////tx_chat_flipper("Badge SecSea: Hey, how are you?\n");
    ////sleep_ms(2000);
    ////tx_chat_flipper("Badge SecSea: Ouais ?\n");
    ////sleep_ms(2000);
    ////tx_chat_flipper("Badge SecSea: pas tres locace dis donc...\n");
    ////sleep_ms(2000);
    ////tx_chat_flipper("Badge SecSea: ...\n");
    ////sleep_ms(2000);
    ////tx_chat_flipper("Badge SecSea: Never\n");
    ////sleep_ms(300);
    ////tx_chat_flipper("Badge SecSea: gonna\n");
    ////sleep_ms(300);
    ////tx_chat_flipper("Badge SecSea: let\n");
    ////sleep_ms(500);
    ////tx_chat_flipper("Badge SecSea: you\n");
    ////sleep_ms(500);
    ////tx_chat_flipper("Badge SecSea: doooown!\n");
    ////sleep_ms(3000);
    ////tx_chat_flipper("Badge SecSea left chat.\n");

    //rx_fsk_printf();
    rx_fsk_raw_printf();

    /* Shutdown */
    printf("wait\n");
    sleep_ms(2000);  /* When out of TX mode, it still emits around the frequency here, but it's okay we are not in IDLE */

    printf("stop\n");
    cmd[0] = CC1101_SPWD;
    ccsend(cmd, NULL, 1);
    sleep_us(100);  /* Have to wait ~100µ before we see the chip powers down */
    log_cc_status();

    /* Bringing CSn to 0 again will wake up the chip */
    sleep_ms(1000);
    printf("reboot\n");
    radio_boot();
    log_cc_status();
    sleep_ms(1000);
    printf("restop\n");
    cmd[0] = CC1101_SPWD;
    ccsend(cmd, NULL, 1);
}
