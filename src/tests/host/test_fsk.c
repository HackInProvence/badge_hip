/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The encoder of the acoustic link (fsk_link.c): prints the symbols of the frames of the texts given on the command
 * line, one line each; run_tests.py compares them with buildFrame() of docs/fsk-buzzer-link.html (node). */

#include <stdio.h>
#include <string.h>

#include "fsk_link.h"

int main(int argc, char **argv) {
    static uint8_t syms[FSK_MAX_SYMS];
    for (int a = 1; a < argc; ++a) {
        int n = fsk_build_frame((const uint8_t *)argv[a], (int)strlen(argv[a]), syms);
        for (int i = 0; i < n; ++i)
            printf("%d", syms[i]);
        printf("\n");
    }
    return 0;
}
