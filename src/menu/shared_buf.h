/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file shared_buf.h
 *
 * \brief A big buffer shared by pages never open at the same time (shared_buf.c): only while the page is open. */

#ifndef _SHARED_BUF_H
#define _SHARED_BUF_H

#include <stdint.h>

#define SHARED_BUF_SIZE 10000  /* Two frames of the screen (2 x GFX_FB_SIZE) */

extern uint8_t shared_buf[SHARED_BUF_SIZE];

#endif /* _SHARED_BUF_H */
