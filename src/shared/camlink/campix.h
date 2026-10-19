/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file campix.h
 *
 * \brief The pictures of the camera for the e-Paper of the badge (plain C, used by the camera module, tested on a PC):
 * a gray picture (8 bits) cropped and scaled to 200 x 200, then dithered to 1 bit (the viewfinder, the videos) or to
 * the 4 grays of an .epi image (the photos, viewed in Médias > Images). */

#ifndef _CAMPIX_H
#define _CAMPIX_H

#include <stdint.h>

#define CAMPIX_SIZE 200
#define CAMPIX_EPI_BYTES (16 + 2 * CAMPIX_SIZE * CAMPIX_SIZE / 8)  /* Header, LSB plane, MSB plane */

/** \brief The center square of a gray picture \p w x \p h, scaled (nearest) to 200 x 200, into \p out (40000 bytes). */
void campix_square(const uint8_t *gray, int w, int h, uint8_t *out);

/** \brief 200 x 200 gray -> 1 bit (5000 bytes, 1 = white, bit 7 = the leftmost pixel), CAMLINK_DITHER_*.
 * \p work: 2 x 202 int16_t for the error diffusions. The gray is changed (the errors). */
void campix_dither1(uint8_t *gray, int dither, uint8_t *out, int16_t *work);

/** \brief 200 x 200 gray -> an .epi file of 2 bits per pixel (CAMPIX_EPI_BYTES), Floyd-Steinberg to 4 grays.
 * The gray is changed. */
void campix_epi(uint8_t *gray, uint8_t *out, int16_t *work);

/** \brief Stretches the contrast of a 200 x 200 gray picture (1 % of the pixels black, 1 % white). */
void campix_stretch(uint8_t *gray);

#endif /* _CAMPIX_H */
