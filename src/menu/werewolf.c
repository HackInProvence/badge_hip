/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Loup-garou (werewolf / mafia), after the rules of "Les Loups-garous de Thiercelieux": a narrator ("meneur", who
 * does not play) chooses the roles in play and opens a party (party.c), 8 to 18 players join; the badge of the
 * narrator deals the cards and drives the game, phase by phase:
 * - the first night: the thief, Cupidon, the lovers who recognize each other;
 * - each night: the seer, the wolves (the little girl may spy on them), the witch;
 * - each day: the dawn (the dead and their cards), the shot of the hunter, the successor of a dead captain, the
 *   election of the captain (the first day), the debate, the vote (the captain counts twice; a tie: the captain
 *   decides, without captain a second vote among the tied players), the verdict.
 * The rules are in werewolf_logic.c, the cards (help) in werewolf_cards.c, the whole in docs/fr/loup_garou.md.
 *
 * Secrecy in a room: during each night phase, every living player chooses in a list (those without the role
 * pretend), so that nobody can tell who acts by looking at who presses buttons, and every player receives the same
 * kind of packet. The roles and the actions travel XORed with a hash of the random key of the player
 * (party_mask()), known by the narrator and the player only.
 *
 * Messages (party_send(), kinds from PARTY_KIND_GAME):
 * - STATE (narrator, every second, 3 times at each phase change): [phase][step][day][players][options][seconds 2]
 *   [alive 4][acted 4][cards of the dead 10, nibbles, 0xF = secret][winner][actor][deaths count][deaths 6]
 *   [winners 4][lovers 2, at the end only][captain][candidates 4][debate]. The step counts the phases: a new step
 *   resets the choices. The actor: the hunter who shoots, the captain who decides or names his successor.
 * - NAMES (narrator, every 3 s, fast during the first seconds): [first][players][3 x (id 4, name 8)]
 *   (the robots of the test mode are not in the roster of the party).
 * - PRIV (narrator -> a player): [nonce 4][masked: step, card, lover, wolves 4, ack, index, info 2, wolf votes 4,
 *   potions]. Sent to every living player at each phase change, to everybody when a wolf votes, and in answer to
 *   each ACT (the answer is the acknowledgement). The info: the 2 cards of the thief, the card seen by the seer,
 *   the wolf seen by the little girl, the victim shown to the witch.
 * - ACT (player -> narrator): [nonce 4][masked: step, seq, a, b, 0 x 4]; seq 0: "send me my information".
 *   Sent again until the PRIV of the step acknowledges the seq: the radio loses packets, duplicates are ignored.
 * - ABORT (narrator, 5 times, then the LEAVE of party.c): the narrator stopped the game.
 *
 * Below 8 players (the minimum of the rules), no game unless Admin > Loup-garou (admin) unlocks it: small games from
 * 4 players (1 wolf below 8), or the test mode: a party starts with a single player, and robots (which can hold any
 * card) complete it up to 8 (tools/test_werewolf.py with 2 badges). */

#include <stdio.h>
#include <string.h>

#include "pico/rand.h"

#include "achievements.h"
#include "app.h"
#include "net.h"
#include "party.h"
#include "social.h"
#include "store.h"
#include "werewolf_cards.h"
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

#define STATE_LEN 46
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
#define CARD_SHOWN_MS 10000  /* The card of the player (long press during the game), then back to the game */
#define MAX_DEATHS 6
#define BOT_ID 0xB0700000u
#define ABORT_REPEATS 5
#define ABORT_GAP_MS 300
#define NO_TIMER 0xFFFF

#define FLAG_DEBUG 0x80  /* In the options byte (party flags): test mode, robots complete the party */

static const uint16_t DEBATE_S[3] = {120, 180, 300};

enum { P_NONE, P_ROLES, P_THIEF, P_CUPID, P_LOVERS, P_SEER, P_WOLVES, P_WITCH, P_DAWN, P_HUNTER, P_SHOT,
       P_SUCCESSOR, P_ELECTION, P_DEBATE, P_VOTE, P_TIEBREAK, P_VOTE2, P_VERDICT, P_END, P_COUNT };
#define NEXT_NIGHT P_COUNT  /* After the deaths: the next night */

typedef struct {
    const char *title;  /* After "Nuit n : " / "Jour n : " */
    const char *log;
    uint16_t secs;
    bool night;
    const char *hint;  /* For the narrator */
} phase_t;

static const phase_t PHASES[P_COUNT] = {
    [P_NONE] = {"Début", "start", 0, false, "La partie commence..."},
    [P_ROLES] = {"Rôles", "roles", 30, false, "Chacun lit sa carte en cachant son écran."},
    [P_THIEF] = {"Voleur", "thief", 30, true, "La nuit tombe. Tous choisissent : seul le voleur compte."},
    [P_CUPID] = {"Cupidon", "cupid", 30, true, "Tous choisissent 2 noms : seul Cupidon compte."},
    [P_LOVERS] = {"Amoureux", "lovers", 15, true, "Les amoureux se reconnaissent ; tous valident."},
    [P_SEER] = {"Voyante", "seer", 30, true, "Tous choisissent : seule la voyante découvre une carte."},
    [P_WOLVES] = {"Loups", "wolves", 45, true,
                  "Les loups choisissent leur victime ; la petite fille peut espionner."},
    [P_WITCH] = {"Sorcière", "witch", 40, true, "Tous choisissent : seule la sorcière agit."},
    [P_DAWN] = {"aube", "dawn", 15, false, "Le jour se lève : annoncez les morts de la nuit."},
    [P_HUNTER] = {"Chasseur", "hunter", 30, false, "Le chasseur est mort : il emporte quelqu'un."},
    [P_SHOT] = {"Chasseur", "shot", 10, false, "Annoncez la victime du chasseur."},
    [P_SUCCESSOR] = {"Capitaine", "successor", 30, false, "Le capitaine est mort : il désigne son successeur."},
    [P_ELECTION] = {"capitaine", "election", 60, false, "Le village élit son capitaine (sa voix compte double)."},
    [P_DEBATE] = {"débat", "debate", 0, false, "Le village débat : qui sont les loups ?"},
    [P_VOTE] = {"vote", "vote", 45, false, "Chacun vote sur son badge."},
    [P_TIEBREAK] = {"égalité", "tiebreak", 30, false, "Égalité : le capitaine tranche."},
    [P_VOTE2] = {"2e vote", "vote2", 45, false, "Égalité : second vote entre les ex aequo."},
    [P_VERDICT] = {"verdict", "verdict", 12, false, "Annoncez l'éliminé et sa carte."},
    [P_END] = {"Fin", "end", 0, false, ""},
};

/* The options on the setup page, in this order */
static const struct { uint8_t opt; const char *name; } OPTIONS[] = {
    {WW_OPT_SEER, "Voyante"}, {WW_OPT_WITCH, "Sorcière"}, {WW_OPT_HUNTER, "Chasseur"}, {WW_OPT_CUPID, "Cupidon"},
    {WW_OPT_GIRL, "Petite fille"}, {WW_OPT_CAPTAIN, "Capitaine"}, {WW_OPT_THIEF, "Voleur"},
};
#define N_OPTIONS ((int)(sizeof(OPTIONS) / sizeof(OPTIONS[0])))

enum { M_NONE, M_NARRATOR, M_PLAYER };
enum { PG_MENU, PG_SETUP, PG_LOBBY, PG_SCAN, PG_WAIT, PG_GAME, PG_ROLES, PG_QUIT, PG_HELP };
enum { MENU_NARRATE, MENU_JOIN, MENU_HELP, MENU_ROWS };
enum { SETUP_PRESET, SETUP_OPTIONS, SETUP_DEBATE = SETUP_OPTIONS + N_OPTIONS, SETUP_WOLVES, SETUP_OPEN, SETUP_ROWS };

