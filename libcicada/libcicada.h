/* FIXME: license, for now defaults to Copyright (C) Miaou 2025 */

#ifndef LIBCICADA_H
#define LIBCICADA_H

// FIXME: where to put that...
#ifndef LIBCICADA_BUZZ_PIN
#warning "Set LIBCICADA_BUZZ_PIN with cmake -D, defaults to 28"
#define LIBCICADA_BUZZ_PIN 28
#endif


uint16_t cica_note(uint8_t note, uint8_t len);
uint16_t cica_silence(uint8_t len);
uint32_t cica_word(uint16_t note_a, uint16_t note_b);


#endif /* LIBCICADA_H */
