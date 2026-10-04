/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** To test the re-programming of the flash, change do_commit to true in the code
 *  To test the re-initialisation of the reserved sector in the flash, uncomment the include "reset.h"
 */


// Include sys/types.h before inttypes.h to work around issue with
// certain versions of GCC and newlib which causes omission of PRIu64
#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include "log.h"
#include "flash_write.h"

// Uncomment to reset the configuration to its initial state
//#include "reset.h"


int main() {
    stdio_usb_init();
    log_set_level(LOG_LEVEL_INFO);

    sleep_ms(200);
    log_info("g_persistence: 0x%p (expected 0x10FFF000)", g_persistence);
    log_info("g_persistence->magic=%.*s", sizeof(g_persistence->magic), g_persistence->magic);
    log_info("g_persistence->flags=%032b", g_persistence->flags_configured);
    log_info("g_persistence->radio_crystal_frequency=%d Hz", g_persistence->radio_crystal_frequency);

    log_info("sector content:");
    for (size_t lines=0; lines<4096/128; ++lines) {
        for (size_t i=0; i<64; ++i) {
            printf("%02x ", ((uint8_t *)(g_persistence))[lines*128+i]);
        }
        printf("\n");
    }

    // Commit before get should fail
    bool res = flash_write_commit();
    log_info("tentative write commit before get_rw_copy: %s (expected false)", res ? "true" : "false");

    // Get and modify
    Persistence *cfg = flash_write_get_rw_copy();
    log_info("local cfg=0x%p (expected NOT 0x10FFF000, not NULL, somewhere in RAM (0x200xxxxx))", cfg);
    ++(cfg->radio_crystal_frequency);

    // Commit should be ok now
    absolute_time_t t0 = get_absolute_time();
    if (/*do_commit=*/false) {
        res = flash_write_commit();
    } else {
        log_warning("did not test flash_write_commit, change the code of the test to test it");
        res = true;
    }
    uint64_t diff_us = absolute_time_diff_us(t0, get_absolute_time());

    log_info("commit changes to flash: %s (expected true)", res ? "true" : "false");
    log_info(" and took %" PRIu64 " µs", diff_us);
    if (! res) {
        for (size_t lines=0; lines<4096/128; ++lines) {
            for (size_t i=0; i<64; ++i) {
                printf("%02x ", ((uint8_t *)(g_persistence))[lines*128+i]);
            }
            printf("\n");
        }
    }

    while(1);  // Let the prints finish
}
