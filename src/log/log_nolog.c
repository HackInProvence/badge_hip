/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** The empty implementation for logs when they are disabled.
 * See log.c for the actual implementation. */

#include "log.h"


void log_set_level(log_level lev) {
    (void)lev;
}

void log_printf(log_level lev, const char *fmt, ...) {
    (void)lev;
    (void)fmt;
}
