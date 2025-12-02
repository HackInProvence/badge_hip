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


/** RaSo states: focused on the state of the USB link */
typedef enum {
    BOOT,
    USB_IN_WAIT,
    USB_IN_VALID,
    RADIO_TX,
} raso_state_t;

static raso_state_t straso = BOOT;
static radio_state_t stradio = CC1101_STATE_IDLE;
static absolute_time_t straso_ts = 0;
static uint8_t buffer[65537];  /* First 2 bytes are the length of remainder (len(payload)+1), third byte is the command, then the payload */
static size_t i_buffer = 0;
static uint8_t send_mode = 1;  /* Mirrors PKTCTRL0.LENGTH_CONFIG */


void emit_buffer(void); /* Split the packet emission from process_packet */
void process_packet(void) {
    /* The sender (our stdin) can wait for us to finish our task */
    if (*(uint16_t *)buffer < 1)  /* We should be able to at least decode the command */
        return;
    uint16_t len = *(uint16_t *)buffer-1;  /* Length of the payload */
    uint8_t cmd = buffer[2];
    uint8_t *payload = &buffer[3];

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
            if (new_mode != send_mode) {
                log_info("change packet length mode to %d", new_mode);
                send_mode = new_mode;
            }
        }
        /* Asserts our needed config was not replaced */
        ccsend((uint8_t[]){CC1101_BURST(CC1101_IOCFG2), 0x02, 0x2E, 0x06, 0x07}, NULL, 4);
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
    uint16_t len = *(uint16_t *)buffer-1;  /* Length of the payload */
    uint8_t *payload = &buffer[3];
    uint8_t pktctrl0;

    /* Save the current value of pktctrl0 to be able to change the length mode on the fly */
    ccread_burst(CC1101_PKTCTRL0, &pktctrl0, 1);
    pktctrl0 &= 0x7C;  /* send_mode is the remaining 2 bits */

    /* Point of no return: reuse the buffer to write registers */
    switch(send_mode) {
    case 0:  /* Fixed length, write length to PKTLEN, then write the packet
              * FIXME: maybe could be merged with infinite packet length mode */
        if (len > 255) {
            log_warning("cannot send more than 255 bytes in send_mode=fixed length");
            return;
        }
        buffer[0] = CC1101_PKTLEN;
        buffer[1] = len & 0xFF;
        ccsend(buffer, NULL, 2);
        break;
    case 1:  /* Variable length: the first byte in the FIFO must be the length of the rest of the payload (255 max) */
        if (len > 255) {
            log_warning("cannot send more than 255 bytes in send_mode=variable length");
            return;
        }
        payload[0] = len & 0xFF;  /* First payload byte is reserved for length, and this should be anticipated by the sender */
        break;
    case 2:  /* Infinite length mode: we have to set PKTLEN to length%256 and switch to fixed length at the right time */
        if (len < 256) {
            /* FIXME: this is because of the first fill of the TX FIFO: we should put it in fixed length when < 255 */
            log_warning("infinite length does not support this few bytes for now (%d < 256)", len);
            return;
        }
        buffer[0] = CC1101_PKTLEN;
        buffer[1] = len&0xFF;
        ccsend(buffer, NULL, 2);
        break;
    default:
        log_warning("unsupported packet length mode: %d", send_mode);
        break;
    }

    /* Flush then fill the FIFO with some data before putting the radio in TX mode */
    buffer[1] = CC1101_SFTX;  /* Flush the TX FIFO to be sure that OUR message is sent */
    buffer[2] = CC1101_BURST(CC1101_TXFIFO);  /* payload starts on buffer[3] so we prefix it with command for the radio then burst send it */
    uint16_t sent = len > 32 ? 32 : len;  /* Keep track of how many we sent */
    ccsend(&buffer[1], NULL, sent+2);

    radio_wait_state(CC1101_STATE_TX, true);

    /* Now split into pieces that won't overflow the TX FIFO */
    bool changed_mode = false;
    while(sent < len) {
        /* Change the length mode if needed (don't forget to restore it afterwards) */
        /*if (len-sent < 255 && !changed_mode) {  FIXME: this is how I understood the spec but it does not work */
        if ((len>>8) == (sent>>8) && !changed_mode) {  /* Only change mode when reaching 0%256 */
            changed_mode = true;
            buffer[0] = CC1101_PKTCTRL0;
            buffer[1] = pktctrl0 | 0;  /* mode 0 == fixed length ; PKTLEN has already been set */
            ccsend(buffer, NULL, 2);
            //printf("changed mode\n");
        }

        /* Fill the TX FIFO if under threshold */
        /* FIXME: we could be async on this to continue receiving data on UART while we send this */
        if(! gpio_get(BADGE_RADIO_GDO2)) {
            buffer[0] = CC1101_TXFIFO;
            buffer[1] = payload[sent];
            ccsend(buffer, NULL, 2);
            ++sent;
            //printf("%d, ", sent);
        }

        ///* Wait for GD0 to go high (preamble+sync has been sent) */
        //while(! gpio_get(BADGE_RADIO_GDO0))  /* FIXME: timeout */
        //    tight_loop_contents();
        //}
    }

    /* Wait for GD0 to go low (packet has been sent) */
    while(gpio_get(BADGE_RADIO_GDO0))
        tight_loop_contents();
    log_info("packet aired\n");

    /* Restore the length mode */
    buffer[0] = CC1101_PKTCTRL0;
    buffer[1] = pktctrl0 | send_mode;
    ccsend(buffer, NULL, 2);
}


