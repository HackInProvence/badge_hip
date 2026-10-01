/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file party.h
 *
 * \brief The lobby of the group games (loup-garou, assassin, tir à la corde): a host opens a party, the cicadas
 * around join it, the host sees the list of the players and starts the game; then the game exchanges its own
 * messages through party_send().
 *
 * NET_PARTY [game][session 2][kind][to 4][payload <= 47]; to = 0: everybody. All at +10 dBm (NET_LOUD).
 * - OPEN (host, every second while the lobby is open): [players][max][flags][name of the host 8]
 * - JOIN (player -> host, every second until it is in the roster): [key 4][name 8]
 * - ROSTER (host, 4 pages per second while the lobby is open and 10 s after the start, then 1 per second):
 *   [page][pages][players][(id 4, name 8) x <= 3]
 * - START (host, 5 times, then every second during the game for the players who missed it):
 *   [players][delay 2, ms: the game starts this long after the packet][seed 4]
 * - LEAVE (anybody): the sender leaves the party (the host: the party is cancelled)
 * - kinds >= PARTY_KIND_GAME: messages of the game, given to the handler of party_set_handler()
 *
 * The roster has at most PARTY_MAX players, the host first (when it plays). Each player sends a random key when it
 * joins: the game can use it to hide a secret from the other players (e.g. the role of the loup-garou, XORed with
 * a hash of the key; known by the host and the player only).
 * */

#ifndef _PARTY_H
#define _PARTY_H

#include <stdbool.h>
#include <stdint.h>

#include "pico/time.h"

#define PARTY_MAX 40
#define PARTY_MAX_OPEN 8  /* Parties heard at once */
#define PARTY_KIND_GAME 16  /* First kind of the messages of the games */
#define PARTY_KIND_LEAVE 5  /* Given to the handler after the start: a player (or the host) left */
#define PARTY_PAYLOAD 47

typedef enum {
    PARTY_GAME_TUG = 1,  /* Tir à la corde (tug.c) */
    PARTY_GAME_ASSASSIN,  /* Assassin (assassin.c) */
    PARTY_GAME_WEREWOLF,  /* Loup-garou (werewolf.c) */
} party_game_t;

typedef enum {
    PARTY_IDLE,
    PARTY_HOSTING,  /* Lobby open, this badge is the host */
    PARTY_SCANNING,  /* Looking for the open parties of a game */
    PARTY_JOINING,  /* JOIN sent, not in the roster yet */
    PARTY_JOINED,  /* In the roster, waiting for the start */
    PARTY_STARTED,  /* The game runs (the roster is final) */
    PARTY_CANCELLED,  /* The host left or cancelled */
} party_state_t;

typedef struct {
    uint32_t id;
    char name[9];
    uint32_t key;  /* Random key of the player (known by the host; 0 for the others) */
} party_player_t;

typedef struct {
    uint32_t host;
    char name[9];  /* Of the host */
    uint16_t session;
    uint8_t players;
    uint8_t max;
    uint8_t flags;
    int16_t rssi;
} party_open_t;

/** \brief A message of a game (kind >= PARTY_KIND_GAME) received: \p to is this badge or 0 (everybody). */
typedef void (*party_handler_t)(uint8_t kind, uint32_t from, uint32_t to, const uint8_t *data, uint8_t len, int rssi);

void party_init(void);
void party_task(absolute_time_t now);

/** \brief Opens a party of \p game; \p plays: the host is a player too (not for a narrator); \p max players;
 * \p flags: free for the game (e.g. the options), given to the players with OPEN. */
void party_host(uint8_t game, bool plays, uint8_t max, uint8_t flags);

/** \brief The host changes the flags of the party (they follow in OPEN). */
void party_set_flags(uint8_t flags);

/** \brief Looks for the open parties of \p game. */
void party_scan(uint8_t game);

/** \brief The parties heard (closest first). \return their number */
int party_found(party_open_t *out, int max);

/** \brief Joins the party of \p host (after party_scan()). */
void party_join(uint32_t host);

/** \brief The host removes a player from the lobby. */
void party_kick(uint32_t id);

/** \brief The host closes the lobby and starts the game in \p delay_ms on all the badges. */
void party_start(uint16_t delay_ms);

/** \brief Leaves the party (the host: cancels it). */
void party_leave(void);

party_state_t party_state(void);
uint8_t party_game(void);
uint8_t party_flags(void);
bool party_is_host(void);
uint32_t party_host_id(void);

/** \brief The players (the roster). \return their number */
int party_players(party_player_t *out, int max);
int party_count(void);
/** \brief Index of \p id in the roster, -1 when not there. */
int party_index(uint32_t id);
const char *party_name(uint32_t id);  /* "?" when unknown */
uint32_t party_key(void);  /* The key of this badge */

/** \brief The time of the start (the same on all the badges, within ~20 ms), and the random seed chosen by the host
 * (the same on all the badges). Valid once PARTY_STARTED. The seed travels in clear: never use it for a secret. */
absolute_time_t party_start_time(void);
uint32_t party_seed(void);

void party_set_handler(party_handler_t handler);

/** \brief Sends a message of the game (kind >= PARTY_KIND_GAME) to \p to (0: everybody). \return false when the
 * radio queue is full (send it again later) */
bool party_send(uint8_t kind, uint32_t to, const void *data, uint8_t len);

/** \brief Hash of a key, to XOR a secret with (32 bits). */
uint32_t party_mask(uint32_t key, uint8_t salt);

#endif /* _PARTY_H */
