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
 * - green to red: RSSI from best to worst.
 *
 * The CC1101 has its own states, but we also need a state machine to handle data on USB tty. */

#include <stdio.h>

#include "pico/stdlib.h"

#include "log.h"
#include "leds.h"
#include "radio.h"

#define SKY LED_RGB(0, 128, 255)
#define BLUE LED_RGB(0, 0, 32)
#define PINK LED_RGB(255, 0, 255)

/* When changing BUFFER_TX_LENGTH, for now radio_source.py has no mean to know that and its assert on size might be wrong */
#define BUFFER_TX_LENGTH 65535


/** RaSo states: focused on the state of the USB link */
typedef enum {
    BOOT,
    USB_IN_WAIT,
    USB_IN_VALID,
    RADIO_TX,
} raso_state_t;

/* Global state management */
static raso_state_t straso = BOOT;
static radio_state_t stradio = CC1101_STATE_IDLE;
static absolute_time_t straso_ts = 0;

/* TX side */
static uint8_t buffer_tx[BUFFER_TX_LENGTH+2];  /* First 2 bytes are the length of remainder (len(payload)+1), third byte is the command, then the payload */
static size_t i_tx = 0;  /* Index to write to buffer_tx when receiving data from USB */
static uint8_t send_mode = 1;  /* Mirrors PKTCTRL0.LENGTH_CONFIG */
/* Config requirements when sending: IOCFG2 is TX FIFO under threshold, IOCFG1 is MISO, IOCFG0 is sending ongoing, FIFOTHR is 33 */
static const uint8_t config_tx[] = {CC1101_BURST(CC1101_IOCFG2), 0x02, 0x2E, 0x06, 0x07};


