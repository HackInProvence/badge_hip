/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include "apps.h"

extern const app_t app_lamp, app_nametag, app_admin_commands, app_admin_type;
extern const app_t app_mines, app_2048, app_taquin, app_sokoban, app_mastermind, app_pendu;
extern const app_t app_decoder, app_weather;
extern const app_t app_talk, app_hotcold, app_hotcold_master, app_radar;

const app_t *const APPS[APP_COUNT] = {
    [APP_LAMP] = &app_lamp,
    [APP_NAMETAG] = &app_nametag,
    [APP_ADMIN_COMMANDS] = &app_admin_commands,
    [APP_ADMIN_TYPE] = &app_admin_type,
    [APP_MINES] = &app_mines,
    [APP_2048] = &app_2048,
    [APP_TAQUIN] = &app_taquin,
    [APP_SOKOBAN] = &app_sokoban,
    [APP_MASTERMIND] = &app_mastermind,
    [APP_PENDU] = &app_pendu,
    [APP_DECODER] = &app_decoder,
    [APP_WEATHER] = &app_weather,
    [APP_TALK] = &app_talk,
    [APP_HOTCOLD] = &app_hotcold,
    [APP_HOTCOLD_MASTER] = &app_hotcold_master,
    [APP_RADAR] = &app_radar,
};
