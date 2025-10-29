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
 * Most of the commands require the screen to not be busy, which means that it must be booted
 * and not currently working. Few of the commands take time (e.g. rendering) and the documentation of the functions
 * explain when to be not busy and whether the screen will be busy afterwards.
 *
 * Another note is that the MOSI/TX and MISO/RX pins are a single pin of the e-Paper device (DIN).
 * You have to put the TX pin to another GPIO function so that it does not overwrite the RX pin while reading.
 *
 * You can compile images using the image2epaper.py script.
 *
 * The usual use of this library is:
 * - screen_init(),
 * - call screen_boot() regularly until it returns true (~15ms),
 * - now you can push an image:
 *   - set the image position with screen_set_image_position(),
 *   - use one of the screen_show_image_* functions,
 *   - or manually screen_set_ws() to load some waveform settings to choose your color mode and refresh style
 *     (black and white, or 4 grays, partial/full refresh, ...),
 *     then screen_push_rams() to load the image in the RAM banks,
 *     then screen_show_rams() to actually show your image,
 *   - wait for screen_busy() to go low (~1s)
 * - either push another image
 * - or go screen_deep_sleep() -> call screen_boot() to reset and push other images.
 *
 * You can find the whole datasheet/documentation of the screen by either searching for ePaper waveshare 1\"54
 * or by searching for the datasheet of the SSD1681, which is the controller of this ePaper model.
 *
 * NOTE: the datasheet recommends to deep sleep as soon as possible,
 *  and to screen_clear() before going for longer sleeps (days),
 *  and to not refresh the screen too much (> 3 minutes between refreshes (!!!)).
 *
 * TODO:
 * - text -> probably too complex, we will send pre-rendered images,
 * - ~~have a ws that uses the RED ram as a mask to NOT update some pixels (overlay)~~
 *   have a ws that does not change pixels that are equals in both RAMs, and update the others
 *   -> have a "rolling frame" and show the diffs,
 * - try to have more gray levels (try to adjust the FR[n]),
 * - have a ws that refresh a zone of the RAM (uses the RED mask to not touch the rest), so that a previous black can be canceled,
 *   then push the new image for that zone and draw it quicker/without traces,
 * - replace all magic numbers (...).
 * */

#ifndef _SCREEN_H
#define _SCREEN_H

#define SCREEN_HEIGHT 200
#define SCREEN_WIDTH 200

/** \brief Initialize the screen library for write operations. */
void screen_init(void);

/** \brief Boots the screen (hard reset, soft reset, setup).
 *
 * Booting is a long process (~21ms) and this function goes through the steps to boot.
 * This function does not block more than hundreds of µs to send commands through the SPI,
 * hence it must be called regularly (e.g. 100Hz) until it returns true.
 * Must be called after \ref screen_init.
 *
 * \return true when ready, after at least 4 calls. */
bool screen_boot(void);

/** \brief Tells whether the screen is busy for now.
 *
 * The screen cannot receive commands while busy.
 * Also returns true when the screen is not ready (\ref screen_boot). */
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
 * This keeps the screen busy for a while (~1.8s).
 *
 * Bypasses the RAM content to display \param on the whole bit but uses the current LUTs,
 * which may change the color if you pushed another LUT beforehand.
 * Does not clear the RAM, so clearing before subimage blitting may result in the previous image showing again.
 *
 * \param bit   The color, 0 for black, 1 for white.
 * */
void screen_clear(bool bit);

/** \brief Put the screen in deep sleep mode. Should be done after pushing images.
 *
 * The screen must not be busy.
 *
 * After being asleep, the screen will stay busy and needs to be booted again, see \ref screen_boot. */
void screen_deep_sleep(void);

/** \brief Show the image fullscreen with 2 colors (black and white).
 *
 * The screen must not be busy.
 * This keeps the screen busy for a while (~1.8s).
 *
 * The screen will be cleared before showing your image.
 *
 * Bytes are always packed 8bits per byte, where a bit is a pixel
 * (1 for white, 0 for black).
 *
 * \param img   The image buffer, which must be of size 5000 (=200*(200/8)) */
void screen_show_image_bw(const uint8_t *img);

/** \brief Show the image fullscreen with 4 colors (4 gray levels).
 *
 * The screen must not be busy.
 * This keeps the screen busy for a while (~1.9s).
 *
 * The screen will be cleared before showing your image.
 *
 * Bytes are always packed 8bits per byte, where a bit is a pixel.
 * The image is organized in 2 planes: one is the most significant bit (\param MSB) of the pixel color,
 * the other is the least significant bit (\param LSB).
 * They form a color between 00 for black to 11 for white.
 *
 * \param lsb   The image LSB plane, which must be of size 5000 (=200*(200/8))
 * \param lsb   The image MSB plane, which must be of size 5000 */
void screen_show_image_4g(const uint8_t *lsb, const uint8_t *msb);

/** \brief Set the screen position of the next image
 *
 * The screen must not be busy.
 *
 * The (0,0) origin is in the lower right angle.
 * The X coordinates can only be controlled by increments of 8 (0*8 to 25*8=200).
 * The Y coordinates are in [0..200].
 *
 * The x1 and y1 coordinate include the last line/column (x1 = x0+image_width).
 *
 * To show the fullscreen, use the macro screen_clear_image_position().
 * Remember that pushing a partial image does not overwrite the RAM outside of the selected window,
 * which will be displayed on the next screen_show_rams()...
 *
 * There may be a bug/feature when you push too much data to the window,
 * it will leak on lines with y < y0.
 *
 * \return The number of bytes of the bitplane to push (image size = (y1-y0)*((x1-x0)//8))
 * \return SIZE_T_MAX when the screen is busy */
