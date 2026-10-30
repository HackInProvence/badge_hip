/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file webremote.h
 *
 * \brief The remote control from a phone (Badge > Télécommande): the ESP32-CAM module is a Wi-Fi access point (or
 * joins the network saved in it) and serves a web page that shows the screen of the badge and has its 4 buttons
 * and a text field. Once on, it stays on in the background: the badge sends its screen each time it changes, the
 * buttons of the page come as the keys of the USB serial port (only the buttons and the text: no SD card, no
 * reboot, no admin mode from the page).
 *
 * The same page updates the firmware of the module from the SD card of the badge (WEBREMOTE_OTA_FILE). */

#ifndef _WEBREMOTE_H
#define _WEBREMOTE_H

#include <stdbool.h>

#include "pico/time.h"

#define WEBREMOTE_OTA_FILE "ESP32/ESP32CAM.BIN"  /* On the SD card: the firmware of the module (esp32cam/) */

/** \brief The service, at each loop (after camport_task()): the screen sent, the keys received. */
void webremote_task(absolute_time_t now);

/** \brief The next key of the page for the buttons of the badge ('a', 'A', 'b'...), -1: none. */
int webremote_key(void);

bool webremote_on(void);

#endif /* _WEBREMOTE_H */
