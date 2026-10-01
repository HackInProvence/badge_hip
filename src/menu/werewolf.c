/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Loup-garou (werewolf / mafia): a narrator ("meneur", who does not play) opens a party (party.c), 5 to 20 players
 * join; the badge of the narrator deals the roles and drives the game, phase by phase (night: Cupidon, the wolves,
 * the seer, the witch; day: dawn, debate, vote, verdict; the hunter when he dies). The rules are in
 * werewolf_logic.c and in docs/fr/loup_garou.md.
 *
 * Secrecy in a room: during each night phase, every living player chooses in a list (the villagers pretend), so
 * that nobody can tell who acts by looking at who presses buttons, and every player receives the same kind of
 * packet. The roles and the actions travel XORed with a hash of the random key of the player (party_mask()), known
 * by the narrator and the player only.
 *
 * Messages (party_send(), kinds from PARTY_KIND_GAME):
 * - STATE (narrator, every second, 3 times at each phase change): [phase][step][day][players][flags][seconds 2]
 *   [alive 4][acted 4][roles of the dead 10, nibbles, 0xF = secret][winner][hunter][deaths count][deaths 6]
 *   [winners 4][lovers 2, at the end only]. The step counts the phases: a new step resets the choices.
 * - NAMES (narrator, every 3 s, fast during the first seconds): [first][players][3 x (id 4, name 8)]
 *   (the robots of the test mode are not in the roster of the party).
 * - PRIV (narrator -> a player): [nonce 4][masked: step, role, lover, wolves 4, ack, index, info 2, wolf votes 4,
 *   potions]. Sent to every living player at each phase change, to everybody when a wolf votes, and in answer to
 *   each ACT (the answer is the acknowledgement).
 * - ACT (player -> narrator): [nonce 4][masked: step, seq, a, b, 0 x 4]; seq 0: "send me my information".
 *   Sent again until the PRIV of the step acknowledges the seq: the radio loses packets, duplicates are ignored.
 * - ABORT (narrator, 5 times, then the LEAVE of party.c): the narrator stopped the game.
 *
 * Test mode: when the narrator badge is in admin mode, a party starts with a single player, and robots complete
 * it up to the minimum (tools/test_werewolf.py with 2 badges). */

#include <stdio.h>
#include <string.h>

#include "pico/rand.h"

#include "achievements.h"
#include "app.h"
#include "net.h"
#include "party.h"
#include "social.h"
#include "store.h"
#include "werewolf_logic.h"

extern const app_t app_werewolf;
void werewolf_service(absolute_time_t now);
bool werewolf_event(char *buf, int len);
static void abort_task(absolute_time_t now);

#define K_STATE (PARTY_KIND_GAME + 0)
#define K_NAMES (PARTY_KIND_GAME + 1)
#define K_PRIV (PARTY_KIND_GAME + 2)
#define K_ACT (PARTY_KIND_GAME + 3)
#define K_ABORT (PARTY_KIND_GAME + 4)
#define K_PARTY_LEAVE 5  /* The LEAVE of party.c, given to the handler during the game: a player left */

#define START_DELAY_MS 3000
#define STATE_MS 1000
#define STATE_BURST 3
#define STATE_BURST_MS 300
#define NAMES_FAST_MS 400
#define NAMES_SLOW_MS 3000
#define NAMES_FAST_FOR_MS 15000
#define PRIV_GAP_MS 40
#define QUEUE_KEEP 3  /* Free places left in the radio queue for the others (beacon, party.c) */
#define ACT_RESEND_MS 1200
#define ACT_JITTER_MS 800
#define ALL_ACTED_GRACE_MS 2000  /* Everybody chose: the phase ends a moment later */
#define ADD_TIME_S 30
#define LOST_MS 20000
#define PEEK_MS 5000
#define MAX_DEATHS 6
#define BOT_ID 0xB0700000u
#define ABORT_REPEATS 5
#define ABORT_GAP_MS 300
#define NO_TIMER 0xFFFF

#define FLAG_ADVANCED 0x01
#define FLAG_DEBATE_SHIFT 1  /* 2 bits: index in DEBATE_S */
#define FLAG_DEBUG 0x80  /* Test mode: robots complete the party */

static const uint16_t DEBATE_S[3] = {120, 180, 300};

enum { P_NONE, P_ROLES, P_CUPID, P_WOLVES, P_SEER, P_WITCH, P_DAWN, P_HUNTER, P_SHOT, P_DEBATE, P_VOTE, P_VERDICT,
       P_END, P_COUNT };

typedef struct {
    const char *title;  /* After "Nuit n : " / "Jour n : " */
    const char *log;
    uint16_t secs;
    bool night;
    const char *hint;  /* For the narrator */
} phase_t;

static const phase_t PHASES[P_COUNT] = {
    [P_NONE] = {"Début", "start", 0, false, "La partie commence..."},
    [P_ROLES] = {"Rôles", "roles", 30, false, "Chacun lit son rôle en cachant son écran."},
    [P_CUPID] = {"Cupidon", "cupid", 30, true, "La nuit tombe. Tous choisissent 2 noms : seul Cupidon compte."},
    [P_WOLVES] = {"Loups", "wolves", 45, true, "Tous choisissent une cible : seuls les loups comptent."},
    [P_SEER] = {"Voyante", "seer", 30, true, "Tous choisissent : seule la voyante découvre un rôle."},
    [P_WITCH] = {"Sorcière", "witch", 30, true, "Tous choisissent : seule la sorcière agit."},
    [P_DAWN] = {"aube", "dawn", 15, false, "Le jour se lève : annoncez les morts de la nuit."},
    [P_HUNTER] = {"Chasseur", "hunter", 30, false, "Le chasseur est mort : il emporte quelqu'un."},
    [P_SHOT] = {"Chasseur", "shot", 10, false, "Annoncez la victime du chasseur."},
    [P_DEBATE] = {"débat", "debate", 0, false, "Le village débat : qui sont les loups ?"},
    [P_VOTE] = {"vote", "vote", 45, false, "Chacun vote sur son badge."},
    [P_VERDICT] = {"verdict", "verdict", 12, false, "Annoncez l'éliminé et son rôle."},
    [P_END] = {"Fin", "end", 0, false, ""},
};

static const char *ROLE_HELP[WW_ROLES] = {
    "Trouve les loups et fais-les éliminer au vote du jour.",
    "Chaque nuit, dévore un villageois avec les autres loups. Reste discret !",
    "Chaque nuit, tu découvres le rôle d'un joueur.",
    "Une potion de vie, une potion de mort : une fois chacune dans la partie.",
    "À ta mort, tu emportes un joueur avec toi.",
    "La première nuit, tu lies deux amoureux : si l'un meurt, l'autre aussi.",
};

enum { M_NONE, M_NARRATOR, M_PLAYER };
enum { PG_MENU, PG_SETUP, PG_LOBBY, PG_SCAN, PG_WAIT, PG_GAME, PG_ROLES, PG_QUIT };

/* Values of the rows of the lists of choices (below WW_MAX: a player) */
#define ROW_NOTHING 0xFE  /* Vote blanc / ne rien faire */
#define ROW_HEAL 0xFD
#define ROW_POISON 0x80  /* | the player */

/* ------ Shared by the narrator and the players: the game as shown ------ */

static int mode = M_NONE;
static int page = PG_MENU, quit_back = PG_MENU;
static bool changed = false;
static uint8_t flags = 0;
static char names[WW_MAX][10];
static uint32_t ids[WW_MAX];

static bool have_state = false;
static uint8_t phase = P_NONE, step = 0, day = 0, n_players = 0, winner = WW_WIN_NONE, hunter = WW_NONE;
static uint32_t alive = 0, acted = 0, winners = 0;
static uint8_t revealed[WW_MAX];  /* The roles of the dead (everybody's at the end), WW_NONE: secret */
static uint8_t deaths[MAX_DEATHS], n_deaths = 0;
static uint8_t lovers_end[2] = {WW_NONE, WW_NONE};
static bool timed = false;
static absolute_time_t deadline = 0;
static int list_sel = 0;  /* Selected row (lists of the pages) */
static uint8_t notified_step = 0;  /* werewolf_event() */
static bool notify_pending = false;

/* The chimes: {hz, ms} pairs, 0 to end */
static const uint16_t TUNE_NIGHT[] = {784, 220, 659, 220, 523, 450, 0};
static const uint16_t TUNE_DAY[] = {523, 180, 659, 180, 784, 400, 0};
static const uint16_t TUNE_STEP[] = {880, 120, 0};
static const uint16_t TUNE_END[] = {523, 150, 659, 150, 784, 150, 1047, 500, 0};
static const uint16_t *tune = NULL;
static absolute_time_t tune_ts = 0;

static bool debug_mode(void) {
    return flags & FLAG_DEBUG;
}

static bool advanced(void) {
    return flags & FLAG_ADVANCED;
}

static uint16_t debate_secs(void) {
    int i = (flags >> FLAG_DEBATE_SHIFT) & 3;
    return DEBATE_S[i < 3 ? i : 1];
}

static int min_players(void) {
    return debug_mode() ? 1 : ww_min_players(advanced());
}