void emit_buffer(void); /* Split the packet emission from process_packet */
void process_packet(void) {
    /* The sender (our stdin) can wait for us to finish our task */
    if (*(uint16_t *)buffer_tx < 1)  /* We should be able to at least decode the command */
        return;
    uint16_t len = *(uint16_t *)buffer_tx-1;  /* Length of the payload */
    uint8_t cmd = buffer_tx[2];
    uint8_t *payload = &buffer_tx[3];

    switch(cmd) {
    case 0xD0:  /* Choose commands that cannot be written easily with a keyboard in minicom... */
        /* Pass config */
        log_info("write %d registers", len/2);
        ccsend(payload, NULL, len);
        /* Detect the length mode in the pushed config */
        for (size_t i=0; i<len-1; i+=2) {
            if (payload[i] != CC1101_PKTCTRL0)
                continue;
            uint8_t new_mode = payload[i+1] & 0x03;
            if (new_mode == 0) {
                /* Simplify our control logic to not support FIXED length (it is a transient submode of INFINITE) */
                log_info("changed packet length mode from FIXED to INFINITE");
                new_mode = 2;
            }
            if (new_mode != send_mode) {
                log_info("set packet length mode: %d", new_mode);
                send_mode = new_mode;
            }
        }
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


void emit_buffer(void) {
    uint16_t len = *(uint16_t *)buffer_tx-1;  /* Length of the payload */
    uint8_t *payload = &buffer_tx[3];
    uint8_t pktctrl0;
    uint16_t sent = 0;  /* Keep track of how many we sent */
    bool changed_mode = false;  /* Changed from infinite to fixed length modes */

    /* TODO: lock a mutex to wait for a receive to be completed */

    /* We expect stradio == IDLE here */

    /* Asserts our config requirements for sending data (mainly GDOx signals) */
    ccsend(config_tx, NULL, sizeof(config_tx));

    /* Save the current value of pktctrl0 to be able to change the length mode on the fly */
    ccread_burst(CC1101_PKTCTRL0, &pktctrl0, 1);
    pktctrl0 &= 0x7C;  /* send_mode is the remaining 2 bits */

    /* Point of no return: reuse the buffer to write registers */
    switch(send_mode) {
    case 0:  /* Fixed length, merged with infinite length mode */
        log_warning("fixed length mode used, should not be possible (send nothing)");
        return;
    case 1:  /* Variable length: the first byte in the FIFO must be the length of the rest of the payload (255 max) */
        if (len > 255) {
            log_warning("cannot send more than 255 bytes in send_mode=variable length");
            return;
        }
        payload[0] = len & 0xFF;  /* First payload byte is reserved for length, and this should be anticipated by the sender */
        changed_mode = true;  /* Prevent change mode to fixed length -> keep variable length mode */
        break;
    case 2:  /* Infinite length mode: we have to set PKTLEN to length%256 and switch to fixed length at the right time */
        buffer_tx[0] = CC1101_PKTLEN;
        buffer_tx[1] = len&0xFF;
        ccsend(buffer_tx, NULL, 2);
        /* Prepare infinite mode in all cases, the code will switch to fixed length when needed */
        buffer_tx[0] = CC1101_PKTCTRL0;
        buffer_tx[1] = pktctrl0 | 2;  /* mode 2 == infinite length */
        ccsend(buffer_tx, NULL, 2);
        break;
    default:
        log_warning("unsupported packet length mode: %d", send_mode);
        break;
    }

    /* Flush then fill the FIFO with some data before putting the radio in TX mode */
    /* payload starts on buffer_tx[3] so we prefix it with command for the radio then burst send it */
    buffer_tx[1] = CC1101_SFTX;  /* Flush the TX FIFO to be sure that OUR message is sent */
    buffer_tx[2] = CC1101_BURST(CC1101_TXFIFO);
    /* Pre-fill the buffer_tx with some data */
    sent = len > 32 ? 32 : len;
    ccsend(&buffer_tx[1], NULL, sent+2);

    stradio = CC1101_STATE_TX;
    radio_wait_state(stradio, true);

    /* Now split into pieces that won't overflow the TX FIFO */
    while(sent < len) {
        /* Change the length mode if needed */
        /* Note: I understood from the spec that we have to change the mode when len-sent < 255,
         *  but it seems that we have to wait for sent%256 <= len%256 on the last slice (when sent/256 == len/256) */
        /* This also works for packet length < 256, even if sent < 32,
         *  and set changed_mode=true for variable length mode to avoid changing mode */
        if ((len>>8) == (sent>>8) && !changed_mode) {
            changed_mode = true;
            buffer_tx[0] = CC1101_PKTCTRL0;
            buffer_tx[1] = pktctrl0 | 0;  /* mode 0 == fixed length ; PKTLEN has already been set */
            ccsend(buffer_tx, NULL, 2);
            log_info("changed mode to fixed length");
        }

        /* Fill the TX FIFO if under threshold */
        /* FIXME: we could be async on this to continue receiving data on stdin while we send this */
        if(! gpio_get(BADGE_RADIO_GDO2)) {
            buffer_tx[0] = CC1101_TXFIFO;
            buffer_tx[1] = payload[sent];
            ccsend(buffer_tx, NULL, 2);
            ++sent;
            //printf("%d, ", sent);
        }
    }

    /* Wait for GD0 to go low (packet has been sent) */
    while(gpio_get(BADGE_RADIO_GDO0))
        tight_loop_contents();
    log_info("packet aired");

}


int main() {
    /* You can also connect to the pico through /dev/serial/by-id/usb-Raspberry_Pi_Pico_E66164084315472C-if00 */
    /* You can also connect to the pico through /dev/serial/by-id/usb-Raspberry_Pi_Pico_5044340588A7511C-if00 */
    while(true) {
        int ch;
        absolute_time_t now = get_absolute_time();
        switch(straso) {
        case BOOT:
            /* All inits */
            stdio_usb_init();
            log_set_level(LOG_LEVEL_INFO);
            radio_init();
            radio_boot();
            leds_init(NULL);

            /* State machine */
            straso = USB_IN_WAIT;
            straso_ts = now;
            leds_anim_breath(BLUE, 3000000);

            log_info("booted, waiting for packets on USB or on air");
            break;
        case USB_IN_WAIT:
            ch = stdio_getchar_timeout_us(0);
            /* Prepare to receive something, only when we are here since a long time
             *  (otherwise we are waiting for an invalid stream to stop) */
            if(ch >= 0 && absolute_time_diff_us(straso_ts, now) > 50000) {
                straso = USB_IN_VALID;
                straso_ts = now;
                leds_anim_ook(SKY, 80000);
                i_tx = 1;
                buffer_tx[0] = ch;
                buffer_tx[1] = 0;
            } else {
                /* Stay in wait */
            }
            break;
        case USB_IN_VALID:
            ch = stdio_getchar_timeout_us(0);
            if(ch >= 0) {
                straso_ts = now;
                buffer_tx[i_tx] = ch;
                ++i_tx;
                if (i_tx >= 2) {
                    uint16_t *payload_len = (uint16_t *)buffer_tx;
                    if (*payload_len+2 > sizeof(buffer_tx)) {
                        log_warning("announced packet too large, dropping and wait for end of stream");
                        /* We just wait that the current stream stops */
                        straso = USB_IN_WAIT;
                        leds_anim_breath(BLUE, 3000000);
                        i_tx = 0;
                        buffer_tx[0] = buffer_tx[1] = 0;
                    }
                    if (i_tx == *payload_len+2) {
                        log_info("process packet of %d bytes", i_tx);
                        straso = RADIO_TX;
                        leds_anim_ook(PINK, 80000);
                        process_packet();
                        straso = USB_IN_VALID;
                        straso_ts = get_absolute_time();  /* process_ may have taken time */
                        leds_anim_ook(SKY, 80000);
                        i_tx = 0;
                        buffer_tx[0] = buffer_tx[1] = 0;
                        /* We can now receive the next packet and stay in USB_IN_VALID */
                    }
                }
            } else if(absolute_time_diff_us(straso_ts, now) > 50000) {
                straso = USB_IN_WAIT;
                straso_ts = now;
                leds_anim_breath(BLUE, 3000000);
                /* When i_tx==0, we are here just after a processed packet, so don't warn */
                if (i_tx) {
                    /* This log is partially false when i_tx == 1, but in this case buffer_tx[1] == 0, so it is not that false... */
                    log_warning("dropping incomplete packet, expected %d, received %d before timeout", (*(uint16_t *)buffer_tx)+2, i_tx);
                    i_tx = 0;
                    buffer_tx[0] = buffer_tx[1] = 0;
                }
            }
            break;
        case RADIO_TX:
            /* For now, packet processing is not asynch, so this state is transient */
            break;
        }
    }
}
