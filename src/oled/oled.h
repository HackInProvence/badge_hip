/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file oled.h
 *
 * \brief OLED API: SSD1306 128x64 screen on I2C, plugged on the left extension port J3 (see pinouts.h).
 *
 * The frame buffers have the gfx.h format (row major, bit 7 = leftmost pixel), 1 = lit pixel:
 * draw them with gfx_set_size(OLED_WIDTH, OLED_HEIGHT) then the gfx_* functions.
 * The frame is sent one page (8 rows, 128 bytes, ~3ms at 400kHz) per call of oled_task(), so nothing blocks for long:
 * a whole frame takes 8 calls (~26ms), hence ~35 frames per second at most.
 * */

#ifndef _OLED_H
#define _OLED_H

#include <stdbool.h>
#include <stdint.h>

#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_FB_SIZE ((OLED_WIDTH*OLED_HEIGHT)/8)

/** \brief Initialize the I2C bus and the screen (addresses 0x3C then 0x3D). \return false when no screen answers */
bool oled_init(void);

bool oled_present(void);

/** \brief Request to show this frame buffer (copied, OLED_FB_SIZE bytes). */
void oled_show(const uint8_t *fb);

/** \brief Send the next page, to call in the main loop. \return true while a frame is being sent */
bool oled_task(void);

/** \brief Turn the screen off (sleep) or on. */
void oled_power(bool on);

#endif /* _OLED_H */
