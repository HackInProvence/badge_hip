/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The achievements and the level of the cicada (achievements.h), and their page: Badge > Succès. */

#include <stdio.h>
#include <string.h>

#include "achievements.h"
#include "app.h"
#include "i18n.h"
#include "social.h"
#include "store.h"

#define XP_PER_MEETING 2

typedef struct {
    const char *name;
    const char *how;  /* How to get it */
    uint8_t xp;
} achv_t;

static const achv_t ACHV[ACHV_COUNT] = {
    [ACHV_FIRST_BOOT] = {N_("Premiers pas"), N_("Allumer sa cigale."), 5},
    [ACHV_MEET_1] = {N_("Bonjour !"), N_("Rencontrer une cigale\n(rester près d'elle)."), 10},
    [ACHV_MEET_10] = {N_("Sociable"), N_("Rencontrer 10 cigales."), 30},
    [ACHV_MEET_50] = {N_("Star du réseau"), N_("Rencontrer 50 cigales."), 80},
    [ACHV_MESSAGE] = {N_("Facteur"), N_("Envoyer un message\n(Social > Messages)."), 10},
    [ACHV_CONTACT] = {N_("Carte de visite"), N_("Recevoir un contact\n(Social > Contacts)."), 15},
    [ACHV_VOTE] = {N_("Citoyen"), N_("Voter (Social > Vote)."), 10},
    [ACHV_CHORUS] = {N_("Choriste"), N_("Chanter dans le choeur."), 15},
    [ACHV_INFECTED] = {N_("Patient"), N_("Attraper le virus\ndes cigales."), 10},
    [ACHV_CURED] = {N_("Remède"), N_("Guérir du virus."), 20},
    [ACHV_DUEL_WIN] = {N_("Duelliste"), N_("Gagner un pierre-feuille-\nciseaux."), 20},
    [ACHV_BATTLE_WIN] = {N_("Amiral"), N_("Gagner une bataille navale."), 30},
    [ACHV_WEREWOLF_PLAY] = {N_("Pleine lune"), N_("Jouer au loup-garou."), 20},
    [ACHV_WEREWOLF_WIN] = {N_("Survivant"), N_("Gagner au loup-garou."), 40},
    [ACHV_ASSASSIN_KILL] = {N_("Ombre"), N_("Éliminer sa cible\nà l'assassin."), 20},
    [ACHV_ASSASSIN_WIN] = {N_("Dernier debout"), N_("Gagner l'assassin."), 50},
    [ACHV_TUG_WIN] = {N_("Costaud"), N_("Gagner le tir à la corde."), 20},
    [ACHV_BOOK_END] = {N_("Héros"), N_("Finir un livre-jeu."), 30},
    [ACHV_RTTTL] = {N_("Mélomane"), N_("Jouer une sonnerie."), 5},
    [ACHV_TRADE] = {N_("Contrebandier"), N_("Échanger une marchandise\n(Social > Contrebande)."), 20},
    [ACHV_TRADE_RARE] = {N_("Trésor"), N_("Obtenir une marchandise\nlégendaire."), 50},
    [ACHV_CARGO_FULL] = {N_("Collectionneur"), N_("Posséder toutes les\nmarchandises."), 100},
    [ACHV_CTF_FLAG] = {N_("Hacker"), N_("Trouver un flag du CTF."), 30},
    [ACHV_CRYPTO] = {N_("Cryptographe"), N_("Résoudre un défi crypto."), 20},
    [ACHV_HOTCOLD] = {N_("Fin limier"), N_("Trouver la balise\nchaud-froid."), 30},
    [ACHV_HUNT433] = {N_("Chasseur d'ondes"), N_("Entendre une télécommande\n433 MHz (Chasse 433 MHz)."), 15},
    [ACHV_RECORD] = {N_("Recordman"), N_("Battre un record\ndans un jeu."), 15},
    [ACHV_VIDEO] = {N_("Cinéphile"), N_("Regarder une vidéo\njusqu'au bout."), 15},
    [ACHV_IMAGE_SENT] = {N_("Photographe"), N_("Envoyer une image\npar radio."), 15},
    [ACHV_SKILLS] = {N_("Expert"), N_("Cocher ses compétences\n(Social > Compétences)."), 10},
    [ACHV_SKILL_MATCH] = {N_("Âmes soeurs"), N_("Croiser une cigale qui\npartage une compétence."), 20},
    [ACHV_BABBLE] = {N_("Cigale bavarde"), N_("Déchiffrer le code Morse\nde la cigale bavarde."), 30},
    [ACHV_ALL] = {N_("Platine"), N_("Obtenir tous les\nautres succès."), 200},
};

/* XP needed for each level (level n: LEVEL_XP[n - 1]) */
static const uint16_t LEVEL_XP[ACHV_LEVELS] = {0, 20, 50, 100, 170, 260, 380, 530, 720, 1000};
static const char *LEVEL_NAMES[ACHV_LEVELS] = {N_("Oeuf"), N_("Larve"), N_("Nymphe"), N_("Mue"),
                                               N_("Jeune cigale"), N_("Cigale"), N_("Chanteuse"),
                                               N_("Virtuose"), N_("Maestro"), N_("Cigale d'or")};

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
        snprintf(event, sizeof(event), _("Niveau %u : %s !"), after, tr(LEVEL_NAMES[after - 1]));
    else
        snprintf(event, sizeof(event), _("Succès : %s"), tr(ACHV[id].name));
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

