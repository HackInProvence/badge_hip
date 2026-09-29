/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file apps.h
 *
 * \brief The list of the applications (app.h) that main.c puts in its menus.
 * */

#ifndef _APPS_H
#define _APPS_H

#include "app.h"

typedef enum {
    APP_LAMP,
    APP_NAMETAG,
    APP_ADMIN_COMMANDS,
    APP_ADMIN_TYPE,
    APP_MINES,
    APP_2048,
    APP_TAQUIN,
    APP_SOKOBAN,
    APP_MASTERMIND,
    APP_PENDU,
    APP_DECODER,
    APP_WEATHER,
    APP_COUNT,
} app_id_t;

extern const app_t *const APPS[APP_COUNT];

#endif /* _APPS_H */
