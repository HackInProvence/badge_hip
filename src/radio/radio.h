/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file radio.h
 *
 * \brief Radio API: interfaces with the CC1101 which controls the radio.
 *
 * Typical usage: \ref radio_init followed by \ref radio_boot, then \ref radio_set_frequency.
 * Then push your packet-mode configuration (TODO), then your data (TODO), then \ref radio_wait_state(TX or RX, true).
 *
 * TODO:
 * - decide whether print_cc_ function worth linking printf (it should except no log),
 * - accessors to push configuration and data to send, access to received bytes and quality indicators,
 * - lock on a messaging protocol and provide methods for that.
 * */

#ifndef _RADIO_H
#define _RADIO_H

#include "pinouts.h"


#ifndef CC1101_fXOSC
/** \brief Define the CC1101 cristal frequency, which we can calibrate with radio_calibrate.c and .py (see also badge_secsea.h) */
#define CC1101_fXOSC 26000000
#endif

/* Verify that configuration worked... */
//#define XSTR(x) STR(x)
//#define STR(x) #x
//#pragma message "CC1101 frequency set to " XSTR(CC1101_fXOSC)


/** \brief Initialize the radio library for write operations. */
void radio_init(void);

/** \brief Boot the radio module (or wake from deep sleep) */
void radio_boot(void);

/** \brief Power down / deep sleep the radio module */
void radio_power_down(void);

/** \brief Sets the frequency (in Hz) of the transmission using calibrated crystal frequency.
 *
 * Must be < 1.6GHz.
 * Floored to the closest CC1101_fXOSC/65536. */
void radio_set_frequency(uint32_t freq_hz);

/** \brief Sets the data rate (in bauds, see datasheet for acceptable values) using calibrated crystal frequency.
 *
 * Must be < 1.6MHz.
 * Flooring depends on compatible exponent.
 * Logs the effective baud rate. */
void radio_set_baud_rate(uint32_t rate_bauds);


/** Internal state value (returned in the status byte) */
typedef enum {
    CC1101_STATE_IDLE = 0b000,
    CC1101_STATE_RX = 0b001,
    CC1101_STATE_TX = 0b010,
    CC1101_STATE_FSTXON = 0b011,
    CC1101_STATE_CALIBRATE = 0b100,
    CC1101_STATE_SETTLING = 0b101,
    CC1101_STATE_RXFIFO_OVERFLOW = 0b110,
    CC1101_STATE_TXFIFO_UNDERFLOW = 0b111,
} radio_state_t;

/** \brief Get the current state of the radio */
radio_state_t radio_state(void);

/** \brief Wait for the internal state to be ready, and optionally also emit the command to change state */
void radio_wait_state(radio_state_t target_state, bool do_change);


/** \brief Default configuration for the flipper chat app.
 *
 * There is no specific configuration found in https://github.com/twisted-pear/esubghz_chat/blob/main/esubghz_chat.c,
 * but the enter_chat function calls subghz_tx_rx_worker, which sets up a GFSK by default,
 * see subghz_device_cc1101_preset_gfsk_9_99kb_async_regs in https://github.com/flipperdevices/flipperzero-firmware/blob/dev/lib/subghz/devices/cc1101_configs.c
 *
 * This uses the packet mode of the CC1101
 *
 * 999 = 9.99kbps */
extern const uint8_t radio_preset_gfsk[];
extern const size_t radio_preset_gfsk_len;


/* Register access type: default is write single byte, but these sets byte to change the access
 * CC1101_READ to read a byte instead of write,
 * CC1101_BURST to read/write multiple bytes (until CSn comes high again) */
#define CC1101_READ(reg) ((reg) | 0x80)
#define CC1101_BURST(reg) ((reg) | 0x40)

