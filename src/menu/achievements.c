/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The achievements and the level of the cicada (achievements.h), and their page: Badge > Succès. */

#include <stdio.h>
#include <string.h>

#include "achievements.h"
#include "app.h"
#include "social.h"
#include "store.h"

#define XP_PER_MEETING 2

typedef struct {
    const char *name;
    const char *how;  /* How to get it */
    uint8_t xp;
} achv_t;

static const achv_t ACHV[ACHV_COUNT] = {
    [ACHV_FIRST_BOOT] = {"Premiers pas", "Allumer sa cigale.", 5},
    [ACHV_MEET_1] = {"Bonjour !", "Rencontrer une cigale\n(rester près d'elle).", 10},
    [ACHV_MEET_10] = {"Sociable", "Rencontrer 10 cigales.", 30},
    [ACHV_MEET_50] = {"Star du réseau", "Rencontrer 50 cigales.", 80},
    [ACHV_MESSAGE] = {"Facteur", "Envoyer un message\n(Social > Messages).", 10},
    [ACHV_CONTACT] = {"Carte de visite", "Recevoir un contact\n(Social > Contacts).", 15},
    [ACHV_VOTE] = {"Citoyen", "Voter (Social > Vote).", 10},
    [ACHV_CHORUS] = {"Choriste", "Chanter dans le choeur.", 15},
    [ACHV_INFECTED] = {"Patient", "Attraper le virus\ndes cigales.", 10},
    [ACHV_CURED] = {"Remède", "Guérir du virus.", 20},
    [ACHV_DUEL_WIN] = {"Duelliste", "Gagner un pierre-feuille-\nciseaux.", 20},
    [ACHV_BATTLE_WIN] = {"Amiral", "Gagner une bataille navale.", 30},
    [ACHV_WEREWOLF_PLAY] = {"Pleine lune", "Jouer au loup-garou.", 20},
    [ACHV_WEREWOLF_WIN] = {"Survivant", "Gagner au loup-garou.", 40},
    [ACHV_ASSASSIN_KILL] = {"Ombre", "Éliminer sa cible\nà l'assassin.", 20},
    [ACHV_ASSASSIN_WIN] = {"Dernier debout", "Gagner l'assassin.", 50},
    [ACHV_TUG_WIN] = {"Costaud", "Gagner le tir à la corde.", 20},
    [ACHV_BOOK_END] = {"Héros", "Finir un livre-jeu.", 30},
    [ACHV_RTTTL] = {"Mélomane", "Jouer une sonnerie.", 5},
    [ACHV_TRADE] = {"Contrebandier", "Échanger une marchandise\n(Social > Contrebande).", 20},
    [ACHV_TRADE_RARE] = {"Trésor", "Obtenir une marchandise\nlégendaire.", 50},
    [ACHV_CARGO_FULL] = {"Collectionneur", "Posséder toutes les\nmarchandises.", 100},
    [ACHV_CTF_FLAG] = {"Hacker", "Trouver un flag du CTF.", 30},
    [ACHV_CRYPTO] = {"Cryptographe", "Résoudre un défi crypto.", 20},
    [ACHV_HOTCOLD] = {"Fin limier", "Trouver la balise\nchaud-froid.", 30},
    [ACHV_HUNT433] = {"Chasseur d'ondes", "Entendre une télécommande\n433 MHz (Chasse 433 MHz).", 15},
    [ACHV_RECORD] = {"Recordman", "Battre un record\ndans un jeu.", 15},
    [ACHV_VIDEO] = {"Cinéphile", "Regarder une vidéo\njusqu'au bout.", 15},
    [ACHV_IMAGE_SENT] = {"Photographe", "Envoyer une image\npar radio.", 15},
    [ACHV_SKILLS] = {"Expert", "Cocher ses compétences\n(Social > Compétences).", 10},
    [ACHV_SKILL_MATCH] = {"Âmes soeurs", "Croiser une cigale qui\npartage une compétence.", 20},
    [ACHV_ALL] = {"Platine", "Obtenir tous les\nautres succès.", 200},
};

/* XP needed for each level (level n: LEVEL_XP[n - 1]) */
static const uint16_t LEVEL_XP[ACHV_LEVELS] = {0, 20, 50, 100, 170, 260, 380, 530, 720, 1000};
static const char *LEVEL_NAMES[ACHV_LEVELS] = {"Oeuf", "Larve", "Nymphe", "Mue", "Jeune cigale", "Cigale",
                                               "Chanteuse", "Virtuose", "Maestro", "Cigale d'or"};

static char event[48];
static bool event_pending = false;


bool achv_unlocked(achv_id_t id) {
    return id < ACHV_COUNT && (store_get()->achievements >> id & 1);
}

void achv_unlock(achv_id_t id) {
    if (id >= ACHV_COUNT || achv_unlocked(id))
        return;
    store_t *s = store_get();
    uint8_t before = achv_level();
    s->achievements |= 1ull << id;
    store_changed();
    uint8_t after = achv_level();
    if (after > before)
        snprintf(event, sizeof(event), "Niveau %u : %s !", after, LEVEL_NAMES[after - 1]);
    else
        snprintf(event, sizeof(event), "Succès : %s", ACHV[id].name);
    event_pending = true;
    printf("achievement: %s (+%u XP, level %u)\n", ACHV[id].name, ACHV[id].xp, after);
    bool all = true;
    for (int i = 0; i < ACHV_ALL; ++i)
        all = all && achv_unlocked(i);
    if (all)
        achv_unlock(ACHV_ALL);
}