static const char *pname(int i) {
    return i >= 0 && i < WW_MAX && names[i][0] ? names[i] : "?";
}

static bool is_alive(int i) {
    return i >= 0 && i < n_players && (alive >> i & 1);
}

static void default_names(void) {
    for (int i = 0; i < WW_MAX; ++i) {
        snprintf(names[i], sizeof(names[i]), "Joueur %d", i + 1);
        ids[i] = 0;
    }
}

/* XOR with a hash of the key of the player and of a random nonce (sent in clear) */
static void mask(uint8_t *d, int len, uint32_t key, uint32_t nonce) {
    for (int j = 0; j < len; j += 4) {
        uint32_t m = party_mask(key ^ nonce, j / 4);
        for (int k = 0; k < 4 && j + k < len; ++k)
            d[j + k] ^= m >> (8 * k);
    }
}

static void phase_title(char *buf, size_t len) {
    if (phase == P_END)
        snprintf(buf, len, "Fin de partie");
    else if (phase == P_HUNTER || phase == P_SHOT)
        snprintf(buf, len, "Le chasseur");
    else if (PHASES[phase].night)
        snprintf(buf, len, "Nuit %u : %s", day, PHASES[phase].title);
    else if (phase >= P_DAWN)
        snprintf(buf, len, "Jour %u : %s", day, PHASES[phase].title);
    else if (phase == P_ROLES)
        snprintf(buf, len, "Les rôles");
    else
        snprintf(buf, len, "Loup-garou");
}

static int secs_left(absolute_time_t now) {
    if (! timed)
        return -1;
    int64_t us = absolute_time_diff_us(now, deadline);
    return us > 0 ? (int)((us + 999999) / 1000000) : 0;
}

static void play(const uint16_t *t) {
    tune = t;
    tune_ts = get_absolute_time();
}

static void tune_task(absolute_time_t now) {
    if (! tune || absolute_time_diff_us(tune_ts, now) < 0)
        return;
    if (! tune[0]) {
        tune = NULL;
        return;
    }
    app_tone(tune[0], tune[1]);
    tune_ts = delayed_by_ms(now, tune[1] + 30);
    tune += 2;
}

/* A new phase on this badge: chime, notification, log of the deaths */
static void new_phase(uint8_t prev) {
    if (phase == P_END)
        play(TUNE_END);
    else if (PHASES[phase].night && ! PHASES[prev].night)
        play(TUNE_NIGHT);
    else if (phase == P_DAWN)
        play(TUNE_DAY);
    else
        play(TUNE_STEP);
    notify_pending = true;
    list_sel = 0;
    changed = true;
}

static void log_deaths(void) {
    for (int i = 0; i < n_deaths; ++i)
        printf("werewolf: death %s (%s)\n", pname(deaths[i]), ww_role_name(revealed[deaths[i]]));
    if (! n_deaths && (phase == P_DAWN || phase == P_VERDICT || phase == P_SHOT))
        printf("werewolf: no death\n");
}


/* ------ Narrator ------ */

static ww_game_t g;
static uint32_t keys[WW_MAX];
static int n_real = 0;  /* The players of the roster; the robots follow */
static uint8_t ch_a[WW_MAX], ch_b[WW_MAX], ch_seq[WW_MAX];
static uint32_t pending = 0;  /* PRIV to send */
static int priv_next = 0, names_next = 0, burst = 0;
static absolute_time_t state_ts = 0, names_ts = 0, priv_ts = 0, game_ts = 0, grace_ts = 0;
static bool grace = false;
static uint8_t victim = WW_NONE, seer_target = WW_NONE, after_shot = P_DEBATE;
static bool hunter_died = false;
static uint8_t dead_hunter = WW_NONE;  /* Shoots in P_HUNTER */
/* Lobby and setup */
static int setup_row = 0;
static bool set_advanced = false;
static int set_debate = 1;
static party_player_t lobby[PARTY_MAX];
static int n_lobby = 0;

static uint32_t real_mask(void) {
    return n_real >= 32 ? 0xFFFFFFFFu : (1u << n_real) - 1;
}

static uint8_t rand_alive(uint32_t except) {
    int n = 0;
    uint8_t c[WW_MAX];
    for (int i = 0; i < g.n; ++i)
        if (ww_alive(&g, i) && ! (except >> i & 1))
            c[n++] = i;
    return n ? c[get_rand_32() % n] : WW_NONE;
}

static uint32_t expected(void) {
    switch (phase) {
    case P_ROLES: case P_CUPID: case P_WOLVES: case P_SEER: case P_WITCH: case P_VOTE:
        return g.alive;
    case P_HUNTER:
        return hunter < WW_MAX ? 1u << hunter : 0;
    default:
        return 0;
    }
}

/* The view (what STATE sends) follows the game */
static void sync_view(void) {
    alive = g.alive;
    n_players = g.n;
    for (int i = 0; i < g.n; ++i)
        revealed[i] = phase == P_END || ! ww_alive(&g, i) ? g.role[i] : WW_NONE;
}

/* The robots of the test mode choose at once: the wolves at random, the others nothing (blank votes) */
static void bots_act(void) {
    for (int i = n_real; i < g.n; ++i) {
        if (! (expected() >> i & 1))
            continue;
        ch_seq[i] = 1;
        ch_a[i] = ch_b[i] = WW_NONE;
        if (phase == P_WOLVES && g.role[i] == WW_WOLF)
            ch_a[i] = rand_alive(ww_wolves(&g));
        else if (phase == P_CUPID && g.role[i] == WW_CUPID) {
            ch_a[i] = rand_alive(0);
            ch_b[i] = rand_alive(1u << ch_a[i]);
        } else if (phase == P_HUNTER)
            ch_a[i] = rand_alive(0);
        acted |= 1u << i;
    }
}

static void enter(uint8_t p, absolute_time_t now) {
    uint8_t prev = phase;
    phase = p;
    if (! ++step)
        step = 1;
    memset(ch_seq, 0, sizeof(ch_seq));
    memset(ch_a, WW_NONE, sizeof(ch_a));
    memset(ch_b, WW_NONE, sizeof(ch_b));
    acted = 0;
    grace = false;
    uint16_t secs = p == P_DEBATE ? debate_secs() : PHASES[p].secs;
    timed = secs > 0;
    deadline = delayed_by_ms(now, secs * 1000u);
    if (p != P_DAWN && p != P_VERDICT && p != P_SHOT)
        n_deaths = 0;
    hunter = p == P_HUNTER || p == P_SHOT ? dead_hunter : WW_NONE;
    sync_view();
    bots_act();
    pending |= (expected() | (p == P_ROLES ? 0xFFFFFFFFu : 0)) & real_mask();
    burst = STATE_BURST;
    state_ts = now;
    printf("werewolf: phase %s, day %u, %d alive\n", PHASES[p].log, day, ww_count_alive(&g));
    new_phase(prev);
}

static void set_deaths(const uint8_t *d, int n) {
    hunter_died = false;
    n_deaths = 0;
    for (int i = 0; i < n; ++i) {
        if (n_deaths < MAX_DEATHS)
            deaths[n_deaths++] = d[i];
        if (g.role[d[i]] == WW_HUNTER) {
            hunter_died = true;
            dead_hunter = d[i];
        }
    }
    sync_view();
}

static void end_game(absolute_time_t now) {
    winner = ww_winner(&g);
    winners = 0;
    for (int i = 0; i < g.n; ++i)
        if (ww_is_winner(&g, i, winner))
            winners |= 1u << i;
    lovers_end[0] = g.lovers[0];
    lovers_end[1] = g.lovers[1];
    enter(P_END, now);
    sync_view();
    timed = false;
    printf("werewolf: end, %s\n", ww_win_text(winner));
}

static void start_night(absolute_time_t now) {
    ++day;
    victim = seer_target = WW_NONE;
    int c = ww_find(&g, WW_CUPID);
    enter(day == 1 && c >= 0 && ww_alive(&g, c) ? P_CUPID : P_WOLVES, now);
}

/* The phases of the night that follow the wolves (a dead role has no phase: its death was announced) */
static void night_after(uint8_t p, absolute_time_t now) {
    int seer = ww_find(&g, WW_SEER), witch = ww_find(&g, WW_WITCH);
    if (p < P_SEER && ww_alive(&g, seer)) {
        enter(P_SEER, now);
        return;
    }
    if (p < P_WITCH && ww_alive(&g, witch)) {
        enter(P_WITCH, now);
        return;
    }
    /* Dawn: the potions of the witch, then the deaths */
    bool heal = false;
    int poison = WW_NONE;
    if (p == P_WITCH && ww_alive(&g, witch) && ch_seq[witch]) {
        if (ch_a[witch] == 1 && victim != WW_NONE && ! g.heal_used) {
            heal = true;
            printf("werewolf: the witch heals %s\n", pname(victim));
        } else if (ch_a[witch] == 2 && ww_alive(&g, ch_b[witch]) && ! g.poison_used) {
            poison = ch_b[witch];
            printf("werewolf: the witch poisons %s\n", pname(poison));
        }
    }
    uint8_t d[WW_MAX];
    int n = ww_dawn(&g, victim, heal, poison, d);
    set_deaths(d, n);
    enter(P_DAWN, now);
    log_deaths();
}

