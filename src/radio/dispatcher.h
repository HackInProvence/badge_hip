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


#ifndef _DISPATCHER_H
#define _DISPATCHER_H


/** Radio should be initialized and woken up */
void dispatcher_init(void);

/** Call this regularly to process incoming messages */
void dispatcher_step();


/** Internal type: buffer states: follow whether we are receiving or full */
typedef enum {
    WAITING,  /* No operation has started, everything is setup */
    FILLING,  /* Receiving bytes, but buffer is not complete */
    PART_READY,  /* Need to flush received bytes to the buffer before overflow */
    READY,  /* A packet has been fully received and is ready to be processed */
} buffer_state_t;

/** Internal type RaSo states: differentiate main states of the process */
typedef enum {
    BOOT,
    RADIO_WAIT,  /* Radio is in RX mode, but no packet is received yet */
    RADIO_RECEIVING,  /* Radio is in carrier sense mode, receiving something, we should not interrupt that */
    RADIO_CONTROL,  /* Radio is being configured or sending a packet */
} raso_state_t;

/** Internal
 * When changing BUFFER_TX_LENGTH, for now radio_source.py has no mean to know that and its assert on size might be wrong */
#define BUFFER_TX_LENGTH 65535


#endif /* _DISPATCHER_H */
