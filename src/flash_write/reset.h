/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file reset.h
 *
 * Include this file in binaries that should overwrite the reserved sector (for reset or initial flash).
 */

#ifndef _FLASH_WRITE_RESET_H
#define _FLASH_WRITE_RESET_H

#include "flash_write.h"


__attribute__((section(".reserved_sector"), used))
const Persistence initial_config = {
    .magic = "Miao",
    .flags_configured = 0,
    .radio_crystal_frequency = 26000000,
};


#endif /* _FLASH_WRITE_RESET_H */