/* After the deaths of the dawn or of the verdict: the hunter shoots, the game ends or goes on */
static void after_deaths(uint8_t next, absolute_time_t now) {
    if (hunter_died) {
        hunter_died = false;
        after_shot = next;
        enter(P_HUNTER, now);
    } else if (ww_winner(&g) != WW_WIN_NONE) {
        end_game(now);
    } else if (next == P_DEBATE) {
        enter(P_DEBATE, now);
    } else {
        start_night(now);
    }
}

static void next_phase(absolute_time_t now) {
    uint8_t d[WW_MAX];
    switch (phase) {
    case P_NONE:
        enter(P_ROLES, now);
        break;
    case P_ROLES:
        start_night(now);
        break;
    case P_CUPID: {
        int c = ww_find(&g, WW_CUPID);
        if (c >= 0 && ch_seq[c] && ww_alive(&g, ch_a[c]) && ww_alive(&g, ch_b[c]) && ww_link(&g, ch_a[c], ch_b[c]))
            printf("werewolf: lovers %s and %s\n", pname(ch_a[c]), pname(ch_b[c]));
        else
            printf("werewolf: no lovers\n");
        enter(P_WOLVES, now);
        break;
    }
    case P_WOLVES: {
        uint8_t votes[WW_MAX];
        int n = 0;
        for (int i = 0; i < g.n; ++i)
            if (ww_alive(&g, i) && g.role[i] == WW_WOLF) {
                uint8_t t = ch_seq[i] ? ch_a[i] : WW_NONE;
                votes[n++] = ww_alive(&g, t) && g.role[t] != WW_WOLF ? t : WW_NONE;
            }
        victim = ww_tally(votes, n, g.n, get_rand_32);
        printf("werewolf: the wolves chose %s\n", victim == WW_NONE ? "nobody" : pname(victim));
        night_after(P_WOLVES, now);
        break;
    }
    case P_SEER:
    case P_WITCH:
        night_after(phase, now);
        break;
    case P_DAWN:
        after_deaths(P_DEBATE, now);
        break;
    case P_HUNTER: {
        int n = 0;
        if (hunter < WW_MAX && ch_seq[hunter] && ww_alive(&g, ch_a[hunter])) {
            n = ww_kill(&g, ch_a[hunter], d, 0);
            printf("werewolf: the hunter shoots %s\n", pname(ch_a[hunter]));
        } else {
            printf("werewolf: the hunter did not shoot\n");
        }
        set_deaths(d, n);
        hunter_died = false;
        enter(P_SHOT, now);
        log_deaths();
        break;
    }
    case P_SHOT:
        if (ww_winner(&g) != WW_WIN_NONE)
            end_game(now);
        else if (after_shot == P_DEBATE)
            enter(P_DEBATE, now);
        else
            start_night(now);
        break;
    case P_DEBATE:
        enter(P_VOTE, now);
        break;
    case P_VOTE: {
        uint8_t votes[WW_MAX];
        int n = 0;
        for (int i = 0; i < g.n; ++i)
            if (ww_alive(&g, i)) {
                uint8_t t = ch_seq[i] ? ch_a[i] : WW_NONE;
                votes[n++] = ww_alive(&g, t) ? t : WW_NONE;
                if (t != WW_NONE && ww_alive(&g, t))
                    printf("werewolf: vote %s -> %s\n", pname(i), pname(t));
            }
        int out = ww_tally(votes, n, g.n, NULL);  /* A tie: nobody */
        printf("werewolf: the village eliminates %s\n", out == WW_NONE ? "nobody" : pname(out));
        int k = out == WW_NONE ? 0 : ww_kill(&g, out, d, 0);
        set_deaths(d, k);
        enter(P_VERDICT, now);
        log_deaths();
        break;
    }
    case P_VERDICT:
        after_deaths(P_CUPID /* = the night */, now);
        break;
    default:
        break;
    }
}

static void send_priv(int i) {
    uint8_t d[20];
    uint32_t nonce = get_rand_32();
    net_put_u32(d, nonce);
    uint8_t *x = d + 4;
    memset(x, WW_NONE, 16);
    x[0] = step;
    x[1] = g.role[i];
    x[2] = g.lovers[0] == i ? g.lovers[1] : g.lovers[1] == i ? g.lovers[0] : WW_NONE;
    net_put_u32(x + 3, g.role[i] == WW_WOLF ? ww_wolves(&g) : 0);
    x[7] = ch_seq[i];
    x[8] = i;
    if (phase == P_SEER && g.role[i] == WW_SEER && seer_target != WW_NONE) {
        x[9] = seer_target;
        x[10] = g.role[seer_target];
    }
    if (phase == P_WITCH && g.role[i] == WW_WITCH)
        x[9] = victim;
    if (phase == P_WOLVES && g.role[i] == WW_WOLF) {
        int k = 0;
        for (int w = 0; w < g.n && k < 4; ++w)
            if (g.role[w] == WW_WOLF)
                x[11 + k++] = ww_alive(&g, w) && ch_seq[w] ? ch_a[w] : WW_NONE;
    }
    x[15] = g.role[i] == WW_WITCH ? (! g.heal_used) | (! g.poison_used) << 1 : 0;
    mask(x, 16, keys[i], nonce);
    party_send(K_PRIV, ids[i], d, sizeof(d));
}

static void send_state(void) {
    uint8_t d[40];
    memset(d, 0, sizeof(d));
    d[0] = phase;
    d[1] = step;
    d[2] = day;
    d[3] = g.n;
    d[4] = flags;
    int secs = secs_left(get_absolute_time());
    d[5] = secs < 0 ? 0xFF : secs;
    d[6] = secs < 0 ? 0xFF : secs >> 8;
    net_put_u32(d + 7, g.alive);
    net_put_u32(d + 11, acted);
    for (int i = 0; i < WW_MAX; ++i) {
        uint8_t r = i < g.n && revealed[i] < WW_ROLES ? revealed[i] : 0xF;
        d[15 + i / 2] |= i & 1 ? r << 4 : r;
    }
    d[25] = winner;
    d[26] = hunter;
    d[27] = n_deaths;
    memcpy(d + 28, deaths, MAX_DEATHS);
    net_put_u32(d + 34, winners);
    d[38] = phase == P_END ? lovers_end[0] : WW_NONE;
    d[39] = phase == P_END ? lovers_end[1] : WW_NONE;
    party_send(K_STATE, 0, d, sizeof(d));
}

static void send_names(void) {
    if (! g.n)
        return;
    int first = names_next * 3;
    if (first >= g.n)
        first = names_next = 0;
    ++names_next;
    uint8_t d[3 + 3 * 12];
    int k = 0;
    d[0] = first;
    d[1] = g.n;
    for (int i = first; i < g.n && k < 3; ++i, ++k) {
        net_put_u32(d + 3 + k * 12, ids[i]);
        memset(d + 3 + k * 12 + 4, 0, 8);
        memcpy(d + 3 + k * 12 + 4, names[i], strnlen(names[i], 8));
    }
    d[2] = k;
    party_send(K_NAMES, 0, d, 3 + k * 12);
}

static void narrator_handle(uint8_t kind, uint32_t from, const uint8_t *data, uint8_t len) {
    int i = party_index(from);
    if (i < 0 || i >= n_real)
        return;
    if (kind == K_PARTY_LEAVE) {
        if (ww_alive(&g, i) && phase != P_END && phase != P_NONE) {
            g.alive &= ~(1u << i);  /* No lover dies of grief, the hunter does not shoot: he left */
            sync_view();
            printf("werewolf: %s left the game\n", pname(i));
            changed = true;
            if (ww_winner(&g) != WW_WIN_NONE)
                end_game(get_absolute_time());
            else
                burst = STATE_BURST;
        }
        return;
    }
    if (kind != K_ACT || len < 12)
        return;
    uint8_t x[8];
    memcpy(x, data + 4, 8);
    mask(x, 8, keys[i], net_u32(data));
    pending |= 1u << i;  /* The answer: the acknowledgement and the information of the player */
    uint8_t s = x[0], seq = x[1], a = x[2], b = x[3];
    if (s != step || ! seq || seq <= ch_seq[i] || ! (expected() >> i & 1))
        return;
    ch_seq[i] = seq;
    ch_a[i] = a;
    ch_b[i] = b;
    if (! (acted >> i & 1))
        changed = true;
    acted |= 1u << i;
    if (phase == P_ROLES)
        printf("werewolf: %s (%s) is ready\n", pname(i), ww_role_name(g.role[i]));
    else
        printf("werewolf: %s (%s) chose %s%s%s\n", pname(i), ww_role_name(g.role[i]), a < WW_MAX ? pname(a) : "-",
               b < WW_MAX ? " and " : "", b < WW_MAX ? pname(b) : "");
    if (phase == P_SEER && g.role[i] == WW_SEER && seer_target == WW_NONE && ww_alive(&g, a) && a != i) {
        seer_target = a;  /* The first choice only: the seer sees one role per night */
        printf("werewolf: the seer sees %s (%s)\n", pname(a), ww_role_name(g.role[a]));
    }
    if (phase == P_WOLVES && g.role[i] == WW_WOLF)
        pending |= g.alive & real_mask();  /* The wolves see the votes of the others; everybody gets a packet alike */
}

