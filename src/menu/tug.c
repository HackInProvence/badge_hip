/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Tir à la corde: a group game in the lobby of party.c. A host opens the party, the cicadas around join it, the host
 * starts it. Every badge then computes the same two teams (Cigales and Fourmis, as in La Fontaine) from the shared
 * seed and the roster (tug_logic.c); with an odd number of players, one of them is the referee.
 * After a synchronized countdown, the players pull for PULL_MS: a press of the left wing then of the right wing is one
 * pull. Each badge sends its count now and then (K_COUNT, to everybody), and every badge sums the counts of each team
 * to place the knot of the rope: the screens agree even far from the host. A team that gets tug_margin() pulls ahead
 * reaches its mark and stops the game at once (the "stopped" flag in K_COUNT stops the others too).
 * Then FINAL_MS of final counts (repeated: the radio loses packets) and the result, the same on every badge (within
 * the packets lost).
 * K_COUNT [pulls 2, LE][flags: bit 0 = stopped]. */

#include <stdio.h>
#include <string.h>

#include "app.h"
#include "achievements.h"
#include "i18n.h"
#include "net.h"
#include "party.h"
#include "tug_logic.h"

#define MIN_PLAYERS 2
#define START_DELAY_MS 2500  /* The START packets of party.c are sent 5 times in 1.5 s */
#define TEAMS_MS 5000  /* The teams are shown */
#define COUNTDOWN_MS 3000  /* 3, 2, 1 */
#define PULL_MS 20000
#define FINAL_MIN_MS 3000  /* The final counts, repeated */
#define LEAVE_AFTER_MS 5000  /* After the result, the badge leaves the party (another game can start) */
#define SEND_BASE_MS 300  /* Period of the counts: more players, slower (40 badges share the channel) */
#define SEND_PER_PLAYER_MS 40
#define PULL_REDRAW_MS 400  /* The e-paper takes ~0.3 s for a fast refresh */
#define K_COUNT PARTY_KIND_GAME

enum { P_HOME, P_SCAN, P_LOBBY, P_GAME };
enum { PH_WAIT, PH_TEAMS, PH_COUNTDOWN, PH_PULL, PH_FINAL, PH_RESULT };
static const char *TEAMS[3] = {N_("Cigales"), N_("Fourmis"), N_("Arbitre")};

extern const app_t app_tug;
void tug_service(absolute_time_t now);

static int page = P_HOME;
static int sel = 0;
static bool changed = false;
/* Lobby */
static party_open_t found[PARTY_MAX_OPEN];
static int n_found = 0;
static absolute_time_t list_ts = 0;
static party_player_t lobby[PARTY_MAX];
static int n_lobby = 0;
static party_state_t last_party = PARTY_IDLE;
/* The game */
static bool in_game = false;  /* Started, until the player goes back to the first page */
static bool ready = false;  /* The roster is complete: the teams are known */
static int n = 0, me = -1;
static uint32_t ids[PARTY_MAX];
static char names[PARTY_MAX][9];
static uint8_t team[PARTY_MAX];
static uint16_t counts[PARTY_MAX];
static uint16_t my_pulls = 0;
static tug_pull_t puller;
static absolute_time_t t0 = 0, stop_ts = 0, send_ts = 0, redraw_ts = 0, leds_off_ts = 0;
static bool stopped = false, finished = false, left_party = false, cancelled = false;
static int phase = PH_WAIT, countdown_shown = 0;
static uint32_t totals[2] = {0, 0}, final_totals[2] = {0, 0};
static int winner = -1, best = -1;

static bool on_page(void) {
    return app_current() == &app_tug;
}

static int send_period_ms(void) {
    return SEND_BASE_MS + SEND_PER_PLAYER_MS * n;
}

static int final_ms(void) {
    int ms = 3 * send_period_ms();
    return ms > FINAL_MIN_MS ? ms : FINAL_MIN_MS;
}

static absolute_time_t pull_start(void) {
    return delayed_by_ms(t0, TEAMS_MS + COUNTDOWN_MS);
}

static void sum_teams(void) {
    totals[0] = totals[1] = 0;
    for (int i = 0; i < n; ++i)
        if (team[i] < 2)
            totals[team[i]] += counts[i];
}

static int team_size(void) {
    return n / 2;
}