/* The rows of the list: the meetings (they bring XP too), then the achievements */
#define LIST_ROWS (ACHV_COUNT + 1)
static int sel = 0;  /* 0: the meetings, n: the achievement n - 1 */
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
        sel = (sel + 1) % LIST_ROWS;
    if (b->pressed & UI_BTN_Y)
        sel = (sel + LIST_ROWS - 1) % LIST_ROWS;
    return true;
}

static void achv_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    char text[64];
    if (detail && sel == 0) {
        ui_title(fb, N_("Rencontres"));
        int y = ui_lines(fb, UI_TITLE_H + 12, &gfx_font_small,
                         N_("Chaque cigale rencontrée\n(rester près d'elle)\nrapporte 2 XP."));
        snprintf(text, sizeof(text), _("%u rencontre%s : %u XP"), social_met_count(), social_met_count() > 1 ? "s" : "",
                 social_met_count() * XP_PER_MEETING);
        ui_lines(fb, y + 12, &gfx_font_small, text);
        ui_footer(fb, N_("G : retour"));
        return;
    }
    if (detail) {
        const achv_t *a = &ACHV[sel - 1];
        ui_title(fb, a->name);
        int y = ui_lines(fb, UI_TITLE_H + 12, &gfx_font_small, a->how);
        snprintf(text, sizeof(text), "+%u XP", a->xp);
        y = ui_lines(fb, y + 12, &gfx_font_small, text);
        ui_lines(fb, y + 8, &gfx_font_small, achv_unlocked(sel - 1) ? N_("Obtenu !") : N_("Pas encore obtenu"));
        ui_footer(fb, N_("G : retour"));
        return;
    }
    uint8_t level = achv_level();
    uint32_t xp = achv_xp();
    snprintf(text, sizeof(text), _("Niv. %u : %s"), level, tr(LEVEL_NAMES[level - 1]));
    ui_title(fb, text);
    /* Progress to the next level */
    uint32_t lo = LEVEL_XP[level - 1], hi = level < ACHV_LEVELS ? LEVEL_XP[level] : lo;
    int done = 0;
    for (int i = 0; i < ACHV_COUNT; ++i)
        done += achv_unlocked(i);
    if (level < ACHV_LEVELS)
        snprintf(text, sizeof(text), _("%lu / %lu XP   %d/%d succès"), (unsigned long)xp, (unsigned long)hi, done,
                 ACHV_COUNT);
    else
        snprintf(text, sizeof(text), _("%lu XP   %d/%d succès"), (unsigned long)xp, done, ACHV_COUNT);
    gfx_text(fb, GFX_WIDTH/2, UI_TITLE_H + 2, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
    ui_gauge(fb, 6, UI_TITLE_H + 21, GFX_WIDTH - 12, 6, hi > lo ? xp - lo : 1, hi > lo ? hi - lo : 1);
    /* The list: 6 rows */
    const int rows = 6, row_h = 20, y0 = UI_TITLE_H + 32;
    int first = sel - rows / 2;
    if (first > LIST_ROWS - rows)
        first = LIST_ROWS - rows;
    if (first < 0)
        first = 0;
    for (int r = first; r < first + rows && r < LIST_ROWS; ++r) {
        int y = y0 + (r - first) * row_h;
        bool on = r == sel;
        if (on)
            gfx_fill_rect(fb, 2, y, GFX_WIDTH - 4, row_h - 1, GFX_BLACK);
        uint8_t fg = on ? GFX_WHITE : GFX_BLACK;
        if (r == 0) {
            /* The meetings: two dots for two cicadas, and their XP */
            gfx_fill_rect(fb, 7, y + 8, 4, 4, fg);
            gfx_fill_rect(fb, 13, y + 8, 4, 4, fg);
            snprintf(text, sizeof(text), _("Rencontres : %u x 2 = %u XP"), social_met_count(),
                     social_met_count() * XP_PER_MEETING);
            char fitted[48];
            ui_fit(&gfx_font_small, fitted, sizeof(fitted), text, GFX_WIDTH - 22 - 2);
            gfx_text(fb, 22, y + 1, &gfx_font_small, fitted, fg, GFX_ALIGN_LEFT);
            continue;
        }
        int i = r - 1;
        /* A box, filled when obtained */
        gfx_rect(fb, 7, y + 5, 10, 10, fg);
        if (achv_unlocked(i))
            gfx_fill_rect(fb, 9, y + 7, 6, 6, fg);
        /* The name and its value: "Contrebandier (+20 XP)" (the longest one fits from x = 22) */
        snprintf(text, sizeof(text), "%s (+%u XP)", tr(ACHV[i].name), ACHV[i].xp);
        char fitted[48];
        ui_fit(&gfx_font_small, fitted, sizeof(fitted), text, GFX_WIDTH - 22 - 2);
        gfx_text(fb, 22, y + 1, &gfx_font_small, fitted, fg, GFX_ALIGN_LEFT);
    }
    ui_footer(fb, N_("D : comment  G : retour"));
}

const app_t app_achievements = {
    .name = N_("Succès"),
    .start = achv_start,
    .buttons = achv_buttons,
    .render = achv_render,
};
