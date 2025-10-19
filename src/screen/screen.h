/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file screen.h
 *
 * \brief Screen API: display images, interface, ...
 *
 * The e-Paper screen is controlled by SPI.
 * It works with a 2MHz connection and images are 200*200 with 2 bit planes.
 * Each plane is packed to 8 pixels per byte, so each plane is 5000 bytes.
 * Transferring 2 planes takes around 5ms.
 * This means that it should be fast enough to use \ref spi_*_blocking functions.
 *
 * However, we can't wait for an image to be completely rendered, as this takes around 1s.
 *
 * If image transfer is not quick enough, we can use DMA with SPI DREQ to transfer data to the screen.
 * However, be aware that the D/C pin must be low when pushing the first byte, then it must be kept high.
 *
 * Another note is that the MOSI/RX and MISO/TX pins are a single pin of the e-Paper device (DIN).
 * You have to put the RX pin to another GPIO function so that it does not short circuit the TX pin while reading.
 *
 * You can compile images using the image2epaper.py script.
 * */

#ifndef _SCREEN_H
#define _SCREEN_H


/** \brief Initialize the screen library for write operations. */
void screen_init(void);

/** \brief Boots the screen (hard reset, soft reset, setup).
 *
 * Must be called regularly (e.g. 100Hz) until it returns true.
 * Must be called after \ref screen_init.
 *
 * \return true when ready. */
bool screen_boot(void);

/** \brief Tells whether the screen is busy for now.
 *
 * The screen cannot receive commands while busy.
 * Also returns false when the screen is not ready (\ref screen_boot). */
bool screen_busy(void);

/** \brief Sets the border color.
 *
 * The screen must not be busy.
 *
 * \param color Choose the color from the LUTs
 *              (for B/W will be 0 or 2=black, 1 or 3=white, for 4 grays will be 0 black to 3 white) */
void screen_border(uint8_t color);

/** \brief Show a uniform image. Clear to white before storing the screen for long times (days).
 *
 * The screen must not be busy.
 *
 * Bypasses the RAM content to display \param on the whole bit but uses the current LUTs,
 * which may change the color if you pushed another LUT beforehand.
 *
 * \param bit   The color, 0 for black, 1 for white.
 * */
void screen_clear(bool bit);

/** \brief Push an image with 1 bit plane (black and white).
 *
 * The image size depends on the currently selected window size.
 * Bytes are always packed 8bits per byte, where a bit is a pixel
 * (1 for white, 0 for black with the recommended waveform settings).
 *
 * \param img       The image.
 * \param len       The image buffer length, in bytes (<= 5000).
 * \param push_lut  Push factory waveform settings beforehand (leave it true).
 */
void screen_set_image_1plane(const uint8_t *img, size_t len, bool push_lut);

/** \brief Push an image with 2 bit planes (4 grays).
 *
 * The image size depends on the currently selected window size.
 * Bytes are always packed 8bits per byte, where a bit is a pixel
 * (00 for black to 11 for white with the recommended waveform settings).
 *
 * \param lsb       The least significant bitplane of the image.
 * \param msb       The most significant bitplane of the image.
 * \param len       The length of both planes, in bytes (<= 5000).
 * \param push_lut  Push factory waveform settings beforehand (leave it true).
 */
void screen_set_image_2planes(const uint8_t *lsb, const uint8_t *msb, size_t len, bool push_lut);


/** \brief Advanced/Hack: push a new Waveform Settings to the screen.
 *
 * You should read the SSD1681 datasheet to understand how to program the waveform settings.
 * It is meant to be able to develop new LUT and enhance your display.
 *
 * The screen must not be busy.
 *
 * \param lut The waveform settings (also called LUTs because most of it are 5 LUTs).
 *            Must be 159 bytes long: 153 for the VS+RP+TP+SP+FR+XON settings + 6 bytes for EOPT,
 *            VGH, VSH1, VSH2, VSL and VCOM.
 * */
void screen_push_ws(const uint8_t *luts);


/** Waveform settings, taken from the Arduino "test suite"
 * 2 colors, only uses the B/W RAM, 0 = black, 1 = white */
extern const uint8_t screen_ws_1681_bw[];

/* Homemade to get 4 colors instead of 2.
 * You have now 2 bit planes per pixels!
 * You must use the RED RAM for the MSB of the pixel, and the B/W RAM for the LSB.
 * 00 is black, 01 is dark, 10 is light, 11 is white*/
extern const uint8_t screen_ws_1681_4grays[];


#endif /* _SCREEN_H */