static void handler(uint8_t kind, uint32_t from, uint32_t to, const uint8_t *data, uint8_t len, int rssi) {
    (void)to;
    (void)rssi;
    if (kind != K_COUNT || len < 3 || ! in_game || ! ready || finished)
        return;
    int i = -1;
    for (int k = 0; k < n; ++k)
        if (ids[k] == from)
            i = k;
    if (i < 0 || i == me)
        return;
    uint16_t c = data[0] | data[1] << 8;
    if (c > counts[i]) {  /* Counts only grow: an old packet changes nothing */
        counts[i] = c;
        changed = true;
    }
    if ((data[2] & 1) && phase == PH_PULL && ! stopped) {
        stopped = true;
        stop_ts = get_absolute_time();
        send_ts = stop_ts;
        printf("tug: stopped by %s (mark reached)\n", names[i]);
    }
}

static void reset_game(void) {
    in_game = ready = stopped = finished = left_party = cancelled = false;
    n = 0;
    me = -1;
    my_pulls = 0;
    puller.armed = false;
    memset(counts, 0, sizeof(counts));
    phase = PH_WAIT;
    countdown_shown = 0;
    winner = best = -1;
    totals[0] = totals[1] = 0;
}

static void leds(uint8_t r, uint8_t g, uint8_t b, int ms, absolute_time_t now) {
    if (! on_page())
        return;
    app_leds(r, g, b);
    leds_off_ts = ms ? delayed_by_ms(now, ms) : 0;
}

/* The roster is complete (the ROSTER pages of party.c): the teams */
static void take_roster(void) {
    n = party_players(lobby, PARTY_MAX);
    for (int i = 0; i < n; ++i)
        if (! lobby[i].id)
            return;  /* A page is missing */
    me = party_index(net_id());
    if (me < 0)
        return;
    for (int i = 0; i < n; ++i) {
        ids[i] = lobby[i].id;
        snprintf(names[i], sizeof(names[i]), "%s", lobby[i].name);
    }
    tug_teams(party_seed(), n, team);
    ready = true;
    changed = true;
    printf("tug: teams of %d players, seed %08lx, I am in %s\n", n, (unsigned long)party_seed(), TEAMS[team[me]]);
    for (int i = 0; i < n; ++i)
        printf("tug: player %s#%04X: %s\n", names[i], (unsigned)(ids[i] & 0xFFFF), TEAMS[team[i]]);
}

static void result(absolute_time_t now) {
    sum_teams();
    final_totals[0] = totals[0];
    final_totals[1] = totals[1];
    winner = tug_winner(totals[0], totals[1]);
    best = -1;
    for (int i = 0; i < n; ++i)
        if (team[i] < 2 && counts[i] && (best < 0 || counts[i] > counts[best]))
            best = i;
    finished = true;
    achv_add(ACHV_CNT_GAMES, 1);
    bool won = winner >= 0 && team[me] == winner;
    if (won) {
        achv_unlock(ACHV_TUG_WIN);
        achv_add(ACHV_CNT_WINS, 1);
    }
    printf("tug: result Cigales %lu - %lu Fourmis, winner %s, me %s %u pulls, best %s %u\n",
           (unsigned long)totals[0], (unsigned long)totals[1], winner < 0 ? "none (draw)" : TEAMS[winner],
           team[me] == TUG_REFEREE ? "referee" : won ? "won" : "lost", my_pulls, best >= 0 ? names[best] : "-",
           best >= 0 ? counts[best] : 0);
    if (on_page())
        app_tone(won ? 1319 : winner < 0 || team[me] == TUG_REFEREE ? 880 : 330, 400);
    leds(won ? 0 : 255, won ? 255 : 0, 0, 3000, now);
}

/* Runs the game, called by the page and (when main.c calls it) in the main loop: the game goes on while the player
 * looks at another page */
