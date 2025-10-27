/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file screen.h
 *
 * \brief Buttons API: fetch and expose the button's state.
 *
 * */

#ifndef _BTNS_H
#define _BTNS_H


/** TODO doc */
void btns_init(void);

/** Returns the state of the A/B/X/Y buttons as flags of an uint8_t
 *
 * FIXME: this is an half baked solution: either we use directly gpio_get instead of this,
 *  or we would need BTNS_ flags to ease its use... */
uint8_t btns_get_state(void);

#endif /* _BTNS_H */
