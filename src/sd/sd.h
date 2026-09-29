/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file sd.h
 *
 * \brief SD card API: read sectors of the micro SD card (board V1.1) in SPI mode, used by FatFs (see fatfs/ff.h).
 *
 * The SD card shares SPI0 with the screen, and is selected through the 74HC139 decoder (address BADGE_SPI0_ADDR_SD).
 * The hardware chip select of the RP2040 goes high between each byte, which the screen accepts but not the SD card:
 * during an SD transaction, SPI0_CSn is driven as a GPIO, then given back to the SPI for the screen.
 * The SPI baud rate is also changed during the SD transactions then restored.
 *
 * Don't use the SD card while pushing data to the screen (it is fine while the screen is busy drawing).
 *
 * Usage: screen_init() (which initializes SPI0), then use FatFs: f_mount(), f_open(), f_read(), ...
 * FatFs calls sd_init() when mounting.
 * */

#ifndef _SD_H
#define _SD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** \brief Initialize the card (can be called again after a card change).
 *
 * Blocks up to ~1s while the card initializes (only called when mounting).
 * \return true when a card answered and is ready */
bool sd_init(void);

/** \brief Whether sd_init() succeeded. */
bool sd_is_ready(void);

/** \brief Read \p count sectors of 512 bytes starting at sector \p lba. */
bool sd_read_blocks(uint32_t lba, uint8_t *buf, size_t count);

/** \brief Level of the card detect pin (the polarity depends on the socket). */
bool sd_detect(void);


/* ------ File system helpers (FatFs) ------ */

#define SD_NAME_MAX 64  /* Longer file names are ignored by sd_list_files() */

/** \brief Mount the card if not done yet (can be called before each use).
 * \return a FatFs FRESULT (FR_OK = 0) */
int sd_mount(void);

/** \brief Forget the mount, e.g. after an error (the card may have been changed): the next sd_mount() mounts again. */
void sd_unmount(void);

/** \brief List the files of \p dir whose name ends with \p ext (case insensitive), sorted by name.
 *
 * \param names Receives up to \p max names (without the directory).
 * \return the number of names */
size_t sd_list_files(const char *dir, const char *ext, char (*names)[SD_NAME_MAX], size_t max);

/** \brief List the sub-directories of \p dir (hidden and system ones excluded), sorted by name. */
size_t sd_list_dirs(const char *dir, char (*names)[SD_NAME_MAX], size_t max);

#endif /* _SD_H */