/** Names of registers */
typedef enum {
    CC1101_IOCFG2 = 0x00,
    CC1101_IOCFG1 = 0x01,
    CC1101_IOCFG0 = 0x02,
    CC1101_FIFOTHR = 0x03,
    CC1101_SYNC1 = 0x04,
    CC1101_SYNC0 = 0x05,
    CC1101_PKTLEN = 0x06,
    CC1101_PKTCTRL1 = 0x07,
    CC1101_PKTCTRL0 = 0x08,
    CC1101_ADDR = 0x09,
    CC1101_CHANNR = 0x0A,
    CC1101_FSCTRL1 = 0x0B,
    CC1101_FSCTRL0 = 0x0C,
    CC1101_FREQ2 = 0x0D,
    CC1101_FREQ1 = 0x0E,
    CC1101_FREQ0 = 0x0F,
    CC1101_MDMCFG4 = 0x10,
    CC1101_MDMCFG3 = 0x11,
    CC1101_MDMCFG2 = 0x12,
    CC1101_MDMCFG1 = 0x13,
    CC1101_MDMCFG0 = 0x14,
    CC1101_DEVIATN = 0x15,
    CC1101_MCSM2 = 0x16,
    CC1101_MCSM1 = 0x17,
    CC1101_MCSM0 = 0x18,
    CC1101_FOCCFG = 0x19,
    CC1101_BSCFG = 0x1A,
    CC1101_AGCCTRL2 = 0x1B,
    CC1101_AGCCTRL1 = 0x1C,
    CC1101_AGCCTRL0 = 0x1D,
    CC1101_WOREVT1 = 0x1E,
    CC1101_WOREVT0 = 0x1F,
    CC1101_WORCTRL = 0x20,
    CC1101_FREND1 = 0x21,
    CC1101_FREND0 = 0x22,
    CC1101_FSCAL3 = 0x23,
    CC1101_FSCAL2 = 0x24,
    CC1101_FSCAL1 = 0x25,
    CC1101_FSCAL0 = 0x26,
    CC1101_RCCTRL1 = 0x27,
    CC1101_RCCTRL0 = 0x28,
    CC1101_FSTEST = 0x29,
    CC1101_PTEST = 0x2A,
    CC1101_AGCTEST = 0x2B,
    CC1101_TEST2 = 0x2C,
    CC1101_TEST1 = 0x2D,
    CC1101_TEST0 = 0x2E,
    // CC1101__NONAME = 0x2F, /* We don't know what's here */
    /* Commands */
    CC1101_SRES = 0x30,  /* Reset */
    CC1101_SFSTXON = 0x31,
    CC1101_SXOFF = 0x32,
    CC1101_SCAL = 0x33,
    CC1101_SRX = 0x34,
    CC1101_STX = 0x35,
    CC1101_SIDLE = 0x36,
    // CC1101__NONAME = 0x37, /* We don't know what's here */
    CC1101_SWOR = 0x38,  /* Wake On Radio */
    CC1101_SPWD = 0x39,  /* PoWer Down (deep sleep) */
    CC1101_SFRX = 0x3A,  /* Flush RX FIFO (in IDLE) */
    CC1101_SFTX = 0x3B,  /* Flush TX FIFO (in IDLE) */
    CC1101_SWORRST = 0x3C,
    CC1101_SNOP = 0x3D,
    CC1101_PATABLE = 0x3E,
    CC1101_TXFIFO = 0x3F,
    CC1101_RXFIFO = 0x3F,  /* Use with CC1101_READ() only */
    /* These registers can only be accessed with READ(BURST()) */
    CC1101_PARTNUM = 0x30,
    CC1101_VERSION = 0x31,
    CC1101_FREQEST = 0x32,
    CC1101_LQI = 0x33,
    CC1101_RSSI = 0x34,
    CC1101_MARCSTATE = 0x35,
    CC1101_WORTIME1 = 0x36,
    CC1101_WORTIME0 = 0x37,
    CC1101_PKTSTATUS = 0x38,
    CC1101_VCO_VC_DAC = 0x39,
    CC1101_TXBYTES = 0x3A,
    CC1101_RXBYTES = 0x3B,
    CC1101_RCCTRL1_STATUS = 0x3C,
    CC1101_RCCTRL0_STATUS = 0x3D,
} radio_register_t;


/** Internal util/debug made accessible to tests */
STATIC void ccsend(const uint8_t *, uint8_t *, size_t);
STATIC void ccread_burst(uint8_t, uint8_t *, size_t);
STATIC void log_cc_status(void);
STATIC void print_cc_configuration(void);


#endif /* _RADIO_H */
