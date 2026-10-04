/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */


#include <string.h>

#include "hardware/flash.h"
#include "pico/binary_info.h"
#include "pico/flash.h"

#include "log.h"
#include "flash_write.h"


const Persistence *g_persistence = (const Persistence *)__reserved_sector_start;

static bool initialized = false;
/* Reserve the whole zone in RAM to be sure that we are able to copy it before erasing and reprogramming the zone */
static uint8_t local_copy[FLASH_RESERVED_SIZE] = {};


//void flash_write_init(void) {
//    // Copy the structure to the local_copy in case 
//    memcpy(&local_copy, g_persistence, sizeof(Persistence));
//    initialized = true;
//}


Persistence *flash_write_get_rw_copy(void) {
    memcpy(local_copy, __reserved_sector_start, FLASH_RESERVED_SIZE);
    initialized = true;
    return (Persistence *)local_copy;
}


/** The call back for flash_safe_execute */
void do_commit(void *param) {
    /* Here, we are in an interrupt-free zone, so we can't log, and we should be fast
     *  but we can read the flash because it's flash_range_ functions that disable XIP
     *  (they are in RAM, but this function is in flash)
     *  (this code does not count in the enter_exit_timeout_ms) */
    uint32_t offset = (uint32_t)(__reserved_sector_start - XIP_BASE);
    //uint32_t offset = PICO_FLASH_SIZE_BYTES - FLASH_RESERVED_SIZE;

    flash_range_erase(offset, FLASH_RESERVED_SIZE);
    flash_range_program(offset, local_copy, FLASH_RESERVED_SIZE);
}


bool flash_write_commit(void) {
    if (! initialized) {
        log_warning("flash_write_commit: called before flash_write_get_rw_copy, nothing to commit.");
        return false;
    }

    int rc = flash_safe_execute(do_commit, NULL, /*enter_exit_timeout_ms=*/1000);
    switch(rc) {
    case PICO_OK:
        break;
    case PICO_ERROR_TIMEOUT:
        log_warning("flash_write_commit: flash_safe_execute could not assert the other core is not running on flash (see flash_safe_execute_core_init).");
        return false;
    case PICO_ERROR_NOT_PERMITTED:
        log_warning("flash_write_commit: flash_safe_execute failed to copy its helper from flash.");
        return false;
    case PICO_ERROR_INSUFFICIENT_RESOURCES:
        log_warning("flash_write_commit: flash_safe_execute failed with not enough ressources.");
        return false;
    default:
        log_warning("flash_write_commit: unknown error.");
        return false;
    }

    /* Compare the flash sector with the local copy to assert that everything was fine */
    rc = memcmp(local_copy, __reserved_sector_start, FLASH_RESERVED_SIZE);

    if (rc != 0) {
        /* This leaves us in an undetermined state... */
        log_warning("flash_write_commit: failed to persist to flash, the content was not written correctly.");
        return false;
    }

    return true;
}
