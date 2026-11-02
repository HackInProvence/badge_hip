/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file flipper.h
 *
 * \brief The Flipper Zero theme (Réglages > Thème), a wink at the Flipper Zero: the menus drawn in its style (ui.h,
 * ui_flipper), and its "desktop": a drawing of the Flipper with the animations of its dolphin on its screen, the
 * home page of the badge (at the start, A in the main menu); the screensaver shows the dolphin asleep.
 *
 * The animations are files of the SD card (FLIPPER/ *.FZA, made by tools/flipper_anims.py from the firmware of the
 * Flipper Zero; (c) Flipper Devices Inc., GNU GPL v3): the firmware of the badge contains none of them. Played as on
 * the Flipper: the passive frames in a loop, then the active ones with a bubble of text; chosen at random among
 * those of the level of the dolphin, here the rank of the badge (Mon grade: 1-4 -> 1, 5-7 -> 2, 8-10 -> 3).
 * */

#ifndef _FLIPPER_H
#define _FLIPPER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app.h"

#define FLIPPER_DIR "FLIPPER"

/** \brief At the start, after store_init(): the style of the menus. */
void flipper_init(void);

/** \brief The Flipper Zero theme is chosen. */
bool flipper_theme(void);
void flipper_set_theme(bool on);

/** \brief The page of the screensaver in the Flipper theme (the dolphin asleep) in \p fb (1 bit, cleared by it),
 * \p scratch: a buffer of FLIPPER_SAVER_SCRATCH bytes at least. False when the theme is not chosen. */
#define FLIPPER_SAVER_SCRATCH 3000
bool flipper_saver_render(uint8_t *fb, uint8_t *scratch, size_t scratch_len);

/** \brief The desktop of the Flipper (the dolphin), the home page in the Flipper theme. */
extern const app_t app_flipper;

#endif /* _FLIPPER_H */
