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
    APP_TALK,
    APP_HOTCOLD,
    APP_HOTCOLD_MASTER,
    APP_RADAR,
    APP_VOTE,
    APP_VOTE_ADMIN,
    APP_PROGRAM,
    APP_PROGRAM_ANNOUNCE,
    APP_INFECTION,
    APP_INFECTION_ZERO,
    APP_MESSAGES,
    APP_CHORUS,
    APP_CHORUS_LEAD,
    APP_CONTACTS,
    APP_DUEL,
    APP_IMAGE_SEND,
    APP_IMAGE_RECV,
    APP_CRYPTO,
    APP_BATTLE,
    APP_HUNT433,
    APP_RADIO_TUNE,
    APP_LEDCAST,
    APP_ANNOUNCES,
    APP_ANNOUNCE_ADMIN,
    APP_RESET,
    APP_BATTCAL,
    APP_WEREWOLF,
    APP_ASSASSIN,
    APP_TUG,
    APP_GAMEBOOK,
    APP_RTTTL,
    APP_PIRATE_RADIO,
    APP_DEMO,
    APP_SMUGGLER,
    APP_SKILLS,
    APP_ACHIEVEMENTS,
    APP_PIRATE_LISTEN,
    APP_COUNT,
} app_id_t;

extern const app_t *const APPS[APP_COUNT];

#endif /* _APPS_H */