static void narrator_launch(absolute_time_t now) {
    n_lobby = party_players(lobby, PARTY_MAX);
    n_real = n_lobby > WW_MAX ? WW_MAX : n_lobby;
    int n = n_real;
    if (debug_mode() && n < ww_min_players(advanced()))
        n = ww_min_players(advanced());
    default_names();
    for (int i = 0; i < n; ++i) {
        if (i < n_real) {
            snprintf(names[i], sizeof(names[i]), "%s", lobby[i].name);
            ids[i] = lobby[i].id;
            keys[i] = lobby[i].key;
        } else {
            snprintf(names[i], sizeof(names[i]), "Robot %u", (uint8_t)(i - n_real + 1));
            ids[i] = BOT_ID + i;
            keys[i] = 0;
        }
    }
    ww_deal(&g, n, advanced(), get_rand_32);  /* Not party_seed(): it is sent in clear */
    party_start(START_DELAY_MS);
    game_ts = party_start_time();
    if (! game_ts)
        game_ts = delayed_by_ms(now, START_DELAY_MS);
    phase = P_NONE;
    step = 0;
    day = 0;
    have_state = true;
    winner = WW_WIN_NONE;
    winners = 0;
    hunter = WW_NONE;
    n_deaths = 0;
    pending = 0;
    names_next = 0;
    names_ts = now;
    timed = false;
    lovers_end[0] = lovers_end[1] = WW_NONE;
    sync_view();
    printf("werewolf: launch, %d players (%d robots), %s mode\n", n, n - n_real, advanced() ? "advanced" : "simple");
    for (int i = 0; i < n; ++i)
        printf("werewolf: %s is %s\n", names[i], ww_role_name(g.role[i]));
    page = PG_GAME;
    list_sel = 0;
}

static void narrator_task(absolute_time_t now) {
    if (page == PG_LOBBY || phase == P_NONE) {
        int c = party_count();
        if (c != n_lobby) {
            n_lobby = party_players(lobby, PARTY_MAX);
            changed = true;
        }
    }
    if (party_state() != PARTY_STARTED)
        return;
    if (phase == P_NONE) {
        if (absolute_time_diff_us(game_ts, now) >= 0)
            next_phase(now);
    } else if (phase != P_END) {
        if (timed && absolute_time_diff_us(deadline, now) >= 0) {
            next_phase(now);
        } else {
            uint32_t e = expected();
            if (e && (acted & e) == e) {
                if (! grace) {
                    grace = true;
                    grace_ts = delayed_by_ms(now, ALL_ACTED_GRACE_MS);
                } else if (absolute_time_diff_us(grace_ts, now) >= 0) {
                    next_phase(now);
                }
            }
        }
    }
    /* The radio: the state, the names, the private information */
    if (phase != P_NONE && absolute_time_diff_us(state_ts, now) >= 0 && net_queue_free() > QUEUE_KEEP) {
        send_state();
        if (burst > 0)
            --burst;
        state_ts = delayed_by_ms(now, burst > 0 ? STATE_BURST_MS : STATE_MS);
    }
    if (absolute_time_diff_us(names_ts, now) >= 0 && net_queue_free() > QUEUE_KEEP) {
        send_names();
        bool fast = absolute_time_diff_us(game_ts, now) < NAMES_FAST_FOR_MS * 1000ll;
        names_ts = delayed_by_ms(now, fast ? NAMES_FAST_MS : NAMES_SLOW_MS);
    }
    if (phase != P_NONE && (pending & real_mask()) && absolute_time_diff_us(priv_ts, now) >= 0
            && net_queue_free() > QUEUE_KEEP) {
        for (int k = 0; k < n_real; ++k) {
            int i = (priv_next + k) % n_real;
            if (pending >> i & 1) {
                send_priv(i);
                pending &= ~(1u << i);
                priv_next = i + 1;
                break;
            }
        }
        priv_ts = delayed_by_ms(now, PRIV_GAP_MS);
    }
}


/* ------ Player ------ */

static uint8_t my_idx = WW_NONE, my_role = WW_NONE, my_lover = WW_NONE;
static uint32_t my_wolves = 0;
static uint8_t priv_step = 0, ack_seq = 0, info1 = WW_NONE, info2 = WW_NONE, potions = 0;
static uint8_t wolf_votes[4];
static bool have_priv = false;
static uint8_t my_seq = 0, my_a = WW_NONE, my_b = WW_NONE, pick1 = WW_NONE;
static uint8_t seen[WW_MAX];  /* The roles seen by the seer */
static absolute_time_t act_ts = 0, last_state = 0, peek_until = 0;
static bool peeking = false, lover_news = false, cancelled = false, end_done = false, i_was_alive = true;
/* Parties found */
static party_open_t found[PARTY_MAX_OPEN];
static int n_found = 0;

static bool me_alive(void) {
    return my_idx < WW_MAX && is_alive(my_idx);
}

/* This badge must choose something in this phase */
static bool must_act(void) {
    if (! have_state || my_idx >= WW_MAX)
        return false;
    switch (phase) {
    case P_ROLES: case P_CUPID: case P_WOLVES: case P_SEER: case P_WITCH: case P_VOTE:
        return me_alive();
    case P_HUNTER:
        return hunter == my_idx;
    default:
        return false;
    }
}

static void reset_player(void) {
    my_idx = my_role = my_lover = WW_NONE;
    my_wolves = 0;
    have_priv = false;
    priv_step = ack_seq = 0;
    my_seq = 0;
    my_a = my_b = pick1 = WW_NONE;
    memset(seen, WW_NONE, sizeof(seen));
    memset(revealed, WW_NONE, sizeof(revealed));
    have_state = false;
    phase = P_NONE;
    step = day = n_players = 0;
    alive = acted = winners = 0;
    winner = WW_WIN_NONE;
    hunter = WW_NONE;
    n_deaths = 0;
    cancelled = end_done = lover_news = peeking = false;
    i_was_alive = true;
    timed = false;
    default_names();
}

static void player_state(const uint8_t *d, uint8_t len, absolute_time_t at) {
    if (len < 40 || d[0] >= P_COUNT || d[3] > WW_MAX)
        return;
    last_state = at;
    bool fresh = ! have_state || d[1] != step;
    uint8_t prev = phase;
    phase = d[0];
    step = d[1];
    day = d[2];
    n_players = d[3];
    flags = d[4];
    uint16_t secs = d[5] | d[6] << 8;
    timed = secs != NO_TIMER;
    if (timed)
        deadline = delayed_by_ms(at, secs * 1000u);
    alive = net_u32(d + 7);
    acted = net_u32(d + 11);
    for (int i = 0; i < WW_MAX; ++i) {
        uint8_t r = d[15 + i / 2] >> (i & 1 ? 4 : 0) & 0xF;
        revealed[i] = r < WW_ROLES ? r : WW_NONE;
    }
    winner = d[25];
    hunter = d[26];
    n_deaths = d[27] > MAX_DEATHS ? MAX_DEATHS : d[27];
    memcpy(deaths, d + 28, MAX_DEATHS);
    winners = net_u32(d + 34);
    lovers_end[0] = d[38];
    lovers_end[1] = d[39];
    have_state = true;
    if (my_idx >= WW_MAX) {
        int i = party_index(net_id());
        if (i >= 0 && i < WW_MAX)
            my_idx = i;
    }
    if (fresh) {
        my_seq = 0;
        my_a = my_b = pick1 = WW_NONE;
        printf("werewolf: phase %s, day %u\n", PHASES[phase].log, day);
        if (phase == P_DAWN || phase == P_VERDICT || phase == P_SHOT)
            log_deaths();
        new_phase(prev);
        if (must_act())
            act_ts = delayed_by_ms(at, 1500 + get_rand_32() % 2000);  /* If the PRIV of the phase got lost */
    }
    if (my_idx < WW_MAX && i_was_alive && ! is_alive(my_idx) && phase != P_NONE) {
        i_was_alive = false;
        printf("werewolf: I am dead\n");
        changed = true;
    }
    if (phase == P_END && ! end_done && my_idx < WW_MAX) {
        end_done = true;
        bool won = winners >> my_idx & 1;
        printf("werewolf: end, %s, I %s\n", ww_win_text(winner), won ? "won" : "lost");
        achv_unlock(ACHV_WEREWOLF_PLAY);
        achv_add(ACHV_CNT_GAMES, 1);
        if (won) {
            achv_unlock(ACHV_WEREWOLF_WIN);
            achv_add(ACHV_CNT_WINS, 1);
        }
    }
    if (fresh || phase == P_END)
        changed = true;
}