size_t screen_set_image_position(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);
#define screen_clear_image_position() screen_set_image_position(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT)

/** \brief Low level: push an image with 2 bit planes in the current screen position.
 *
 * The screen must not be busy.
 *
 * The image size depends on the currently selected window size.
 * The image is organized in 2 planes: one is the most significant bit (\param MSB) of the pixel color,
 * the other is the least significant bit (\param LSB).
 * The LUT decides what is done with these information (nothing, black, white, gray, ...).
 *
 * When pushing NULL to a plane, it's use will be deactivated (RAM will be bypassed to 0).
 *
 * Use with \ref screen_push_ws and \ref screen_show_rams.
 *
 * \param lsb       The least significant bitplane of the image (or NULL).
 * \param msb       The most significant bitplane of the image (or NULL).
 * \param len       The length of both planes, in bytes (<= 5000).
 * \param push_lut  Push factory waveform settings beforehand (leave it true).
 */
void screen_push_rams(const uint8_t *lsb, const uint8_t *msb, size_t len);

/** \brief Low level: actually show the image in RAM using the current waveform settings pushed to screen.
 *
 * The screen must not be busy.
 * This keeps the screen busy for a while.
 *
 * Use with \ref screen_push_ws and \ref screen_push_rams. */
void screen_show_rams(void);

/** \brief Low level: push a new Waveform Settings to the screen.
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


/** Waveform settings, taken from the Arduino "test suite".
 * 2 colors, only uses the B/W RAM, 0 = black, 1 = white.
 * 3 steps: show image inversed, clear black/white trice, show your image (~1.8s) */
extern const uint8_t screen_ws_1681_bw[];

/** Homemade waveform settings to get 4 colors instead of 2.
 * You have now 2 bit planes per pixels!
 * You must use the RED RAM for the MSB of the pixel, and the B/W RAM for the LSB.
 * 00 is black, 01 is dark, 10 is light, 11 is white
 * 3 steps: show image inversed, clear black/white trice, show your image (~1.9s) */
extern const uint8_t screen_ws_1681_4grays[];

/** Homemade waveform settings which whiten pixels 10 and darken pixels 01,
 * but leave untouched pixels 00 and 11. */
extern const uint8_t screen_ws_1681_diff[];


typedef enum {
    SSD1681_DRIVER_CTRL = 0x01,
    SSD1681_GATE_CTRL = 0x03,
    SSD1681_SOURCE_CTRL = 0x04,
    //SSD1681_CODE_SETTING_PROGRAM = 0x08,
    //SSD1681_CODE_SETTING_WRITE = 0x09,
    //SSD1681_CODE_SETTING_READ = 0x0A,
    SSD1681_BOOSTER_CTRL = 0x0C,
    SSD1681_DEEP_SLEEP = 0x10,
    SSD1681_DATA_ENTRY = 0x11,
    SSD1681_SWRESET = 0x12,
    SSD1681_HV_DETECT = 0x14,
    SSD1681_VCI_DETECT = 0x15,
    SSD1681_TEMP_CTRL = 0x18,
    SSD1681_TEMP_WRITE = 0x1A,
    SSD1681_TEMP_READ = 0x1B,
    SSD1681_TEMP_WRITE_EXT = 0x1C,
    SSD1681_ACTIVATE = 0x20,
    SSD1681_DISPLAY_CTRL1 = 0x21,
    /* CTRL2 flags, 8 bits, what is done by ACTIVATE:
     * 0x80: enable clock,
     * 0x40: enable analog,
     * 0x20: load temp,
     * 0x10: load LUT,
     * 0x08: for display mode 2 instead of display mode 1,
     * 0x04: do display,
     * 0x02: disable analog after activation,
     * 0x01: disable clock after activation */
    SSD1681_DISPLAY_CTRL2 = 0x22,
    SSD1681_RAM0_WRITE = 0x24,  /* LSB bank, aka black/white */
    SSD1681_RAM1_WRITE = 0x26,  /* MSB bank, aka red */
    SSD1681_RAM_READ = 0x27,
    SSD1681_VCOM_SENSE = 0x28,
    SSD1681_VCOM_DURATION = 0x29,
    SSD1681_VCOM_OTP = 0x2A,
    SSD1681_VCOM_GLITCH = 0x2B,
    SSD1681_VCOM_CTRL = 0x2C,
    SSD1681_OTP_READ = 0x2D,
    SSD1681_UID_READ = 0x2E,
    SSD1681_STATUS = 0x2F,
    SSD1681_OTP_WS_WRITE = 0x30,
    SSD1681_OTP_WS_LOAD = 0x31,
    SSD1681_WS_WRITE = 0x32,
    SSD1681_CRC_CALC = 0x34,
    SSD1681_CRC_READ = 0x35,
    SSD1681_OTP_SELECT = 0x36,
    SSD1681_DISPLAY_OPTION = 0x37,
    SSD1681_UID_WRITE = 0x38,
    SSD1681_OTP_MODE = 0x39,
    SSD1681_BORDER_CTRL = 0x3C,
    SSD1681_EOPT_CTRL = 0x3F,
    SSD1681_RAM_SELECT = 0x41,  /* Which bank for next READ */
    SSD1681_RAM_XRANGE = 0x41,
    SSD1681_RAM_YRANGE = 0x45,
    SSD1681_RAM2_PATTERN = 0x46,
    SSD1681_RAM1_PATTERN = 0x47,
    SSD1681_RAM_XSTART = 0x4E,
    SSD1681_RAM_YSTART = 0x4F,
    SSD1681_NOP = 0x7F,
} screen_register_t;

#endif /* _SCREEN_H */
