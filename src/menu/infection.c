/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The (harmless) virus of the cicadas: the patient zero is chosen in the admin menu, an infected badge coughs
 * a NET_INFECTION packet every few seconds (+10 dBm), a healthy badge close to it for a while gets infected.
 * The cure: solve a riddle (then the badge is immune). A talk about how worms spread, live.
 * NET_INFECTION [generation]: the generation of the infection (0 = patient zero). */

#include <stdio.h>
#include <string.h>

#include "achievements.h"
#include "pico/rand.h"

#include "app.h"
#include "i18n.h"
#include "net.h"
#include "store.h"

#define COUGH_MS 4000
#define EXPOSURE_RSSI (-80)  /* Close (coughs at +10 dBm: ~-70 to -83 at 1 m), to calibrate on site */
#define EXPOSURES 3  /* Coughs heard within EXPOSURE_WINDOW_MS */
#define EXPOSURE_WINDOW_MS 30000
#define CONTAGION_PERCENT 60
#define AUDIBLE_EVERY 5  /* ~22 s between the coughs heard */
#define FLASH_MS 150

enum { HEALTHY = 0, INFECTED = 1, IMMUNE = 2 };

typedef struct {
    const char *question;  /* Lines separated by '\n' */
    const char *answer;  /* Upper case */
} riddle_t;

static const riddle_t RIDDLES[] = {
    {"Je chante tout l'été\nsur les pins de Provence.\nQui suis-je ?", "CIGALE"},
    {"Le vent du nord qui\nsouffle sur La Ciotat.", "MISTRAL"},
    {"Le jeu de boules né\nà La Ciotat en 1907.", "PETANQUE"},
    {"Les frères qui ont filmé\nun train à La Ciotat.", "LUMIERE"},
    {"On le publie pour\ncorriger une faille.", "PATCH"},
};
#define N_RIDDLES ((int)(sizeof(RIDDLES) / sizeof(RIDDLES[0])))

static uint8_t generation = 0;
static absolute_time_t cough_ts = 0;
static int exposures = 0;
static absolute_time_t exposure_start = 0;
static bool event = false;
static int riddle = 0;
static ui_edit_t edit;
static bool editing = false;
static bool wrong = false;
static unsigned coughs = 0;
static bool coughed = false;
static bool flash_on = false;
static absolute_time_t flash_end = 0;

static int state(void) {
    uint8_t s = store_get()->infection;
    return s == INFECTED || s == IMMUNE ? s : HEALTHY;
}

static void set_state(int s) {
    store_get()->infection = s;
    store_changed();
    event = true;
}

static void handle_cough(const net_packet_t *p) {
    if (state() != HEALTHY || p->rssi < EXPOSURE_RSSI || p->len < 1)
        return;
    if (! exposures || absolute_time_diff_us(exposure_start, p->at) > EXPOSURE_WINDOW_MS * 1000ll) {
        exposures = 0;
        exposure_start = p->at;
    }
    if (++exposures < EXPOSURES)
        return;
    exposures = 0;
    if ((int)(get_rand_32() % 100) >= CONTAGION_PERCENT) {
        printf("infection: exposed but not infected\n");
        return;
    }
    generation = p->data[0] + 1;
    riddle = get_rand_32() % N_RIDDLES;
    printf("infection: infected by %08lX (generation %u)\n", (unsigned long)p->src, generation);
    achv_unlock(ACHV_INFECTED);
    set_state(INFECTED);
}

void infection_init(void) {
    net_subscribe(NET_INFECTION, handle_cough);
}

/* Called in the main loop: an infected badge coughs (radio), with a red flash; one cough out of AUDIBLE_EVERY is
 * heard too (and shown in the footer), not to be unbearable during a talk */
void infection_task(absolute_time_t now) {
    if (flash_on && absolute_time_diff_us(flash_end, now) >= 0) {
        flash_on = false;
        if (! (app_current() && app_current()->owns_leds))
            app_leds(0, 0, 0);
    }
    if (state() == INFECTED && absolute_time_diff_us(cough_ts, now) >= 0) {
        net_send(NET_INFECTION, &generation, 1, NET_LOUD);
        cough_ts = delayed_by_ms(now, COUGH_MS + get_rand_32() % 1000);
        if (! (app_current() && app_current()->owns_leds)) {
            app_leds(255, 0, 0);  /* Nothing in mute mode */
            flash_on = true;
            flash_end = delayed_by_ms(now, FLASH_MS);
        }
        if (++coughs % AUDIBLE_EVERY == 1) {
            app_cough();  /* Silent in mute mode */
            coughed = true;
        }
    }
}