static void player_priv(const uint8_t *data, uint8_t len) {
    if (len < 20)
        return;
    uint8_t x[16];
    memcpy(x, data + 4, 16);
    mask(x, 16, party_key(), net_u32(data));
    if (x[1] >= WW_ROLES || x[8] >= WW_MAX)
        return;  /* Not for this key */
    if (x[1] != my_role)
        printf("werewolf: role %s\n", ww_role_name(x[1]));
    my_role = x[1];
    my_idx = x[8];
    if (x[2] != my_lover && x[2] < WW_MAX) {
        lover_news = true;
        printf("werewolf: in love with %s\n", pname(x[2]));
    }
    my_lover = x[2];
    my_wolves = net_u32(x + 3);
    priv_step = x[0];
    ack_seq = x[7];
    info1 = x[9];
    info2 = x[10];
    memcpy(wolf_votes, x + 11, 4);
    potions = x[15];
    have_priv = true;
    if (priv_step == step && phase == P_SEER && my_role == WW_SEER && info1 < WW_MAX && info2 < WW_ROLES
            && seen[info1] != info2) {
        seen[info1] = info2;
        printf("werewolf: seen %s is %s\n", pname(info1), ww_role_name(info2));
    }
    changed = true;
}

static void player_names(const uint8_t *d, uint8_t len) {
    if (len < 3 || d[1] > WW_MAX)
        return;
    for (int k = 0; k < d[2] && k < 3 && 3 + (k + 1) * 12 <= len; ++k) {
        int i = d[0] + k;
        if (i >= WW_MAX)
            break;
        ids[i] = net_u32(d + 3 + k * 12);
        char name[9];
        memcpy(name, d + 3 + k * 12 + 4, 8);
        name[8] = 0;
        if (name[0] && strcmp(name, names[i])) {
            snprintf(names[i], sizeof(names[i]), "%s", name);
            changed = true;
        }
        if (ids[i] == net_id())
            my_idx = i;
    }
}

static void send_act(void) {
    uint8_t d[12];
    uint32_t nonce = get_rand_32();
    net_put_u32(d, nonce);
    uint8_t x[8] = {step, my_seq, my_a, my_b, 0, 0, 0, 0};
    mask(x, 8, party_key(), nonce);
    memcpy(d + 4, x, 8);
    party_send(K_ACT, party_host_id(), d, sizeof(d));
}

static void player_task(absolute_time_t now) {
    party_state_t s = party_state();
    if (s == PARTY_CANCELLED && ! cancelled && phase != P_END) {
        cancelled = true;
        printf("werewolf: the party was cancelled\n");
        changed = true;
    }
    if (page == PG_WAIT) {
        static int shown = -1;
        if (party_count() != shown) {
            shown = party_count();
            changed = true;
        }
        if (s == PARTY_STARTED) {
            page = PG_GAME;
            static party_player_t roster[PARTY_MAX];  /* The names of the roster until NAMES comes */
            int n = party_players(roster, PARTY_MAX);
            for (int i = 0; i < n && i < WW_MAX; ++i)
                if (roster[i].name[0])
                    snprintf(names[i], sizeof(names[i]), "%s", roster[i].name);
            int i = party_index(net_id());
            my_idx = i >= 0 && i < WW_MAX ? i : WW_NONE;
            act_ts = delayed_by_ms(now, START_DELAY_MS + 2000 + get_rand_32() % 2000);
            printf("werewolf: the game starts\n");
            changed = true;
        }
    }
    if (s != PARTY_STARTED || page == PG_WAIT || cancelled || phase == P_END)
        return;
    /* The choice again until the narrator acknowledges it, or a request for the information of the phase */
    bool unacked = my_seq && (priv_step != step || ack_seq < my_seq);
    bool missing = my_role == WW_NONE || (must_act() && priv_step != step);
    if ((unacked || missing) && absolute_time_diff_us(act_ts, now) >= 0) {
        send_act();
        act_ts = delayed_by_ms(now, ACT_RESEND_MS + get_rand_32() % ACT_JITTER_MS);
    }
    if (peeking && absolute_time_diff_us(peek_until, now) >= 0) {
        peeking = false;
        changed = true;
    }
}


/* ------ Radio ------ */

static void handle(uint8_t kind, uint32_t from, uint32_t to, const uint8_t *data, uint8_t len, int rssi) {
    (void)to;
    (void)rssi;
    if (mode == M_NARRATOR) {
        narrator_handle(kind, from, data, len);
        return;
    }
    if (mode != M_PLAYER || from != party_host_id())
        return;
    absolute_time_t now = get_absolute_time();
    if ((kind == K_ABORT || kind == K_PARTY_LEAVE) && ! cancelled && phase != P_END) {
        cancelled = true;  /* The narrator stopped the game */
        printf("werewolf: the narrator stopped the game\n");
        changed = true;
        return;
    }
    switch (kind) {
    case K_STATE: player_state(data, len, now); break;
    case K_NAMES: player_names(data, len); break;
    case K_PRIV: player_priv(data, len); break;
    default: break;
    }
}

/* Called by the main loop (and by the page): the game goes on while the page is closed */
void werewolf_service(absolute_time_t now) {
    tune_task(now);
    abort_task(now);
    if (mode == M_NARRATOR)
        narrator_task(now);
    else if (mode == M_PLAYER)
        player_task(now);
}

/* A phase changed while the page is closed: the main loop shows a notification (opens the page from the menus) */
bool werewolf_event(char *buf, int len) {
    if (! notify_pending || mode == M_NONE || app_current() == &app_werewolf)
        return false;
    notify_pending = false;
    if (step == notified_step)
        return false;
    notified_step = step;
    char title[32];
    phase_title(title, sizeof(title));
    snprintf(buf, len, "Loup-garou : %s", title);
    return true;
}

static int abort_left = 0;  /* The narrator stopped the game: ABORT sent again, then the party is left */
static absolute_time_t abort_ts = 0;

static void abort_task(absolute_time_t now) {
    if (! abort_left || absolute_time_diff_us(abort_ts, now) < 0)
        return;
    if (party_state() != PARTY_STARTED || party_game() != PARTY_GAME_WEREWOLF) {
        abort_left = 0;  /* Another party replaced it */
        return;
    }
    if (party_send(K_ABORT, 0, NULL, 0) && --abort_left == 0)
        party_leave();
    abort_ts = delayed_by_ms(now, ABORT_GAP_MS);
}

static void leave_game(void) {
    if (mode == M_NARRATOR && party_state() == PARTY_STARTED && phase != P_END) {
        abort_left = ABORT_REPEATS;  /* The players must know: the LEAVE of party.c is sent once */
        abort_ts = get_absolute_time();
        printf("werewolf: left\n");
    } else if (mode != M_NONE) {
        party_leave();
        printf("werewolf: left\n");
    }
    mode = M_NONE;
    page = PG_MENU;
    phase = P_NONE;
    have_state = false;
    timed = false;
}


/* ------ The page ------ */

static uint8_t rows[WW_MAX + 2];
static int n_rows = 0;

/* The rows of the list of choices of this phase (the same for the role and for those who pretend, if possible) */
static void build_rows(void) {
    n_rows = 0;
    if (phase == P_WITCH || phase == P_VOTE)
        rows[n_rows++] = ROW_NOTHING;
    if (phase == P_WITCH && my_role == WW_WITCH) {
        if ((potions & 1) && info1 < WW_MAX)
            rows[n_rows++] = ROW_HEAL;
        if (potions & 2)
            for (int i = 0; i < n_players; ++i)
                if (is_alive(i))
                    rows[n_rows++] = ROW_POISON | i;
        return;
    }
    bool with_me = phase == P_CUPID || phase == P_HUNTER;
    for (int i = 0; i < n_players; ++i)
        if (is_alive(i) && (with_me || i != my_idx))
            rows[n_rows++] = i;
}

static int wolf_votes_for(int t) {
    int c = 0, k = 0;
    for (int w = 0; w < n_players && k < 4; ++w)
        if (my_wolves >> w & 1)
            c += wolf_votes[k++] == t;
    return c;
}

static void choice_label(int r, char *buf, size_t len) {
    uint8_t v = rows[r];
    bool chosen = my_seq && (v == my_a || (phase == P_CUPID && v == my_b) || (phase == P_WITCH && my_role == WW_WITCH
                  && ((v == ROW_HEAL && my_a == 1) || (v == (ROW_POISON | my_b) && my_a == 2))));
    if (phase == P_WITCH && my_role != WW_WITCH)
        chosen = my_seq && (v == my_a || (v == ROW_NOTHING && my_a == WW_NONE));
    if (phase == P_VOTE && v == ROW_NOTHING)
        chosen = my_seq && my_a == WW_NONE;
    const char *mark = chosen ? "> " : (phase == P_CUPID && v == pick1) ? "1 " : "";
    if (v == ROW_NOTHING) {
        snprintf(buf, len, "%s%s", mark, phase == P_VOTE ? "Vote blanc" : "Ne rien faire");
    } else if (v == ROW_HEAL) {
        snprintf(buf, len, "%sSauver %s", mark, pname(info1));
    } else if (v & ROW_POISON) {
        snprintf(buf, len, "%sEmpoisonner %s", mark, pname(v & ~ROW_POISON));
    } else {
        char tag[24] = "";
        if (my_role == WW_WOLF && (my_wolves >> v & 1))
            snprintf(tag, sizeof(tag), " (loup)");
        else if (seen[v] < WW_ROLES)
            snprintf(tag, sizeof(tag), " (%s)", ww_role_name(seen[v]));
        int votes = phase == P_WOLVES && my_role == WW_WOLF && priv_step == step ? wolf_votes_for(v) : 0;
        if (votes)
            snprintf(tag + strlen(tag), sizeof(tag) - strlen(tag), " [%d]", votes);
        snprintf(buf, len, "%s%s%s", mark, pname(v), tag);
    }
}

