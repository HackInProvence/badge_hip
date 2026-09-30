/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdio.h>
#include <string.h>

#include "hardware/gpio.h"
#include "hardware/irq.h"

#include "ook_rx.h"
#include "ook_tx.h"
#include "pinouts.h"
#include "radio.h"
#include "radio_tools.h"
#include "social.h"


#define BAUDS 9995
#define PATABLE 0xC0  /* +10dBm @433MHz, same as the Flipper GFSK preset */
#define TX_TIMEOUT_US 1000000
#define XOSC_DIVIDER 192  /* IOCFG0 = 0x3F outputs CLK_XOSC/192 on GDO0 */
#define XOSC_MEASURE_US 1000000
#define XOSC_MIN_HZ 25000000  /* Plausible range for the measure (the CC1101 accepts 26 to 27MHz crystals) */
#define XOSC_MAX_HZ 28000000

typedef enum {
    R_IDLE,
    R_SENDING,
    R_CARRIER,
    R_MEASURING,
} radio_tools_state_t;

static radio_tools_state_t state = R_IDLE;
static absolute_time_t start_ts = 0;
static uint32_t carrier_max_us = 0;
static unsigned count = 0;
static uint8_t chip_version = 0;
static uint32_t xosc_hz = 0;
static volatile uint32_t edges = 0;
static char message[64] = "";


static void configure(void) {
    radio_wait_state(CC1101_STATE_IDLE, true);
    radio_write_registers(radio_preset_gfsk, radio_preset_gfsk_len);
    /* The registers that the OOK receiver (ook_rx.c) changes and that the preset keeps at their reset values:
     * with FREND0 = 0x11, the packets would be sent with PATABLE[1] (nothing) */
    radio_write_registers((const uint8_t[]){CC1101_FREND0, 0x10, CC1101_FREND1, 0x56, CC1101_MDMCFG0, 0xF8}, 6);
    radio_set_power(PATABLE);
    radio_set_frequency(RADIO_TOOLS_FREQ_HZ);
    radio_set_baud_rate(BAUDS);
}


void radio_tools_init(void) {
    radio_init();
    radio_reset();  /* Same as the Flipper: reset, then only the preset registers differ from the defaults */
    radio_read_registers(CC1101_VERSION, &chip_version, 1);
    configure();
    printf("radio: CC1101 version 0x%02x\n", chip_version);
    /* Measure the crystal (~1s, in the background) and reconfigure the radio with it */
    radio_tools_measure_xosc();
}


uint8_t radio_tools_chip_version(void) {
    return chip_version;
}


const char *radio_tools_message(void) {
    return message;
}


void radio_tools_reconfigure(void) {
    configure();
}


/* Back to idle: a feature that listens in OOK all the time (talk badge) gets its receiver back */
static void back_to_idle(void) {
    state = R_IDLE;
    if (ook_rx_active())
        ook_rx_resume();
}


bool radio_tools_idle(void) {
    return state == R_IDLE;
}


/* Sync word and power: the chat of the Flipper, or the network of the cicadas (see social.c) */
static void set_profile(uint8_t sync1, uint8_t sync0, uint8_t patable) {
    radio_wait_state(CC1101_STATE_IDLE, true);
    radio_write_registers((const uint8_t[]){CC1101_SYNC1, sync1, CC1101_SYNC0, sync0}, 4);
    radio_set_power(patable);
}

void radio_tools_profile_chat(uint8_t patable) {
    set_profile(0x46, 0x4C, patable);
}


void radio_tools_profile_social(uint8_t patable) {
    set_profile(RADIO_TOOLS_SOCIAL_SYNC1, RADIO_TOOLS_SOCIAL_SYNC0, patable);
}


static char last_text[64] = "";

const char *radio_tools_last_text(void) {
    return last_text;
}

unsigned radio_tools_send(void) {
    if (state != R_IDLE || ook_tx_busy())
        return 0;
    configure();  /* The whole GFSK profile: another feature may have left the radio in OOK (ook_rx.c) */
    set_profile(0x46, 0x4C, PATABLE);  /* Flipper chat */
    char msg[64];
    int len = snprintf(msg, sizeof(msg), "SecSea %s coucou #%u\n", social_name(), ++count);
    if (! radio_tx_packet((const uint8_t *)msg, len))
        return 0;
    snprintf(last_text, sizeof(last_text), "%.*s", len - 1, msg);  /* Without the newline */
    state = R_SENDING;
    start_ts = get_absolute_time();
    snprintf(message, sizeof(message), "Envoi du message #%u", count);
    printf("radio: sending message #%u\n", count);
    return count;
}


void radio_tools_carrier_start(uint32_t max_ms) {
    if (state != R_IDLE || ook_tx_busy())
        return;
    configure();  /* From the GFSK profile (the radio may be in OOK, see ook_rx.c) */
    radio_wait_state(CC1101_STATE_IDLE, true);
    /* Asynchronous serial mode: the data to send is read on GDO0, keep it high for a constant frequency (f + deviation) */
    radio_write_registers((const uint8_t[]){CC1101_IOCFG0, 0x2E, CC1101_PKTCTRL0, 0x32}, 4);
    gpio_init(BADGE_RADIO_GDO0);
    gpio_put(BADGE_RADIO_GDO0, 1);
    gpio_set_dir(BADGE_RADIO_GDO0, GPIO_OUT);
    radio_write_registers((const uint8_t[]){CC1101_STX}, 1);
    state = R_CARRIER;
    start_ts = get_absolute_time();
    carrier_max_us = max_ms * 1000;
    snprintf(message, sizeof(message), "Porteuse sur %.2f MHz", RADIO_TOOLS_FREQ_HZ / 1e6);
    printf("radio: carrier on\n");
}