void tug_service(absolute_time_t now) {
    if (leds_off_ts && absolute_time_diff_us(leds_off_ts, now) >= 0) {
        leds_off_ts = 0;
        if (on_page())
            app_leds(0, 0, 0);
    }
    if (! in_game) {
        if (party_state() == PARTY_STARTED && party_game() == PARTY_GAME_TUG) {
            reset_game();
            in_game = true;
            t0 = party_start_time();
            party_set_handler(handler);
            page = P_GAME;
            changed = true;
            printf("tug: game started, %d players\n", party_count());
        }
        return;
    }
    if (! finished && ! left_party && party_state() != PARTY_STARTED) {
        if (! cancelled)
            printf("tug: party cancelled\n");
        cancelled = true;
        changed |= phase != PH_RESULT;
    }
    if (! ready) {
        take_roster();
        if (! ready)
            return;
    }

    /* The phases, from the synchronized start time */
    int ph;
    if (absolute_time_diff_us(now, delayed_by_ms(t0, TEAMS_MS)) > 0)
        ph = PH_TEAMS;
    else if (absolute_time_diff_us(now, pull_start()) > 0)
        ph = PH_COUNTDOWN;
    else if (! stopped && absolute_time_diff_us(now, delayed_by_ms(pull_start(), PULL_MS)) > 0)
        ph = PH_PULL;
    else if (! finished && absolute_time_diff_us(now, delayed_by_ms(stopped ? stop_ts : delayed_by_ms(pull_start(),
                                                                              PULL_MS), final_ms())) > 0)
        ph = PH_FINAL;
    else
        ph = PH_RESULT;
    if (ph == PH_FINAL && ! stopped) {
        stopped = true;
        stop_ts = delayed_by_ms(pull_start(), PULL_MS);
        send_ts = now;
        printf("tug: time is up\n");
    }
    if (ph != phase) {
        phase = ph;
        changed = true;
        if (ph == PH_PULL) {
            printf("tug: pull!\n");
            if (on_page())
                app_tone(1760, 300);
            leds(0, 255, 0, team[me] == TUG_REFEREE ? 1000 : 0, now);
            puller.armed = false;
            send_ts = now;
        } else if (ph == PH_FINAL) {
            leds(0, 0, 0, 0, now);
        } else if (ph == PH_RESULT) {
            result(now);
        }
    }
    if (ph == PH_COUNTDOWN) {
        int left = (absolute_time_diff_us(now, pull_start()) + 999999) / 1000000;
        if (left != countdown_shown) {
            countdown_shown = left;
            changed = true;
            printf("tug: countdown %d\n", left);
            if (on_page())
                app_tone(880, 150);
            static const uint8_t COLORS[4][3] = {{0, 0, 0}, {255, 255, 0}, {255, 128, 0}, {255, 0, 0}};
            int c = left > 3 ? 3 : left;
            leds(COLORS[c][0], COLORS[c][1], COLORS[c][2], 0, now);
        }
    }

    /* The pulls of the teams: a team at its mark wins at once */
    if (ph == PH_PULL) {
        sum_teams();
        int32_t diff = (int32_t)totals[0] - (int32_t)totals[1];
        if (diff >= tug_margin(team_size()) || -diff >= tug_margin(team_size())) {
            stopped = true;
            stop_ts = now;
            send_ts = now;
            printf("tug: the %s reached their mark (%lu - %lu)\n", TEAMS[diff > 0 ? 0 : 1], (unsigned long)totals[0],
                   (unsigned long)totals[1]);
            changed = true;
        }
    }

    /* This badge's count, again and again (the final one too) */
    if ((ph == PH_PULL || ph == PH_FINAL || stopped) && ! finished && team[me] != TUG_REFEREE
            && absolute_time_diff_us(send_ts, now) >= 0) {
        uint8_t d[3] = {my_pulls, my_pulls >> 8, stopped};
        if (party_send(K_COUNT, 0, d, sizeof(d)))
            send_ts = delayed_by_ms(now, send_period_ms());
    }

    if (finished && ! left_party && absolute_time_diff_us(delayed_by_ms(stop_ts, final_ms() + LEAVE_AFTER_MS), now) >= 0) {
        left_party = true;
        if (party_state() == PARTY_STARTED && party_game() == PARTY_GAME_TUG)
            party_leave();  /* Another game can start; the result stays on the page */
        printf("tug: left the party\n");
    }
}


/* ------ The page ------ */

static void found_label(int i, char *buf, size_t len) {
    snprintf(buf, len, _("%s  %u joueur%s"), found[i].name, found[i].players, found[i].players > 1 ? "s" : "");
}

static void home_label(int i, char *buf, size_t len) {
    snprintf(buf, len, "%s", i ? N_("Rejoindre une partie") : N_("Créer une partie"));
}