/* The roles (narrator; everybody at the end): "x " dead, "<3" lover */
static void role_label(int i, char *buf, size_t len) {
    int role = mode == M_NARRATOR ? g.role[i] : revealed[i];
    bool lover = mode == M_NARRATOR ? (g.lovers[0] == i || g.lovers[1] == i) : (lovers_end[0] == i || lovers_end[1] == i);
    snprintf(buf, len, "%s%s : %s%s", is_alive(i) ? "" : "x ", pname(i), ww_role_name(role), lover ? " <3" : "");
}

/* Another group game is in progress (party.c runs one at a time): its name, NULL when none */
static const char *other_game(void) {
    party_state_t s = party_state();
    if (party_game() == PARTY_GAME_WEREWOLF || (s != PARTY_HOSTING && s != PARTY_JOINING && s != PARTY_JOINED
                                               && s != PARTY_STARTED))
        return NULL;
    return party_game() == PARTY_GAME_TUG ? "tir à la corde" : party_game() == PARTY_GAME_ASSASSIN ? "Assassin"
           : "un autre jeu";
}

static void menu_label(int i, char *buf, size_t len) {
    snprintf(buf, len, i ? "Rejoindre une partie" : "Mener une partie");
}

static void setup_label(int i, char *buf, size_t len) {
    switch (i) {
    case 0: snprintf(buf, len, "Mode : %s", set_advanced ? "avancé" : "simple"); break;
    case 1: snprintf(buf, len, "Débat : %u min", DEBATE_S[set_debate] / 60); break;
    default: snprintf(buf, len, "> Ouvrir la partie"); break;
    }
}

static void lobby_label(int i, char *buf, size_t len) {
    snprintf(buf, len, "%s", lobby[i].name);
}

static void found_label(int i, char *buf, size_t len) {
    snprintf(buf, len, "%s  %u j.  %s", found[i].name, found[i].players,
             found[i].flags & FLAG_ADVANCED ? "avancé" : "simple");
}

/* A list from \p y (ui_list() starts below the title) */
static void list_at(uint8_t *fb, int y0, int visible, int count, int sel, void (*label)(int, char *, size_t)) {
    int first = sel - visible / 2;
    if (first > count - visible)
        first = count - visible;
    if (first < 0)
        first = 0;
    char text[64], fitted[64];
    for (int i = first; i < count && i < first + visible; ++i) {
        int y = y0 + (i - first) * UI_ROW_H;
        label(i, text, sizeof(text));
        ui_fit_preview(&gfx_font_small, fitted, sizeof(fitted), text, GFX_WIDTH - 20);
        bool s = i == sel;
        if (s)
            gfx_fill_rect(fb, 2, y, GFX_WIDTH - 8, UI_ROW_H - 1, GFX_BLACK);
        gfx_text(fb, 8, y + 1, &gfx_font_small, fitted, s ? GFX_WHITE : GFX_BLACK, GFX_ALIGN_LEFT);
    }
    if (count > visible) {
        int h = visible * UI_ROW_H, bar = h * visible / count;
        gfx_fill_rect(fb, GFX_WIDTH - 4, y0 + (h - bar) * first / (count - visible), 3, bar, GFX_BLACK);
    }
}

static void timer_text(char *buf, size_t len, absolute_time_t now) {
    int s = secs_left(now);
    if (s < 0)
        buf[0] = 0;
    else
        snprintf(buf, len, "%d:%02d", s / 60, s % 60);
}

/* The deaths of the dawn, of the verdict or of the shot of the hunter, from \p y */
static int render_deaths(uint8_t *fb, int y) {
    const char *head = phase == P_DAWN ? "Cette nuit :" : phase == P_VERDICT ? "Le village a éliminé :"
                       : "Le chasseur a emporté :";
    if (! n_deaths) {
        const char *none = phase == P_DAWN ? "Personne n'est mort." : phase == P_VERDICT ? "Égalité : personne."
                           : "Personne.";
        y = ui_lines(fb, y, &gfx_font_small, head);
        return ui_lines(fb, y + 2, &gfx_font_medium, none);
    }
    y = ui_lines(fb, y, &gfx_font_small, head);
    for (int i = 0; i < n_deaths && i < 4; ++i) {
        char line[40];
        int d = deaths[i];
        snprintf(line, sizeof(line), "%s (%s)", pname(d), ww_role_name(d < WW_MAX ? revealed[d] : WW_NONE));
        /* Medium when it fits (2 deaths at most: room below), small otherwise */
        bool medium = n_deaths <= 2 && gfx_text_width(&gfx_font_medium, line) <= GFX_WIDTH - 4;
        y = ui_lines(fb, y, medium ? &gfx_font_medium : &gfx_font_small, line);
    }
    return y;
}

static void render_end(uint8_t *fb) {
    int y = ui_wrapped(fb, UI_TITLE_H + 10, &gfx_font_medium, ww_win_text(winner), 2);
    if (mode == M_PLAYER && my_idx < WW_MAX) {
        y = ui_lines(fb, y + 8, &gfx_font_large, winners >> my_idx & 1 ? "Gagné !" : "Perdu...");
        char text[40];
        snprintf(text, sizeof(text), "Tu étais : %s", ww_role_name(my_role));
        ui_lines(fb, y + 8, &gfx_font_small, text);
    }
    ui_footer(fb, "D : les rôles  G long : fin");
}

static void render_narrator(uint8_t *fb, absolute_time_t now) {
    char text[64];
    if (phase == P_NONE) {
        ui_title(fb, "Loup-garou");
        ui_lines(fb, 70, &gfx_font_medium, "Distribution\ndes rôles...");
        return;
    }
    phase_title(text, sizeof(text));
    ui_title(fb, text);
    if (phase == P_END) {
        render_end(fb);
        return;
    }
    int y = UI_TITLE_H + 2;
    char timer[12];
    timer_text(timer, sizeof(timer), now);
    if (phase == P_DAWN || phase == P_VERDICT || phase == P_SHOT) {
        snprintf(text, sizeof(text), "%s   En vie : %d / %u", timer, ww_count_alive(&g), g.n);
        y = ui_lines(fb, y, &gfx_font_small, text);
        render_deaths(fb, y + 4);
    } else {
        if (timer[0])
            y = ui_lines(fb, y, &gfx_font_large, timer);
        uint32_t e = expected();
        int n = snprintf(text, sizeof(text), "En vie : %d / %u", ww_count_alive(&g), g.n);
        if (e)
            snprintf(text + n, sizeof(text) - n, "   Choix : %d / %d", __builtin_popcount(acted & e),
                     __builtin_popcount(e));
        y = ui_lines(fb, y, &gfx_font_small, text);
        ui_wrapped(fb, y + 4, &gfx_font_small, PHASES[phase].hint, 3);
    }
    ui_footer(fb, "D : suite  X : +30 s");
}

/* The list of choices of a phase: the prompt, the timer, the rows. \return the footer */
static const char *render_choices(uint8_t *fb, int y, const char *timer, const char *lost_footer) {
    char text[48];
    const char *prompt = "Fais semblant de choisir";
    if (phase == P_VOTE)
        prompt = "Qui éliminer ?";
    else if (phase == P_HUNTER)
        prompt = "Qui emporter ?";
    else if (phase == P_CUPID && my_role == WW_CUPID)
        prompt = pick1 == WW_NONE ? "1er amoureux ?" : "2e amoureux ?";
    else if (phase == P_CUPID)
        prompt = pick1 == WW_NONE ? "Fais semblant : 1er nom" : "Fais semblant : 2e nom";
    else if (phase == P_WOLVES && my_role == WW_WOLF)
        prompt = "Votre victime ?";
    else if (phase == P_SEER && my_role == WW_SEER)
        prompt = "Qui sonder ?";
    else if (phase == P_WITCH && my_role == WW_WITCH)
        prompt = info1 < WW_MAX ? "" : "Pas de victime";
    if (phase == P_SEER && my_role == WW_SEER && priv_step == step && info1 < WW_MAX && info2 < WW_ROLES) {
        snprintf(text, sizeof(text), "%s : %s", pname(info1), ww_role_name(info2));
        prompt = text;
    } else if (phase == P_WITCH && my_role == WW_WITCH && info1 < WW_MAX) {
        snprintf(text, sizeof(text), "Victime : %s", pname(info1));
        prompt = text;
    }
    gfx_text(fb, 4, y, &gfx_font_small, prompt, GFX_BLACK, GFX_ALIGN_LEFT);
    gfx_text(fb, GFX_WIDTH - 4, y, &gfx_font_small, timer, GFX_BLACK, GFX_ALIGN_RIGHT);
    build_rows();
    if (list_sel >= n_rows)
        list_sel = n_rows ? n_rows - 1 : 0;
    list_at(fb, y + 18, 6, n_rows, list_sel, choice_label);
    if (lost_footer)
        return lost_footer;
    if (! my_seq)
        return "Flancs : choix  D : valider";
    if (ack_seq < my_seq || priv_step != step)
        return "Envoi...";
    return phase == P_SEER && my_role == WW_SEER ? "Choix reçu" : "Choix reçu  D : changer";
}

