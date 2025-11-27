/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdio.h>

#include "pico/stdlib.h"

#include "log.h"
#include "leds.h"
#include "radio.h"

#define ORANGE LED_RGB(255, 64, 0)
#define GREEN LED_RGB(0, 16, 0)

typedef enum {
    BOOT,
    RECEIVING,
    WAITING,
    WAITING_LONG,
} state_t;

static state_t state = BOOT;
static absolute_time_t state_ts = 0;
static uint8_t buffer[66];  /* First byte is the length (including the first byte), second byte is command, then the payload */
static size_t i_buffer = 0;
static bool valid_buffer = false;
static uint8_t send_mode = 1;  /* Corresponds to PKTCTRL0.LENGTH_CONFIG */


void process_packet(void) {
    /* The sender (our stdin) can wait for us to finish our task */
    uint8_t len = buffer[0];
    uint8_t cmd = buffer[1];
    uint8_t *payload = &buffer[2];

    switch(cmd) {
    case 0xD0:  /* Choose commands that cannot be written easily with a keyboard in minicom... */
        /* Pass config */
        log_info("write given registers");
        ccsend(payload, NULL, len-2);
        for (size_t i=0; i<len-3; i+=2) {
            if (payload[i] != CC1101_PKTCTRL0)
                continue;
            uint8_t new_mode = payload[i+1] & 0x03;
            if (new_mode != send_mode) {
                log_info("change packet length mode to %d", new_mode);
                send_mode = new_mode;
            }
        }
        break;
    case 0xD1:
        /* Set frequency */
        uint32_t freq = *(uint32_t *)payload;
        log_info("set frequency to %d", freq);
        radio_set_frequency(freq);
        break;
    case 0xD2:
        /* Pass packet then send */
        switch(send_mode) {
        case 0:  /* Fixed length, write length to PKTLEN, then write the packet */
            buffer[0] = CC1101_PKTLEN;
            buffer[1] = len-2;
            ccsend(buffer, NULL, 2);
            break;
        case 1:  /* Variable length: the first byte in the FIFO must be the length of the rest of the payload (63 max) */
            buffer[2] = len-3;
            break;
        default:
            log_warning("unsupported packet length mode: %d", send_mode);
            break;
        }
        buffer[0] = CC1101_SFTX;  /* Flush the TX FIFO to be sure that OUR message is sent */
        buffer[1] = CC1101_BURST(CC1101_TXFIFO);
        ccsend(buffer, NULL, len);
        radio_wait_state(CC1101_STATE_TX, true);

        /* FIXME: this expects that GDO0 is correctly set up */
        /* Wait for GD0 to go high (preamble+sync has been sent) */
        /* FIXME: we could be async on this to continue receiving data on UART while we send this */
        while(! gpio_get(BADGE_RADIO_GDO0))  /* FIXME: timeout */
            tight_loop_contents();

        /* Wait for GD0 to go low (packet has been sent) */
        while(gpio_get(BADGE_RADIO_GDO0))
            tight_loop_contents();
        break;
    case 0xD3:
        /* Set baud rate */
        uint32_t rate = *(uint32_t *)payload;
        log_info("set baud rate to %d", rate);
        radio_set_baud_rate(rate);
        break;
    default:
        log_warning("unknown command received: %02X", cmd);
        break;
    }
}


int main() {
    stdio_usb_init();
    log_set_level(LOG_LEVEL_INFO);
    radio_init();
    radio_boot();
    leds_init(NULL);
    leds_anim_fixed(GREEN);

    /* First version will be an hex echoer which blinks */
    /* You can also connect to the pico through /dev/serial/by-id/usb-Raspberry_Pi_Pico_E66164084315472C-if00 */
    /* You can also connect to the pico through /dev/serial/by-id/usb-Raspberry_Pi_Pico_5044340588A7511C-if00 */
    state = RECEIVING;
    state_ts = get_absolute_time();
    while(true) {
        int ch = stdio_getchar_timeout_us(50000);
        absolute_time_t now = get_absolute_time();
        if (ch >= 0) {
            if (state != RECEIVING) {
                state = RECEIVING;
                state_ts = now;
                leds_anim_ook(ORANGE, 80000);
                i_buffer = 0;
                valid_buffer = true;
            }
            if (valid_buffer) {
                buffer[i_buffer] = ch;
                ++i_buffer;
                if (buffer[0] > sizeof(buffer)) {
                    log_warning("invalid size packet announced, dropping and wait for end of stream");
                    valid_buffer = false;  /* We just wait that the current stream stops */
                    i_buffer = 0;
                }
                if (valid_buffer && i_buffer == buffer[0]) {
                    log_info("received %d bytes", i_buffer);
                    process_packet();
                    i_buffer = 0;
                }
            }
            //printf("%02X", ch);
            //sleep_ms(1000);  /* Is there a buffer? What happens? The sender waits */
        } else {
            if (state == RECEIVING && absolute_time_diff_us(state_ts, now) > 50000) {
                state = WAITING;
                state_ts = now;
                leds_anim_fixed(0);
                if (valid_buffer && i_buffer) {
                    log_warning("dropping incomplete packet, expected %d, received %d before timeout", buffer[0], i_buffer);
                    valid_buffer = false;
                    i_buffer = 0;
                }
            } else if (state == WAITING && absolute_time_diff_us(state_ts, now) > 1000000) {
                state = WAITING_LONG;
                state_ts = now;
                leds_anim_fixed(GREEN);
            }
        }
    }
}