static void tug_start(absolute_time_t now) {
    tug_service(now);
    if (in_game) {
        page = P_GAME;
    } else if (party_game() == PARTY_GAME_TUG && (party_state() == PARTY_HOSTING || party_state() == PARTY_JOINING
                                                   || party_state() == PARTY_JOINED)) {
        page = P_LOBBY;  /* The lobby goes on */
        party_set_handler(handler);
    } else {
        page = P_HOME;
    }
    list_ts = 0;
    changed = true;
}

/* Another group game runs on this badge (party.c has a single party at a time) */
static const char *busy_game(void) {
    if (party_state() != PARTY_STARTED || party_game() == PARTY_GAME_TUG)
        return NULL;
    return party_game() == PARTY_GAME_ASSASSIN ? N_("Assassin") : party_game() == PARTY_GAME_WEREWOLF ? N_("Loup-garou")
           : "?";
}

static void back_home(void) {
    if (party_game() == PARTY_GAME_TUG && party_state() != PARTY_IDLE)
        party_leave();  /* Also after the result: the service would see the party started again */
    reset_game();
    page = P_HOME;
    sel = 0;
    changed = true;
}

static bool tug_buttons(const app_buttons_t *b, absolute_time_t now) {
    if ((b->long_pressed & UI_BTN_A) && ! (page == P_GAME && phase == PH_PULL))
        return false;  /* Leaves the page: the party and the game go on */
    switch (page) {
    case P_HOME:
        if (b->pressed & UI_BTN_A)
            return false;
        if (b->pressed & (UI_BTN_X | UI_BTN_Y))
            sel ^= 1;
        if ((b->pressed & UI_BTN_B) && ! busy_game()) {
            party_set_handler(handler);
            if (sel == 0) {
                party_host(PARTY_GAME_TUG, true, PARTY_MAX, 0);
                printf("tug: hosting\n");
                page = P_LOBBY;
            } else {
                party_scan(PARTY_GAME_TUG);
                printf("tug: scanning\n");
                n_found = 0;
                sel = 0;
                list_ts = 0;
                page = P_SCAN;
            }
        }
        break;
    case P_SCAN:
        if (b->released_short & UI_BTN_A) {
            party_leave();
            page = P_HOME;
            sel = 1;
        }
        if (n_found && (b->pressed & UI_BTN_Y))
            sel = (sel + n_found - 1) % n_found;
        if (n_found && (b->pressed & UI_BTN_X))
            sel = (sel + 1) % n_found;
        if (n_found && (b->pressed & UI_BTN_B)) {
            party_set_handler(handler);
            party_join(found[sel].host);
            printf("tug: joining %s\n", found[sel].name);
            page = P_LOBBY;
        }
        break;
    case P_LOBBY:
        if (party_state() == PARTY_CANCELLED || party_state() == PARTY_IDLE) {
            if (b->pressed & (UI_BTN_A | UI_BTN_B))
                back_home();
            break;
        }
        if (b->released_short & UI_BTN_A) {
            printf("tug: left the lobby\n");
            back_home();
            break;
        }
        if ((b->pressed & UI_BTN_B) && party_state() == PARTY_HOSTING && party_count() >= MIN_PLAYERS) {
            printf("tug: start, %d players\n", party_count());
            party_start(START_DELAY_MS);
            tug_service(now);
        }
        break;
    case P_GAME:
        if (phase == PH_PULL && ready && team[me] != TUG_REFEREE && ! stopped) {
            if (tug_pull(&puller, b->pressed & UI_BTN_A, b->pressed & UI_BTN_B)) {
                ++my_pulls;
                counts[me] = my_pulls;
                changed = true;
                if (my_pulls % 10 == 0)
                    printf("tug: %u pulls\n", my_pulls);
            }
        } else if ((phase == PH_RESULT || cancelled) && (b->pressed & UI_BTN_B)) {
            back_home();
        } else if ((phase == PH_RESULT || cancelled) && (b->released_short & UI_BTN_A)) {
            back_home();
            return false;
        }
        break;
    }
    return true;
}

