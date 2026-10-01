/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file skills.h
 *
 * \brief The skills of the participant ("Compétences": électronique, Flipper Zero, Android...), checked in
 * Social > Compétences. They are sent in the beacon (bit n = skill n): the cicadas around that share a skill are
 * listed, and announced when they come close; they travel in the contact cards (vCard property CATEGORIES) and
 * their pictograms are on the name tag.
 * */

#ifndef _SKILLS_H
#define _SKILLS_H

#include <stdint.h>

#include "gfx.h"
#include "skills_icons.h"

#define SKILLS_COUNT SKILLS_ICONS
#define SKILLS_ICON_SIZE 16

const char *skills_name(int i);

/** \brief The pictogram of skill \p i at (\p x, \p y), black pixels in \p color. */
void skills_draw_icon(uint8_t *fb, int x, int y, int i, gfx_color_t color);

/** \brief The pictograms of the skills of \p mask in a row centered on \p cx, at most \p max. \return how many */
int skills_draw_row(uint8_t *fb, int cx, int y, uint32_t mask, int max, gfx_color_t color);

/** \brief "Électronique,Flipper Zero" (the value of the vCard property CATEGORIES). */
void skills_to_text(uint32_t mask, char *buf, int len);

/** \brief The skills named in a CATEGORIES value (unknown names ignored, case and accents of ASCII letters ignored). */
uint32_t skills_from_text(const char *text);

int skills_count(uint32_t mask);

#endif /* _SKILLS_H */
