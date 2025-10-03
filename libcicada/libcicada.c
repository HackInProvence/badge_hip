/* FIXME: license, for now defaults to Copyright (C) Miaou 2025 */

#include <stdio.h>
#include "libcicada.h"

uint16_t cica_note(uint8_t note, uint8_t len) {
    return (len << 8) | note;
}

uint16_t cica_silence(uint8_t len) {
    return len<<8;
}

uint32_t cica_word(uint16_t note_a, uint16_t note_b) {
    return (note_b << 16) | note_a;
}
