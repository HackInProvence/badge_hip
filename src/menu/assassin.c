/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Assassin: a group game that lasts the whole conference, in the lobby of party.c.
 * At the start, the host draws a secret ring (each player hunts the next one) and a secret code for each player, and
 * sends each player ITS target only (K_TARGET, addressed to it, XORed with party_mask() of its key), again until it
 * acknowledges (K_TARGET_ACK). The players go on using their badge; the page shows the target, a hot / cold gauge
 * (RSSI of its beacons) and the survivors.
 * Kill: very close to the target, D sends K_KILL to it (again every KILL_RESEND_MS for KILL_TRY_MS) with a proof
 * (hash of the code of the target and of the killer's id). The target accepts it only with a strong RSSI (>=
 * ASSASSIN_KILL_RSSI: the badges almost touch) and a valid proof; it then sends K_DEAD to the killer with its own
 * target sealed with its code (assassin_logic.h: only the killer can open it) again until K_DEAD_ACK, and tells the
 * others with STATUS packets. A player who leaves the game alive puts his sealed target in the STATUS packets: his
 * hunter opens it and goes on. The last one standing (or the one whose target is himself) wins and tells the others.
 * Every badge sends a STATUS now and then: the dead, the records of the leavers, the winner (assassin_logic.h);
 * the badges merge them, so a lost packet is not a lost game.
 * The game is kept in static variables: it goes on while the player uses the other pages. A badge that reboots
 * leaves the game (its hunter then has no target: the host can't help, he doesn't know the ring after the start).
 *
 * Known weaknesses (it's a game at a hacker conference):
 * - party.c sends the key of a player in clear in its JOIN packet: someone who recorded the lobby can open the
 *   K_TARGET packets (target, codes) and then kill anybody whose code it learnt.
 * - A K_KILL refused (too far) shows the proof for this killer id: replayed by a radio close to the target with the
 *   same sender id, it is accepted. The proof doesn't change between attempts.
 * - The RSSI check is made by the victim on the KILL packet: a stronger transmitter (or an antenna) kills from afar.
 * - Fake STATUS packets can declare players dead (the bitmap only grows) or a winner.
 * - Sniffing the K_DEAD/STATUS traffic tells who killed whom (not the targets).
 *
 * NET_PARTY kinds: K_TARGET [target ^ m1 4][code of the target ^ m2 4][own code ^ m3 4] (m = party_mask(key, salt)),
 * K_TARGET_ACK [], K_KILL [proof 4], K_DEAD [sealed 8], K_DEAD_ACK [], K_STATUS (assassin_status_pack()). */

#include <stdio.h>
#include <string.h>

#include "pico/rand.h"

#include "app.h"
#include "achievements.h"
#include "assassin_logic.h"
#include "net.h"
#include "party.h"
#include "social.h"
#include "store.h"

/* A KILL is accepted from this RSSI (dBm, measured by the victim on the KILL packet sent at +10 dBm): about -40 to
 * -50 when the badges almost touch, -60 to -70 at 1-2 m. To calibrate with two badges: the victim prints
 * "assassin: KILL from ... rssi N" for every attempt. */
#ifndef ASSASSIN_KILL_RSSI  /* A test build can relax it (-DASSASSIN_KILL_RSSI=-90) */
#define ASSASSIN_KILL_RSSI (-50)
#endif
#define MIN_PLAYERS 3
#define MIN_PLAYERS_ADMIN 2  /* The host in admin mode (tests with two badges, tools/test_party_games.py) */
#define START_DELAY_MS 2500
#define TARGET_RESEND_MS 250  /* The host sends the targets in turn, to the players who didn't acknowledge */
#define TARGET_GIVE_UP_MS 180000
#define KILL_RESEND_MS 400
#define KILL_TRY_MS 4000
#define DEAD_RESEND_MS 500
#define DEAD_TRY_MS 20000  /* Then a dead player may leave even without the acknowledgement of the killer */
#define STATUS_PERIOD_MS 20000  /* Every badge in the game (40 badges: 2 packets/s) */
#define STATUS_JITTER_MS 5000
#define BURST_GAP_MS 400  /* STATUS repeated after a death, a leave, the end */
#define BURST_DEATH 4
#define BURST_LEAVE 8
#define BURST_END 6
#define RSSI_FRESH_MS 30000
#define MESSAGE_MS 5000  /* "Cible éliminée", "Trop loin ou absente" */
#define LEDS_MS 3000

enum { K_TARGET = PARTY_KIND_GAME, K_TARGET_ACK, K_KILL, K_DEAD, K_DEAD_ACK, K_STATUS };
enum { S_IDLE, S_WAIT, S_ALIVE, S_DEAD, S_LEFT, S_ENDED };
enum { P_HOME, P_SCAN, P_LOBBY, P_GAME };
enum { KILL_NONE, KILL_TRYING, KILL_OK, KILL_FAILED };

void assassin_service(absolute_time_t now);
bool assassin_event(char *buf, int len);

static int page = P_HOME;
static int sel = 0;
static bool changed = false;
static bool confirm_leave = false;
/* Lobby */
static party_open_t found[PARTY_MAX_OPEN];
static int n_found = 0;
static absolute_time_t list_ts = 0;
static party_player_t lobby[PARTY_MAX];
static int n_lobby = 0;
static party_state_t last_party = PARTY_IDLE;
/* The game */
static int st = S_IDLE;
static bool ready = false;  /* The roster is complete */
static bool cancelled = false;
static int n = 0;
static uint8_t me = ASSASSIN_NONE;
static uint32_t ids[PARTY_MAX];
static char names[PARTY_MAX][9];
static assassin_game_t g;
static bool have_target = false;  /* K_TARGET received (or drawn by this badge, the host) */
static uint32_t target_id = 0, tcode = 0, mycode = 0;
static uint8_t target = ASSASSIN_NONE;
static int kills = 0;
static bool games_counted = false;
/* The host */
static bool hosting = false;
static uint32_t keys[PARTY_MAX];
static uint8_t ring[PARTY_MAX];
static uint32_t codes[PARTY_MAX];
static uint64_t pending = 0;  /* Players who didn't acknowledge their target yet */
static int next_send = 0;
static absolute_time_t target_ts = 0, host_since = 0;
/* A kill attempt */
static int kill_state = KILL_NONE;
static absolute_time_t kill_until = 0, kill_ts = 0, message_until = 0;
static uint8_t last_victim = ASSASSIN_NONE, last_victim_next = ASSASSIN_NONE;
static uint32_t last_victim_code = 0;
/* Dead */
static uint8_t killer = ASSASSIN_NONE;
static uint8_t dead_next = ASSASSIN_NONE;
static uint32_t dead_next_code = 0;
static bool dead_acked = false;
static absolute_time_t dead_until = 0, dead_ts = 0;
/* Radio */
static absolute_time_t status_ts = 0, leds_off_ts = 0;
static int burst = 0;
static bool leaving = false;  /* Left alive: the badge leaves the party after the burst */
static int16_t party_rssi = 0;
static absolute_time_t party_rssi_ts = 0;
static int16_t hunt_rssi = 0;
static bool hunt_heard = false;
static absolute_time_t hunt_ts = 0;
/* For main.c */
static char event_msg[48];
static bool event_pending = false;

static uint8_t index_of(uint32_t id) {
    for (int i = 0; i < n; ++i)
        if (ids[i] == id)
            return i;
    return ASSASSIN_NONE;
}

static const char *name_of(uint8_t i) {
    return i < n ? names[i] : "?";
}

static void event(const char *msg) {
    snprintf(event_msg, sizeof(event_msg), "%s", msg);
    event_pending = true;
}

/* A notification for main.c (like duel_invited()): returns it once */
bool assassin_event(char *buf, int len) {
    if (! event_pending)
        return false;
    event_pending = false;
    snprintf(buf, len, "%s", event_msg);
    return true;
}

static void leds(uint8_t r, uint8_t gr, uint8_t b, absolute_time_t now) {
    app_leds(r, gr, b);
    leds_off_ts = delayed_by_ms(now, LEDS_MS);
}

static void count_game(void) {
    if (! games_counted) {
        games_counted = true;
        achv_add(ACHV_CNT_GAMES, 1);
    }
}

static void start_burst(int count, absolute_time_t now) {
    if (burst < count)
        burst = count;
    status_ts = now;
}

static void send_status(void) {
    uint8_t d[PARTY_PAYLOAD];
    int len = assassin_status_pack(&g, d, sizeof(d), st == S_LEFT ? me : ASSASSIN_NONE);
    party_send(K_STATUS, 0, d, len);
}

static void win(absolute_time_t now) {
    st = S_ENDED;
    g.winner = me;
    kill_state = KILL_NONE;
    achv_unlock(ACHV_ASSASSIN_WIN);
    achv_add(ACHV_CNT_WINS, 1);
    count_game();
    start_burst(BURST_END, now);
    printf("assassin: victory, %d kill(s)\n", kills);
    event("Assassin : victoire !");
    app_tone(1568, 400);
    leds(0, 255, 0, now);
    changed = true;
}

static void check_end(absolute_time_t now) {
    if (st == S_ALIVE && (target == me || assassin_survivors(&g) <= 1)) {
        win(now);
    } else if (g.winner != ASSASSIN_NONE && g.winner != me && st != S_ENDED && st != S_IDLE) {
        st = S_ENDED;
        kill_state = KILL_NONE;
        count_game();
        printf("assassin: game over, winner %s\n", name_of(g.winner));
        changed = true;
    }
}

/* The target died or left: follow the records of the leavers */
static void follow_target(absolute_time_t now) {
    if (st != S_ALIVE || target == ASSASSIN_NONE || ! (g.dead >> target & 1))
        return;
    uint32_t code = tcode;
    uint8_t t = assassin_follow(&g, ids, target, &code);
    if (t != ASSASSIN_NONE) {
        printf("assassin: %s left the game, new target %s\n", name_of(target), name_of(t));
        target = t;
        tcode = code;
        kill_state = KILL_NONE;
        event("Assassin : nouvelle cible");
        changed = true;
        check_end(now);
    }
}

static void die(uint8_t k, int rssi, absolute_time_t now) {
    st = S_DEAD;
    killer = k;
    g.dead |= 1ull << me;
    dead_next = target;
    dead_next_code = tcode;
    dead_acked = false;
    dead_until = delayed_by_ms(now, DEAD_TRY_MS);
    dead_ts = now;
    kill_state = KILL_NONE;
    start_burst(BURST_DEATH, now);
    printf("assassin: killed by %s (rssi %d), %d survivor(s)\n", name_of(k), rssi, assassin_survivors(&g));
    char msg[48];
    snprintf(msg, sizeof(msg), "Éliminé par %s", name_of(k));
    event(msg);
    app_tone(220, 600);
    leds(255, 0, 0, now);
    changed = true;
}

static void handler(uint8_t kind, uint32_t from, uint32_t to, const uint8_t *data, uint8_t len, int rssi) {
    if (st == S_IDLE)
        return;
    absolute_time_t now = get_absolute_time();
    uint8_t k = index_of(from);
    if (k != ASSASSIN_NONE && k == target) {
        party_rssi = rssi;  /* The hot / cold gauge also uses the packets of the target */
        party_rssi_ts = now;
    }
    switch (kind) {
    case K_TARGET:
        if (to != net_id() || from != party_host_id() || len < 12)
            return;
        if (! have_target) {
            uint32_t key = party_key();
            target_id = net_u32(data) ^ party_mask(key, 1);
            tcode = net_u32(data + 4) ^ party_mask(key, 2);
            mycode = net_u32(data + 8) ^ party_mask(key, 3);
            have_target = true;
            printf("assassin: target received\n");
            changed = true;
        }
        party_send(K_TARGET_ACK, from, NULL, 0);  /* Again for each copy: the acknowledgement may be lost */
        return;
    case K_TARGET_ACK:
        if (hosting && to == net_id() && k != ASSASSIN_NONE && (pending >> k & 1)) {
            pending &= ~(1ull << k);
            printf("assassin: %s has its target%s\n", name_of(k), pending ? "" : ", all the targets acknowledged");
        }
        return;
    case K_KILL: {
        if (to != net_id() || len < 4 || k == ASSASSIN_NONE || ! have_target || me == ASSASSIN_NONE)
            return;
        printf("assassin: KILL from %s, rssi %d\n", name_of(k), rssi);
        if (net_u32(data) != assassin_hash(mycode, from)) {
            printf("assassin: KILL from %s refused: wrong proof\n", name_of(k));
            return;
        }
        if (st == S_DEAD && killer == k) {
            dead_acked = false;  /* It didn't get our K_DEAD: again */
            dead_until = delayed_by_ms(now, DEAD_TRY_MS);
            dead_ts = now;
            return;
        }
        if (st != S_ALIVE)
            return;
        if (rssi < ASSASSIN_KILL_RSSI) {
            printf("assassin: KILL from %s refused: rssi %d < %d\n", name_of(k), rssi, ASSASSIN_KILL_RSSI);
            return;
        }
        if (kill_state == KILL_TRYING && target == k && net_id() > from) {
            /* The last two attack each other at the same time: the highest id wins (the other one accepts) */
            printf("assassin: KILL from %s refused: we attack each other, this badge wins\n", name_of(k));
            return;
        }
        die(k, rssi, now);
        return;
    }
    case K_DEAD: {
        if (to != net_id() || len < ASSASSIN_SEAL || k == ASSASSIN_NONE || (k != target && k != last_victim))
            return;
        uint32_t nid, ncode;
        assassin_unseal(data, k == target ? tcode : last_victim_code, &nid, &ncode);
        uint8_t next = index_of(nid);
        if (next == ASSASSIN_NONE) {
            printf("assassin: K_DEAD from %s: wrong seal\n", name_of(k));
            return;
        }
        party_send(K_DEAD_ACK, from, NULL, 0);
        bool moved = false;
        if (k == target) {
            /* The kill is done */
            ++kills;
            achv_unlock(ACHV_ASSASSIN_KILL);
            achv_add(ACHV_CNT_KILLS, 1);
            g.dead |= 1ull << k;
            last_victim = k;
            last_victim_code = tcode;
            last_victim_next = next;
            printf("assassin: killed %s, new target %s, %d survivor(s)\n", name_of(k), name_of(next),
                   assassin_survivors(&g));
            kill_state = KILL_OK;
            message_until = delayed_by_ms(now, MESSAGE_MS);
            app_tone(1319, 300);
            leds(0, 255, 0, now);
            moved = true;
            start_burst(BURST_DEATH, now);
        } else if (next != last_victim_next && target == last_victim_next) {
            /* Our victim had killed its own target meanwhile: its new target is ours */
            printf("assassin: %s passes a newer target: %s\n", name_of(k), name_of(next));
            last_victim_next = next;
            moved = true;
        }
        if (moved) {
            target = next;
            tcode = ncode;
            if (st == S_DEAD) {
                /* Killed meanwhile: our killer gets this target instead */
                dead_next = next;
                dead_next_code = ncode;
                dead_acked = false;
                dead_until = delayed_by_ms(now, DEAD_TRY_MS);
                dead_ts = now;
            } else {
                event("Assassin : nouvelle cible");
            }
            changed = true;
            check_end(now);
        }
        return;
    }
    case K_DEAD_ACK:
        if (to == net_id() && st == S_DEAD && k == killer && ! dead_acked) {
            dead_acked = true;
            printf("assassin: %s has our target\n", name_of(k));
            changed = true;
        }
        return;
    case K_STATUS: {
        if (! ready)
            return;
        uint64_t before = g.dead;
        if (assassin_status_merge(&g, data, len, me)) {
            for (int i = 0; i < n; ++i)
                if ((g.dead & ~before) >> i & 1)
                    printf("assassin: %s is out, %d survivor(s)\n", names[i], assassin_survivors(&g));
            changed = true;
            follow_target(now);
            check_end(now);
        }
        return;
    }
    default:
        return;  /* K_LEAVE of party.c: a dead player left, nothing to do */
    }
}

static uint32_t random32(void) {
    return get_rand_32();
}

/* The roster is complete: the players, and for the host the ring */
static void take_roster(absolute_time_t now) {
    int count = party_players(lobby, PARTY_MAX);
    for (int i = 0; i < count; ++i)
        if (! lobby[i].id)
            return;  /* A page is missing */
    int self = party_index(net_id());
    if (self < 0 || count < 2)
        return;
    n = count;
    me = self;
    for (int i = 0; i < n; ++i) {
        ids[i] = lobby[i].id;
        keys[i] = lobby[i].key;
        snprintf(names[i], sizeof(names[i]), "%s", lobby[i].name);
    }
    assassin_game_init(&g, n);
    ready = true;
    printf("assassin: %d players\n", n);
    if (hosting) {
        /* The secret ring: drawn with the random generator of this badge, not with the shared seed */
        assassin_ring(n, ring, random32);
        for (int i = 0; i < n; ++i)
            codes[i] = get_rand_32();
        target_id = ids[ring[me]];
        tcode = codes[ring[me]];
        mycode = codes[me];
        have_target = true;
        pending = (n < 64 ? (1ull << n) - 1 : ~0ull) & ~(1ull << me);
        host_since = now;
        target_ts = now;
        printf("assassin: the host sends the targets\n");
    }
}

static void reset_game(void) {
    st = S_IDLE;
    ready = cancelled = have_target = hosting = leaving = games_counted = confirm_leave = false;
    n = kills = burst = 0;
    me = target = killer = dead_next = last_victim = last_victim_next = ASSASSIN_NONE;
    kill_state = KILL_NONE;
    pending = 0;
    hunt_heard = false;
    party_rssi_ts = 0;
}

static void begin(absolute_time_t now) {
    reset_game();
    st = S_WAIT;
    hosting = party_is_host();
    party_set_handler(handler);
    status_ts = delayed_by_ms(now, STATUS_PERIOD_MS / 2 + get_rand_32() % STATUS_JITTER_MS);
    page = P_GAME;
    changed = true;
    printf("assassin: game started%s\n", hosting ? " (host)" : "");
}

/* Runs the game, called by the page and (when main.c calls it) in the main loop: the game goes on while the player
 * looks at another page */
void assassin_service(absolute_time_t now) {
    if (leds_off_ts && absolute_time_diff_us(leds_off_ts, now) >= 0) {
        leds_off_ts = 0;
        app_leds(0, 0, 0);
    }
    if (st == S_IDLE) {
        if (party_state() == PARTY_STARTED && party_game() == PARTY_GAME_ASSASSIN)
            begin(now);
        else
            return;
    }
    if (st == S_ENDED && ! burst && party_state() == PARTY_STARTED && party_game() == PARTY_GAME_ASSASSIN) {
        /* The end was sent to everybody: out of the party, so that another game can start (the result stays shown) */
        party_leave();
        printf("assassin: party left after the end\n");
    }
    if (party_state() != PARTY_STARTED && st != S_ENDED && ! leaving) {
        /* The party ended under the game (party.c: a LEAVE of the host before the start, a new party) */
        st = S_ENDED;
        cancelled = true;
        count_game();
        printf("assassin: party cancelled\n");
        changed = true;
        return;
    }
    if (! ready) {
        take_roster(now);
        if (! ready)
            return;
    }
    if (st == S_WAIT && have_target) {
        target = index_of(target_id);
        if (target != ASSASSIN_NONE && target != me) {
            st = S_ALIVE;
            printf("assassin: my target is %s\n", name_of(target));
            changed = true;
        } else {
            printf("assassin: bad target, waiting for another one\n");
            have_target = false;
        }
    }

    /* The host: the targets, in turn, until acknowledged */
    if (hosting && pending && absolute_time_diff_us(target_ts, now) >= 0) {
        if (absolute_time_diff_us(host_since, now) > TARGET_GIVE_UP_MS * 1000ll) {
            printf("assassin: some players never got their target\n");
            pending = 0;
        } else {
            for (int k = 0; k < n; ++k) {
                int i = (next_send + k) % n;
                if (pending >> i & 1) {
                    uint8_t d[12];
                    net_put_u32(d, ids[ring[i]] ^ party_mask(keys[i], 1));
                    net_put_u32(d + 4, codes[ring[i]] ^ party_mask(keys[i], 2));
                    net_put_u32(d + 8, codes[i] ^ party_mask(keys[i], 3));
                    if (party_send(K_TARGET, ids[i], d, sizeof(d)))
                        next_send = i + 1;
                    break;
                }
            }
            target_ts = delayed_by_ms(now, TARGET_RESEND_MS);
        }
    }

    /* A kill attempt: K_KILL again and again, until the K_DEAD of the target or the time out */
    if (kill_state == KILL_TRYING) {
        if (st != S_ALIVE) {
            kill_state = KILL_NONE;
        } else if (absolute_time_diff_us(kill_until, now) >= 0) {
            kill_state = KILL_FAILED;
            message_until = delayed_by_ms(now, MESSAGE_MS);
            printf("assassin: no answer from %s: too far or absent\n", name_of(target));
            app_tone(330, 200);
            changed = true;
        } else if (absolute_time_diff_us(kill_ts, now) >= 0) {
            uint8_t d[4];
            net_put_u32(d, assassin_hash(tcode, net_id()));
            if (party_send(K_KILL, ids[target], d, sizeof(d)))
                kill_ts = delayed_by_ms(now, KILL_RESEND_MS);
        }
    }
    if ((kill_state == KILL_OK || kill_state == KILL_FAILED) && absolute_time_diff_us(message_until, now) >= 0) {
        kill_state = KILL_NONE;
        changed = true;
    }

    /* Dead: our target to our killer, until acknowledged */
    if (st == S_DEAD && ! dead_acked && killer != ASSASSIN_NONE && absolute_time_diff_us(dead_ts, now) >= 0
            && absolute_time_diff_us(now, dead_until) > 0) {
        uint8_t d[ASSASSIN_SEAL];
        assassin_seal(d, dead_next < n ? ids[dead_next] : 0, dead_next_code, mycode);
        if (party_send(K_DEAD, ids[killer], d, sizeof(d)))
            dead_ts = delayed_by_ms(now, DEAD_RESEND_MS);
    }

    /* STATUS: in bursts after an event, and now and then */
    if (absolute_time_diff_us(status_ts, now) >= 0) {
        bool periodic = st == S_ALIVE || st == S_DEAD || (st == S_LEFT && hosting);
        if (burst || periodic) {
            send_status();
            if (burst)
                --burst;
        }
        status_ts = delayed_by_ms(now, burst ? BURST_GAP_MS : STATUS_PERIOD_MS - STATUS_JITTER_MS
                                  + get_rand_32() % (2 * STATUS_JITTER_MS));
        if (! burst && leaving) {
            /* Left alive: our record is out, leave the party */
            leaving = false;
            party_leave();
            reset_game();
            printf("assassin: left the party\n");
            changed = true;
        }
    }

    /* The hot / cold gauge: the beacons of the target (social.c), or its packets of the game */
    if (st == S_ALIVE && target != ASSASSIN_NONE && absolute_time_diff_us(hunt_ts, now) > 1000000) {
        hunt_ts = now;
        social_neighbour_t near[SOCIAL_MAX_NEIGHBOURS];
        int count = social_neighbours(near, SOCIAL_MAX_NEIGHBOURS);
        bool heard = false;
        int16_t rssi = 0;
        for (int i = 0; i < count; ++i)
            if (near[i].id == ids[target]) {
                heard = true;
                rssi = near[i].rssi;
            }
        if (! heard && party_rssi_ts && absolute_time_diff_us(party_rssi_ts, now) < RSSI_FRESH_MS * 1000ll) {
            heard = true;
            rssi = party_rssi;
        }
        if (heard != hunt_heard || rssi / 5 != hunt_rssi / 5)
            changed = true;
        hunt_heard = heard;
        hunt_rssi = rssi;
    }
}


/* ------ The page ------ */

static int min_players(void) {
    return store_get()->admin == STORE_ADMIN_ON ? MIN_PLAYERS_ADMIN : MIN_PLAYERS;
}

static void found_label(int i, char *buf, size_t len) {
    snprintf(buf, len, "%s  %u joueur%s", found[i].name, found[i].players, found[i].players > 1 ? "s" : "");
}

static void home_label(int i, char *buf, size_t len) {
    snprintf(buf, len, "%s", i ? "Rejoindre une partie" : "Créer une partie");
}

/* Another group game runs on this badge (party.c has a single party at a time) */
static const char *busy_game(void) {
    if (party_state() != PARTY_STARTED || party_game() == PARTY_GAME_ASSASSIN)
        return NULL;
    return party_game() == PARTY_GAME_TUG ? "Tir à la corde" : party_game() == PARTY_GAME_WEREWOLF ? "Loup-garou" : "?";
}

static void assassin_start(absolute_time_t now) {
    assassin_service(now);
    confirm_leave = false;
    if (st != S_IDLE) {
        page = P_GAME;
    } else if (party_game() == PARTY_GAME_ASSASSIN && (party_state() == PARTY_HOSTING
               || party_state() == PARTY_JOINING || party_state() == PARTY_JOINED)) {
        page = P_LOBBY;  /* The lobby goes on */
        party_set_handler(handler);
    } else {
        page = P_HOME;
    }
    list_ts = 0;
    changed = true;
}

static void back_home(void) {
    if (party_game() == PARTY_GAME_ASSASSIN && (party_state() != PARTY_STARTED || st == S_ENDED || st == S_DEAD))
        party_leave();
    reset_game();
    page = P_HOME;
    sel = 0;
    changed = true;
}

static bool dead_may_leave(absolute_time_t now) {
    return st == S_DEAD && ! hosting && (dead_acked || absolute_time_diff_us(dead_until, now) >= 0);
}

static bool assassin_buttons(const app_buttons_t *b, absolute_time_t now) {
    if (b->long_pressed & UI_BTN_A)
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
                party_host(PARTY_GAME_ASSASSIN, true, PARTY_MAX, 0);
                printf("assassin: hosting\n");
                page = P_LOBBY;
            } else {
                party_scan(PARTY_GAME_ASSASSIN);
                printf("assassin: scanning\n");
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
            printf("assassin: joining %s\n", found[sel].name);
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
            printf("assassin: left the lobby\n");
            back_home();
            break;
        }
        if ((b->pressed & UI_BTN_B) && party_state() == PARTY_HOSTING && party_count() >= min_players()) {
            printf("assassin: start, %d players\n", party_count());
            party_start(START_DELAY_MS);
            assassin_service(now);
        }
        break;
    case P_GAME:
        if (confirm_leave) {
            if (b->pressed & UI_BTN_B) {
                /* Leave alive: our target goes, sealed, to our hunter */
                confirm_leave = false;
                g.dead |= 1ull << me;
                g.left |= 1ull << me;
                assassin_seal(g.record[me], ids[target], tcode, mycode);
                st = S_LEFT;
                count_game();
                start_burst(BURST_LEAVE, now);
                leaving = ! hosting;  /* The host stays in the party: its LEAVE would cancel it for everybody */
                printf("assassin: left the game alive\n");
            } else if (b->released_short & UI_BTN_A) {
                confirm_leave = false;
            }
            break;
        }
        if (st == S_ALIVE) {
            if ((b->pressed & UI_BTN_B) && kill_state != KILL_TRYING && target < n) {
                kill_state = KILL_TRYING;
                kill_until = delayed_by_ms(now, KILL_TRY_MS);
                kill_ts = now;
                printf("assassin: kill attempt on %s\n", name_of(target));
                assassin_service(now);
            } else if ((b->pressed & (UI_BTN_X | UI_BTN_Y)) && target < n) {
                confirm_leave = true;
            } else if (b->released_short & UI_BTN_A) {
                return false;
            }
        } else if (st == S_DEAD) {
            if ((b->pressed & UI_BTN_B) && dead_may_leave(now)) {
                count_game();
                printf("assassin: left the game (dead)\n");
                back_home();
            } else if (b->released_short & UI_BTN_A) {
                return false;
            }
        } else if (st == S_ENDED) {
            if (b->pressed & UI_BTN_B) {
                back_home();
            } else if (b->released_short & UI_BTN_A) {
                return false;
            }
        } else if (b->released_short & UI_BTN_A) {
            return false;
        }
        break;
    }
    changed = true;
    return true;
}