static bool tug_task(absolute_time_t now) {
    tug_service(now);
    if (page == P_SCAN && absolute_time_diff_us(list_ts, now) > 1000000) {
        list_ts = now;
        int k = party_found(found, PARTY_MAX_OPEN);
        if (k != n_found)
            changed = true;
        n_found = k;
        if (sel >= n_found)
            sel = n_found ? n_found - 1 : 0;
        changed = true;
    }
    if (page == P_LOBBY) {
        if (party_state() != last_party) {
            last_party = party_state();
            changed = true;
        }
        if (absolute_time_diff_us(list_ts, now) > 1000000) {
            list_ts = now;
            n_lobby = party_players(lobby, PARTY_MAX);
            changed = true;
        }
    }
    if (! changed)
        return false;
    if (page == P_GAME && phase == PH_PULL && absolute_time_diff_us(redraw_ts, now) < PULL_REDRAW_MS * 1000ll)
        return false;  /* Not faster than the screen */
    redraw_ts = now;
    changed = false;
    return true;
}

#define NAME_ROW_H 19

/* Names in 2 columns from \p y (the players of a team, of the lobby); "+N" when they don't fit */
static void draw_names(uint8_t *fb, int y, const char (*list)[9], int count) {
    int rows = (UI_FOOTER_Y - 2 - y) / NAME_ROW_H;
    char text[24], fitted[24];
    for (int i = 0; i < count && i < 2 * rows; ++i) {
        if (i == 2 * rows - 1 && count > 2 * rows)
            snprintf(text, sizeof(text), _("+%d autres"), count - i);
        else
            snprintf(text, sizeof(text), "%s", list[i]);
        ui_fit_preview(&gfx_font_small, fitted, sizeof(fitted), text, GFX_WIDTH/2 - 10);
        gfx_text(fb, i % 2 ? GFX_WIDTH/2 + 4 : 6, y + i / 2 * NAME_ROW_H, &gfx_font_small, fitted, GFX_BLACK,
                 GFX_ALIGN_LEFT);
    }
}

static void render_lobby(uint8_t *fb) {
    static char list[PARTY_MAX][9];
    char text[48];
    for (int i = 0; i < n_lobby; ++i)
        snprintf(list[i], sizeof(list[i]), "%s", lobby[i].id ? lobby[i].name : "...");
    switch (party_state()) {
    case PARTY_HOSTING:
        ui_title(fb, N_("Ta partie"));
        snprintf(text, sizeof(text), _("%d joueur%s"), n_lobby, n_lobby > 1 ? "s" : "");
        ui_lines(fb, UI_TITLE_H + 4, &gfx_font_small, n_lobby >= MIN_PLAYERS ? text
                 : N_("Il faut 2 joueurs au moins."));
        draw_names(fb, UI_TITLE_H + 24, (const char (*)[9])list, n_lobby);
        ui_footer(fb, n_lobby >= MIN_PLAYERS ? N_("G : annuler  D : lancer") : N_("G : annuler"));
        break;
    case PARTY_JOINING:
        ui_title(fb, N_("Rejoindre"));
        ui_lines(fb, 70, &gfx_font_small, N_("Connexion..."));
        ui_footer(fb, N_("G : annuler"));
        break;
    case PARTY_JOINED:
        ui_title(fb, N_("Salle d'attente"));
        snprintf(text, sizeof(text), _("Partie de %s"), party_name(party_host_id()));
        ui_lines(fb, UI_TITLE_H + 2, &gfx_font_small, text);
        ui_lines(fb, UI_TITLE_H + 21, &gfx_font_small, N_("En attente du lancement..."));
        draw_names(fb, UI_TITLE_H + 44, (const char (*)[9])list, n_lobby);
        ui_footer(fb, N_("G : quitter"));
        break;
    default:
        ui_title(fb, N_("Tir à la corde"));
        ui_lines(fb, 60, &gfx_font_small, N_("Partie annulée par l'hôte\n(ou lancée sans toi)."));
        ui_footer(fb, N_("D : OK"));
        break;
    }
}

