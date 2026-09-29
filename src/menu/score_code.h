/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file score_code.h
 *
 * \brief Signed scores shown as a QR code, e.g. for a leaderboard of the conference.
 *
 * Text of the QR code: HIP26:<game>:<score>:<badge id>:<cicada name>:<signature>
 * The signature is SipHash-2-4 of the text before it, with a 128 bits key stored masked in the firmware:
 * a score can't be made up without extracting the key from the firmware (which is also a challenge...).
 * tools/score_check.py checks the texts and ranks the scores.
 * */

#ifndef _SCORE_CODE_H
#define _SCORE_CODE_H

#include <stddef.h>
#include <stdint.h>

#define SCORE_CODE_MAX 96  /* Length of the text */

/** \brief SipHash-2-4 (64 bits) of \p len bytes with a 128 bits key. */
uint64_t score_code_siphash(const uint8_t key[16], const uint8_t *data, size_t len);

/** \brief The signed text of a score: \p game e.g. "SIMON", \p score e.g. "12". */
void score_code_text(char *buf, size_t len, const char *game, const char *score, uint32_t badge_id, const char *name);

/** \brief Draws the QR code of \p text in the frame buffer (GFX_WIDTH wide), \p scale pixels per module,
 * centered horizontally from \p y. Returns its size in pixels, 0 if the text is too long.
 * With \p fb NULL, only returns the size. */
int score_code_draw(uint8_t *fb, const char *text, int y, int scale);

#endif /* _SCORE_CODE_H */
