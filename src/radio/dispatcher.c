/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file
 *
 * \brief Abstracts the radio RX/TX side and notifies on RX.
 *
 * Set up callbacks for message types and be called back on receiving these.
 * Queue messages to send. */

#include "log.h"
#include "dispatcher.h"

/* Compromise to not use too much RAM while having space to receive a little more than one screen */
#define BUFFER_TX_LENGTH 128
#define BUFFER_RX_LENGTH 8192


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


/* Global state management */
static raso_state_t st_raso = BOOT;
static critical_section_t cs_update_st_raso;  /* Protect changes of st_raso in main from interrupts */

/* TX side TODO: we need less, because this is an RX of the USB side */
static buffer_state_t st_tx = WAITING;
static absolute_time_t st_tx_ts = 0;
static uint8_t buffer_tx[BUFFER_TX_LENGTH+2];  /* First 2 bytes are the length of remainder (len(payload)+1), third byte is the command, then the payload */
static size_t i_tx = 0;  /* Index to write to buffer_tx when receiving data from USB */
static uint8_t send_mode = 1;  /* Mirrors PKTCTRL0.LENGTH_CONFIG */
/* Config requirements when sending: IOCFG2 is TX FIFO under threshold, IOCFG1 is MISO, IOCFG0 is sending ongoing, FIFOTHR is 33 */
static const uint8_t config_tx[] = {CC1101_BURST(CC1101_IOCFG2), 0x02, 0x2E, 0x06, 0x07};

/* RX side */
static buffer_state_t st_rx = WAITING;
static uint8_t buffer_rx[BUFFER_RX_LENGTH+64]; /* +64 to be able to flush the RX FIFO in cases of overflow TODO: drop with SFRX instead */
static size_t i_rx = 0;
static absolute_time_t t_rise = 0, t_fall = 0;  /* Measure reception time */
static int16_t rssi = INT16_MIN;
/* Config requirements when receiving: IOCFG2 is carrier sense, IOCFG1 is MISO, IOCFG0 is RX FIFO over threshold, FIFOTHR is 32 */
static const uint8_t config_rx[] = {CC1101_BURST(CC1101_IOCFG2), 0x0E, 0x2E, 0x00, 0x07};


/** We should not read the last byte available in the FIFO because there is a bug that would duplicate reinsert it in the FIFO,
 *   except when we know that the receive operation is finished (which is suggested to be done with packet sizes, and we do it with CS...),
 *   so incl_last=false to exclude this last byte, and true when we know the comm is finished */
void read_rx_fifo(bool incl_last) {
    //printf("read_rx_fifo(n=");
    uint8_t n;
    ccread_burst(CC1101_RXBYTES, &n, 1);
    /* bit 7 is RXFIFO_OVERFLOW, means we have let it fill */
    if (n>>7)
        log_warning("read_rx_fifo: RX overflow (did not emptied it quickly enough), i_rx=%d\n", i_rx);
    /* We should not have been called, nothing received */
    if (n==0) {
        //printf("0),");
        return;
    }
    //printf("%d,to_read=", n);
    if (! incl_last)
        --n;

    /* Receive but don't overflow our receiving buffer */
    size_t to_read = i_rx+n > BUFFER_RX_LENGTH ? BUFFER_RX_LENGTH-i_rx : n;
    //printf("%d).", to_read);
    if (to_read)
        ccread_burst(CC1101_RXFIFO, buffer_rx+i_rx, to_read);
    /* If this read would have overflown our RX buffer, we still need to drop what remains
     * TODO: can we do this with a SRFX instead? */
    if (n-to_read)
        /* FIXME: signal buffer overflow */
        ccread_burst(CC1101_RXFIFO, buffer_rx+BUFFER_RX_LENGTH, n-to_read);  /* Flushes remaining bytes to avoid RX overflow which would stall the radio */
    i_rx += to_read;

    /* Also read the RSSI because we have received the SYNC word (we have data) and it won't change now */
    rssi = (int8_t)ccread_status_reg(CC1101_RSSI) - 74;
}

/** Interrupt on GDO0/GDO2 signal start of radio RX, but we may not be in an RX state */
void radio_events(uint gpio, uint32_t events) {
    /* FIXME: this enables being able to asynch send packets, but for now, only asynch RX */
    if (st_raso != RADIO_WAIT && st_raso != RADIO_RECEIVING)
        return;

    absolute_time_t now = get_absolute_time();

    if (gpio == BADGE_RADIO_GDO0 && (events & GPIO_IRQ_EDGE_RISE)) {
        /* Note: if MDMCFG2.SYNC_MODE is not carrier-sense, we CAN receive bytes here at any time... -> should be prevented when pushing a configuration */
        st_rx = PART_READY;
    /* FIXME: this is done on both GDO0 and GDO2 */
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


void emit_buffer(void) {
    uint16_t len = *(uint16_t *)buffer_tx-1;  /* Length of the payload */
    uint8_t *payload = &buffer_tx[3];
    uint8_t pktctrl0;
    uint16_t sent = 0;  /* Keep track of how many we sent */
    bool changed_mode = false;  /* Changed from infinite to fixed length modes */

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
        log_warning("unsupported packet length mode: %d", send_mode);
        break;
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