static void draw_rope(uint8_t *fb) {
    const int y = 100, half = 60;
    sum_teams();
    if (team[me] < 2) {  /* This badge's team, inverted */
        int w = gfx_text_width(&gfx_font_small, TEAMS[team[me]]) + 6;
        int x = team[me] == TUG_CIGALES ? 2 : GFX_WIDTH - 2 - w;
        gfx_fill_rect(fb, x, 58, w, gfx_font_small.height + 2, GFX_BLACK);
    }
    gfx_text(fb, 5, 59, &gfx_font_small, TEAMS[0], team[me] == TUG_CIGALES ? GFX_WHITE : GFX_BLACK, GFX_ALIGN_LEFT);
    gfx_text(fb, GFX_WIDTH - 5, 59, &gfx_font_small, TEAMS[1], team[me] == TUG_FOURMIS ? GFX_WHITE : GFX_BLACK,
             GFX_ALIGN_RIGHT);
    gfx_fill_rect(fb, 4, y - 1, GFX_WIDTH - 8, 3, GFX_BLACK);  /* The rope */
    gfx_fill_rect(fb, GFX_WIDTH/2 - half - 1, y - 14, 3, 28, GFX_BLACK);  /* The marks */
    gfx_fill_rect(fb, GFX_WIDTH/2 + half - 1, y - 14, 3, 28, GFX_BLACK);
    gfx_fill_rect(fb, GFX_WIDTH/2, y + 6, 1, 6, GFX_BLACK);  /* The middle */
    int x = GFX_WIDTH/2 + tug_knot((int32_t)totals[0] - (int32_t)totals[1], tug_margin(team_size()), half, 88);
    gfx_fill_rect(fb, x - 6, y - 10, 12, 21, GFX_BLACK);  /* The knot */
    gfx_rect(fb, x - 8, y - 12, 16, 25, GFX_BLACK);
    char text[16];
    snprintf(text, sizeof(text), "%lu", (unsigned long)totals[0]);
    gfx_text(fb, 5, y + 18, &gfx_font_medium, text, GFX_BLACK, GFX_ALIGN_LEFT);
    snprintf(text, sizeof(text), "%lu", (unsigned long)totals[1]);
    gfx_text(fb, GFX_WIDTH - 5, y + 18, &gfx_font_medium, text, GFX_BLACK, GFX_ALIGN_RIGHT);
}

static void render_game(uint8_t *fb, absolute_time_t now) {
    char text[64];
    if (cancelled && phase != PH_RESULT) {
        ui_title(fb, N_("Tir à la corde"));
        ui_lines(fb, 60, &gfx_font_small, N_("Partie annulée par l'hôte."));
        ui_footer(fb, N_("G : retour  D : OK"));
        return;
    }
    if (! ready) {
        ui_title(fb, N_("Tir à la corde"));
        ui_lines(fb, 60, &gfx_font_small, N_("Réception de la liste\ndes joueurs..."));
        ui_footer(fb, N_("G long : quitter la page"));
        return;
    }
    bool referee = team[me] == TUG_REFEREE;
    switch (phase) {
    case PH_WAIT:
    case PH_TEAMS: {
        ui_title(fb, N_("Les équipes"));
        if (referee) {
            ui_lines(fb, UI_TITLE_H + 8, &gfx_font_medium, N_("Tu es l'arbitre !"));
            ui_wrapped(fb, UI_TITLE_H + 40, &gfx_font_small, N_("Nombre impair de joueurs : tu ne tires pas, "
                       "tu regardes la corde."), 4);
        } else {
            ui_lines(fb, UI_TITLE_H + 4, &gfx_font_small, N_("Tu es dans l'équipe des"));
            ui_lines(fb, UI_TITLE_H + 22, &gfx_font_medium, TEAMS[team[me]]);
            static char mates[PARTY_MAX][9];
            int k = 0;
            for (int i = 0; i < n; ++i)
                if (team[i] == team[me] && i != me)
                    snprintf(mates[k++], sizeof(mates[0]), "%s", names[i]);
            ui_lines(fb, UI_TITLE_H + 46, &gfx_font_small, k ? N_("Avec :") : N_("Seul contre tous !"));
            draw_names(fb, UI_TITLE_H + 66, (const char (*)[9])mates, k);
        }
        ui_footer(fb, N_("Prépare-toi..."));
        break;
    }
    case PH_COUNTDOWN:
        ui_title(fb, N_("Prêts ?"));
        snprintf(text, sizeof(text), "%d", countdown_shown);
        gfx_text(fb, GFX_WIDTH/2, 70, &gfx_font_large, text, GFX_BLACK, GFX_ALIGN_CENTER);
        snprintf(text, sizeof(text), referee ? _("Tu es l'arbitre") : _("Équipe des %s"), tr(TEAMS[team[me]]));
        ui_lines(fb, 120, &gfx_font_small, text);
        ui_footer(fb, referee ? N_("Regarde la corde") : N_("G puis D : une traction"));
        break;
    case PH_PULL:
    case PH_FINAL: {
        int64_t left = absolute_time_diff_us(now, delayed_by_ms(pull_start(), PULL_MS));
        if (phase == PH_PULL)
            snprintf(text, sizeof(text), _("Tirez ! %d s"), left > 0 ? (int)((left + 999999) / 1000000) : 0);
        else
            snprintf(text, sizeof(text), N_("Terminé !"));
        ui_title(fb, text);
        draw_rope(fb);
        if (referee)
            snprintf(text, sizeof(text), N_("Arbitre"));
        else
            snprintf(text, sizeof(text), _("Toi : %u traction%s"), my_pulls, my_pulls > 1 ? "s" : "");
        ui_lines(fb, 150, &gfx_font_small, text);
        ui_footer(fb, phase == PH_FINAL ? N_("Décompte final...") : referee ? N_("Regarde la corde")
                  : N_("G puis D, vite !"));
        break;
    }
    default:
        ui_title(fb, N_("Résultat"));
        if (winner < 0)
            snprintf(text, sizeof(text), N_("Égalité !"));
        else
            snprintf(text, sizeof(text), _("Victoire des\n%s !"), tr(TEAMS[winner]));
        int y = ui_lines(fb, UI_TITLE_H + 4, &gfx_font_medium, text);
        if (! referee && winner >= 0)
            y = ui_lines(fb, y + 2, &gfx_font_small, team[me] == winner ? N_("Ton équipe a gagné !") :
                         N_("Ton équipe a perdu..."));
        snprintf(text, sizeof(text), _("Cigales %lu - %lu Fourmis"), (unsigned long)final_totals[0],
                 (unsigned long)final_totals[1]);
        y = ui_lines(fb, y + 4, &gfx_font_small, text);
        if (! referee) {
            snprintf(text, sizeof(text), _("Tes tractions : %u"), my_pulls);
            y = ui_lines(fb, y, &gfx_font_small, text);
        }
        if (best >= 0) {
            snprintf(text, sizeof(text), _("Meilleur : %u, %s"), counts[best], names[best]);
            ui_lines(fb, y, &gfx_font_small, text);
        }
        ui_footer(fb, N_("G : retour  D : rejouer"));
        break;
    }
}

