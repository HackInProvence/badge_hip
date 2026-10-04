/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file flash_write.h
 *
 * \brief Library that reserves a sector in the flash for persistent configuration.
 *
 * The goal of the lib is trifold:
 * - always reserve a sector at the end of the flash for persistent data,
 * - give a library to read/write a persistent structure from running code,
 * - give means to flash a default configuration (e.g. with a test program).
 *
 * See reset.h to set initial value for this sector.
 * */

#ifndef _FLASH_WRITE_H
#define _FLASH_WRITE_H


/** This size must be the same in reserved_sector.ld and must be a multiple of 4k */
#define FLASH_RESERVED_SIZE 4096

typedef struct {
    char magic[4];  /**< Expected to be Miao (all 1s when flash is erased) */
    uint32_t flags_configured;  /**< Flags to specify which values are non-default */
    uint32_t radio_crystal_frequency;  /**< Calibration for the crystal that drives the radio (in Hz) */
    // /!\ do update reset.h when changing fields of this struct
} Persistence;

/* The global pointer to the Persistence structure, use other functions for this library to modify it */
extern const uint8_t __reserved_sector_start[];  /* Available at link time, must be an array */
extern const Persistence *g_persistence;  /**< Friendly name to use in programs, points to the first byte of the reserved sector(s) */


///** Not needed to access \p g_persistence, but needed for other functions of this lib */
//void flash_write_init(void);

/** \brief Get a copy of the flashed persistence configuration to be able to modify it.
 *
 * Implemented with a single RAM copy so this always returns the same pointer.
 * Copy the content of the flash on each call.
 * The copy is not committed automatically, so modifications are not persisted until flash_write_commit() is called. */
Persistence *flash_write_get_rw_copy(void);

/** \brief Update flash with the content of the updated flash_write_get_rw_copy().
 *
 * Fails when flash_write_get_rw_copy() was not called beforehand.
 * Takes around 57ms for a 4k sector.
 *
 * \warning Do not call this too often, as the flash will fail over time (10k writes). */
bool flash_write_commit(void);


#endif /* _FLASH_WRITE_H */