/* Values of the rows of the lists of choices (below WW_MAX: a player) */
#define ROW_POISON 0x80  /* | the player */
#define ROW_SPY 0xF6
#define ROW_SLEEP 0xF7
#define ROW_CARD2 0xF8
#define ROW_CARD1 0xF9
#define ROW_KEEP 0xFA
#define ROW_CONTINUE 0xFB
#define ROW_NOHEAL 0xFC
#define ROW_HEAL 0xFD
#define ROW_NOTHING 0xFE  /* Vote blanc / personne */

/* ------ Shared by the narrator and the players: the game as shown ------ */

static int mode = M_NONE;
static int page = PG_MENU, quit_back = PG_MENU;
static bool changed = false;
static uint8_t options = 0;  /* WW_OPT_* | FLAG_DEBUG: the party flags */
static uint8_t debate = 1;  /* Index in DEBATE_S */
static char names[WW_MAX][10];
static uint32_t ids[WW_MAX];

static bool have_state = false;
static uint8_t phase = P_NONE, step = 0, day = 0, n_players = 0, winner = WW_WIN_NONE, actor = WW_NONE;
static uint8_t captain = WW_NONE;
static uint32_t alive = 0, acted = 0, winners = 0, candidates = 0;
static uint8_t revealed[WW_MAX];  /* The cards of the dead (everybody's at the end), WW_NONE: secret */
static uint8_t deaths[MAX_DEATHS], n_deaths = 0;
static uint8_t lovers_end[2] = {WW_NONE, WW_NONE};
static bool timed = false;
static absolute_time_t deadline = 0;
static int list_sel = 0;  /* Selected row (lists of the pages) */
static uint8_t notified_step = 0;  /* werewolf_event() */
static bool notify_pending = false;
/* The help: a card, its details, where it goes back */
static int help_card = 0, help_back = PG_MENU;
static bool help_details = false;
static absolute_time_t help_until = 0;  /* The card of the player during the game: shown for a while */

/* The chimes: {hz, ms} pairs, 0 to end */
static const uint16_t TUNE_NIGHT[] = {784, 220, 659, 220, 523, 450, 0};
static const uint16_t TUNE_DAY[] = {523, 180, 659, 180, 784, 400, 0};
static const uint16_t TUNE_STEP[] = {880, 120, 0};
static const uint16_t TUNE_END[] = {523, 150, 659, 150, 784, 150, 1047, 500, 0};
static const uint16_t *tune = NULL;
static absolute_time_t tune_ts = 0;

static bool debug_mode(void) {
    return options & FLAG_DEBUG;
}

static uint16_t debate_secs(void) {
    return DEBATE_S[debate < 3 ? debate : 1];
}

/* Admin > Loup-garou (admin): below 8 players, no game unless unlocked (store ww_unlock) */
enum { WW_UNLOCK_RULE, WW_UNLOCK_SMALL, WW_UNLOCK_ROBOTS, WW_UNLOCKS };
#define WW_SMALL_MIN_PLAYERS 4  /* Small games: 1 wolf, 3 others at least */

static int unlock_mode(void) {
    uint8_t u = store_get()->ww_unlock;
    return u < WW_UNLOCKS ? u : WW_UNLOCK_RULE;
}

