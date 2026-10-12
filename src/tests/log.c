/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/unique_id.h"

#include "log.h"


/* When BADGE_DISABLE_LOGS is defined */
void test_without_logs(void) {
    printf("Except from this message and the END message, nothing should be printed.\n");
    printf("END\n");
}

void test_with_logs(void) {
    log_info("interesting value: %d (should not be printed)", 42);
    log_warning("beware: %d (should be printed)", 99);
    log_printf(-1, /*add_newline=*/true, "this may be printed");  /* Appears when the enum int is signed, hence the level is 255 or another UINTxx_MAX*/

    log_set_level(LOG_LEVEL_INFO);
    log_printf(LOG_LEVEL_INFO, /*add_newline=*/true, "A, this should be printed as [INFO]");
    log_printf(LOG_LEVEL_NONE, /*add_newline=*/true, "B, this should be printed as []");

    log_set_level(LOG_LEVEL_WARNING);
    log_info("C, this should not be printed");
    log_warning("D, this should be printed as [WARN]");

    log_set_level(LOG_LEVEL_NONE);
    log_info("E, this should not be printed");
    log_warning("F, this should not be printed");

}

int main() {
    stdio_usb_init();

#ifdef BADGE_DISABLE_LOGS
    test_without_logs();
#else
    test_with_logs();
#endif

    printf("Now testing panic(), should print something, but the UART link may shut down too quickly.\n");
    char myid[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
    pico_get_unique_board_id_string(myid, 2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1);
    panic("panicked on 0b%08b (my ID is %s)\n", 0xFF, myid);
}
