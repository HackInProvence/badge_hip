/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file assassin_logic.h
 *
 * \brief The rules of the assassin game (assassin.c) without the radio nor the screen, tested on the PC
 * (src/tests/host/test_party_games.c): the ring of the targets, the proof of a kill, the sealed hand-over of a target,
 * the shared state of the game (who is dead, who left) and the STATUS packet.
 *
 * Each player has a secret code, given by the host to him and to his hunter only. A kill proves the code
 * (assassin_hash(code, id of the killer)); a dead or leaving player hands his target (id and code) over "sealed" with
 * his own code: only his hunter can open it.
 * The players are numbered by their index in the roster of the party (at most ASSASSIN_LOGIC_MAX).
 * */

#ifndef _ASSASSIN_LOGIC_H
#define _ASSASSIN_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

#define ASSASSIN_LOGIC_MAX 64
#define ASSASSIN_NONE 0xFF
#define ASSASSIN_SEAL 8  /* Bytes of a sealed target */
#define ASSASSIN_STATUS_RECORDS 3  /* Sealed records of the players who left, relayed in each STATUS packet */

/** \brief The shared state of a game: every badge merges what the others tell (STATUS packets). */
typedef struct {
    uint8_t n;  /* Players */
    uint64_t dead;  /* Bit i: the player i is dead or left */
    uint64_t left;  /* Bit i: the player i left the game alive, his sealed target is in record[i] */
    uint8_t record[ASSASSIN_LOGIC_MAX][ASSASSIN_SEAL];
    uint8_t winner;  /* ASSASSIN_NONE while the game goes on */
} assassin_game_t;

void assassin_game_init(assassin_game_t *g, int n);

/** \brief The ring of the targets: \p next[i] is the target of the player i, one random cycle through all the
 * players; \p rnd gives the random numbers (secret: the host draws it alone). */
void assassin_ring(int n, uint8_t *next, uint32_t (*rnd)(void));

/** \brief FNV-1a of two words: the proof of a kill (the code of the target and the id of the killer), the masks. */
uint32_t assassin_hash(uint32_t a, uint32_t b);

/** \brief Seals the target \p id and its \p code with the \p key (the code of the player who hands it over). */
void assassin_seal(uint8_t *out, uint32_t id, uint32_t code, uint32_t key);
void assassin_unseal(const uint8_t *in, uint32_t key, uint32_t *id, uint32_t *code);

int assassin_survivors(const assassin_game_t *g);

/** \brief The target after the players who left (a leaver's target goes to his hunter): opens their records with
 * the code of the target, \p ids are the ids of the roster.
 * \return the index of the target (\p code updated), or ASSASSIN_NONE when the target is dead without a record */
uint8_t assassin_follow(const assassin_game_t *g, const uint32_t *ids, uint8_t target, uint32_t *code);

/** \brief Writes a STATUS packet: [n][dead 8, LE][winner][records (index, sealed 8) <= ASSASSIN_STATUS_RECORDS],
 * the records from the leaver \p first (ASSASSIN_NONE: any) and around. \return its length */
int assassin_status_pack(const assassin_game_t *g, uint8_t *d, int max, uint8_t first);

/** \brief Merges a STATUS packet into \p g (deaths are never undone; \p me is never marked dead by the others:
 * this badge knows best). \return true when something changed */
bool assassin_status_merge(assassin_game_t *g, const uint8_t *d, int len, uint8_t me);

#endif /* _ASSASSIN_LOGIC_H */
