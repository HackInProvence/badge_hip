/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */


#include <stdarg.h>
#include <stdio.h>

#include "log.h"


static log_level level = LOG_LEVEL_WARNING;

static log_level clamp_lev(log_level lev) {
    if (lev < LOG_LEVEL_INFO)  /* Our enum probably won't be signed but it may happen */
        return LOG_LEVEL_INFO;
    else if (lev > LOG_LEVEL_NONE)
        return LOG_LEVEL_NONE;
    return lev;
}


void log_set_level(log_level lev) {
    level = clamp_lev(lev);
}

void log_printf(log_level lev, const char *fmt, ...) {
    if (lev < level)
        return;

    /* Choose a prefix to printf it before the msg */
    lev = clamp_lev(lev);
    char *pref;
    switch(lev)
    {
    case LOG_LEVEL_INFO:
        pref = "[INFO] ";
        break;
    case LOG_LEVEL_WARNING:
        pref = "[WARN] ";
        break;
    default:
        pref = "[] ";
        break;
    }
    printf(pref);

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    puts(NULL);
}
