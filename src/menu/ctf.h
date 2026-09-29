/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file ctf.h
 *
 * \brief CTF challenges of the menu (see docs/idees_reseau_extensions_ctf.md).
 *
 * First challenge: the Konami code, typed with the 4 buttons in the "Saisir un code" page:
 * up up down down left right left right B A, with up = left flank (Y), down = right flank (X),
 * left = left wing (A), right = right wing (B), and B, A = the wings labeled B and A.
 * The flags are stored obfuscated in the firmware: finding them in a dump is another challenge.
 * */

#ifndef _CTF_H
#define _CTF_H

#include <stdbool.h>
#include <stdint.h>

#define CTF_N_FLAGS 1
#define CTF_CODE_LEN 10

/* Inputs of the code */
typedef enum {
    CTF_UP,
    CTF_DOWN,
    CTF_LEFT,
    CTF_RIGHT,
    CTF_B,
    CTF_A,
} ctf_input_t;

/** \brief Start typing a code. */
void ctf_code_start(void);

/** \brief Add a button (btn_mask of btns_get_state()) to the code being typed.
 * \return the number of inputs typed so far */
int ctf_code_press(uint8_t btn_mask);

/** \brief The code typed so far, for display (e.g. "^ ^ v v"). */
void ctf_code_text(char *buf, int len);

/** \brief After CTF_CODE_LEN inputs: whether the code unlocked a flag (then stored as found). */
bool ctf_code_check(void);

/** \brief The flag n, in clear (only once found), or NULL. */
const char *ctf_flag(int n, char *buf, int len);

int ctf_found_count(void);

#endif /* _CTF_H */
