/* FIXME: license, for now defaults to Copyright (C) Miaou 2025 */

#include <stdio.h>
#include "libcicada.h"


uint8_t cica_note(uint8_t note, uint8_t len) {
    return ((len & 0xF) << 4) | (note & 0xF);
}

uint8_t cica_silence(uint8_t len) {
    return (len & 0xF) << 4;
}

uint32_t cica_word(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return (d << 24) | (c << 16) | (b << 8) | a;
}
