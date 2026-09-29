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
#include <string.h>

#include "pico/stdlib.h"
#include "pico/sync.h"

#include "log.h"
#include "leds.h"
#include "radio.h"

#define SKY LED_RGB(0, 128, 255)
#define BLUE LED_RGB(0, 0, 32)
#define PINK LED_RGB(255, 0, 255)
#define RED LED_RGB(32, 0, 0)
#define WHITE LED_RGB(64, 64, 64)

/* When changing BUFFER_TX_LENGTH, for now radio_source.py has no mean to know that and its assert on size might be wrong */
#define BUFFER_TX_LENGTH 65535
#define BUFFER_RX_LENGTH 65535


/** Buffer states: follow whether we are receiving or full */
typedef enum {
    WAITING,  /* No operation has started, everything is setup */
    FILLING,  /* Receiving bytes, but buffer is not complete */
    PART_READY,  /* Need to flush received bytes to the buffer before overflow */
    READY,  /* A packet has been fully received and is ready to be processed */
} buffer_state_t;

/** RaSo states: differentiate main states of the process */
typedef enum {
    BOOT,
    RADIO_WAIT,  /* Radio is in RX mode, but no packet is received yet */
    RADIO_RECEIVING,  /* Radio is in carrier sense mode, receiving something, we should not interrupt that */
    RADIO_CONTROL,  /* Radio is being configured or sending a packet */
} raso_state_t;

/* Global state management (volatile: shared with the GPIO IRQ) */
static volatile raso_state_t st_raso = BOOT;
static critical_section_t cs_update_st_raso;  /* Protect changes of stradio to be interrupted */

/* TX side */
static buffer_state_t st_tx = WAITING;
static absolute_time_t st_tx_ts = 0;
static uint8_t buffer_tx[BUFFER_TX_LENGTH+2];  /* First 2 bytes are the length of remainder (len(payload)+1), third byte is the command, then the payload */
static size_t i_tx = 0;  /* Index to write to buffer_tx when receiving data from USB */
static uint8_t send_mode = 1;  /* Mirrors PKTCTRL0.LENGTH_CONFIG */
/* Config requirements when sending: IOCFG2 is TX FIFO under threshold, IOCFG1 is MISO, IOCFG0 is sending ongoing, FIFOTHR is 33 */
static const uint8_t config_tx[] = {CC1101_BURST(CC1101_IOCFG2), 0x02, 0x2E, 0x06, 0x07};

/* RX side */
static volatile buffer_state_t st_rx = WAITING;  /* volatile: shared with the GPIO IRQ */
static uint8_t buffer_rx[BUFFER_RX_LENGTH+64]; /* +64 to be able to flush the RX FIFO in cases of overflow */
static size_t i_rx = 0;
static absolute_time_t t_rise = 0, t_fall = 0;  /* Measure reception time */
static int16_t rssi = INT16_MIN;
/* Config requirements when receiving: IOCFG2 is carrier sense, IOCFG1 is MISO, IOCFG0 is RX FIFO over threshold, FIFOTHR is 32 */
static const uint8_t config_rx[] = {CC1101_BURST(CC1101_IOCFG2), 0x0E, 0x2E, 0x00, 0x07};


/* Assumes that it is called with the radio in the control state */
void emit_buffer(void); /* Split the packet emission from process_packet */

