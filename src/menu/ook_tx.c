/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* OOK transmitter: a Princeton frame (PT2262 / EV1527, 24 bits), like a Flipper Zero remote.
 * The CC1101 is in asynchronous serial mode: it sends the carrier while GDO0 (driven by the RP2040) is high.
 * The edges are timed by a hardware alarm (the main loop is not blocked), the radio is given back to the
 * network by ook_tx_task() once the frames are sent. */

#include <stdio.h>

#include "hardware/gpio.h"
#include "pico/time.h"

#include "net.h"
#include "ook_rx.h"
#include "ook_tx.h"
#include "pinouts.h"
#include "radio.h"
#include "radio_tools.h"

#define TE_US 400  /* The time element of the Flipper (tools/ook_sub.py) */
#define N_DURATIONS (24 * 2 + 2)  /* 24 bits (pulse, gap), the stop pulse and the guard gap */

static const uint8_t OOK_TX_REGS[] = {
    CC1101_IOCFG0, 0x2E,  /* GDO0 in high impedance: it is the data input, driven by the RP2040 */
    CC1101_PKTCTRL0, 0x32,  /* Asynchronous serial mode, infinite length */
    CC1101_MDMCFG4, 0x17,
    CC1101_MDMCFG3, 0x32,
    CC1101_MDMCFG2, 0x30,  /* ASK/OOK, no sync word */
    CC1101_MDMCFG1, 0x00,
    CC1101_MDMCFG0, 0x00,
    CC1101_MCSM0, 0x18,
    CC1101_FREND1, 0xB6,
    CC1101_FREND0, 0x11,  /* PA_POWER = 1: "1" = PATABLE[1], "0" = PATABLE[0] */
};
static const uint8_t PATABLE_OOK[2] = {0x00, 0xC0};  /* Off, +10 dBm */

static uint16_t durations[N_DURATIONS];  /* Carrier on (even index), off (odd index) */
static volatile int step = 0, frames_left = 0;
static volatile bool sending = false, done = false;


static int64_t next_edge(alarm_id_t id, void *data) {
    (void)id;
    (void)data;
    if (++step == N_DURATIONS) {
        step = 0;
        if (--frames_left == 0) {
            gpio_put(BADGE_RADIO_GDO0, 0);
            done = true;
            return 0;  /* No more alarms */
        }
    }
    gpio_put(BADGE_RADIO_GDO0, step % 2 == 0);
    return durations[step];  /* From the time this edge was scheduled: no drift */
}


bool ook_tx_princeton(uint32_t code, int frames) {
    if (sending || ook_rx_active() || ! radio_tools_idle() || net_transmitting() || frames < 1)
        return false;  /* Not now: another user of the radio, or a packet on air */
    /* 1 = long pulse, short gap; 0 = short pulse, long gap; most significant bit first */
    for (int i = 0; i < 24; ++i) {
        bool bit = (code >> (23 - i)) & 1;
        durations[2 * i] = bit ? 3 * TE_US : TE_US;
        durations[2 * i + 1] = bit ? TE_US : 3 * TE_US;
    }
    durations[48] = TE_US;
    durations[49] = 30 * TE_US;

    net_pause(true);
    radio_wait_state(CC1101_STATE_IDLE, true);
    radio_write_registers(OOK_TX_REGS, sizeof(OOK_TX_REGS));
    radio_set_patable(PATABLE_OOK, sizeof(PATABLE_OOK));
    gpio_init(BADGE_RADIO_GDO0);
    gpio_put(BADGE_RADIO_GDO0, 0);
    gpio_set_dir(BADGE_RADIO_GDO0, GPIO_OUT);
    radio_write_registers((const uint8_t[]){CC1101_STX}, 1);

    step = -1;  /* The first alarm starts the first pulse */
    frames_left = frames;
    done = false;
    sending = true;
    /* After the calibration of the synthesizer (~1 ms) */
    if (add_alarm_in_us(1500, next_edge, NULL, true) < 0)
        done = true;  /* No alarm available: ook_tx_task() gives the radio back */
    return true;
}


bool ook_tx_busy(void) {
    return sending;
}


void ook_tx_task(void) {
    if (! sending || ! done)
        return;
    radio_write_registers((const uint8_t[]){CC1101_SIDLE}, 1);
    gpio_set_dir(BADGE_RADIO_GDO0, GPIO_IN);
    radio_tools_reconfigure();  /* Back to GFSK, with the power of the network */
    sending = false;
    net_pause(false);
    printf("ook tx: done\n");
}