int main() {
    /* You can also connect to the pico through /dev/serial/by-id/usb-Raspberry_Pi_Pico_E66164084315472C-if00 */
    /* You can also connect to the pico through /dev/serial/by-id/usb-Raspberry_Pi_Pico_5044340588A7511C-if00 */
    while(true) {
        int ch;
        absolute_time_t now = get_absolute_time();
        switch(straso) {
        case BOOT:
            stdio_usb_init();
            log_set_level(LOG_LEVEL_INFO);
            radio_init();
            radio_boot();
            leds_init(NULL);
            leds_anim_breath(BLUE, 3000000);
            straso = USB_IN_WAIT;
            straso_ts = now;
            /* Config requirements: IOCFG2 is TX FIFO under threshold, IOCFG1 is MISO, IOCFG0 is sending ongoing, FIFOTHR is 33 */
            ccsend((uint8_t[]){CC1101_BURST(CC1101_IOCFG2), 0x02, 0x2E, 0x06, 0x07}, NULL, 4);
            log_info("booted, waiting for packets on USB");
            break;
        case USB_IN_WAIT:
            ch = stdio_getchar_timeout_us(0);
            /* Prepare to receive something, only when we are here since a long time
             *  (otherwise we are waiting for an invalid stream to stop) */
            if(ch >= 0 && absolute_time_diff_us(straso_ts, now) > 50000) {
                straso = USB_IN_VALID;
                straso_ts = now;
                leds_anim_ook(SKY, 80000);
                i_buffer = 1;
                buffer[0] = ch;
                buffer[1] = 0;
            } else {
                /* Stay in wait */
            }
            break;
        case USB_IN_VALID:
            ch = stdio_getchar_timeout_us(0);
            if(ch >= 0) {
                straso_ts = now;
                buffer[i_buffer] = ch;
                ++i_buffer;
                if (i_buffer >= 2) {
                    uint16_t *payload_len = (uint16_t *)buffer;
                    if (*payload_len+2 > sizeof(buffer)) {
                        log_warning("announced packet too large, dropping and wait for end of stream");
                        /* We just wait that the current stream stops */
                        straso = USB_IN_WAIT;
                        leds_anim_breath(BLUE, 3000000);
                        i_buffer = 0;
                        buffer[0] = buffer[1] = 0;
                    }
                    if (i_buffer == *payload_len+2) {
                        log_info("process packet of %d bytes", i_buffer);
                        straso = RADIO_TX;
                        leds_anim_ook(PINK, 80000);
                        process_packet();
                        straso = USB_IN_VALID;
                        straso_ts = get_absolute_time();  /* process_ may have taken time */
                        leds_anim_ook(SKY, 80000);
                        i_buffer = 0;
                        buffer[0] = buffer[1] = 0;
                        /* We can now receive the next packet and stay in USB_IN_VALID */
                    }
                }
            } else if(absolute_time_diff_us(straso_ts, now) > 50000) {
                straso = USB_IN_WAIT;
                straso_ts = now;
                leds_anim_breath(BLUE, 3000000);
                /* When i_buffer==0, we are here just after a processed packet, so don't warn */
                if (i_buffer) {
                    /* This log is partially false when i_buffer == 1, but in this case buffer[1] == 0, so it is not that false... */
                    log_warning("dropping incomplete packet, expected %d, received %d before timeout", (*(uint16_t *)buffer)+2, i_buffer);
                    i_buffer = 0;
                    buffer[0] = buffer[1] = 0;
                }
            }
            break;
        case RADIO_TX:
            /* For now, packet processing is not asynch, so this state is transient */
            break;
        }
    }
}
