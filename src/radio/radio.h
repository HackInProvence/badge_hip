/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file screen.h
 *
 * \brief Radio API:
 *
 * */

#ifndef _RADIO_H
#define _RADIO_H

#include "badge_pinout.h"

// Redefine pins while we test on Pico W because some pins are not exposed (23 for GD0 and 24,25 for SPI1)
#ifndef BADGE_SECSEA
#undef BADGE_SPI1_CSn_RADIO
#undef BADGE_SPI1_RX_MISO_RADIO_SO
#undef BADGE_RADIO_GDO2
#define BADGE_SPI1_CSn_RADIO 13
#define BADGE_SPI1_RX_MISO_RADIO_SO 28
#define BADGE_RADIO_GDO2 21
#endif


/** \brief Initialize the radio library for write operations. */
void radio_init(void);


#endif /* _RADIO_H */