/* Assumes that it is called with a complete buffer_tx and a radio in the control state */
void process_packet(void) {
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
        for (size_t i=0; i+1<len; i+=2) {  /* i<len-1 would overflow with len == 0 */
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
            uint32_t freq;
            memcpy(&freq, payload, sizeof(freq));  /* payload is not aligned: a direct uint32_t read HardFaults on Cortex-M0+ */
            log_info("set frequency to %lu", (unsigned long)freq);
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
            uint32_t rate;
            memcpy(&rate, payload, sizeof(rate));  /* payload is not aligned: a direct uint32_t read HardFaults on Cortex-M0+ */
            log_info("set baud rate to %lu", (unsigned long)rate);
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

    if (len == 0 && send_mode != 1) {
        /* Would underflow sent = len-1 below and send 64k of garbage
         * (in variable length mode, the length byte is always sent so len >= 1) */
        log_warning("empty packet, nothing to send");
        return;
    }

    /* The radio should already be in the IDLE state */
    radio_wait_state(CC1101_STATE_IDLE, true);

    /* Asserts our config requirements for sending data (mainly GDOx signals) */
    ccsend(config_tx, NULL, sizeof(config_tx));

    /* Save the current value of pktctrl0 to be able to change the length mode on the fly */
    ccread_burst(CC1101_PKTCTRL0, &pktctrl0, 1);
    //log_info("pktctrl0 %02x", pktctrl0);
    pktctrl0 &= 0x7C;  /* send_mode is the last 2 bits, first bit is unused */

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
        /* Move payload base to 1 to the left, to be able to write the length (overwrites buffer_tx[2] which is the command) */
        --payload;
        payload[0] = len & 0xFF;  /* First payload byte is reserved for length, and this should be anticipated by the sender */
        changed_mode = true;  /* Prevent change mode to fixed length -> keep variable length mode */
        ++len;  /* We now have to send the length byte too */
        //log_info("variable length mode, len adjusted %d", len);
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
        log_warning("unsupported packet length mode: %d (send nothing)", send_mode);
        return;
    }

    /* Flush then fill the FIFO with some data before putting the radio in TX mode */
    /* payload starts on buffer_tx[3] or [2] so we prefix it with command for the radio then burst send it */
    /* The starting point depends on length mode (variable length -> buffer_tx[2] is the first byte, otherwise buffer_tx[3]) */
    *(payload-2) = CC1101_SFTX;  /* Flush the TX FIFO to be sure that OUR message is sent */
    *(payload-1) = CC1101_BURST(CC1101_TXFIFO);
    /* Pre-fill the buffer_tx with some data */
    sent = len > 32 ? 32 : len-1;  /* len-1 to be sure to enter the while loop and go to fixed length mode */
    ccsend(payload-2, NULL, sent+2);
    //log_info("wrote %02x %02x %02x %02x... sent %d", *(payload-2), *(payload-1), payload[0], payload[1], sent);

    /* Start sending what's in the TX FIFO and continue until the packet is completely sent */
    radio_wait_state(CC1101_STATE_TX, true);

    /* Wait for GDO0 to be asserted, meaning we sent the preamble/syncword */
    while(! gpio_get(BADGE_RADIO_GDO0))
        tight_loop_contents();

    /* Now split into pieces that won't overflow the TX FIFO */
    while(sent < len) {
        /* Change the length mode if needed */
        /* Note: I understood from the spec that we have to change the mode when len-sent < 255,
         *  but it seems that we have to wait for sent%256 <= len%256 on the last slice (when sent/256 == len/256) */
        /* This also works for packet length < 256, even if sent < 32,
         *  and set changed_mode=true for variable length mode to avoid changing mode */
        //if ((len>>8) == (sent>>8) && !changed_mode) {
        if (len-sent < 32 && !changed_mode) {
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
    log_info("packet aired, send %d bytes", sent);

    len = ccread_status_reg(CC1101_TXBYTES);
    if (len&0x80)
        // FIXME: go to IDLE
        log_warning("TX underflow -> \"panic\"");
    len &= 0x7F;
    if (len)
        log_warning("remaining %d bytes in TXFIFO", len);

    /* We should be IDLE here, because either variable packet or fixed packet length made the radio disable itself */
}


void read_rx_fifo(bool incl_last) {
    /* We should not read more than n-1 bytes because there is a bug that would duplicate the last byte,
     *  except when we know that the receive operation is finished (which is suggested to be done with packet sizes, and we do it with CS...) */
    //printf("read_rx_fifo(n=");
    uint8_t n;
    ccread_burst(CC1101_RXBYTES, &n, 1);
    if (n>>7)
        panic("RX overflow from read_rx_fifo, i_rx=%d\n", i_rx);
    if (n==0) {
        //printf("0),");
        return;
    }
    //printf("%d,to_read=", n);
    if (! incl_last)
        --n;
    size_t to_read = i_rx+n > BUFFER_RX_LENGTH ? BUFFER_RX_LENGTH-i_rx : n;
    //printf("%d).", to_read);
    if (to_read)
        ccread_burst(CC1101_RXFIFO, buffer_rx+i_rx, to_read);
    if (n-to_read)
        /* FIXME: signal buffer overflow */
        ccread_burst(CC1101_RXFIFO, buffer_rx+BUFFER_RX_LENGTH, n-to_read);  /* Flushes remaining bytes to avoid RX overflow which would stall the radio */
    i_rx += to_read;

    /* Also read the RSSI because we have received the SYNC word (we have data) and it won't change now */
    rssi = (int8_t)ccread_status_reg(CC1101_RSSI)/2 - 74;  /* RSSI_dBm = RSSI_dec/2 - RSSI_offset */
}

/* Use IRQs to signal start of radio RX, but we may not be in an RX state */
void radio_events(uint gpio, uint32_t events) {
    /* FIXME: this enables being able to asynch send packets, but for now, only asynch RX */
    if (st_raso != RADIO_WAIT && st_raso != RADIO_RECEIVING)
        return;

    absolute_time_t now = get_absolute_time();

    if (gpio == BADGE_RADIO_GDO0 && (events & GPIO_IRQ_EDGE_RISE)) {
        /* Note: if MDMCFG2.SYNC_MODE is not carrier-sense, we CAN receive bytes here at any time... -> should be prevented when pushing a configuration */
        st_rx = PART_READY;
    } else if ((events & GPIO_IRQ_EDGE_RISE) && (st_raso == RADIO_WAIT)) {  /* Don't start receiving if we did not handle the previous packet first */
        t_rise = now;
        st_rx = FILLING;
        st_raso = RADIO_RECEIVING;  /* Atomic enough because no other code modifying this can interrupt this IRQ */
    } else if (events & GPIO_IRQ_EDGE_FALL) {
        t_fall = now;
        st_rx = READY;
        /* Don't change st_raso now, wait for the main loop to retrieve data beforehand, and let it handle reset */
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
    critical_section_init(&cs_update_st_raso);

    /* The radio should already be in the IDLE state,
     *  but pushing the reset button does not reset its state */
    radio_wait_state(CC1101_STATE_IDLE, true);
    ccsend((uint8_t[]){CC1101_SFRX}, NULL, 1);

    /* Push sensible defaults for when there is no USB to push new commands (test nomad)
     * (this pushes IOCFG and FIFTHR but they will overridden by config_rx) */
    ccsend(radio_preset_gfsk, NULL, radio_preset_gfsk_len);

    /* Initialize radio to receive */
    ccsend(config_rx, NULL, sizeof(config_rx));
    radio_set_frequency(433920000);
    radio_set_baud_rate(9999);

    st_raso = RADIO_WAIT;  /* Can't be interrupted: no IRQ yet */
    st_tx_ts = get_absolute_time();

    /* Setup an IRQ to watch carrier sense.
     * It's hard to receive packets of unknown lengths with the CC1101,
     *  as the reception correctly starts with CS being high, but it does not end with CS being low again...
     * We need to go back to IDLE when reception is done */
    gpio_set_irq_callback(radio_events);  /* There is a single callback for all GPIO events */
    gpio_set_irq_enabled(BADGE_RADIO_GDO0, GPIO_IRQ_EDGE_RISE                     , true);
    gpio_set_irq_enabled(BADGE_RADIO_GDO2, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    irq_set_enabled(IO_IRQ_BANK0, true);
    radio_wait_state(CC1101_STATE_RX, true);

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
                        if (*payload_len+2 > sizeof(buffer_tx)) {
                            log_warning("announced USB packet too large, dropping and wait for end of stream");
                            /* We just wait that the current stream stops (WAITING ignores chars until 50ms of silence).
                             * Note: this is st_tx, setting st_raso to WAITING (== BOOT) would block the whole loop */
                            st_tx = WAITING;
                            i_tx = 0;
                            buffer_tx[0] = buffer_tx[1] = 0;
                        } else if (i_tx == *payload_len+2) {
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
                    process_packet();
                    /* We can now receive the next packet hence we stay in FILLING */
                    st_tx = FILLING;
                    st_tx_ts = get_absolute_time();  /* process_ may have taken time, and stdin will be blocked for this time */
                    i_tx = 0;
                    buffer_tx[0] = buffer_tx[1] = 0;
                    /* And set the radio back to waiting */
                    ccsend(config_rx, NULL, sizeof(config_rx));
                    st_raso = RADIO_WAIT;
                    rssi = INT16_MIN;
                    radio_wait_state(CC1101_STATE_RX, true);
                }
                break;
            }
        }

        /* Handle RX to USB direction */
        /* Note: if you want fail-safes and debug code, see tests/radio.c on the parent commit of this one */
        if(st_raso == RADIO_WAIT || st_raso == RADIO_RECEIVING) {
            switch(st_rx) {
            case WAITING:
                /* Probably here with RADIO_WAIT, in which case it's ok */
                break;
            case FILLING:
                break;
            case PART_READY:
                /* FIFO threshold reached, do fetch incoming data */
                read_rx_fifo(false);
                break;
            case READY:
                /* Carrier sense went down, we finished receiving a packet */
                /* When using CS, the reception of bytes continues after CS is cleared...
                 * To avoid that, we reset to IDLE then RX */
                /* We may be too slow, as the next transmission may have started while we are pushing our payload on USB
                 *  and recalibrating the radio */
                radio_wait_state(CC1101_STATE_IDLE, true);
                /* RX termination based on CS seems to only works when nothing was received yet... */
                //ccsend((uint8_t[]){CC1101_SFRX, CC1101_MCSM2, 0x07}, NULL, 3);

                /* There may be still data for us in the RX FIFO */
                read_rx_fifo(true);

                int8_t eoff = ccread_status_reg(CC1101_FREQEST);
                uint8_t lqi = ccread_status_reg(CC1101_LQI) & 0x7F;  /* Discard CRC OK */
                printf("new packet, RSSI %+ 3ddBm, LQI % 3d, est. freq. % 7lli Hz, len % 4d, in % 7.02f ms,\ncontent: ",
                        rssi, lqi, ((int64_t)(eoff)*CC1101_fXOSC)>>14, i_rx, absolute_time_diff_us(t_rise, t_fall)/1000.f);
                for (size_t i=0; i<i_rx; ++i)
                    printf("%02x ", buffer_rx[i]);
                printf("\n");

                /* Reset buffer state */
                i_rx = 0;
                st_rx = WAITING;
                rssi = INT16_MIN;

                //ccsend((uint8_t[]){CC1101_SFRX}, NULL, 1);  /* We emptied the RX FIFO beforehand */
                radio_wait_state(CC1101_STATE_RX, true);
                /* Indicate to the IRQ that we are now ready to re-receive */
                critical_section_enter_blocking(&cs_update_st_raso);
                st_raso = RADIO_WAIT;
                critical_section_exit(&cs_update_st_raso);
                break;
            }

        }

        /* Compute color, we have priorities: radio first, then USB, then waiting (keep alive) */
        if (st_raso == RADIO_RECEIVING) {  /* FIXME: inverse all tests to avoid lhs wrongly set */
            /* If receiving with known RSSI: fixed red to green */
            if (rssi > INT16_MIN) {
                float f = rssi;
                f = (f + 80.f)/90.f;  /* -80 = RED, +10 = GREEN */
                f = fminf(1.f, fmaxf(0.f, f));
                uint8_t r = clamp2byte(cosf(f*M_PI/2.f));
                uint8_t g = clamp2byte(cosf((f-1)*M_PI/2.f));
                led.color = LED_RGB(r,g,0);
            /* If receiving but rssi unknown (waiting for SYNC): fixed white */
            } else {
                led.color = WHITE;
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