/* An audible cough just happened: the main loop shows it */
bool infection_coughed(void) {
    bool c = coughed;
    coughed = false;
    return c;
}

/* The badge just got infected (or cured): the main loop shows it */
bool infection_event(void) {
    bool e = event;
    event = false;
    return e && state() == INFECTED;
}

/* Admin menu: this badge becomes the patient zero */
void infection_patient_zero(void) {
    generation = 0;
    riddle = get_rand_32() % N_RIDDLES;
    set_state(INFECTED);
    printf("infection: patient zero\n");
}

static void infection_start(absolute_time_t now) {
    (void)now;
    editing = false;
    wrong = false;
}

static bool infection_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (editing) {
        int r = app_edit_buttons(&edit, b);
        if (r == UI_EDIT_DONE) {
            char answer[UI_EDIT_MAX + 1];
            ui_edit_result(&edit, answer, sizeof(answer));
            editing = false;
            if (! strcmp(answer, RIDDLES[riddle].answer)) {
                set_state(IMMUNE);
                printf("infection: cured\n");
                achv_unlock(ACHV_CURED);
            } else {
                wrong = true;
                printf("infection: wrong answer\n");
            }
        } else if (r == UI_EDIT_CANCEL) {
            editing = false;
        }
        return true;
    }
    if (b->pressed & UI_BTN_A)
        return false;
    if ((b->released_short & UI_BTN_B) && state() == INFECTED) {
        ui_edit_start(&edit, "", 12, UI_CHARSET_UPPER);
        editing = true;
        wrong = false;
    }
    return true;
}

static void infection_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    if (editing) {
        ui_edit_render(fb, &edit, N_("Remède"), RIDDLES[riddle].question);
        return;
    }
    ui_title(fb, N_("Virus des cigales"));
    switch (state()) {
    case INFECTED: {
        gfx_fill_rect(fb, 0, 36, GFX_WIDTH, 40, GFX_BLACK);
        gfx_text(fb, GFX_WIDTH/2, 43, &gfx_font_large, N_("INFECTÉ"), GFX_WHITE, GFX_ALIGN_CENTER);
        char text[80];
        snprintf(text, sizeof(text), _("Génération %u. Vous toussez :\nles cigales proches peuvent\n"
                                       "être contaminées !"), generation);
        ui_lines(fb, 84, &gfx_font_small, text);
        ui_lines(fb, 142, &gfx_font_small, wrong ? N_("Mauvaise réponse...") : N_("Le remède : une énigme."));
        ui_footer(fb, N_("G : retour  D : se soigner"));
        break;
    }
    case IMMUNE:
        gfx_text(fb, GFX_WIDTH/2, 45, &gfx_font_large, N_("Immunisé"), GFX_BLACK, GFX_ALIGN_CENTER);
        ui_lines(fb, 90, &gfx_font_small, N_("Vous avez trouvé le remède :\nce virus ne vous atteint plus."));
        ui_footer(fb, N_("G : retour"));
        break;
    default:
        gfx_text(fb, GFX_WIDTH/2, 45, &gfx_font_large, N_("En forme"), GFX_BLACK, GFX_ALIGN_CENTER);
        ui_lines(fb, 90, &gfx_font_small, N_("Un virus (inoffensif) circule\nde cigale en cigale...\n"
                                              "Restez trop près d'une cigale\ninfectée et vous l'attrapez !"));
        ui_footer(fb, N_("G : retour"));
        break;
    }
}

const app_t app_infection = {
    .name = N_("Virus des cigales"),
    .start = infection_start,
    .buttons = infection_buttons,
    .render = infection_render,
};

/* ------ Admin: patient zero, everybody cured ------ */

static void zero_start(absolute_time_t now) {
    (void)now;
}

static bool zero_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (b->pressed & UI_BTN_B)
        infection_patient_zero();
    if (b->pressed & UI_BTN_Y) {
        set_state(HEALTHY);
        printf("infection: this badge cured\n");
    }
    return ! (b->pressed & UI_BTN_A);
}

static void zero_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    ui_title(fb, N_("Virus : patient zéro"));
    ui_lines(fb, 40, &gfx_font_small, state() == INFECTED ? N_("Ce badge est infecté :\nil tousse toutes les 4 s.") :
             N_("Infectez ce badge pour\nlancer l'épidémie : les\nbadges qui restent près de\n"
                "lui l'attrapent (60 %)."));
    ui_lines(fb, 120, &gfx_font_small, N_("Flanc G : guérir ce badge"));
    ui_footer(fb, N_("G : retour  D : infecter"));
}

const app_t app_infection_zero = {
    .name = N_("Virus : patient zéro"),
    .start = zero_start,
    .buttons = zero_buttons,
    .render = zero_render,
};
