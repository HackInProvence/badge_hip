/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include "apps.h"

extern const app_t app_lamp, app_nametag, app_admin_commands, app_admin_type;
extern const app_t app_mines, app_2048, app_taquin, app_sokoban, app_mastermind, app_pendu;
extern const app_t app_decoder, app_weather;
extern const app_t app_talk, app_hotcold, app_hotcold_master, app_radar;
extern const app_t app_vote, app_vote_admin, app_program, app_program_announce, app_infection, app_infection_zero;
extern const app_t app_messages, app_chorus, app_chorus_lead, app_contacts, app_duel;
extern const app_t app_image_send, app_image_recv, app_crypto;

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
    [APP_VOTE] = &app_vote,
    [APP_VOTE_ADMIN] = &app_vote_admin,
    [APP_PROGRAM] = &app_program,
    [APP_PROGRAM_ANNOUNCE] = &app_program_announce,
    [APP_INFECTION] = &app_infection,
    [APP_INFECTION_ZERO] = &app_infection_zero,
    [APP_MESSAGES] = &app_messages,
    [APP_CHORUS] = &app_chorus,
    [APP_CHORUS_LEAD] = &app_chorus_lead,
    [APP_CONTACTS] = &app_contacts,
    [APP_DUEL] = &app_duel,
    [APP_IMAGE_SEND] = &app_image_send,
    [APP_IMAGE_RECV] = &app_image_recv,
    [APP_CRYPTO] = &app_crypto,
};
