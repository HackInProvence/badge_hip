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
 * - give means to flash a default configuration (e.g. with a test program)
 *   using target_compile_definitions(flash_write INTERFACE FLASH_INITILIZE_PERSISTENCE).
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
    // /!\ do update flash_write.c when changing fields of this struct
} Persistence;


/** \brief Returns a pointer to a valid Persistence (read only access).
 *
 * If the Persistence has not been flashed yet (or magic is not the expected value),
 *  returns a pointer to the default initializer of this struct (probably from flash),
 * But if the Persistence structure has the correct magic, returns the pointer to the flash were it is stored.
 *
 * You cannot change this and hence must call flash_write_get_rw_copy() followed by flash_write_commit()
 *  to commit changes. */
const Persistence *flash_write_get_ro_ptr(void);

/** \brief Get a copy of the flashed persistence configuration to be able to modify and flash_write_commit() it.
 *
 * Copy the content of the flash on each call.
 * Implemented with a single RAM copy so this always returns the same pointer and reset the content of previous call.
 * The copy is not committed automatically, so modifications are not persisted until flash_write_commit() is called.
 *
 * If the content of the flash does not contain what seems like a legit configuration (invalid magic),
 *  the local copy is first initialized with default values. */
Persistence *flash_write_get_rw_copy(void);

/** \brief Update flash with the content of the updated flash_write_get_rw_copy().
 *
 * Fails when flash_write_get_rw_copy() was not called beforehand.
 * Takes around 57ms for a 4k sector.
 *
 * \warning Do not call this too often, as the flash will fail over time (10k writes). */
bool flash_write_commit(void);


#endif /* _FLASH_WRITE_H */