static void render_player(uint8_t *fb, absolute_time_t now) {
    char text[80];
    if (cancelled) {
        ui_title(fb, "Loup-garou");
        ui_box(fb, "Partie arrêtée\npar le meneur");
        ui_footer(fb, "G long : quitter");
        return;
    }
    if (! have_state || phase == P_NONE) {
        ui_title(fb, "Loup-garou");
        ui_lines(fb, 70, &gfx_font_medium, "La partie\ncommence...");
        ui_footer(fb, "G : menu");
        return;
    }
    phase_title(text, sizeof(text));
    ui_title(fb, text);
    if (phase == P_END) {
        render_end(fb);
        return;
    }
    char timer[12];
    timer_text(timer, sizeof(timer), now);
    bool lost = absolute_time_diff_us(last_state, now) > LOST_MS * 1000ll;
    const char *footer = lost ? "Meneur hors de portée !" : "G : menu  D long : rôle";

    if (lover_news && my_lover < WW_MAX) {
        snprintf(text, sizeof(text), "Cupidon t'a lié(e)\nà %s", pname(my_lover));
        ui_box(fb, text);
        ui_footer(fb, "D : compris");
        return;
    }
    if (peeking || phase == P_ROLES) {
        int y = UI_TITLE_H + 2;
        if (my_role >= WW_ROLES) {
            ui_lines(fb, y + 40, &gfx_font_medium, "Rôle en attente...");
            ui_footer(fb, footer);
            return;
        }
        y = ui_lines(fb, y, &gfx_font_large, ww_role_name(my_role));
        y = ui_wrapped(fb, y, &gfx_font_small, ROLE_HELP[my_role], 3);
        text[0] = 0;
        if (my_role == WW_WOLF && __builtin_popcount(my_wolves) > 1) {
            snprintf(text, sizeof(text), "Loups :");
            for (int i = 0; i < n_players; ++i)
                if ((my_wolves >> i & 1) && i != my_idx)
                    snprintf(text + strlen(text), sizeof(text) - strlen(text), " %s", pname(i));
        } else if (my_lover < WW_MAX) {
            snprintf(text, sizeof(text), "Amoureux : %s", pname(my_lover));
        }
        if (text[0])
            ui_wrapped(fb, y + 2, &gfx_font_small, text, 2);
        if (phase == P_ROLES && ! peeking)
            footer = my_seq ? (ack_seq >= my_seq && priv_step == step ? "Compris, reçu" : "Envoi...")
                     : "Cache l'écran !  D : compris";
        ui_footer(fb, footer);
        return;
    }
    int y = UI_TITLE_H + 2;
    if (my_idx < WW_MAX && ! me_alive() && ! (phase == P_HUNTER && hunter == my_idx)) {
        if (phase == P_DAWN || phase == P_VERDICT || phase == P_SHOT) {
            snprintf(text, sizeof(text), "Tu es mort(e)   %s", timer);
            y = ui_lines(fb, y, &gfx_font_small, text);
            render_deaths(fb, y + 4);
        } else {
            y = ui_lines(fb, y + 16, &gfx_font_large, "Tu es mort(e)");
            y = ui_lines(fb, y + 4, &gfx_font_small, "Chut ! Tu ne joues plus,\nmais tu suis la partie.");
            if (timer[0])
                ui_lines(fb, y + 4, &gfx_font_medium, timer);
        }
        ui_footer(fb, lost ? footer : "G : menu");
        return;
    }
    switch (phase) {
    case P_DAWN: case P_VERDICT: case P_SHOT:
        y = ui_lines(fb, y, &gfx_font_small, timer);
        render_deaths(fb, y + 4);
        break;
    case P_DEBATE:
        y = ui_lines(fb, y + 8, &gfx_font_medium, "Débattez !");
        y = ui_lines(fb, y + 4, &gfx_font_large, timer);
        snprintf(text, sizeof(text), "%d joueurs en vie", __builtin_popcount(alive));
        y = ui_lines(fb, y + 6, &gfx_font_small, text);
        ui_wrapped(fb, y + 4, &gfx_font_small, "Qui sont les loups ? Le vote suit.", 2);
        break;
    default:
        if (phase == P_HUNTER && hunter != my_idx) {
            snprintf(text, sizeof(text), "Le chasseur %s\nchoisit sa cible...", pname(hunter));
            ui_lines(fb, y + 40, &gfx_font_small, text);
            break;
        }
        if (must_act())
            footer = render_choices(fb, y, timer, lost ? footer : NULL);
        break;
    }
    ui_footer(fb, footer);
}

static void render_roles(uint8_t *fb) {
    ui_title(fb, "Les rôles");
    list_at(fb, UI_TITLE_H + 3, 7, n_players, list_sel, role_label);
    ui_footer(fb, "G : retour");
}

static void ww_render(uint8_t *fb, absolute_time_t now) {
    char text[96];
    switch (page) {
    case PG_MENU:
        ui_title(fb, "Loup-garou");
        list_at(fb, UI_TITLE_H + 3, 2, 2, list_sel, menu_label);
        if (other_game()) {
            snprintf(text, sizeof(text), "Une partie de %s est en cours : quitte-la d'abord.", other_game());
            ui_wrapped(fb, UI_TITLE_H + 60, &gfx_font_small, text, 4);
        } else {
            ui_wrapped(fb, UI_TITLE_H + 60, &gfx_font_small,
                       "Un meneur (qui ne joue pas) et 5 à 20 joueurs, chacun avec son badge.", 4);
        }
        ui_footer(fb, "G : retour  D : choisir");
        break;
    case PG_SETUP:
        ui_title(fb, "Mener une partie");
        list_at(fb, UI_TITLE_H + 3, 3, 3, setup_row, setup_label);
        ui_wrapped(fb, UI_TITLE_H + 74, &gfx_font_small, set_advanced
                   ? "Loups, voyante, sorcière, chasseur, Cupidon (9+), villageois. 7 à 20 joueurs."
                   : "Loups, voyante, villageois. 5 à 20 joueurs.", 4);
        ui_footer(fb, setup_row < 2 ? "Flancs : ligne  D : changer" : "G : retour  D : ouvrir");
        break;
    case PG_LOBBY: {
        ui_title(fb, "Partie ouverte");
        int min = min_players();
        snprintf(text, sizeof(text), "%d joueur%s (min. %d)", n_lobby, n_lobby > 1 ? "s" : "", min);
        int y = ui_lines(fb, UI_TITLE_H + 2, &gfx_font_small, text);
        if (debug_mode())
            y = ui_lines(fb, y, &gfx_font_small, "Test : des robots complètent");
        if (n_lobby)
            list_at(fb, y + 2, (UI_FOOTER_Y - 4 - y) / UI_ROW_H, n_lobby, list_sel < n_lobby ? list_sel : 0,
                    lobby_label);
        else
            ui_wrapped(fb, y + 20, &gfx_font_small, "Les joueurs : Jeux > Loup-garou > Rejoindre.", 3);
        ui_footer(fb, n_lobby >= min ? "G long : annuler  D : lancer" : "G long : annuler");
        break;
    }
    case PG_SCAN:
        ui_title(fb, "Rejoindre");
        if (n_found) {
            list_at(fb, UI_TITLE_H + 3, 7, n_found, list_sel < n_found ? list_sel : 0, found_label);
            ui_footer(fb, "G : retour  D : rejoindre");
        } else {
            ui_lines(fb, 60, &gfx_font_small, "Recherche des parties...\nLe meneur doit ouvrir\nla sienne.");
            ui_footer(fb, "G : retour");
        }
        break;
    case PG_WAIT: {
        ui_title(fb, "Loup-garou");
        if (cancelled || party_state() == PARTY_CANCELLED || party_state() == PARTY_IDLE) {
            ui_box(fb, "Partie annulée");
            ui_footer(fb, "G long : quitter");
            break;
        }
        bool in = party_state() == PARTY_JOINED;
        snprintf(text, sizeof(text), "%s\n%d joueurs\nMode %s, débat %u min", in ? "Inscrit(e) !" : "Inscription...",
                 party_count(), flags & FLAG_ADVANCED ? "avancé" : "simple", debate_secs() / 60);
        int y = ui_lines(fb, UI_TITLE_H + 14, &gfx_font_small, text);
        ui_wrapped(fb, y + 12, &gfx_font_small, "En attente du lancement par le meneur...", 2);
        ui_footer(fb, "G : menu  G long : quitter");
        break;
    }
    case PG_ROLES:
        render_roles(fb);
        break;
    case PG_QUIT:
        ui_title(fb, "Loup-garou");
        ui_box(fb, mode == M_NARRATOR ? "Arrêter la partie\npour tous ?" : "Quitter\nla partie ?");
        ui_footer(fb, "G : non  D : oui");
        break;
    default:
        if (mode == M_NARRATOR)
            render_narrator(fb, now);
        else
            render_player(fb, now);
        break;
    }
}

