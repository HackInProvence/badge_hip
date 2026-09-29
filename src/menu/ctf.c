/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdio.h>
#include <string.h>

#include "ctf.h"
#include "store.h"


/* Buttons flags from btns_get_state() */
#define BTN_A 0x01  /* left wing */
#define BTN_B 0x02  /* right wing */
#define BTN_X 0x04  /* right flank */
#define BTN_Y 0x08  /* left flank */

/* The Konami code, with the 4 buttons of the badge: up, down = the flanks, left, right = the wings... then B and A.
 * As left = A and right = B, the wings are pressed 6 times in a row. */
static const uint8_t KONAMI[CTF_CODE_LEN] = {BTN_Y, BTN_Y, BTN_X, BTN_X, BTN_A, BTN_B, BTN_A, BTN_B, BTN_B, BTN_A};

/* Flag 0, XORed with the key "cigale" and 7*i (nothing readable with "strings") */
static const uint8_t FLAG0[] = {
    0x30, 0x2B, 0x2A, 0x27, 0x35, 0x07, 0x32, 0x33, 0x6F, 0x30, 0x1E, 0x45, 0x06, 0x6D, 0x66, 0x39, 0x7B, 0x26, 0x71,
    0xDF, 0xB4, 0x87, 0x86, 0x9B, 0xBE, 0xB6, 0x8E, 0xB8, 0x98, 0xD9, 0xDF, 0xEF, 0xE3, 0xB6, 0xF5, 0xFE, 0xE2,
};

static uint8_t typed[CTF_CODE_LEN];
static int n_typed = 0;


void ctf_code_start(void) {
    n_typed = 0;
}


int ctf_code_press(uint8_t btn_mask) {
    if (n_typed < CTF_CODE_LEN && btn_mask)
        typed[n_typed++] = btn_mask;
    return n_typed;
}


void ctf_code_text(char *buf, int len) {
    /* ^ v for the flanks, < > for the wings: the player sees what he typed, not whether it is right */
    int pos = 0;
    buf[0] = 0;
    for (int i = 0; i < n_typed && pos < len - 3; ++i) {
        char c = typed[i] == BTN_Y ? '^' : typed[i] == BTN_X ? 'v' : typed[i] == BTN_A ? '<' : '>';
        pos += snprintf(buf + pos, len - pos, "%c ", c);
    }
}


bool ctf_code_check(void) {
    if (n_typed != CTF_CODE_LEN || memcmp(typed, KONAMI, CTF_CODE_LEN))
        return false;
    store_t *s = store_get();
    if (! (s->flags_found & 1)) {
        s->flags_found |= 1;
        store_changed();
    }
    return true;
}


const char *ctf_flag(int n, char *buf, int len) {
    if (n != 0 || ! (store_get()->flags_found & (1u << n)) || len <= (int)sizeof(FLAG0))
        return NULL;
    static const char key[] = "cigale";
    for (size_t i = 0; i < sizeof(FLAG0); ++i)
        buf[i] = FLAG0[i] ^ key[i % 6] ^ (uint8_t)(i * 7);
    buf[sizeof(FLAG0)] = 0;
    return buf;
}


int ctf_found_count(void) {
    int n = 0;
    for (int i = 0; i < CTF_N_FLAGS; ++i)
        n += (store_get()->flags_found >> i) & 1;
    return n;
}