uint16_t achv_add(achv_counter_t counter, uint16_t n) {
    if (counter >= STORE_ACHV_COUNTERS)
        return 0;
    store_t *s = store_get();
    uint32_t v = s->achv_counters[counter] + n;
    s->achv_counters[counter] = v > 0xFFFF ? 0xFFFF : v;
    store_changed();
    return s->achv_counters[counter];
}

uint32_t achv_xp(void) {
    uint32_t xp = social_met_count() * XP_PER_MEETING;
    for (int i = 0; i < ACHV_COUNT; ++i)
        if (achv_unlocked(i))
            xp += ACHV[i].xp;
    return xp;
}

uint8_t achv_level(void) {
    uint32_t xp = achv_xp();
    uint8_t level = 1;
    while (level < ACHV_LEVELS && xp >= LEVEL_XP[level])
        ++level;
    return level;
}

const char *achv_level_name(uint8_t level) {
    return LEVEL_NAMES[level < 1 ? 0 : level > ACHV_LEVELS ? ACHV_LEVELS - 1 : level - 1];
}

bool achv_event(char *msg, int len) {
    if (! event_pending)
        return false;
    event_pending = false;
    snprintf(msg, len, "%s", event);
    return true;
}

void achv_task(void) {
    uint16_t met = social_met_count();
    if (met >= 1)
        achv_unlock(ACHV_MEET_1);
    if (met >= 10)
        achv_unlock(ACHV_MEET_10);
    if (met >= 50)
        achv_unlock(ACHV_MEET_50);
    if (store_get()->skills)
        achv_unlock(ACHV_SKILLS);
}

void achievements_init(void) {
    achv_unlock(ACHV_FIRST_BOOT);
}


/* ------ The page: level, XP, then the list (right wing: how to get the selected one) ------ */

static int sel = 0;
static bool detail = false;

static void achv_start(absolute_time_t now) {
    (void)now;
    sel = 0;
    detail = false;
}

static bool achv_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (detail) {
        if (b->pressed & (UI_BTN_A | UI_BTN_B))
            detail = false;
        return true;
    }
    if (b->pressed & UI_BTN_A)
        return false;
    if (b->pressed & UI_BTN_B)
        detail = true;
    if (b->pressed & UI_BTN_X)
        sel = (sel + 1) % ACHV_COUNT;
    if (b->pressed & UI_BTN_Y)
        sel = (sel + ACHV_COUNT - 1) % ACHV_COUNT;
    return true;
}

static void achv_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    char text[64];
    if (detail) {
        const achv_t *a = &ACHV[sel];
        ui_title(fb, a->name);
        int y = ui_lines(fb, UI_TITLE_H + 12, &gfx_font_small, a->how);
        snprintf(text, sizeof(text), "+%u XP", a->xp);
        y = ui_lines(fb, y + 12, &gfx_font_small, text);
        ui_lines(fb, y + 8, &gfx_font_small, achv_unlocked(sel) ? "Obtenu !" : "Pas encore obtenu");
        ui_footer(fb, "G : retour");
        return;
    }
    uint8_t level = achv_level();
    uint32_t xp = achv_xp();
    snprintf(text, sizeof(text), "Niv. %u : %s", level, LEVEL_NAMES[level - 1]);
    ui_title(fb, text);
    /* Progress to the next level */
    uint32_t lo = LEVEL_XP[level - 1], hi = level < ACHV_LEVELS ? LEVEL_XP[level] : lo;
    int done = 0;
    for (int i = 0; i < ACHV_COUNT; ++i)
        done += achv_unlocked(i);
    if (level < ACHV_LEVELS)
        snprintf(text, sizeof(text), "%lu / %lu XP   %d/%d succès", (unsigned long)xp, (unsigned long)hi, done,
                 ACHV_COUNT);
    else
        snprintf(text, sizeof(text), "%lu XP   %d/%d succès", (unsigned long)xp, done, ACHV_COUNT);
    gfx_text(fb, GFX_WIDTH/2, UI_TITLE_H + 2, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
    ui_gauge(fb, 6, UI_TITLE_H + 21, GFX_WIDTH - 12, 6, hi > lo ? xp - lo : 1, hi > lo ? hi - lo : 1);
    /* The list: 6 rows */
    const int rows = 6, row_h = 20, y0 = UI_TITLE_H + 32;
    int first = sel - rows / 2;
    if (first > ACHV_COUNT - rows)
        first = ACHV_COUNT - rows;
    if (first < 0)
        first = 0;
    for (int i = first; i < first + rows && i < ACHV_COUNT; ++i) {
        int y = y0 + (i - first) * row_h;
        bool on = i == sel;
        if (on)
            gfx_fill_rect(fb, 2, y, GFX_WIDTH - 4, row_h - 1, GFX_BLACK);
        uint8_t fg = on ? GFX_WHITE : GFX_BLACK;
        /* A box, filled when obtained */
        gfx_rect(fb, 7, y + 5, 10, 10, fg);
        if (achv_unlocked(i))
            gfx_fill_rect(fb, 9, y + 7, 6, 6, fg);
        /* The name and its value: "Contrebandier (+20 XP)" (the longest one fits from x = 22) */
        snprintf(text, sizeof(text), "%s (+%u XP)", ACHV[i].name, ACHV[i].xp);
        char fitted[48];
        ui_fit(&gfx_font_small, fitted, sizeof(fitted), text, GFX_WIDTH - 22 - 2);
        gfx_text(fb, 22, y + 1, &gfx_font_small, fitted, fg, GFX_ALIGN_LEFT);
    }
    ui_footer(fb, "D : comment  G : retour");
}

const app_t app_achievements = {
    .name = "Succès",
    .start = achv_start,
    .buttons = achv_buttons,
    .render = achv_render,
};
