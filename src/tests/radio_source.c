/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file radio.h
 *
 * \brief RAdio SOurce: USB <-> CC1101 bridge.
 *
 * See radio_source.py to interact with this firmware.
 *
 * Designed to expose a CC1101 on USB tty for transmitting packets over the air.
 * Now extended to also receive data on RF and forward it to USB tty.
 *
 * LED color:
 * - blue breathing: wait for packet (from USB to be aired, from air to USB),
 * - light blue: receiving something on USB,
 * - pink: airing something,
 * - green to red: RSSI from best to worst,
 * - white: receiving, but RSSI unknown yet (if you see it, means the sync word has not been received),
 * - quick rainbow: error on USB receiving end.
 *
 * The CC1101 has its own states, but we also need a state machine to handle data on USB tty.
 *
 * Quick description of the UART protocol:
 * - 2 bytes for length (little endian) -> length is the rest of the packet (includes the command but not the length),
 * - 1 byte for command,
 * - (len-1) payload bytes */

#include <math.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/sync.h"

#include "log.h"
#include "dispatcher.h"
#include "leds.h"
#include "radio.h"

#define SKY LED_RGB(0, 128, 255)
#define BLUE LED_RGB(0, 0, 32)
#define PINK LED_RGB(255, 0, 255)
#define RED LED_RGB(32, 0, 0)
#define WHITE LED_RGB(64, 64, 64)

/* TX side TODO: we need less, because this is an RX of the USB side */
static buffer_state_t st_tx = WAITING;
static absolute_time_t st_tx_ts = 0;
static size_t i_tx = 0;  /* Index to write to buffer_tx when receiving data from USB */

/* Shared with dispatcher.c, for now FIXME */
extern raso_state_t st_raso;
extern critical_section_t cs_update_st_raso;
extern size_t i_rx;
extern const uint8_t config_rx[];
extern const size_t config_rx_len;
extern int16_t rssi;
extern uint8_t send_mode;
extern uint8_t buffer_tx[];


/* Assumes that it is called with the radio in the control state */
void emit_buffer(void); /* Split the packet emission from process_usb_command */

/* Assumes that it is called with a complete buffer_tx and a radio in the control state */
void process_usb_command(void) {
    if (*(uint16_t *)buffer_tx < 1)  /* We should be able to decode the command */
        return;
    uint16_t len = *(uint16_t *)buffer_tx-1;  /* Length of the payload */
    uint8_t cmd = buffer_tx[2];
    uint8_t *payload = &buffer_tx[3];

    if (st_raso != RADIO_CONTROL) {
        log_warning("cannot send a packet on non-reserved radio, state is %d, expect %d", st_raso, RADIO_CONTROL);
        return;
    }

    switch(cmd) {
    case 0xD0:  /* Choose commands that cannot be written easily with a keyboard in minicom... */
        /* Pass config */
        log_info("write %d registers", len/2);
        /* Filter the configuration to prevent known erroneous states */
        for (size_t i=0; i<len-1; i+=2) {
            if (payload[i] == CC1101_PKTCTRL0) {
                uint8_t new_mode = payload[i+1] & 0x03;
                if (new_mode == 0) {
                    /* Simplify our control logic to not support FIXED length (it is a transient submode of INFINITE) */
                    log_warning("changed packet length mode from FIXED to INFINITE");
                    new_mode = 2;
                }
                if (new_mode != send_mode) {
                    log_info("set packet length mode: %d", new_mode);
                    send_mode = new_mode;
                }
            }
            if (payload[i] == CC1101_MDMCFG2) {
                if (! (payload[i+1] & 0x04)) {
                    log_warning("changed MDMCFG2.SYNC_MODE to use carrier-sense, we rely on that");
                    payload[i+1] |= 0x04;
                }
            }
        }
        ccsend(payload, NULL, len);
        break;
    case 0xD1:
        /* Set frequency */
        if(len == 4) {
            uint32_t freq = *(uint32_t *)payload;
            log_info("set frequency to %d", freq);
            radio_set_frequency(freq);
        } else
            log_warning("expecting 7 bytes for frequency command, received %d", len+3);
        break;
    case 0xD2:
        emit_buffer();
        break;
    case 0xD3:
        /* Set baud rate */
        if(len == 4) {
            uint32_t rate = *(uint32_t *)payload;
            log_info("set baud rate to %d", rate);
            radio_set_baud_rate(rate);
        } else
            log_warning("expecting 7 bytes for baud rate command, received %d", len+3);
        break;
    default:
        log_warning("unknown command received: %02X", cmd);
        break;
    }
}


