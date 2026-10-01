/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file werewolf_cards.h
 *
 * \brief The cards of the loup-garou (the help of the game and the role of the player): an original 1-bit
 * illustration (tools/werewolf_icons.py), the name, the camp and the powers of each role, in our own words.
 * */

#ifndef _WEREWOLF_CARDS_H
#define _WEREWOLF_CARDS_H

#include <stdint.h>

#include "werewolf_logic.h"

#define WW_ICON_SIZE 32  /* Pixels, drawn twice as big */
#define WW_ICON_BYTES (WW_ICON_SIZE * WW_ICON_SIZE / 8)

/* The cards: the roles (ww_role_t), then the captain and the lovers */
enum { WW_CARD_CAPTAIN = WW_ROLES, WW_CARD_LOVERS, WW_CARDS };

typedef struct {
    const char *name;
    const char *camp;
    const char *power;  /* Under the illustration: 3 lines at most */
    const char *details;  /* The second page: 7 lines at most */
    const uint8_t *icon;  /* WW_ICON_BYTES: 4 bytes per row, bit 7 = the leftmost pixel, 1 = black */
} ww_card_t;

extern const ww_card_t WW_CARD[WW_CARDS];

#endif /* _WEREWOLF_CARDS_H */
