/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/unique_id.h"

#include "log.h"


int main() {
    stdio_usb_init();

    log_info("interesting value: %d", 42);
    log_warning("something broke: %d", 99);
    log_printf(-1, "this may be printed");  /* Appears when the enum int is signed, hence the level is 255 or another UINTxx_MAX*/

    log_set_level(LOG_LEVEL_INFO);
    log_printf(LOG_LEVEL_INFO, "A, this should be printed");
    log_printf(LOG_LEVEL_NONE, "B, this should be printed");

    log_set_level(LOG_LEVEL_WARNING);
    log_info("C, this should not be printed");
    log_warning("D, this should be printed");

    log_set_level(LOG_LEVEL_NONE);
    log_info("E, this should not be printed");
    log_warning("F, this should not be printed");

    char myid[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
    pico_get_unique_board_id_string(myid, 2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1);
    panic("panicked on 0b%08b (my ID is %s)\n", 0xFF, myid);
}