static void player_choose(absolute_time_t now) {
    build_rows();
    if (! n_rows || list_sel >= n_rows)
        return;
    uint8_t v = rows[list_sel];
    if (phase == P_WOLVES && my_role == WW_WOLF && v < WW_MAX && (my_wolves >> v & 1))
        return;  /* Not a wolf */
    if (phase == P_CUPID) {
        if (pick1 == WW_NONE || pick1 == v) {
            pick1 = v;
            return;
        }
        my_a = pick1;
        my_b = v;
        pick1 = WW_NONE;
    } else if (phase == P_WITCH && my_role == WW_WITCH) {
        my_a = v == ROW_HEAL ? 1 : (v & ROW_POISON) && v != ROW_NOTHING ? 2 : 0;
        my_b = my_a == 2 ? v & ~ROW_POISON : WW_NONE;
    } else {
        my_a = v < WW_MAX ? v : WW_NONE;
        my_b = WW_NONE;
    }
    ++my_seq;
    act_ts = now;
    bool witch = phase == P_WITCH && my_role == WW_WITCH;
    printf("werewolf: my choice %s%s%s\n", witch ? (my_a == 1 ? "heal" : my_a == 2 ? "poison" : "nothing")
           : my_a < WW_MAX ? pname(my_a) : "-", my_b < WW_MAX ? " " : "", my_b < WW_MAX ? pname(my_b) : "");
}

static bool game_buttons(const app_buttons_t *b, absolute_time_t now) {
    if (b->long_pressed & UI_BTN_A) {
        quit_back = page;
        page = PG_QUIT;
        return true;
    }
    if (b->released_short & UI_BTN_A)
        return false;  /* Back to the menus: the game goes on */
    if (phase == P_END) {
        if (b->released_short & UI_BTN_B) {
            page = PG_ROLES;
            list_sel = 0;
        }
        return true;
    }
    if (mode == M_NARRATOR) {
        if (b->long_pressed & UI_BTN_B) {
            page = PG_ROLES;
            list_sel = 0;
        } else if ((b->released_short & UI_BTN_B) && phase != P_NONE) {
            printf("werewolf: the narrator skips\n");
            next_phase(now);
        } else if ((b->pressed & UI_BTN_X) && timed) {
            deadline = delayed_by_ms(deadline, ADD_TIME_S * 1000);
            burst = 1;
            state_ts = now;
        }
        return true;
    }
    if (cancelled)
        return true;
    if (lover_news) {
        if (b->released_short & UI_BTN_B)
            lover_news = false;
        return true;
    }
    if (b->long_pressed & UI_BTN_B) {
        peeking = true;
        peek_until = delayed_by_ms(now, PEEK_MS);
        return true;
    }
    if (phase == P_ROLES) {
        if ((b->released_short & UI_BTN_B) && my_role < WW_ROLES && me_alive()) {
            my_a = 1;
            my_b = WW_NONE;
            ++my_seq;
            act_ts = now;
        }
        return true;
    }
    if (! must_act())
        return true;
    build_rows();
    if (n_rows && (b->pressed & UI_BTN_Y))
        list_sel = (list_sel + n_rows - 1) % n_rows;
    if (n_rows && (b->pressed & UI_BTN_X))
        list_sel = (list_sel + 1) % n_rows;
    if (b->released_short & UI_BTN_B)
        player_choose(now);
    return true;
}

static bool ww_buttons(const app_buttons_t *b, absolute_time_t now) {
    switch (page) {
    case PG_MENU:
        if (b->pressed & (UI_BTN_X | UI_BTN_Y))
            list_sel ^= 1;
        if (b->pressed & UI_BTN_A)
            return false;
        if ((b->pressed & UI_BTN_B) && other_game()) {
            printf("werewolf: a party of %s is in progress\n", other_game());
        } else if (b->pressed & UI_BTN_B) {
            if (list_sel == 0) {
                page = PG_SETUP;
                setup_row = 0;
            } else {
                reset_player();
                party_set_handler(handle);
                party_scan(PARTY_GAME_WEREWOLF);
                n_found = 0;
                page = PG_SCAN;
                list_sel = 0;
            }
        }
        return true;
    case PG_SETUP:
        if (b->pressed & UI_BTN_Y)
            setup_row = (setup_row + 2) % 3;
        if (b->pressed & UI_BTN_X)
            setup_row = (setup_row + 1) % 3;
        if (b->pressed & UI_BTN_A) {
            page = PG_MENU;
            list_sel = 0;
        }
        if (b->pressed & UI_BTN_B) {
            if (setup_row == 0) {
                set_advanced = ! set_advanced;
            } else if (setup_row == 1) {
                set_debate = (set_debate + 1) % 3;
            } else if (! other_game()) {
                bool debug = store_get()->admin == STORE_ADMIN_ON;
                flags = (set_advanced ? FLAG_ADVANCED : 0) | set_debate << FLAG_DEBATE_SHIFT | (debug ? FLAG_DEBUG : 0);
                reset_player();
                memset(&g, 0, sizeof(g));
                n_lobby = 0;
                party_set_handler(handle);
                party_host(PARTY_GAME_WEREWOLF, false, WW_MAX, flags);
                mode = M_NARRATOR;
                page = PG_LOBBY;
                list_sel = 0;
                printf("werewolf: narrator, %s mode, debate %u s%s\n", set_advanced ? "advanced" : "simple",
                       debate_secs(), debug ? ", test mode" : "");
            }
        }
        return true;
    case PG_LOBBY:
        if (b->long_pressed & UI_BTN_A) {
            quit_back = page;
            page = PG_QUIT;
            return true;
        }
        if (b->released_short & UI_BTN_A)
            return false;
        if (n_lobby && (b->pressed & UI_BTN_Y))
            list_sel = (list_sel + n_lobby - 1) % n_lobby;
        if (n_lobby && (b->pressed & UI_BTN_X))
            list_sel = (list_sel + 1) % n_lobby;
        if ((b->released_short & UI_BTN_B) && n_lobby >= min_players())
            narrator_launch(now);
        return true;
    case PG_SCAN:
        if (b->pressed & UI_BTN_A) {
            party_leave();
            page = PG_MENU;
            list_sel = 1;
            return true;
        }
        if (n_found && (b->pressed & UI_BTN_Y))
            list_sel = (list_sel + n_found - 1) % n_found;
        if (n_found && (b->pressed & UI_BTN_X))
            list_sel = (list_sel + 1) % n_found;
        if (n_found && (b->pressed & UI_BTN_B) && list_sel < n_found) {
            party_join(found[list_sel].host);
            flags = found[list_sel].flags;
            mode = M_PLAYER;
            page = PG_WAIT;
            printf("werewolf: joining %s\n", found[list_sel].name);
        }
        return true;
    case PG_WAIT:
        if (b->long_pressed & UI_BTN_A) {
            leave_game();
            return true;
        }
        return ! (b->released_short & UI_BTN_A);
    case PG_ROLES:
        if (b->pressed & UI_BTN_Y)
            list_sel = (list_sel + n_players - 1) % (n_players ? n_players : 1);
        if (b->pressed & UI_BTN_X)
            list_sel = (list_sel + 1) % (n_players ? n_players : 1);
        if (b->released_short & UI_BTN_A) {
            page = PG_GAME;
            list_sel = 0;
        }
        return true;
    case PG_QUIT:
        if (b->released_short & UI_BTN_B) {
            leave_game();
            list_sel = 0;
        } else if (b->released_short & UI_BTN_A) {
            page = quit_back;
        }
        return true;
    default:
        return game_buttons(b, now);
    }
}

static void ww_start(absolute_time_t now) {
    (void)now;
    if (mode == M_NONE) {
        page = PG_MENU;
        list_sel = 0;
    } else if (page == PG_QUIT || page == PG_ROLES) {
        page = quit_back == PG_LOBBY || quit_back == PG_WAIT ? quit_back : PG_GAME;
    }
    changed = true;
}

static bool ww_task(absolute_time_t now) {
    werewolf_service(now);
    notify_pending = false;  /* The page is shown: no notification */
    notified_step = step;
    if (page == PG_SCAN) {
        static absolute_time_t list_ts = 0;
        if (absolute_time_diff_us(list_ts, now) >= 0) {
            list_ts = delayed_by_ms(now, 1000);
            int n = party_found(found, PARTY_MAX_OPEN);
            changed |= n != n_found || n > 0;  /* The players of each party change too */
            n_found = n;
        }
    }
    /* The timer: every 5 s, every second at the end (the e-Paper refresh shows) */
    static int shown = -2;
    if (page == PG_GAME) {
        int s = secs_left(now);
        if (s != shown && (s <= 10 || s % 5 == 0 || shown < 0)) {
            shown = s;
            changed = true;
        }
    }
    bool c = changed;
    changed = false;
    return c;
}

static bool ww_calm(void) {
    return page != PG_GAME || ! timed;
}

const app_t app_werewolf = {
    .name = "Loup-garou",
    .start = ww_start,
    .buttons = ww_buttons,
    .task = ww_task,
    .render = ww_render,
    .calm = ww_calm,
    .no_saver = true,
};
