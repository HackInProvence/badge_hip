/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file camport.h
 *
 * \brief The link with the ESP32-CAM module (src/shared/camlink/camlink.h), shared by the camera (camera.c) and the
 * remote control (webremote.c): the search of the port where the module is plugged, the UART read by an interrupt,
 * the commands and the data sent again until acknowledged, the data received acknowledged once handled.
 *
 * The link is open while a user wants it (camport_want()). The receiver takes the long packets of the camera (the
 * viewfinder, the files) only with the big buffer given by the camera page (camport_big_buffer()); otherwise a small
 * one, enough for the remote control (its keys). */

#ifndef _CAMPORT_H
#define _CAMPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pico/time.h"

#include "camlink.h"

typedef enum { CAMPORT_CAMERA, CAMPORT_WEB, CAMPORT_USERS } camport_user_t;

/** \brief A packet of the module for a user: the data (CL_FRAME, CL_KEY...) is acknowledged after the call, once. */
typedef void (*camport_handler_t)(const camlink_rx_t *rx, absolute_time_t now);

/** \brief \p user wants the link (on: \p handler gets its packets, \p ready is called each time the module is found)
 * or no longer (off). */
void camport_want(camport_user_t user, bool on, camport_handler_t handler, void (*ready)(void));

/** \brief The receiver in \p buf (\p cap bytes, CAMLINK_ENCODED_MAX for the long packets), NULL: the small one. */
void camport_big_buffer(uint8_t *buf, size_t cap);

/** \brief The service, at each loop: the bytes received, the search of the module, the packets sent again. */
void camport_task(absolute_time_t now);

typedef enum { CAMPORT_OFF, CAMPORT_SEARCHING, CAMPORT_READY, CAMPORT_NO_CAMERA } camport_state_t;
camport_state_t camport_state(void);
const char *camport_module(void);  /* Its name and version, "" before it answers */
int camport_proto(void);  /* The version of its protocol (CAMLINK_PROTO), 0 before it answers */
const char *camport_where(void);  /* The port where it is (or tried now) */

/** \brief A command or a piece of data, sent again until the module acknowledges it: \p small copied (at most
 * CAMPORT_SMALL bytes), then \p ext (not copied: it must stay valid until acknowledged, e.g. the screen).
 * \return false when the queue is full */
#define CAMPORT_SMALL 72
bool camport_send(uint8_t type, const void *small, size_t small_len, const uint8_t *ext, size_t ext_len);

/** \brief Packets waiting for their acknowledgement of this \p type (0: of any type). */
int camport_pending(uint8_t type);

/** \brief Sent once, no acknowledgement awaited (e.g. the viewfinder off when the page closes). */
void camport_send_once(uint8_t type, const void *payload, size_t len);

#endif /* _CAMPORT_H */
