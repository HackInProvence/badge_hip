/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** To test the re-programming of the flash, change do_commit to true in the code
 *  To test the re-initialisation of the reserved sector in the flash,
 *   uncomment the target_compile_definitions in CMakeLists.
 *
 *  0. write garbage to flash: dd if=/dev/random of=tmp.bin bs=4096 count=1 && picotool load tmp.bin -t bin -o 10fff000
 *  1. test with FLASH_INITILIZE_PERSISTENCE and garbage before flashing
 *  2. test without FLASH_INITILIZE_PERSISTENCE with or without garbage
 * /!\ FLASH_INITILIZE_PERSISTENCE only re-defaults on program, not on boot, so an image can still have wrong flash content
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


/* To make the tests smarter (points to the persistence zone to test for garbage) */
extern const uint8_t __reserved_sector_start[];


int main() {
    stdio_usb_init();
    log_set_level(LOG_LEVEL_INFO);
    sleep_ms(200); // Wait a little, otherwise the USB stdout is not readable by the host
    log_info("start");

    // To write garbage and test:
    // dd if=/dev/random of=tmp.bin bs=4096 count=1 && picotool load tmp.bin -t bin -o 10fff000
    bool garbage = memcmp(__reserved_sector_start, "Miao", 4) != 0;
#ifdef FLASH_INITILIZE_PERSISTENCE
    bool reflashed_persist = true;
#else
    bool reflashed_persist = false;
#endif
    log_info("%sGARBAGE detected (%s)",
             garbage ? "" : "NOT ",
             reflashed_persist ? "expected NOT GARBAGE (reflashed)" : "can't tell if that's expected");

    //log_info("sector content:");
    //for (size_t lines=0; lines<4096/128; ++lines) {
    //    for (size_t i=0; i<64; ++i) {
    //        printf("%02x ", ((uint8_t *)(ro_persistence))[lines*128+i]);
    //    }
    //    printf("\n");
    //}

    if (garbage) {
        const Persistence *ro_persistence = flash_write_get_ro_ptr();
        log_info("ro_persistence: 0x%p (expected NOT 0x10FFF000 and NOT 0x200xxxxx)", ro_persistence);
        log_info("ro_persistence->magic=%.*s (expected Miao)", sizeof(ro_persistence->magic), ro_persistence->magic);
        log_info("ro_persistence->flags=%032b (expected 0)", ro_persistence->flags_configured);
        log_info("ro_persistence->radio_crystal_frequency=%d Hz (expected 26.000000MHz)", ro_persistence->radio_crystal_frequency);
    } else {
        const Persistence *ro_persistence = flash_write_get_ro_ptr();
        log_info("ro_persistence: 0x%p (expected 0x10FFF000)", ro_persistence);
        log_info("ro_persistence->magic=%.*s (expected Miao)", sizeof(ro_persistence->magic), ro_persistence->magic);
        log_info("ro_persistence->flags=%032b", ro_persistence->flags_configured);
        log_info("ro_persistence->radio_crystal_frequency=%d Hz", ro_persistence->radio_crystal_frequency);
    }

    /* Commit before get_rw should fail, even after get_ro */
    bool res = flash_write_commit();
    log_info("tentative write commit before get_rw_copy: %s (expected false)", res ? "true" : "false");

    /* Get and modify */
    Persistence *cfg = flash_write_get_rw_copy();
    log_info("local cfg=0x%p (expected NOT 0x10FFF000, not NULL, somewhere in RAM (0x200xxxxx))", cfg);
    log_info("local cfg->magic=%.*s (expected Miao%s)",
             sizeof(cfg->magic), cfg->magic,
             garbage ? ", VERY IMPORTANT as there is GARBAGE in flash" : "");
    log_info("local cfg->flags=%032b", cfg->flags_configured);
    log_info("local cfg->radio_crystal_frequency=%d Hz", cfg->radio_crystal_frequency);
    ++(cfg->radio_crystal_frequency);

    /* Commit should be ok now */
    absolute_time_t t0 = get_absolute_time();
    if (/*dont_commit=*/true) {
        log_warning("did not test flash_write_commit, change the code of the test to test that");
    } else {
        res = flash_write_commit();
        uint64_t diff_us = absolute_time_diff_us(t0, get_absolute_time());

        log_info("commit changes to flash: %s (expected true)", res ? "true" : "false");
        log_info(" and took %" PRIu64 " µs", diff_us);
        if (! res) {
            for (size_t lines=0; lines<4096/128; ++lines) {
                for (size_t i=0; i<64; ++i) {
                    printf("%02x ", ((uint8_t *)(__reserved_sector_start))[lines*128+i]);
                }
                printf("\n");
            }
        }

        const Persistence *ro_persistence = flash_write_get_ro_ptr();
        log_info("after commit, ro_persistence: 0x%p (expected 0x10FFF000)", ro_persistence);
        log_info("ro_persistence->magic=%.*s (expected Miao)", sizeof(ro_persistence->magic), ro_persistence->magic);
        log_info("ro_persistence->flags=%032b (expected same as local cfg)", ro_persistence->flags_configured);
        log_info("ro_persistence->radio_crystal_frequency=%d Hz (expected same as local cfg+1)", ro_persistence->radio_crystal_frequency);
    }

    while(1);  // Let the prints finish
}