static void tug_render(uint8_t *fb, absolute_time_t now) {
    switch (page) {
    case P_HOME: {
        const char *busy = busy_game();
        ui_title(fb, N_("Tir à la corde"));
        if (busy) {
            char text[80];
            snprintf(text, sizeof(text), _("Une partie de %s est en cours sur ton badge : termine-la ou quitte-la "
                     "d'abord."), tr(busy));
            ui_wrapped(fb, 60, &gfx_font_small, text, 5);
            ui_footer(fb, N_("G : retour"));
        } else {
            ui_list(fb, 2, sel, home_label);
            ui_wrapped(fb, UI_TITLE_H + 2 * UI_ROW_H + 16, &gfx_font_small, N_("Deux équipes tirent sur la corde : "
                       "appuie sur G puis D, le plus vite possible !"), 4);
            ui_footer(fb, N_("G : retour  D : choisir"));
        }
        break;
    }
    case P_SCAN:
        ui_title(fb, N_("Rejoindre"));
        if (n_found) {
            ui_list(fb, n_found, sel, found_label);
            ui_footer(fb, N_("G : retour  D : rejoindre"));
        } else {
            ui_lines(fb, 60, &gfx_font_small, N_("Recherche des parties\nde tir à la corde..."));
            ui_footer(fb, N_("G : retour"));
        }
        break;
    case P_LOBBY:
        render_lobby(fb);
        break;
    default:
        render_game(fb, now);
        break;
    }
}

static bool tug_calm(void) {
    return page != P_GAME || phase == PH_WAIT || phase == PH_RESULT || cancelled;
}

static void tug_stop(void) {
    app_leds(0, 0, 0);
    leds_off_ts = 0;
}

const app_t app_tug = {
    .name = N_("Tir à la corde"),
    .start = tug_start,
    .buttons = tug_buttons,
    .task = tug_task,
    .render = tug_render,
    .calm = tug_calm,
    .stop = tug_stop,
    .no_saver = true,
};