void radio_tools_carrier_stop(void) {
    if (state != R_CARRIER)
        return;
    radio_wait_state(CC1101_STATE_IDLE, true);
    gpio_set_dir(BADGE_RADIO_GDO0, GPIO_IN);
    configure();  /* Back to the packet mode */
    back_to_idle();
    snprintf(message, sizeof(message), "Porteuse arrêtée");
    printf("radio: carrier off\n");
}


bool radio_tools_carrier_on(void) {
    return state == R_CARRIER;
}


/* Raw handler of GDO0 only: the SDK has a single "callback" per core, shared with the IR receiver */
static void count_edge(void) {
    uint32_t events = gpio_get_irq_event_mask(BADGE_RADIO_GDO0);
    if (! events)
        return;
    gpio_acknowledge_irq(BADGE_RADIO_GDO0, events);
    ++edges;
}


void radio_tools_measure_xosc(void) {
    if (state != R_IDLE || ook_tx_busy())
        return;
    radio_wait_state(CC1101_STATE_IDLE, true);
    radio_write_registers((const uint8_t[]){CC1101_IOCFG0, 0x3F}, 2);  /* CLK_XOSC/192 on GDO0 */
    gpio_init(BADGE_RADIO_GDO0);
    edges = 0;
    gpio_add_raw_irq_handler(BADGE_RADIO_GDO0, count_edge);
    gpio_set_irq_enabled(BADGE_RADIO_GDO0, GPIO_IRQ_EDGE_RISE, true);
    irq_set_enabled(IO_IRQ_BANK0, true);
    state = R_MEASURING;
    start_ts = get_absolute_time();
    snprintf(message, sizeof(message), "Mesure du quartz...");
}


bool radio_tools_measuring(void) {
    return state == R_MEASURING;
}


uint32_t radio_tools_xosc_hz(void) {
    return xosc_hz;
}


void radio_tools_task(absolute_time_t now) {
    int64_t elapsed = absolute_time_diff_us(start_ts, now);
    switch (state) {
    case R_SENDING:
        /* Leave time to the radio to leave IDLE (calibration) before polling */
        if (elapsed < 5000)
            break;
        radio_state_t st = radio_state();
        if (st == CC1101_STATE_IDLE) {
            back_to_idle();
            snprintf(message, sizeof(message), "Message #%u envoyé", count);
            printf("radio: message #%u sent in %lld ms\n", count, elapsed / 1000);
        } else if (st == CC1101_STATE_TXFIFO_UNDERFLOW || elapsed > TX_TIMEOUT_US) {
            radio_write_registers((const uint8_t[]){CC1101_SIDLE}, 1);
            radio_write_registers((const uint8_t[]){CC1101_SFTX}, 1);
            back_to_idle();
            snprintf(message, sizeof(message), "Echec de l'envoi (état %d)", st);
            printf("radio: %s\n", message);
        }
        break;
    case R_CARRIER:
        if (elapsed > (int64_t)carrier_max_us)
            radio_tools_carrier_stop();
        break;
    case R_MEASURING:
        if (elapsed < XOSC_MEASURE_US)
            break;
        gpio_set_irq_enabled(BADGE_RADIO_GDO0, GPIO_IRQ_EDGE_RISE, false);
        gpio_remove_raw_irq_handler(BADGE_RADIO_GDO0, count_edge);
        xosc_hz = (uint32_t)((uint64_t)edges * XOSC_DIVIDER * 1000000 / elapsed);
        gpio_set_dir(BADGE_RADIO_GDO0, GPIO_IN);
        /* The badges have either a 26MHz or a 27MHz crystal, a wrong value shifts the frequency by ~16MHz.
         * The measure can miss edges when the CPU is busy (hundreds of ppm, i.e. hundreds of kHz at 433MHz),
         * so it only chooses the crystal: the nominal value (or the calibrated CC1101_fXOSC) is more precise. */
        if (xosc_hz > XOSC_MIN_HZ && xosc_hz < XOSC_MAX_HZ) {
            uint32_t nominal = xosc_hz < 26500000 ? 26000000 : 27000000;
            int32_t calibrated_diff = (int32_t)(CC1101_fXOSC - nominal);
            if (calibrated_diff > -20000 && calibrated_diff < 20000)
                nominal = CC1101_fXOSC;  /* Calibrated value of the same crystal */
            radio_set_xosc(nominal);
        }
        configure();  /* Recompute the frequency and baud rate, and restore GDO0 */
        back_to_idle();
        snprintf(message, sizeof(message), "Quartz : %.4f MHz", xosc_hz / 1e6);
        printf("radio: crystal measured %lu Hz (default CC1101_fXOSC = %d Hz), using %lu Hz\n",
               (unsigned long)xosc_hz, CC1101_fXOSC, (unsigned long)radio_get_xosc());
        break;
    default:
        break;
    }
}