///* Wait for a end of packet then decorticate an UART 8N1 */
//static uint8_t uart[0x1f+3];
//static size_t uart_length = 0;
//void parse_buffer_uart(void) {
//    /* The payload format is 8N1: 0 ........ 1, and we are misaligned,
//     *  because the sync word is FF 33 which is encoded as 0 11111111 1 0 11001100 1
//     *  and our sync word is     FF 66 which is               11111111   01100110 */
//    /* Then we also have to reverse the bitorder, because UART is LSB */
//    uart_length = 0;
//    for (size_t i=0; i/8 < i_rx; ++i) {
//        uint8_t bit = buffer_rx[(i+2)/8] >> (7-((i+2)%8)) & 0x01;  /* MSB order */
//        switch(i%10) {
//        case 0:  /* Expects the 0 in 0........1 */
//            if(bit)
//                return;
//            uart[uart_length] = 0;
//            break;
//        case 9:  /* Expects the 1 in 0........1 */
//            if(!bit)
//                return;
//            ++uart_length;
//            break;
//        default:  /* Swap bit order as UART is LSB */
//            size_t i_decode = (i%10)-1;
//            uart[uart_length] |= (bit << i_decode);
//            //printf("bit %d: %d, uart[%d]=%d (%d)\n", i,bit,uart_length,uart[uart_length],i_decode);
//            break;
//        }
//    }
//}


