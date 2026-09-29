/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file credits.h
 *
 * \brief The people and organizations behind the badge, one page each (menu Réglages > Infos > Crédits).
 * */

#ifndef _CREDITS_H
#define _CREDITS_H

#include <stdint.h>

/** \brief Number of pages. */
int credits_count(void);

/** \brief Draws the page (0..credits_count()-1) in the frame buffer, with its title and footer. */
void credits_render(uint8_t *fb, int page);

/** \brief Name on the page, e.g. for the logs. */
const char *credits_name(int page);

#endif /* _CREDITS_H */
