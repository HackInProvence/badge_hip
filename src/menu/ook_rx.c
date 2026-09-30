/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdio.h>
#include <string.h>

#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/sync.h"

#include "net.h"
#include "ook_rx.h"
#include "pinouts.h"
#include "radio.h"
#include "radio_tools.h"
#include "remote.h"

#define RING 1024  /* Durations, signed: > 0 carrier on, < 0 silence */
#define MIN_FRAME 40  /* Durations of the shortest frames worth decoding */
#define OVERLAP 120  /* Durations decoded again with the next part (> 2 Princeton frames of 50 durations) */

/* Asynchronous OOK receiver, like the "AM650" preset of the Flipper Zero */
static const uint8_t OOK_REGS[] = {
    CC1101_IOCFG0, 0x0D,  /* GDO0: serial data output (demodulated) */
    CC1101_FIFOTHR, 0x07,
    CC1101_PKTCTRL0, 0x32,  /* Asynchronous serial mode, infinite length */
    CC1101_FSCTRL1, 0x06,
    CC1101_MDMCFG4, 0x17,  /* Channel bandwidth 650 kHz */
    CC1101_MDMCFG3, 0x32,
    CC1101_MDMCFG2, 0x30,  /* ASK/OOK, no sync word */
    CC1101_MDMCFG1, 0x00,
    CC1101_MDMCFG0, 0x00,
    CC1101_MCSM0, 0x18,
    CC1101_FOCCFG, 0x18,
    CC1101_AGCCTRL2, 0x07,
    CC1101_AGCCTRL1, 0x00,
    CC1101_AGCCTRL0, 0x91,
    CC1101_FREND1, 0xB6,
    CC1101_FREND0, 0x11,
};

static volatile int16_t ring[RING];
static volatile uint16_t head = 0, count = 0;
static volatile uint32_t last_edge_us = 0, pulses = 0;
static int users = 0;
static ookdec_signal_t signal;  /* Static: 1 KB */
static ookdec_result_t last;
static uint32_t frames = 0;


static void edge(void) {
    uint32_t events = gpio_get_irq_event_mask(BADGE_RADIO_GDO0);
    if (! events)
        return;
    gpio_acknowledge_irq(BADGE_RADIO_GDO0, events);
    uint32_t t = time_us_32(), d = t - last_edge_us;
    last_edge_us = t;
    if (d > 32767)
        d = 32767;
    /* The level that just ended is the opposite of the level now */
    ring[head] = gpio_get(BADGE_RADIO_GDO0) ? -(int16_t)d : (int16_t)d;
    head = (head + 1) % RING;
    if (count < RING)
        ++count;
    ++pulses;
}


void ook_rx_start(void) {
    if (users++)
        return;  /* Already listening */
    net_pause(true);
    radio_wait_state(CC1101_STATE_IDLE, true);
    radio_write_registers(OOK_REGS, sizeof(OOK_REGS));
    head = count = 0;
    last_edge_us = time_us_32();
    gpio_init(BADGE_RADIO_GDO0);
    gpio_set_dir(BADGE_RADIO_GDO0, GPIO_IN);
    gpio_add_raw_irq_handler(BADGE_RADIO_GDO0, edge);
    gpio_set_irq_enabled(BADGE_RADIO_GDO0, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    irq_set_enabled(IO_IRQ_BANK0, true);
    radio_write_registers((const uint8_t[]){CC1101_SRX}, 1);
}


void ook_rx_resume(void) {
    if (! users)
        return;
    /* Another feature used the radio meanwhile (message, carrier): the OOK registers again */
    radio_wait_state(CC1101_STATE_IDLE, true);
    radio_write_registers(OOK_REGS, sizeof(OOK_REGS));
    gpio_set_dir(BADGE_RADIO_GDO0, GPIO_IN);
    radio_write_registers((const uint8_t[]){CC1101_SRX}, 1);
}


static void decode(uint32_t silence);

void ook_rx_stop(void) {
    if (! users || users > 1) {
        if (users)
            --users;
        return;  /* Someone still listens */
    }
    /* The last user: decode what was received before leaving (the end of a transmission) */
    if (count >= MIN_FRAME)
        decode(time_us_32() - last_edge_us);
    users = 0;
    gpio_set_irq_enabled(BADGE_RADIO_GDO0, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
    gpio_remove_raw_irq_handler(BADGE_RADIO_GDO0, edge);
    if (radio_tools_idle())
        radio_tools_reconfigure();  /* Back to GFSK (otherwise the feature using the radio already did it) */
    net_pause(false);
}


bool ook_rx_active(void) {
    return users > 0;
}


void ook_rx_dump(void) {
    printf("ook: last signal, %u durations:", signal.n);
    for (uint16_t i = 0; i < signal.n; ++i)
        printf(" %c%u", i % 2 ? '-' : '+', signal.us[i]);
    printf("\n");
}


uint32_t ook_rx_quiet_us(void) {
    return time_us_32() - last_edge_us;
}


uint32_t ook_rx_frames(void) {
    return frames;
}


bool ook_rx_get(uint32_t *seen, ookdec_result_t *result) {
    if (*seen == frames)
        return false;
    *seen = frames;
    *result = last;
    return true;
}


uint32_t ook_rx_pulses(void) {
    return pulses;
}


/* Copies the ring into the signal for the decoder: starts with a carrier, merges the durations of the same level */
static void build_signal(uint32_t silence_us) {
    uint32_t irq = save_and_disable_interrupts();
    uint16_t n = count, start = (head + RING - n) % RING;
    signal.n = 0;
    uint16_t i = 0;
    for (; i < n && signal.n < OOKDEC_MAX_PULSES - 1; ++i) {
        int16_t v = ring[(start + i) % RING];
        bool high = v > 0;
        uint16_t us = high ? v : -v;
        if (signal.n == 0 && ! high)
            continue;  /* The decoder wants a carrier first */
        bool last_high = (signal.n % 2) == 0 ? false : true;  /* Index even = high, odd = low */
        if (signal.n && high == last_high) {
            uint32_t sum = signal.us[signal.n - 1] + us;
            signal.us[signal.n - 1] = sum > 65535 ? 65535 : sum;  /* Missed edge: same level again */
        } else {
            signal.us[signal.n++] = us;
        }
    }
    /* What did not fit stays for the next decoding, with an overlap: a frame across the cut is not lost */
    count = i < n ? (n - i + OVERLAP < n ? n - i + OVERLAP : n) : 0;
    restore_interrupts(irq);
    /* The silence since the last edge ends the frame */
    if (signal.n % 2 == 1)
        signal.us[signal.n++] = silence_us > 65535 ? 65535 : silence_us;
}


void ook_rx_task(absolute_time_t now) {
    (void)now;
    if (! users)
        return;
    uint32_t silence = time_us_32() - last_edge_us;
    uint16_t n = count;
    /* A frame ended (a long silence), or the decoder can be filled (noise, or a long transmission) */
    bool ended = silence >= OOK_RX_FRAME_GAP_US && n >= MIN_FRAME;
    if (! ended && n < OOKDEC_MAX_PULSES - 1) {
        if (silence >= OOK_RX_FRAME_GAP_US && n)
            count = 0;  /* Too short to be a frame: noise */
        return;
    }
    decode(silence);
}


static void decode(uint32_t silence) {
    build_signal(silence);
    if (signal.n < MIN_FRAME || ! ookdec_decode(&signal, &last))
        return;
    ++frames;
    printf("ook: %s\n", last.text);
    if (! strcmp(last.protocol, "Princeton"))
        remote_princeton((uint32_t)last.code);
}