static bool assassin_task(absolute_time_t now) {
    assassin_service(now);
    if (page == P_GAME && st == S_IDLE)
        page = P_HOME;  /* Left the game */
    if (page == P_SCAN && absolute_time_diff_us(list_ts, now) > 1000000) {
        list_ts = now;
        n_found = party_found(found, PARTY_MAX_OPEN);
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
    if (page == P_GAME && st == S_DEAD && ! dead_acked) {
        static bool could_leave = false;
        if (dead_may_leave(now) != could_leave) {
            could_leave = ! could_leave;
            changed = true;
        }
    }
    bool c = changed;
    changed = false;
    return c;
}

#define NAME_ROW_H 19

/* Names in 2 columns from \p y; "+N" when they don't fit */
static void draw_names(uint8_t *fb, int y, int count) {
    int rows = (UI_FOOTER_Y - 2 - y) / NAME_ROW_H;
    char text[24], fitted[24];
    for (int i = 0; i < count && i < 2 * rows; ++i) {
        if (i == 2 * rows - 1 && count > 2 * rows)
            snprintf(text, sizeof(text), "+%d autres", count - i);
        else
            snprintf(text, sizeof(text), "%s", lobby[i].id ? lobby[i].name : "...");
        ui_fit_preview(&gfx_font_small, fitted, sizeof(fitted), text, GFX_WIDTH/2 - 10);
        gfx_text(fb, i % 2 ? GFX_WIDTH/2 + 4 : 6, y + i / 2 * NAME_ROW_H, &gfx_font_small, fitted, GFX_BLACK,
                 GFX_ALIGN_LEFT);
    }
}

static void render_lobby(uint8_t *fb) {
    char text[48];
    switch (party_state()) {
    case PARTY_HOSTING:
        ui_title(fb, "Ta partie");
        if (n_lobby >= min_players())
            snprintf(text, sizeof(text), "%d joueurs", n_lobby);
        else
            snprintf(text, sizeof(text), "Il faut %d joueurs au moins.", min_players());
        ui_lines(fb, UI_TITLE_H + 4, &gfx_font_small, text);
        draw_names(fb, UI_TITLE_H + 24, n_lobby);
        ui_footer(fb, n_lobby >= min_players() ? "G : annuler  D : lancer" : "G : annuler");
        break;
    case PARTY_JOINING:
        ui_title(fb, "Rejoindre");
        ui_lines(fb, 70, &gfx_font_small, "Connexion...");
        ui_footer(fb, "G : annuler");
        break;
    case PARTY_JOINED:
        ui_title(fb, "Salle d'attente");
        snprintf(text, sizeof(text), "Partie de %s", party_name(party_host_id()));
        ui_lines(fb, UI_TITLE_H + 2, &gfx_font_small, text);
        ui_lines(fb, UI_TITLE_H + 21, &gfx_font_small, "En attente du lancement...");
        draw_names(fb, UI_TITLE_H + 44, n_lobby);
        ui_footer(fb, "G : quitter");
        break;
    default:
        ui_title(fb, "Assassin");
        ui_lines(fb, 60, &gfx_font_small, "Partie annulée par l'hôte\n(ou lancée sans toi).");
        ui_footer(fb, "D : OK");
        break;
    }
}

static const char *heat(int rssi) {
    return rssi >= ASSASSIN_KILL_RSSI ? "Brûlant !" : rssi >= -60 ? "Chaud" : rssi >= -70 ? "Tiède" :
           rssi >= SOCIAL_RSSI_CLOSE ? "Froid" : "Glacial";
}

static void render_game(uint8_t *fb, absolute_time_t now) {
    char text[80];
    int survivors = ready ? assassin_survivors(&g) : 0;
    if (confirm_leave) {
        ui_title(fb, "Abandonner ?");
        ui_wrapped(fb, 50, &gfx_font_small, "Tu quittes la partie : ta cible ira à ton chasseur. Pas de retour "
                   "possible.", 5);
        ui_footer(fb, "G : non  D : abandonner");
        return;
    }
    switch (st) {
    case S_WAIT:
        ui_title(fb, "Assassin");
        ui_lines(fb, 60, &gfx_font_small, ready ? "Distribution des cibles..." : "Réception de la liste\n"
                 "des joueurs...");
        ui_footer(fb, "G : retour");
        break;
    case S_ALIVE: {
        ui_title(fb, "Assassin");
        ui_lines(fb, UI_TITLE_H + 3, &gfx_font_small, "Ta cible :");
        const gfx_font_t *font = gfx_text_width(&gfx_font_large, name_of(target)) <= GFX_WIDTH - 6 ? &gfx_font_large
                                 : &gfx_font_medium;  /* A wide name: smaller, not cut */
        ui_lines(fb, UI_TITLE_H + 19 + (font == &gfx_font_large ? 0 : 4), font, name_of(target));
        if (hunt_heard) {
            ui_gauge(fb, 20, 88, GFX_WIDTH - 40, 12, hunt_rssi + 100, 100 + ASSASSIN_KILL_RSSI);
            ui_lines(fb, 103, &gfx_font_small, heat(hunt_rssi));
        } else {
            ui_gauge(fb, 20, 88, GFX_WIDTH - 40, 12, 0, 1);
            ui_lines(fb, 103, &gfx_font_small, "Pas captée");
        }
        snprintf(text, sizeof(text), "Survivants : %d / %d", survivors, n);
        ui_lines(fb, 122, &gfx_font_small, text);
        if (kill_state == KILL_TRYING)
            snprintf(text, sizeof(text), "Tentative...");
        else if (kill_state == KILL_FAILED)
            snprintf(text, sizeof(text), "Trop loin ou absente");
        else if (kill_state == KILL_OK)
            snprintf(text, sizeof(text), "Cible éliminée !");
        else
            snprintf(text, sizeof(text), kills ? "Victimes : %d" : "Flanc : abandonner", kills);
        ui_lines(fb, 146, &gfx_font_small, text);
        ui_footer(fb, "G : retour  D : éliminer");
        break;
    }
    case S_DEAD: {
        ui_title(fb, "Éliminé");
        snprintf(text, sizeof(text), "Éliminé par\n%s", name_of(killer));
        int y = ui_lines(fb, UI_TITLE_H + 6, &gfx_font_medium, text);
        snprintf(text, sizeof(text), "Survivants : %d / %d", survivors, n);
        y = ui_lines(fb, y, &gfx_font_small, text);
        snprintf(text, sizeof(text), "Tes victimes : %d", kills);
        y = ui_lines(fb, y, &gfx_font_small, text);
        if (! dead_acked && ! dead_may_leave(now))
            ui_lines(fb, y, &gfx_font_small, "Ta cible passe à ton tueur...");
        else if (hosting)
            ui_wrapped(fb, y, &gfx_font_small, "Ton badge héberge la partie : garde-le allumé.", 2);
        ui_footer(fb, dead_may_leave(now) ? "G : retour  D : quitter" : "G : retour");
        break;
    }
    case S_LEFT:
        ui_title(fb, "Assassin");
        snprintf(text, sizeof(text), "Tu as abandonné.\nSurvivants : %d / %d", survivors, n);
        ui_lines(fb, 60, &gfx_font_small, text);
        ui_footer(fb, "G : retour");
        break;
    default:
        ui_title(fb, "Fin de la partie");
        if (cancelled) {
            ui_lines(fb, 60, &gfx_font_small, "L'hôte a quitté la partie :\nelle est terminée.");
        } else if (g.winner == me) {
            gfx_text(fb, GFX_WIDTH/2, 55, &gfx_font_large, "Victoire !", GFX_BLACK, GFX_ALIGN_CENTER);
            snprintf(text, sizeof(text), "Dernière cigale debout\nVictimes : %d", kills);
            ui_lines(fb, 100, &gfx_font_small, text);
        } else {
            snprintf(text, sizeof(text), "Gagnant :\n%s", name_of(g.winner));
            int y = ui_lines(fb, 50, &gfx_font_medium, text);
            snprintf(text, sizeof(text), "Tes victimes : %d", kills);
            ui_lines(fb, y + 8, &gfx_font_small, text);
        }
        ui_footer(fb, "G : retour  D : nouvelle partie");
        break;
    }
}

static void assassin_render(uint8_t *fb, absolute_time_t now) {
    switch (page) {
    case P_HOME: {
        const char *busy = busy_game();
        ui_title(fb, "Assassin");
        if (busy) {
            char text[80];
            snprintf(text, sizeof(text), "Une partie de %s est en cours sur ton badge : termine-la d'abord.", busy);
            ui_wrapped(fb, 60, &gfx_font_small, text, 5);
            ui_footer(fb, "G : retour");
        } else {
            ui_list(fb, 2, sel, home_label);
            char text[140];
            snprintf(text, sizeof(text), "Chacun a une cible secrète : approche-toi tout près pour "
                     "l'éliminer. %d joueurs min.", min_players());
            ui_wrapped(fb, UI_TITLE_H + 2 * UI_ROW_H + 10, &gfx_font_small, text, 4);
            ui_footer(fb, "G : retour  D : choisir");
        }
        break;
    }
    case P_SCAN:
        ui_title(fb, "Rejoindre");
        if (n_found) {
            ui_list(fb, n_found, sel, found_label);
            ui_footer(fb, "G : retour  D : rejoindre");
        } else {
            ui_lines(fb, 60, &gfx_font_small, "Recherche des parties\nd'assassin...");
            ui_footer(fb, "G : retour");
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

static void assassin_stop(void) {
    confirm_leave = false;
}

const app_t app_assassin = {
    .name = "Assassin",
    .start = assassin_start,
    .buttons = assassin_buttons,
    .task = assassin_task,
    .render = assassin_render,
    .stop = assassin_stop,
};
