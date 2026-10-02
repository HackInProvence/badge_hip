/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Admin menu (hidden, see ADMIN_SEQUENCE in main.c): remote commands to all the badges, type of the badge */

#include <stdio.h>

#include "app.h"
#include "remote.h"
#include "store.h"

/* ------ Remote commands ------ */

typedef struct {
    const char *label;
    uint8_t command;
} command_t;

static const command_t COMMANDS[] = {
    {"Muet (conférence)", REMOTE_MUTE},
    {"Fin du mode muet", REMOTE_UNMUTE},
    {"Cigale : chanter", REMOTE_CIGALE},
    {"Talk : éteint", REMOTE_TALK + 0},
    {"Talk : vert", REMOTE_TALK + 1},
    {"Talk : orange (5 min)", REMOTE_TALK + 2},
    {"Talk : rouge (fini)", REMOTE_TALK + 3},
    {"Talk : rouge énervé", REMOTE_TALK + 4},
    {"Mise en sommeil", REMOTE_SLEEP},
};
#define N_COMMANDS ((int)(sizeof(COMMANDS) / sizeof(COMMANDS[0])))

static int cmd_selected = 0;

static void cmd_label(int i, char *buf, size_t len) {
    snprintf(buf, len, "%s", COMMANDS[i].label);
}

static void commands_start(absolute_time_t now) {
    (void)now;
}

static bool commands_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (b->pressed & UI_BTN_A)
        return false;
    if (b->pressed & UI_BTN_Y)
        cmd_selected = (cmd_selected + N_COMMANDS - 1) % N_COMMANDS;
    if (b->pressed & UI_BTN_X)
        cmd_selected = (cmd_selected + 1) % N_COMMANDS;
    if (b->pressed & UI_BTN_B) {
        printf("admin: sending command 0x%02x\n", COMMANDS[cmd_selected].command);
        remote_send(COMMANDS[cmd_selected].command);
    }
    return true;
}

static void commands_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    ui_title(fb, "Commandes radio");
    ui_list(fb, N_COMMANDS, cmd_selected, cmd_label);
    ui_footer(fb, "G : retour  D : envoyer à tous");
}

const app_t app_admin_commands = {
    .name = "Commandes radio",
    .start = commands_start,
    .buttons = commands_buttons,
    .render = commands_render,
};


/* ------ Type of the badge (shown by the name tag) ------ */

static const char *TYPES[] = {"Participant", "Orateur", "Staff"};
static int type_selected = 0;

static void type_label(int i, char *buf, size_t len) {
    uint8_t t = store_get()->badge_type;
    snprintf(buf, len, "%s%s", TYPES[i], i == (t < 3 ? t : 0) ? "  (actuel)" : "");
}

static void type_start(absolute_time_t now) {
    (void)now;
    uint8_t t = store_get()->badge_type;
    type_selected = t < 3 ? t : 0;
}

static bool type_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (b->pressed & UI_BTN_A)
        return false;
    if (b->pressed & UI_BTN_Y)
        type_selected = (type_selected + 2) % 3;
    if (b->pressed & UI_BTN_X)
        type_selected = (type_selected + 1) % 3;
    if (b->pressed & UI_BTN_B) {
        store_get()->badge_type = type_selected;
        store_changed();
        printf("admin: badge type %s\n", TYPES[type_selected]);
    }
    return true;
}

static void type_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    ui_title(fb, "Type du badge");
    ui_list(fb, 3, type_selected, type_label);
    ui_lines(fb, 110, &gfx_font_small, "Affiché par le\nbadge nominatif.");
    ui_footer(fb, "G : retour  D : choisir");
}

const app_t app_admin_type = {
    .name = "Type du badge",
    .start = type_start,
    .buttons = type_buttons,
    .render = type_render,
};
