/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* A big buffer shared by pages that are never open at the same time (the RAM of the RP2040 is short): the two frames
 * of the speed reader (rsvp.c), a frame of the OLED video (oled_demo.c), the measure of a text of the name tag
 * (nametag.c). A page uses it only while it is open; never keep data in it from one page to the next. */

#include "shared_buf.h"

uint8_t shared_buf[SHARED_BUF_SIZE] __attribute__((aligned(4)));
