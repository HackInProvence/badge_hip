/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include "apps.h"

extern const app_t app_lamp, app_nametag, app_admin_commands, app_admin_type;

const app_t *const APPS[APP_COUNT] = {
    [APP_LAMP] = &app_lamp,
    [APP_NAMETAG] = &app_nametag,
    [APP_ADMIN_COMMANDS] = &app_admin_commands,
    [APP_ADMIN_TYPE] = &app_admin_type,
};