static int min_players(void) {
    return debug_mode() ? 1 : unlock_mode() == WW_UNLOCK_SMALL ? WW_SMALL_MIN_PLAYERS : WW_MIN_PLAYERS;
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

/* The roles of the options, for the pages ("voyante, sorcière..."); \p n: the length of \p buf */
static void options_text(char *buf, size_t n, uint8_t opts) {
    buf[0] = 0;
    for (int i = 0; i < N_OPTIONS; ++i)
        if (opts & OPTIONS[i].opt)
            snprintf(buf + strlen(buf), n - strlen(buf), "%s%s", buf[0] ? ", " : "", OPTIONS[i].name);
    if (! buf[0])
        snprintf(buf, n, "loups et villageois seulement");
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
    else if (phase == P_SUCCESSOR)
        snprintf(buf, len, "Le capitaine");
    else if (PHASES[phase].night)
        snprintf(buf, len, "Nuit %u : %s", day, PHASES[phase].title);
    else if (phase >= P_DAWN)
        snprintf(buf, len, "Jour %u : %s", day, PHASES[phase].title);
    else if (phase == P_ROLES)
        snprintf(buf, len, "Ta carte");
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

/* A new phase on this badge: chime, notification */
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
/* The night */
static uint8_t victim = WW_NONE, seer_target = WW_NONE, spy_seen = WW_NONE;
static bool spy_caught = false;
/* The deaths: the hunter shoots, then the game ends or a dead captain names his successor, then \p after */
static bool hunter_pending = false;
static uint8_t dead_hunter = WW_NONE, after = P_DEBATE;
/* Setup and lobby */
static int setup_row = 0;
static uint8_t set_options = WW_OPT_ALL;
static uint8_t set_debate = 1;
static party_player_t lobby[PARTY_MAX];
static int n_lobby = 0;

static uint32_t real_mask(void) {
    return n_real >= 32 ? 0xFFFFFFFFu : (1u << n_real) - 1;
}

/* A random player of \p mask, alive */
static uint8_t rand_of(uint32_t mask_) {
    return ww_pick(mask_ & g.alive, get_rand_32);
}

/* The players \p actor may harm (vote against, attack, poison, shoot) */
static uint32_t harmable(int a) {
    uint32_t m = 0;
    for (int i = 0; i < g.n; ++i)
        if (ww_may_harm(&g, a, i))
            m |= 1u << i;
    return m;
}

static uint32_t expected(void) {
    switch (phase) {
    case P_ROLES: case P_THIEF: case P_CUPID: case P_LOVERS: case P_SEER: case P_WOLVES: case P_WITCH:
    case P_ELECTION: case P_VOTE: case P_VOTE2:
        return g.alive;
    case P_HUNTER: case P_SUCCESSOR: case P_TIEBREAK:
        return actor < WW_MAX ? 1u << actor : 0;
    default:
        return 0;
    }
}

/* The view (what STATE sends) follows the game */
static void sync_view(void) {
    alive = g.alive;
    n_players = g.n;
    captain = g.captain;
    for (int i = 0; i < g.n; ++i)
        revealed[i] = phase == P_END || ! ww_alive(&g, i) ? g.role[i] : WW_NONE;
}

static void girl_spies(int i) {
    if (spy_seen != WW_NONE || g.role[i] != WW_GIRL)
        return;  /* Once a night */
    spy_caught = ww_spy(&g, get_rand_32(), &spy_seen);
    printf("werewolf: the little girl spies on %s%s\n", pname(spy_seen), spy_caught ? ", caught!" : "");
}

/* The robots of the test mode: they can hold any card, they choose at random (blank votes now and then) */
static void bots_act(void) {
    for (int i = n_real; i < g.n; ++i) {
        if (! (expected() >> i & 1))
            continue;
        uint8_t a = WW_NONE, b = WW_NONE, r = g.role[i];
        uint32_t rnd = get_rand_32();
        switch (phase) {
        case P_ROLES: case P_LOVERS: a = 1; break;
        case P_THIEF:
            if (r == WW_THIEF)
                a = ww_thief_must_take(&g) ? 1 + rnd % 2 : rnd % 3;
            break;
        case P_CUPID:
            if (r == WW_CUPID) {
                a = rand_of(0xFFFFFFFFu);
                b = a < WW_MAX ? rand_of(~(1u << a)) : WW_NONE;
            }
            break;
        case P_WOLVES:
            if (r == WW_WOLF)
                a = rand_of(harmable(i) & ~ww_wolves(&g));
            else if (r == WW_GIRL && (a = rnd % 2))
                girl_spies(i);
            break;
        case P_WITCH:
            if (r == WW_WITCH) {
                a = (victim != WW_NONE && ! g.heal_used && rnd % 2 ? 1 : 0) | (! g.poison_used && rnd % 3 == 0 ? 2 : 0);
                b = rand_of(harmable(i));
            }
            break;
        case P_ELECTION: a = rand_of(0xFFFFFFFFu); break;
        case P_VOTE: a = rnd % 2 ? rand_of(harmable(i)) : WW_NONE; break;
        case P_VOTE2: case P_TIEBREAK: a = rand_of(harmable(i) & candidates); break;
        case P_HUNTER: a = rand_of(harmable(i)); break;
        case P_SUCCESSOR: a = rand_of(0xFFFFFFFFu); break;
        default: break;
        }
        ch_seq[i] = 1;
        ch_a[i] = a;
        ch_b[i] = b;
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
    actor = p == P_HUNTER ? dead_hunter : p == P_SUCCESSOR || p == P_TIEBREAK ? g.captain : WW_NONE;
    if (p != P_TIEBREAK && p != P_VOTE2)
        candidates = 0;
    sync_view();
    bots_act();
    pending |= (expected() | (p == P_ROLES ? 0xFFFFFFFFu : 0)) & real_mask();
    burst = STATE_BURST;
    state_ts = now;
    printf("werewolf: phase %s, day %u, %d alive\n", PHASES[p].log, day, ww_count_alive(&g));
    new_phase(prev);
}

static void set_deaths(const uint8_t *d, int n) {
    n_deaths = 0;
    for (int i = 0; i < n; ++i) {
        if (n_deaths < MAX_DEATHS)
            deaths[n_deaths++] = d[i];
        if (g.role[d[i]] == WW_HUNTER) {
            hunter_pending = true;  /* He shoots before the game goes on */
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
    timed = false;
    printf("werewolf: end, %s\n", ww_win_text(winner));
}

/* A phase of the night takes place: its role is in play and not known dead (a card left to the thief keeps its
 * phase: nobody must learn that it is not held) */
static bool night_phase_on(uint8_t p) {
    int h;
    switch (p) {
    case P_THIEF: return day == 1 && (g.options & WW_OPT_THIEF);
    case P_CUPID: case P_LOVERS: return day == 1 && (g.options & WW_OPT_CUPID);
    case P_SEER: h = ww_find(&g, WW_SEER); return (g.options & WW_OPT_SEER) && (h < 0 || ww_alive(&g, h));
    case P_WITCH: h = ww_find(&g, WW_WITCH); return (g.options & WW_OPT_WITCH) && (h < 0 || ww_alive(&g, h));
    case P_WOLVES: return true;
    default: return false;
    }
}

static void dawn(absolute_time_t now) {
    bool heal = false;
    int poison = WW_NONE;
    int w = ww_find(&g, WW_WITCH);
    if (phase == P_WITCH && w >= 0 && ww_alive(&g, w) && ch_seq[w] && ch_a[w] != WW_NONE) {
        if ((ch_a[w] & 1) && victim != WW_NONE && ! g.heal_used) {
            heal = true;
            printf("werewolf: the witch heals %s\n", pname(victim));
        }
        if ((ch_a[w] & 2) && ww_may_harm(&g, w, ch_b[w]) && ! g.poison_used) {
            poison = ch_b[w];
            printf("werewolf: the witch poisons %s\n", pname(poison));
        }
    }
    uint8_t d[WW_MAX];
    int n = ww_dawn(&g, victim, heal, poison, d);
    set_deaths(d, n);
    enter(P_DAWN, now);
    log_deaths();
}

/* The next phase of the night after \p p (P_NONE: the first one), or the dawn */
static void night_next(uint8_t p, absolute_time_t now) {
    static const uint8_t NIGHT[] = {P_THIEF, P_CUPID, P_LOVERS, P_SEER, P_WOLVES, P_WITCH};
    bool past = p == P_NONE;
    for (unsigned k = 0; k < sizeof(NIGHT); ++k) {
        if (past && night_phase_on(NIGHT[k])) {
            enter(NIGHT[k], now);
            return;
        }
        past |= NIGHT[k] == p;
    }
    dawn(now);
}

static void start_night(absolute_time_t now) {
    ++day;
    victim = seer_target = spy_seen = WW_NONE;
    spy_caught = false;
    night_next(P_NONE, now);
}

/* After deaths: the hunter shoots, the game ends, a dead captain names his successor, then \p next */
static void resolve(uint8_t next, absolute_time_t now) {
    if (hunter_pending) {
        hunter_pending = false;
        after = next;
        enter(P_HUNTER, now);
    } else if (ww_winner(&g) != WW_WIN_NONE) {
        end_game(now);
    } else if (g.captain != WW_NONE && ! ww_alive(&g, g.captain)) {
        after = next;
        enter(P_SUCCESSOR, now);
    } else if (next == NEXT_NIGHT) {
        start_night(now);
    } else {
        enter(next, now);
    }
}

static void eliminate(int out, absolute_time_t now) {
    uint8_t d[WW_MAX];
    int k = out == WW_NONE ? 0 : ww_kill(&g, out, d, 0);
    printf("werewolf: the village eliminates %s\n", out == WW_NONE ? "nobody" : pname(out));
    set_deaths(d, k);
    enter(P_VERDICT, now);
    log_deaths();
}

static void next_phase(absolute_time_t now) {
    uint8_t d[WW_MAX], votes[WW_MAX];
    switch (phase) {
    case P_NONE:
        enter(P_ROLES, now);
        break;
    case P_ROLES:
        start_night(now);
        break;
    case P_THIEF: {
        int t = ww_find(&g, WW_THIEF);
        if (t >= 0 && ww_alive(&g, t)) {
            int k = ch_seq[t] && ch_a[t] >= 1 && ch_a[t] <= 2 ? ch_a[t] - 1 : -1;
            if (k < 0 && ww_thief_must_take(&g))
                k = get_rand_32() % 2;  /* Two wolves: he must take one */
            ww_thief_swap(&g, t, k);
            if (k < 0)
                printf("werewolf: the thief keeps his card\n");
            else
                printf("werewolf: the thief %s takes %s\n", pname(t), ww_role_name(g.role[t]));
        }
        night_next(P_THIEF, now);
        break;
    }
    case P_CUPID: {
        int c = ww_find(&g, WW_CUPID);
        if (c >= 0 && ch_seq[c] && ww_alive(&g, ch_a[c]) && ww_alive(&g, ch_b[c]) && ww_link(&g, ch_a[c], ch_b[c]))
            printf("werewolf: lovers %s and %s\n", pname(ch_a[c]), pname(ch_b[c]));
        else
            printf("werewolf: no lovers\n");
        night_next(P_CUPID, now);
        break;
    }
    case P_WOLVES: {
        uint32_t wolves = ww_wolves(&g);
        for (int i = 0; i < g.n; ++i) {
            uint8_t t = ch_seq[i] ? ch_a[i] : WW_NONE;
            votes[i] = (wolves >> i & 1) && t < g.n && ww_may_harm(&g, i, t) && ! (wolves >> t & 1) ? t : WW_NONE;
        }
        uint32_t tied = 0;
        victim = ww_count_votes(&g, votes, WW_NONE, &tied);
        if (victim == WW_NONE && tied)
            victim = ww_pick(tied, get_rand_32);  /* A tie among the wolves: drawn */
        printf("werewolf: the wolves chose %s\n", victim == WW_NONE ? "nobody" : pname(victim));
        int girl = ww_find(&g, WW_GIRL);
        if (spy_caught && ww_alive(&g, girl)) {
            victim = girl;  /* Caught: she dies instead of the victim */
            printf("werewolf: the little girl was caught, she dies instead\n");
        }
        night_next(P_WOLVES, now);
        break;
    }
    case P_LOVERS: case P_SEER:
        night_next(phase, now);
        break;
    case P_WITCH:
        dawn(now);
        break;
    case P_DAWN:
        resolve(day == 1 && (g.options & WW_OPT_CAPTAIN) && g.captain == WW_NONE ? P_ELECTION : P_DEBATE, now);
        break;
    case P_HUNTER: {
        int n = 0, t = actor < WW_MAX && ch_seq[actor] ? ch_a[actor] : WW_NONE;
        if (actor < WW_MAX && ww_may_harm(&g, actor, t)) {
            n = ww_kill(&g, t, d, 0);
            printf("werewolf: the hunter shoots %s\n", pname(t));
        } else {
            printf("werewolf: the hunter did not shoot\n");
        }
        set_deaths(d, n);
        enter(P_SHOT, now);
        log_deaths();
        break;
    }
    case P_SHOT:
        resolve(after, now);
        break;
    case P_SUCCESSOR: {
        int s = actor < WW_MAX && ch_seq[actor] ? ch_a[actor] : WW_NONE;
        if (! ww_alive(&g, s))
            s = rand_of(0xFFFFFFFFu);  /* No choice: drawn */
        g.captain = s;
        printf("werewolf: the new captain is %s\n", pname(s));
        sync_view();
        resolve(after, now);
        break;
    }
    case P_ELECTION: {
        for (int i = 0; i < g.n; ++i)
            votes[i] = ch_seq[i] ? ch_a[i] : WW_NONE;
        uint32_t tied = 0;
        int c = ww_count_votes(&g, votes, WW_NONE, &tied);
        if (c == WW_NONE)
            c = ww_pick(tied ? tied : g.alive, get_rand_32);  /* A tie or no vote: drawn */
        g.captain = c;
        printf("werewolf: %s is elected captain\n", pname(c));
        sync_view();
        enter(P_DEBATE, now);
        break;
    }
    case P_DEBATE:
        enter(P_VOTE, now);
        break;
    case P_VOTE:
    case P_VOTE2: {
        for (int i = 0; i < g.n; ++i) {
            uint8_t t = ch_seq[i] ? ch_a[i] : WW_NONE;
            bool ok = t < g.n && ww_may_harm(&g, i, t) && (phase == P_VOTE || (candidates >> t & 1));
            votes[i] = ok ? t : WW_NONE;
            if (ok)
                printf("werewolf: vote %s -> %s\n", pname(i), pname(t));
        }
        int out;
        uint32_t tied;
        ww_day_t r = ww_day_vote(&g, votes, phase == P_VOTE2, &out, &tied);
        if (r == WW_DAY_TIEBREAK || r == WW_DAY_REVOTE) {
            candidates = tied;
            printf("werewolf: tie, %s\n", r == WW_DAY_TIEBREAK ? "the captain decides" : "second vote");
            enter(r == WW_DAY_TIEBREAK ? P_TIEBREAK : P_VOTE2, now);
        } else {
            if (out != WW_NONE && ww_alive(&g, g.captain) && votes[g.captain] == out && tied)
                printf("werewolf: tie, the vote of the captain decides\n");
            eliminate(r == WW_DAY_OUT ? out : WW_NONE, now);
        }
        break;
    }
    case P_TIEBREAK: {
        int t = actor < WW_MAX && ch_seq[actor] ? ch_a[actor] : WW_NONE;
        eliminate(t < g.n && (candidates >> t & 1) && ww_may_harm(&g, actor, t) ? t : WW_NONE, now);
        break;
    }
    case P_VERDICT:
        resolve(NEXT_NIGHT, now);
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
    x[2] = ww_partner(&g, i);
    net_put_u32(x + 3, g.role[i] == WW_WOLF ? ww_wolves(&g) : 0);
    x[7] = ch_seq[i];
    x[8] = i;
    if (phase == P_THIEF && g.role[i] == WW_THIEF) {
        x[9] = g.center[0];
        x[10] = g.center[1];
    } else if (phase == P_SEER && g.role[i] == WW_SEER && seer_target != WW_NONE) {
        x[9] = seer_target;
        x[10] = g.role[seer_target];
    } else if (phase == P_WOLVES && g.role[i] == WW_GIRL) {
        x[9] = spy_seen;
    } else if (phase == P_WITCH && g.role[i] == WW_WITCH) {
        x[9] = victim;
    }
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
    uint8_t d[STATE_LEN];
    memset(d, 0, sizeof(d));
    d[0] = phase;
    d[1] = step;
    d[2] = day;
    d[3] = g.n;
    d[4] = options;
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
    d[26] = actor;
    d[27] = n_deaths;
    memcpy(d + 28, deaths, MAX_DEATHS);
    net_put_u32(d + 34, winners);
    d[38] = phase == P_END ? lovers_end[0] : WW_NONE;
    d[39] = phase == P_END ? lovers_end[1] : WW_NONE;
    d[40] = g.captain;
    net_put_u32(d + 41, candidates);
    d[45] = debate;
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
    if (kind == PARTY_KIND_LEAVE) {
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
    if (phase == P_ROLES || phase == P_LOVERS)
        printf("werewolf: %s (%s) is ready\n", pname(i), ww_role_name(g.role[i]));
    else
        printf("werewolf: %s (%s) chose %s%s%s\n", pname(i), ww_role_name(g.role[i]),
               a < WW_MAX ? pname(a) : "-", b < WW_MAX ? " and " : "", b < WW_MAX ? pname(b) : "");
    if (phase == P_SEER && g.role[i] == WW_SEER && seer_target == WW_NONE && ww_alive(&g, a) && a != i) {
        seer_target = a;  /* The first choice only: the seer sees one card per night */
        printf("werewolf: the seer sees %s (%s)\n", pname(a), ww_role_name(g.role[a]));
    }
    if (phase == P_WOLVES && g.role[i] == WW_GIRL && a == 1)
        girl_spies(i);
    if (phase == P_WOLVES && g.role[i] == WW_WOLF)
        pending |= g.alive & real_mask();  /* The wolves see the votes of the others; everybody gets a packet alike */
}

static void narrator_launch(absolute_time_t now) {
    n_lobby = party_players(lobby, PARTY_MAX);
    n_real = n_lobby > WW_MAX_PLAYERS ? WW_MAX_PLAYERS : n_lobby;
    int n = n_real;
    if (debug_mode() && n < WW_MIN_PLAYERS)
        n = WW_MIN_PLAYERS;
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
    ww_deal(&g, n, options & WW_OPT_ALL, get_rand_32);  /* Not party_seed(): it is sent in clear */
    party_start(START_DELAY_MS);
    game_ts = party_start_time();
    if (! game_ts)
        game_ts = delayed_by_ms(now, START_DELAY_MS);
    phase = P_NONE;
    step = 0;
    day = 0;
    have_state = true;
    winner = WW_WIN_NONE;
    winners = candidates = 0;
    actor = WW_NONE;
    n_deaths = 0;
    pending = 0;
    hunter_pending = false;
    names_next = 0;
    names_ts = now;
    timed = false;
    lovers_end[0] = lovers_end[1] = WW_NONE;
    sync_view();
    char text[96];
    options_text(text, sizeof(text), options & WW_OPT_ALL);
    printf("werewolf: launch, %d players (%d robots), %d wolves, %s\n", n, n - n_real, ww_wolves_for(n), text);
    for (int i = 0; i < n; ++i)
        printf("werewolf: %s is %s\n", names[i], ww_role_name(g.role[i]));
    if (g.options & WW_OPT_THIEF)
        printf("werewolf: the cards left are %s and %s\n", ww_role_name(g.center[0]), ww_role_name(g.center[1]));
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
static uint8_t my_seq = 0, my_a = WW_NONE, my_b = WW_NONE, pick1 = WW_NONE;
static uint8_t seen[WW_MAX];  /* The cards seen by the seer, the wolves seen by the little girl */
static absolute_time_t act_ts = 0, last_state = 0;
static bool cancelled = false, end_done = false, i_was_alive = true;
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
    case P_ROLES: case P_THIEF: case P_CUPID: case P_LOVERS: case P_SEER: case P_WOLVES: case P_WITCH:
    case P_ELECTION: case P_VOTE: case P_VOTE2:
        return me_alive();
    case P_HUNTER: case P_SUCCESSOR: case P_TIEBREAK:
        return actor == my_idx;
    default:
        return false;
    }
}

static bool priv_fresh(void) {
    return priv_step == step;
}

static void reset_player(void) {
    my_idx = my_role = my_lover = WW_NONE;
    my_wolves = 0;
    priv_step = ack_seq = 0;
    my_seq = 0;
    my_a = my_b = pick1 = WW_NONE;
    memset(seen, WW_NONE, sizeof(seen));
    memset(revealed, WW_NONE, sizeof(revealed));
    have_state = false;
    phase = P_NONE;
    step = day = n_players = 0;
    alive = acted = winners = candidates = 0;
    winner = WW_WIN_NONE;
    actor = captain = WW_NONE;
    n_deaths = 0;
    cancelled = end_done = false;
    i_was_alive = true;
    timed = false;
    default_names();
}

static void player_state(const uint8_t *d, uint8_t len, absolute_time_t at) {
    if (len < STATE_LEN || d[0] >= P_COUNT || d[3] > WW_MAX)
        return;
    last_state = at;
    bool fresh = ! have_state || d[1] != step;
    uint8_t prev = phase;
    phase = d[0];
    step = d[1];
    day = d[2];
    n_players = d[3];
    options = d[4];
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
    actor = d[26];
    n_deaths = d[27] > MAX_DEATHS ? MAX_DEATHS : d[27];
    memcpy(deaths, d + 28, MAX_DEATHS);
    winners = net_u32(d + 34);
    lovers_end[0] = d[38];
    lovers_end[1] = d[39];
    if (d[40] != captain && d[40] < WW_MAX)
        printf("werewolf: captain %s\n", pname(d[40]));
    captain = d[40];
    candidates = net_u32(d + 41);
    debate = d[45];
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
        if (page == PG_HELP && help_back == PG_GAME)
            page = PG_GAME;  /* The card shown: the new phase first */
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
    if (x[2] != my_lover && x[2] < WW_MAX)
        printf("werewolf: in love with %s\n", pname(x[2]));
    my_lover = x[2];
    my_wolves = net_u32(x + 3);
    priv_step = x[0];
    ack_seq = x[7];
    info1 = x[9];
    info2 = x[10];
    memcpy(wolf_votes, x + 11, 4);
    potions = x[15];
    if (priv_fresh() && phase == P_SEER && my_role == WW_SEER && info1 < WW_MAX && info2 < WW_ROLES
            && seen[info1] != info2) {
        seen[info1] = info2;
        printf("werewolf: seen %s is %s\n", pname(info1), ww_role_name(info2));
    }
    if (priv_fresh() && phase == P_WOLVES && my_role == WW_GIRL && info1 < WW_MAX && seen[info1] != WW_WOLF) {
        seen[info1] = WW_WOLF;
        printf("werewolf: spied %s, a wolf\n", pname(info1));
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
    bool unacked = my_seq && (! priv_fresh() || ack_seq < my_seq);
    bool missing = my_role == WW_NONE || (must_act() && ! priv_fresh());
    if ((unacked || missing) && absolute_time_diff_us(act_ts, now) >= 0) {
        send_act();
        act_ts = delayed_by_ms(now, ACT_RESEND_MS + get_rand_32() % ACT_JITTER_MS);
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
    if ((kind == K_ABORT || kind == PARTY_KIND_LEAVE) && ! cancelled && phase != P_END) {
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

static uint8_t rows[WW_MAX + 3];
static int n_rows = 0;

static void add_players(uint32_t m) {
    for (int i = 0; i < n_players; ++i)
        if (m >> i & 1)
            rows[n_rows++] = i;
}

/* The witch chooses in two steps (the potion of life, then the poison); the others pretend the same way */
static bool second_step(void) {
    return (phase == P_CUPID || phase == P_WITCH) && pick1 != WW_NONE;
}

/* The rows of the list of choices of this phase (the same for the role and for those who pretend, if possible) */
static void build_rows(void) {
    n_rows = 0;
    uint32_t living = alive & ((1u << n_players) - 1);
    uint32_t others = living & ~(my_idx < WW_MAX ? 1u << my_idx : 0);
    uint32_t harm = others & ~(my_lover < WW_MAX ? 1u << my_lover : 0);  /* Never against the lover */
    switch (phase) {
    case P_THIEF:
        if (my_role == WW_THIEF && priv_fresh() && info1 < WW_ROLES) {
            if (! (info1 == WW_WOLF && info2 == WW_WOLF))
                rows[n_rows++] = ROW_KEEP;
            rows[n_rows++] = ROW_CARD1;
            rows[n_rows++] = ROW_CARD2;
        } else {
            add_players(others);
        }
        break;
    case P_CUPID: case P_ELECTION:
        add_players(living);
        break;
    case P_WOLVES:
        if (my_role == WW_GIRL) {
            rows[n_rows++] = ROW_SLEEP;
            rows[n_rows++] = ROW_SPY;
        } else {
            add_players(my_role == WW_WOLF ? harm : others);
        }
        break;
    case P_WITCH:
        if (my_role != WW_WITCH) {
            add_players(others);
        } else if (! second_step()) {
            if ((potions & 1) && info1 < WW_MAX) {
                rows[n_rows++] = ROW_HEAL;
                rows[n_rows++] = ROW_NOHEAL;
            } else {
                rows[n_rows++] = ROW_CONTINUE;
            }
        } else {
            rows[n_rows++] = ROW_NOTHING;
            for (int i = 0; i < n_players && (potions & 2); ++i)
                if (harm >> i & 1)
                    rows[n_rows++] = ROW_POISON | i;
        }
        break;
    case P_VOTE:
        rows[n_rows++] = ROW_NOTHING;
        add_players(harm);
        break;
    case P_VOTE2:
        rows[n_rows++] = ROW_NOTHING;
        add_players(harm & candidates);
        break;
    case P_TIEBREAK:
        add_players(harm & candidates);
        break;
    case P_HUNTER:
        add_players(harm);
        break;
    case P_SUCCESSOR:
        add_players(living);
        break;
    default:
        add_players(others);
        break;
    }
}

static int wolf_votes_for(int t) {
    int c = 0, k = 0;
    for (int w = 0; w < n_players && k < 4; ++w)
        if (my_wolves >> w & 1)
            c += wolf_votes[k++] == t;
    return c;
}

/* My choice of this phase, as a row value (to mark it in the list) */
static bool chosen_row(uint8_t v) {
    if (! my_seq)
        return false;
    if (phase == P_CUPID)
        return v == my_a || v == my_b;
    if (phase == P_THIEF && my_role == WW_THIEF && priv_fresh())
        return v == (my_a == 1 ? ROW_CARD1 : my_a == 2 ? ROW_CARD2 : ROW_KEEP);
    if (phase == P_WOLVES && my_role == WW_GIRL)
        return v == (my_a == 1 ? ROW_SPY : ROW_SLEEP);
    if (phase == P_WITCH && my_role == WW_WITCH)
        return v == ((my_a & 2) ? (ROW_POISON | my_b) : ROW_NOTHING);
    if (v == ROW_NOTHING)
        return my_a == WW_NONE;
    return v == my_a;
}

static void choice_label(int r, char *buf, size_t len) {
    uint8_t v = rows[r];
    const char *mark = chosen_row(v) ? "> " : (phase == P_CUPID && v == pick1) ? "1 " : "";
    switch (v) {
    case ROW_NOTHING: snprintf(buf, len, "%s%s", mark, phase == P_WITCH ? "Personne" : "Vote blanc"); return;
    case ROW_HEAL: snprintf(buf, len, "%sSauver %s", mark, pname(info1)); return;
    case ROW_NOHEAL: snprintf(buf, len, "%sNe pas sauver", mark); return;
    case ROW_CONTINUE: snprintf(buf, len, "%sContinuer", mark); return;
    case ROW_KEEP: snprintf(buf, len, "%sGarder ma carte", mark); return;
    case ROW_CARD1: case ROW_CARD2:
        snprintf(buf, len, "%sPrendre : %s", mark, ww_role_name(v == ROW_CARD1 ? info1 : info2));
        return;
    case ROW_SLEEP: snprintf(buf, len, "%sDormir", mark); return;
    case ROW_SPY: snprintf(buf, len, "%sEspionner les loups", mark); return;
    default: break;
    }
    if (v & ROW_POISON) {
        snprintf(buf, len, "%sEmpoisonner %s", mark, pname(v & ~ROW_POISON));
        return;
    }
    char tag[32] = "";
    if (my_role == WW_WOLF && (my_wolves >> v & 1))
        snprintf(tag, sizeof(tag), " (loup)");
    else if (seen[v] < WW_ROLES)
        snprintf(tag, sizeof(tag), " (%s)", ww_role_name(seen[v]));
    if (v == my_lover)
        snprintf(tag + strlen(tag), sizeof(tag) - strlen(tag), " <3");
    if (v == captain)
        snprintf(tag + strlen(tag), sizeof(tag) - strlen(tag), " (cap.)");
    int votes = phase == P_WOLVES && my_role == WW_WOLF && priv_fresh() ? wolf_votes_for(v) : 0;
    if (votes)
        snprintf(tag + strlen(tag), sizeof(tag) - strlen(tag), " [%d]", votes);
    snprintf(buf, len, "%s%s%s", mark, pname(v), tag);
}

/* The cards (narrator; everybody at the end): "x " dead, "<3" lover, "(cap.)" captain */
static void role_label(int i, char *buf, size_t len) {
    int role = mode == M_NARRATOR ? g.role[i] : revealed[i];
    bool lover = mode == M_NARRATOR ? ww_partner(&g, i) != WW_NONE : (lovers_end[0] == i || lovers_end[1] == i);
    snprintf(buf, len, "%s%s : %s%s%s", is_alive(i) ? "" : "x ", pname(i), ww_role_name(role), lover ? " <3" : "",
             i == captain ? " (cap.)" : "");
}

static void menu_label(int i, char *buf, size_t len) {
    snprintf(buf, len, "%s", i == MENU_NARRATE ? "Mener une partie" : i == MENU_JOIN ? "Rejoindre une partie"
             : "Aide : les rôles");
}

static void setup_label(int i, char *buf, size_t len) {
    if (i == SETUP_PRESET) {
        snprintf(buf, len, "Préréglage : %s", set_options == WW_OPT_ALL ? "classique"
                 : set_options == WW_OPT_BEGINNER ? "débutant" : "à la carte");
    } else if (i < SETUP_DEBATE) {
        const int k = i - SETUP_OPTIONS;
        snprintf(buf, len, "[%c] %s", set_options & OPTIONS[k].opt ? 'x' : ' ', OPTIONS[k].name);
    } else if (i == SETUP_DEBATE) {
        snprintf(buf, len, "Débat : %u min", DEBATE_S[set_debate] / 60);
    } else if (i == SETUP_WOLVES) {
        snprintf(buf, len, "Loups : 2, ou 3 dès 12 j.");
    } else {
        snprintf(buf, len, "> Ouvrir la partie");
    }
}

static void lobby_label(int i, char *buf, size_t len) {
    snprintf(buf, len, "%s", lobby[i].name);
}

static void found_label(int i, char *buf, size_t len) {
    snprintf(buf, len, "%s  %u joueur%s", found[i].name, found[i].players, found[i].players > 1 ? "s" : "");
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

/* Left aligned text cut in lines at the spaces to fit \p w pixels (at most \p max_lines), from \p y.
 * \return the y after the last line */
static int text_box(uint8_t *fb, int x, int y, int w, const gfx_font_t *font, const char *text, int max_lines) {
    char line[96];
    int lines = 0;
    while (*text && lines < max_lines) {
        while (*text == ' ')
            ++text;
        size_t n = 0, fit = 0;
        while (text[n] && n < sizeof(line) - 1) {
            size_t end = n;
            while (text[end] == ' ')
                ++end;
            while (text[end] && text[end] != ' ' && end < sizeof(line) - 1)
                ++end;
            memcpy(line, text, end);
            line[end] = 0;
            if (gfx_text_width(font, line) > w && fit)
                break;
            fit = n = end;
            if (gfx_text_width(font, line) > w)
                break;
        }
        line[fit] = 0;
        if (lines == max_lines - 1 && text[fit])
            ui_check_width(font, text, w, "text cut");  /* The text does not fit: traced */
        gfx_text(fb, x, y, font, line, GFX_BLACK, GFX_ALIGN_LEFT);
        ui_check_bottom(y + font->height, line);
        y += font->height + 3;
        text += fit;
        ++lines;
    }
    return y;
}

/* An illustration of a card, twice as big, in a frame */
static void draw_icon(uint8_t *fb, int x, int y, const uint8_t *icon) {
    gfx_rect(fb, x - 3, y - 3, 2 * WW_ICON_SIZE + 6, 2 * WW_ICON_SIZE + 6, GFX_BLACK);
    for (int r = 0; r < WW_ICON_SIZE; ++r)
        for (int c = 0; c < WW_ICON_SIZE; ++c)
            if (icon[r * (WW_ICON_SIZE / 8) + c / 8] & (0x80 >> (c % 8)))
                gfx_fill_rect(fb, x + 2 * c, y + 2 * r, 2, 2, GFX_BLACK);
}

/* A card: the illustration, the name and the camp beside it, its power below (or \p extra: the other wolves) */
static void render_card(uint8_t *fb, const char *title, int card, const char *extra) {
    const ww_card_t *c = &WW_CARD[card];
    ui_title(fb, title);
    draw_icon(fb, 7, UI_TITLE_H + 7, c->icon);
    int y = text_box(fb, 82, UI_TITLE_H + 10, GFX_WIDTH - 86, &gfx_font_medium, c->name, 2);
    text_box(fb, 82, y + 4, GFX_WIDTH - 86, &gfx_font_small, c->camp, 2);
    y = UI_TITLE_H + 2 * WW_ICON_SIZE + 12;
    text_box(fb, 4, y, GFX_WIDTH - 8, &gfx_font_small, extra ? extra : c->power, 3);
}

static void render_help(uint8_t *fb) {
    const ww_card_t *c = &WW_CARD[help_card];
    if (help_details) {
        ui_title(fb, c->name);
        text_box(fb, 4, UI_TITLE_H + 2, GFX_WIDTH - 8, &gfx_font_small, c->details, 7);
        ui_footer(fb, "D : la carte  G : retour");
        return;
    }
    render_card(fb, help_back == PG_GAME && help_card == my_role ? "Ta carte" : "Les rôles", help_card, NULL);
    char text[40];
    snprintf(text, sizeof(text), "%d/%d  Flancs, D : détails", help_card + 1, WW_CARDS);
    ui_footer(fb, help_back == PG_GAME ? "D : détails  G : la partie" : text);
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
        const char *none = phase == P_DAWN ? "Personne n'est mort." : phase == P_VERDICT ? "Personne."
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
    ui_footer(fb, "D : les cartes  G long : fin");
}

static void render_narrator(uint8_t *fb, absolute_time_t now) {
    char text[64];
    if (phase == P_NONE) {
        ui_title(fb, "Loup-garou");
        ui_lines(fb, 70, &gfx_font_medium, "Distribution\ndes cartes...");
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

/* The prompt of a list of choices */
static const char *prompt_text(char *text, size_t len) {
    bool real = false;  /* The role of this badge acts (the others pretend) */
    const char *p = "Fais semblant de choisir";
    switch (phase) {
    case P_THIEF:
        if (my_role == WW_THIEF && priv_fresh() && info1 < WW_ROLES)
            p = info1 == WW_WOLF && info2 == WW_WOLF ? "Deux loups : prends-en un !" : "Ta carte ou une autre ?";
        break;
    case P_CUPID:
        real = my_role == WW_CUPID;
        p = real ? (pick1 == WW_NONE ? "1er amoureux ?" : "2e amoureux ?")
            : pick1 == WW_NONE ? "Fais semblant : 1er nom" : "Fais semblant : 2e nom";
        break;
    case P_SEER:
        if (my_role == WW_SEER && priv_fresh() && info1 < WW_MAX && info2 < WW_ROLES) {
            snprintf(text, len, "%s : %s", pname(info1), ww_role_name(info2));
            return text;
        }
        if (my_role == WW_SEER)
            p = "Qui sonder ?";
        break;
    case P_WOLVES:
        if (my_role == WW_WOLF)
            p = "Votre victime ?";
        if (my_role == WW_GIRL) {
            if (priv_fresh() && info1 < WW_MAX) {
                snprintf(text, len, "Tu as vu %s (loup)", pname(info1));
                return text;
            }
            p = "Espionner ? (risqué)";
        }
        break;
    case P_WITCH:
        if (my_role != WW_WITCH)
            p = second_step() ? "Fais semblant (2/2)" : "Fais semblant (1/2)";
        else if (second_step())
            p = (potions & 2) ? "Empoisonner ?" : "Plus de poison";
        else if (info1 < WW_MAX) {
            snprintf(text, len, "Victime : %s", pname(info1));
            return text;
        } else
            p = "Pas de victime";
        break;
    case P_ELECTION: p = "Qui sera capitaine ?"; break;
    case P_VOTE: p = "Qui éliminer ?"; break;
    case P_VOTE2: p = "2e vote : qui éliminer ?"; break;
    case P_TIEBREAK: p = "Égalité : qui éliminer ?"; break;
    case P_HUNTER: p = "Qui emporter ?"; break;
    case P_SUCCESSOR: p = "Ton successeur ?"; break;
    default: break;
    }
    return p;
}

/* The list of choices of a phase: the prompt, the timer, the rows. \return the footer */
static const char *render_choices(uint8_t *fb, int y, const char *timer, const char *lost_footer) {
    char text[48];
    gfx_text(fb, 4, y, &gfx_font_small, prompt_text(text, sizeof(text)), GFX_BLACK, GFX_ALIGN_LEFT);
    gfx_text(fb, GFX_WIDTH - 4, y, &gfx_font_small, timer, GFX_BLACK, GFX_ALIGN_RIGHT);
    build_rows();
    if (list_sel >= n_rows)
        list_sel = n_rows ? n_rows - 1 : 0;
    list_at(fb, y + 18, 6, n_rows, list_sel, choice_label);
    if (lost_footer)
        return lost_footer;
    if (! my_seq)
        return "Flancs : choix  D : valider";
    if (ack_seq < my_seq || ! priv_fresh())
        return "Envoi...";
    return phase == P_SEER && my_role == WW_SEER ? "Choix reçu" : "Choix reçu  D : changer";
}

/* A phase where only one player acts (the hunter, the captain): what the others see */
static void render_waiting(uint8_t *fb, int y) {
    char text[64];
    if (phase == P_HUNTER)
        snprintf(text, sizeof(text), "Le chasseur %s\nchoisit sa cible...", pname(actor));
    else if (phase == P_SUCCESSOR)
        snprintf(text, sizeof(text), "Le capitaine %s\nchoisit son successeur...", pname(actor));
    else
        snprintf(text, sizeof(text), "Égalité : le capitaine\n%s tranche...", pname(actor));
    ui_lines(fb, y + 40, &gfx_font_small, text);
}

static void render_player(uint8_t *fb, absolute_time_t now) {
    char text[96];
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
    if (phase == P_ROLES) {
        if (my_role >= WW_ROLES) {
            ui_title(fb, "Ta carte");
            ui_lines(fb, 80, &gfx_font_medium, "Carte en attente...");
            ui_footer(fb, "G : menu");
            return;
        }
        text[0] = 0;
        if (my_role == WW_WOLF && __builtin_popcount(my_wolves) > 1) {
            snprintf(text, sizeof(text), "Avec toi :");
            for (int i = 0; i < n_players; ++i)
                if ((my_wolves >> i & 1) && i != my_idx)
                    snprintf(text + strlen(text), sizeof(text) - strlen(text), " %s", pname(i));
        }
        render_card(fb, "Ta carte", my_role, text[0] ? text : NULL);
        ui_footer(fb, my_seq ? (ack_seq >= my_seq && priv_fresh() ? "Compris, reçu" : "Envoi...")
                  : "Cache l'écran !  D : compris");
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
    const char *footer = lost ? "Meneur hors de portée !" : "G : menu  D long : carte";
    int y = UI_TITLE_H + 2;
    if (my_idx < WW_MAX && ! me_alive() && ! must_act()) {
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
        y = ui_lines(fb, y + 4, &gfx_font_medium, "Débattez !");
        y = ui_lines(fb, y + 2, &gfx_font_large, timer);
        snprintf(text, sizeof(text), "%d joueurs en vie", __builtin_popcount(alive));
        y = ui_lines(fb, y + 4, &gfx_font_small, text);
        if (captain < WW_MAX) {
            snprintf(text, sizeof(text), "Capitaine : %s", pname(captain));
            y = ui_lines(fb, y, &gfx_font_small, text);
        }
        ui_wrapped(fb, y + 2, &gfx_font_small, "Le vote suit.", 1);
        break;
    case P_LOVERS:
        if (my_lover < WW_MAX)
            snprintf(text, sizeof(text), "Ton amoureux(se) :\n%s", pname(my_lover));
        else
            snprintf(text, sizeof(text), "Les amoureux\nse reconnaissent...");
        ui_lines(fb, y, &gfx_font_small, timer);
        ui_box(fb, text);
        if (! lost)
            footer = my_seq ? (ack_seq >= my_seq && priv_fresh() ? "Vu, reçu" : "Envoi...") : "D : vu";
        break;
    default:
        if (! must_act()) {
            if (phase == P_HUNTER || phase == P_SUCCESSOR || phase == P_TIEBREAK)
                render_waiting(fb, y);
            break;
        }
        footer = render_choices(fb, y, timer, lost ? footer : NULL);
        break;
    }
    ui_footer(fb, footer);
}

static void render_roles(uint8_t *fb) {
    ui_title(fb, "Les cartes");
    list_at(fb, UI_TITLE_H + 3, 7, n_players, list_sel, role_label);
    ui_footer(fb, "G : retour");
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

static void ww_render(uint8_t *fb, absolute_time_t now) {
    char text[128];
    switch (page) {
    case PG_MENU:
        ui_title(fb, "Loup-garou");
        list_at(fb, UI_TITLE_H + 3, MENU_ROWS, MENU_ROWS, list_sel, menu_label);
        if (other_game())
            snprintf(text, sizeof(text), "Une partie de %s est en cours : quitte-la d'abord.", other_game());
        else
            snprintf(text, sizeof(text), "Un meneur (qui ne joue pas) et 8 à 18 joueurs, chacun avec son badge.");
        ui_wrapped(fb, UI_TITLE_H + 76, &gfx_font_small, text, 3);
        ui_footer(fb, "G : retour  D : choisir");
        break;
    case PG_SETUP:
        ui_title(fb, "Mener une partie");
        list_at(fb, UI_TITLE_H + 3, 7, SETUP_ROWS, setup_row, setup_label);
        ui_footer(fb, setup_row == SETUP_OPEN ? "G : retour  D : ouvrir"
                  : setup_row == SETUP_WOLVES ? "Selon la règle du jeu" : "Flancs : ligne  D : changer");
        break;
    case PG_LOBBY: {
        ui_title(fb, "Partie ouverte");
        int min = min_players();
        snprintf(text, sizeof(text), "%d joueur%s (%d à %d), %d loups", n_lobby, n_lobby > 1 ? "s" : "", min,
                 WW_MAX_PLAYERS, ww_wolves_for(debug_mode() && n_lobby < WW_MIN_PLAYERS ? WW_MIN_PLAYERS
                                               : n_lobby < min_players() ? min_players() : n_lobby));
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
        snprintf(text, sizeof(text), "%s  %d joueur%s", in ? "Inscrit(e) !" : "Inscription...", party_count(),
                 party_count() > 1 ? "s" : "");
        int y = ui_lines(fb, UI_TITLE_H + 6, &gfx_font_small, text);
        char roles[96];
        options_text(roles, sizeof(roles), options & WW_OPT_ALL);
        snprintf(text, sizeof(text), "Rôles : %s.", roles);
        y = ui_wrapped(fb, y + 4, &gfx_font_small, text, 4);
        ui_wrapped(fb, y + 6, &gfx_font_small, "En attente du meneur...", 1);
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
    case PG_HELP:
        render_help(fb);
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
    switch (phase) {
    case P_CUPID:
        if (pick1 == WW_NONE || pick1 == v) {
            pick1 = v;
            list_sel = 0;
            return;
        }
        my_a = pick1;
        my_b = v;
        pick1 = WW_NONE;
        break;
    case P_WITCH:
        if (pick1 == WW_NONE) {
            pick1 = v == ROW_HEAL ? 1 : 0;  /* The potion of life; then the poison */
            list_sel = 0;
            return;
        }
        if (my_role == WW_WITCH) {
            my_a = (pick1 ? 1 : 0) | ((v & ROW_POISON) && v != ROW_NOTHING ? 2 : 0);
            my_b = my_a & 2 ? v & ~ROW_POISON : WW_NONE;
        } else {
            my_a = v;  /* Pretending: any name */
            my_b = WW_NONE;
        }
        pick1 = WW_NONE;
        break;
    case P_THIEF:
        my_a = my_role != WW_THIEF ? v : v == ROW_CARD1 ? 1 : v == ROW_CARD2 ? 2 : 0;  /* The others pretend */
        my_b = WW_NONE;
        break;
    case P_WOLVES:
        if (my_role == WW_WOLF && v < WW_MAX && (my_wolves >> v & 1))
            return;  /* Not a wolf */
        my_a = my_role == WW_GIRL ? v == ROW_SPY : v;
        my_b = WW_NONE;
        break;
    default:
        my_a = v < WW_MAX ? v : WW_NONE;
        my_b = WW_NONE;
        break;
    }
    ++my_seq;
    act_ts = now;
    printf("werewolf: my choice %u %u\n", my_a, my_b);
}

static void open_card(int card, int back, absolute_time_t now) {
    help_card = card;
    help_details = false;
    help_back = back;
    help_until = delayed_by_ms(now, CARD_SHOWN_MS);
    page = PG_HELP;
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
    if ((b->long_pressed & UI_BTN_B) && my_role < WW_ROLES) {
        open_card(my_role, PG_GAME, now);  /* My card, for a while */
        return true;
    }
    if (phase == P_ROLES || phase == P_LOVERS) {
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

static void open_party(void) {
    bool debug = unlock_mode() == WW_UNLOCK_ROBOTS;  /* Admin > Loup-garou (admin): the test mode with robots */
    options = set_options | (debug ? FLAG_DEBUG : 0);
    debate = set_debate;
    reset_player();
    memset(&g, 0, sizeof(g));
    n_lobby = 0;
    party_set_handler(handle);
    party_host(PARTY_GAME_WEREWOLF, false, WW_MAX_PLAYERS, options);
    mode = M_NARRATOR;
    page = PG_LOBBY;
    list_sel = 0;
    char text[96];
    options_text(text, sizeof(text), set_options);
    printf("werewolf: narrator, debate %u s%s, %s\n", debate_secs(), debug ? ", test mode" : "", text);
}

static bool ww_buttons(const app_buttons_t *b, absolute_time_t now) {
    switch (page) {
    case PG_MENU:
        if (b->pressed & UI_BTN_Y)
            list_sel = (list_sel + MENU_ROWS - 1) % MENU_ROWS;
        if (b->pressed & UI_BTN_X)
            list_sel = (list_sel + 1) % MENU_ROWS;
        if (b->pressed & UI_BTN_A)
            return false;
        if ((b->pressed & UI_BTN_B) && list_sel == MENU_HELP) {
            open_card(0, PG_MENU, now);
        } else if ((b->pressed & UI_BTN_B) && other_game()) {
            printf("werewolf: a party of %s is in progress\n", other_game());
        } else if (b->pressed & UI_BTN_B) {
            if (list_sel == MENU_NARRATE) {
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
            setup_row = (setup_row + SETUP_ROWS - 1) % SETUP_ROWS;
        if (b->pressed & UI_BTN_X)
            setup_row = (setup_row + 1) % SETUP_ROWS;
        if (b->pressed & UI_BTN_A) {
            page = PG_MENU;
            list_sel = MENU_NARRATE;
        }
        if (b->pressed & UI_BTN_B) {
            if (setup_row == SETUP_PRESET)
                set_options = set_options == WW_OPT_ALL ? WW_OPT_BEGINNER : WW_OPT_ALL;
            else if (setup_row < SETUP_DEBATE)
                set_options ^= OPTIONS[setup_row - SETUP_OPTIONS].opt;
            else if (setup_row == SETUP_DEBATE)
                set_debate = (set_debate + 1) % 3;
            else if (setup_row == SETUP_OPEN && ! other_game())
                open_party();
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
            list_sel = MENU_JOIN;
            return true;
        }
        if (n_found && (b->pressed & UI_BTN_Y))
            list_sel = (list_sel + n_found - 1) % n_found;
        if (n_found && (b->pressed & UI_BTN_X))
            list_sel = (list_sel + 1) % n_found;
        if (n_found && (b->pressed & UI_BTN_B) && list_sel < n_found) {
            party_join(found[list_sel].host);
            options = found[list_sel].flags;
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
    case PG_HELP:
        if (b->pressed & (UI_BTN_X | UI_BTN_Y)) {
            help_card = (help_card + ((b->pressed & UI_BTN_X) ? 1 : WW_CARDS - 1)) % WW_CARDS;
            help_details = false;
            help_until = delayed_by_ms(now, CARD_SHOWN_MS);
        }
        if (b->released_short & UI_BTN_B)
            help_details = ! help_details;
        if (b->released_short & UI_BTN_A) {
            page = help_back;
            list_sel = help_back == PG_MENU ? MENU_HELP : 0;
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
    } else if (page == PG_QUIT || page == PG_ROLES || page == PG_HELP) {
        page = page == PG_QUIT && (quit_back == PG_LOBBY || quit_back == PG_WAIT) ? quit_back : PG_GAME;
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
    if (page == PG_HELP && help_back == PG_GAME && absolute_time_diff_us(help_until, now) >= 0) {
        page = PG_GAME;  /* The card of the player does not stay on the screen */
        changed = true;
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


/* ------ Admin > Loup-garou (admin): below 8 players (the minimum of the rules), no game unless unlocked here ------ */

static const char *UNLOCK_NAMES[WW_UNLOCKS] = {"8 joueurs minimum", "Petites parties (4+)", "Test : robots"};
static const char *UNLOCK_HELP[WW_UNLOCKS] = {
    "La règle du jeu :\nde 8 à 18 joueurs.",
    "Dès 4 joueurs, avec\n1 loup-garou sous 8.",
    "Dès 1 joueur : des robots\ncomplètent jusqu'à 8.",
};
static int unlock_sel = 0;

static void unlock_label(int i, char *buf, size_t len) {
    snprintf(buf, len, "%s %s", i == unlock_mode() ? "(o)" : "( )", UNLOCK_NAMES[i]);
}

static void unlock_start(absolute_time_t now) {
    (void)now;
    unlock_sel = unlock_mode();
    printf("werewolf: unlock page, mode %d\n", unlock_sel);
}

static bool unlock_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (b->pressed & UI_BTN_A)
        return false;
    if (b->pressed & UI_BTN_X)
        unlock_sel = (unlock_sel + 1) % WW_UNLOCKS;
    if (b->pressed & UI_BTN_Y)
        unlock_sel = (unlock_sel + WW_UNLOCKS - 1) % WW_UNLOCKS;
    if (b->pressed & UI_BTN_B) {
        store_get()->ww_unlock = unlock_sel;
        store_changed();
        printf("werewolf: unlock %s\n", UNLOCK_NAMES[unlock_sel]);
    }
    return true;
}

static void unlock_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    ui_title(fb, "Loup-garou (admin)");
    ui_list(fb, WW_UNLOCKS, unlock_sel, unlock_label);
    ui_lines(fb, UI_TITLE_H + 3 + WW_UNLOCKS * UI_ROW_H + 10, &gfx_font_small, UNLOCK_HELP[unlock_sel]);
    ui_footer(fb, "Flancs : choisir  D : valider");
}

const app_t app_werewolf_admin = {
    .name = "Loup-garou (admin)",
    .start = unlock_start,
    .buttons = unlock_buttons,
    .render = unlock_render,
};