int main() {
    /* You can also connect to the pico through /dev/serial/by-id/usb-Raspberry_Pi_Pico_E66164084315472C-if00 */
    /* You can also connect to the pico through /dev/serial/by-id/usb-Raspberry_Pi_Pico_5044340588A7511C-if00 */

    /* All inits */
    stdio_usb_init();
    log_set_level(LOG_LEVEL_INFO);
    radio_init();
    radio_boot();
    leds_init(NULL);

    dispatcher_init();
    st_tx_ts = get_absolute_time();

    /* Push sensible defaults for when there is no USB to push new commands (test nomad)
     * (this pushes IOCFG and FIFTHR but they will overridden by config_rx) */
    //ccsend(radio_preset_gfsk_flip, NULL, radio_preset_gfsk_flip_len);

    log_info("booted, waiting for packets on USB or on air");

    /* Keep a constant reference time for the animation even though we constantly re-compute the animation,
     *  so that animation seem to deploy correctly */
    leds_anim_t led = {
        .tref = get_absolute_time(),
    };
    while(true) {
        int ch;
        absolute_time_t now = get_absolute_time();

        /* Handle USB to TX direction */
        if(st_raso == RADIO_WAIT || st_raso == RADIO_RECEIVING) {
            switch(st_tx) {
            case WAITING:
                ch = stdio_getchar_timeout_us(0);
                if(ch >= 0 && absolute_time_diff_us(st_tx_ts, now) > 50000) {
                    /* Received something and its been a while we didn't, so it's the start of a new packet */
                    st_tx = FILLING;
                    st_tx_ts = now;
                    i_tx = 1;
                    buffer_tx[0] = ch;
                    buffer_tx[1] = 0;
                } else if (ch >= 0) {
                    /* Consider we are still in the same stream, we have to wait longer between chars */
                    st_tx_ts = now;
                }
                break;
            case FILLING:
                ch = stdio_getchar_timeout_us(0);
                if(ch >= 0) {
                    st_tx_ts = now;
                    buffer_tx[i_tx++] = ch;
                    if (i_tx >= 2) {
                        /* TODO: see if clarifications in SWIM serial2swim work and report them here */
                        uint16_t *payload_len = (uint16_t *)buffer_tx;
                        if (*payload_len+2 > BUFFER_TX_LENGTH+2) {
                            log_warning("announced USB packet too large, dropping and wait for end of stream");
                            /* We just wait that the current stream stops */
                            st_raso = WAITING;  /* FIXME: st_tx??? */
                            i_tx = 0;
                            buffer_tx[0] = buffer_tx[1] = 0;
                        }
                        if (i_tx == *payload_len+2) {
                            log_info("received USB packet of %d bytes", i_tx);
                            st_tx = READY;
                        }
                    }
                /* Inactive for too long, reset buffer reception */
                } else if(absolute_time_diff_us(st_tx_ts, now) > 50000) {
                    st_tx = WAITING;
                    st_tx_ts = now;
                    /* When i_tx==0, we may be here just after a processed packet, so don't warn */
                    if (i_tx) {
                        /* This log is partially false when i_tx == 1, but in this case buffer_tx[1] == 0, so it is not that false... */
                        log_warning("dropping incomplete USB packet, expected %d, received %d before timeout", (*(uint16_t *)buffer_tx)+2, i_tx);
                        i_tx = 0;
                        buffer_tx[0] = buffer_tx[1] = 0;
                    }
                }
                break;
            case PART_READY:
                log_warning("unexpected st_tx state: PART_READY");  /* This may flood your console */
                break;
            case READY:
                /* TODO: move to a dispatcher_push_config */
                /* Acquire the critical section to prevent IRQs from changing st_raso, if not done yet */
                critical_section_enter_blocking(&cs_update_st_raso);
                /* If the radio is available (not receiving), reserve it */
                if (st_raso == RADIO_WAIT) {
                    st_raso = RADIO_CONTROL;
                }
                critical_section_exit(&cs_update_st_raso);

                /* Only process if we could reserve the radio (will configure or send a packet) */
                if (st_raso == RADIO_CONTROL) {
                    log_info("pause radio, process USB command");
                    radio_wait_state(CC1101_STATE_IDLE, true);
                    ccsend((uint8_t[]){CC1101_SFRX}, NULL, 1);  /* Also flush the RX FIFO to avoid overflows */
                    i_rx = 0;  /* If there was something in the RX buffer, also flush that */
                    led.kind = LED_OOK;
                    led.color = PINK;
                    led.period = 80000;
                    leds_set_anim(&led);
                    process_usb_command();
                    /* We can now receive the next packet hence we stay in FILLING */
                    st_tx = FILLING;
                    st_tx_ts = get_absolute_time();  /* process_ may have taken time, and stdin will be blocked for this time */
                    i_tx = 0;
                    buffer_tx[0] = buffer_tx[1] = 0;
                    /* And set the radio back to waiting */
                    ccsend(config_rx, NULL, config_rx_len);
                    st_raso = RADIO_WAIT;
                    rssi = INT16_MIN;
                    radio_wait_state(CC1101_STATE_RX, true);
                }
                break;
            }
        }

        /* Handle RX to USB direction */
        dispatcher_step();

        /* Compute color, we have priorities: radio first, then USB, then waiting (keep alive) */
        if (st_raso == RADIO_RECEIVING) {  /* FIXME: inverse all tests to avoid lhs wrongly set */
            /* If receiving with known RSSI: fixed red to green
             * If receiving but rssi unknown (waiting for SYNC): fixed white
             * -> we don't know here with global var rssi whether we are synched,
             *    because it is set in read_rx_fifo which is called *after* a packet received.
             *    So we use an intermediate (maybe) inaccurate value */
            int16_t rssi = (int8_t)ccread_status_reg(CC1101_RSSI) - 74;
            //log_info("RSSI %+ 3ddBm", rssi);  /* Varies a lot */
            if (rssi > INT16_MIN) {
                float f = rssi;
                f = (f + 80.f)/90.f;  /* -80 = RED, +10 = GREEN */
                f = fminf(1.f, fmaxf(0.f, f));
                uint8_t r = clamp2byte(cosf(f*M_PI/2.f));
                uint8_t g = clamp2byte(cosf((f-1)*M_PI/2.f));
                led.color = LED_RGB(r,g,0);
            //} else {
            //    led.color = WHITE;
            }
            led.kind = LED_FIXED;
        /* If airing something: pink -> this will also be pink when pushing commands to the radio
         *  (TODO: insert a variable to differentiate the cases, because buffer_tx[2] will soon be overwritten)
         *  Also TODO: this is synchronous for now, so we never reach here when emitting... */
        //} else if (st_raso == RADIO_CONTROL) {
        //    led.kind = LED_OOK;
        //    led.color = PINK;
        //    led.period = 80000;
        /* If receiving something on USB: OOK sky */
        } else if (st_tx != WAITING) {
            led.kind = LED_OOK;
            led.color = SKY;
            led.period = 80000;
        /* If error on USB: quick rainbow */
        } else if (st_tx == WAITING && absolute_time_diff_us(st_tx_ts, now) < 50000) {
            led.kind = LED_WHEEL;
            led.period = 80000;
        /* Otherwise waiting for USB/air: breathing blue */
        } else {
            led.kind = LED_BREATH;
            led.color = BLUE;
            led.period = 3000000;
        }
        leds_set_anim(&led);
    }
}
