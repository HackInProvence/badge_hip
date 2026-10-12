/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file log.h
 *
 * \brief Log API: output messages in the form "[LEVEL] msg\n" according to currently selected log level.
 *
 * The default log level is LOG_LEVEL_WARNING.
 * Use log_set_level(LOG_LEVEL_NONE) to disable logs. */

#ifndef _LOG_H
#define _LOG_H

#include <stdbool.h>

/* Use -DBADGE_DISABLE_LOGS=yes on your cmake . command to disable them */
#ifdef BADGE_DISABLE_LOGS
#pragma message "Log API disabled by configuration, no messages on stdout."
#endif /* BADGE_DISABLE_LOGS */


typedef enum {
    LOG_LEVEL_INFO = 0,
    LOG_LEVEL_WARNING = 1,
    /* No ERROR level: use printf or panic(fmt, ...) */
    LOG_LEVEL_NONE = 2,
} log_level;


/** \brief Set the log level which decides whether future calls to log_info or log_warning are displayed.
 *
 * The default value is LOG_LEVEL_WARNING.
 *
 * \param lev   Display messages of this level and above (LOG_LEVEL_NONE to disable at runtime). */
void log_set_level(log_level lev);

/** Log with INFO level and adds a newline */
#define log_info(fmt, ...) log_printf(LOG_LEVEL_INFO, true, fmt, ##__VA_ARGS__)

/** Log with WARNING level and adds a newline */
#define log_warning(fmt, ...) log_printf(LOG_LEVEL_WARNING, true, fmt, ##__VA_ARGS__)

/** If you don't want the automatic newline inserted by log_info and log_warning, call this one */
void log_printf(log_level lev, bool add_newline, const char *fmt, ...);


#endif /* _LOG_H */
