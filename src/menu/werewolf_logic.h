/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file werewolf_logic.h
 *
 * \brief The rules of the loup-garou (werewolf.c), without the radio nor the screen: the roles dealt, the votes,
 * the deaths (with the lovers who die of grief) and the end of the game. Tested on the PC (tests/host/test_werewolf.c).
 * The players are numbered 0..n-1 (their place in the roster of the party).
 * */

#ifndef _WEREWOLF_LOGIC_H
#define _WEREWOLF_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

#define WW_MAX 20  /* Players at most (the alive masks are 32 bits) */
#define WW_NONE 0xFF  /* No player (no vote, nobody died...) */
#define WW_MIN_SIMPLE 5
#define WW_MIN_ADVANCED 7
#define WW_CUPID_MIN 9  /* Cupidon from this number of players (advanced mode) */

typedef enum {
    WW_VILLAGER,
    WW_WOLF,
    WW_SEER,  /* Voyante: sees the role of a player each night */
    WW_WITCH,  /* Sorcière: a healing potion and a poison, once each */
    WW_HUNTER,  /* Chasseur: shoots someone when he dies */
    WW_CUPID,  /* Cupidon: links two lovers the first night */
    WW_ROLES,
} ww_role_t;

typedef enum {
    WW_WIN_NONE,  /* The game goes on */
    WW_WIN_VILLAGE,
    WW_WIN_WOLVES,
    WW_WIN_LOVERS,  /* A wolf and a villager in love, the last two alive */
    WW_WIN_DRAW,  /* Nobody alive */
} ww_win_t;

typedef struct {
    uint8_t n;
    uint8_t role[WW_MAX];
    uint32_t alive;  /* Bit i: player i is alive */
    uint8_t lovers[2];  /* WW_NONE: no couple */
    bool heal_used, poison_used;  /* The potions of the witch */
} ww_game_t;

typedef uint32_t (*ww_rand_t)(void);

int ww_min_players(bool advanced);
int ww_wolves_for(int n);

/** \brief Deals the roles of \p n players at random: the wolves (see ww_wolves_for()), the seer, and in advanced
 * mode the witch, the hunter, and Cupidon from WW_CUPID_MIN players; villagers for the others. Everybody alive. */
void ww_deal(ww_game_t *g, int n, bool advanced, ww_rand_t rnd);

static inline bool ww_alive(const ww_game_t *g, int i) {
    return i >= 0 && i < g->n && (g->alive >> i & 1);
}

int ww_count_alive(const ww_game_t *g);
/** \brief The index of the first player with role \p r (alive or not), -1 when nobody has it. */
int ww_find(const ww_game_t *g, ww_role_t r);
/** \brief Bit mask of the wolves (alive or not). */
uint32_t ww_wolves(const ww_game_t *g);

/** \brief The target with the most votes. \p votes[i]: the vote of voter i (WW_NONE: no vote), targets < \p n.
 * A tie: chosen at random among the most voted with \p rnd, nobody (WW_NONE) when \p rnd is NULL.
 * No vote at all: WW_NONE. */
int ww_tally(const uint8_t *votes, int n_votes, int n, ww_rand_t rnd);

/** \brief Kills player \p i (if alive) and the other lover, who dies of grief. The dead are appended to \p deaths
 * (\p n_deaths of them already, WW_MAX at most). \return the new number of deaths */
int ww_kill(ww_game_t *g, int i, uint8_t *deaths, int n_deaths);

/** \brief The end of a night: the victim of the wolves (WW_NONE: nobody) dies unless \p healed, the target of the
 * poison (WW_NONE: none) dies. The potions used are marked. \return the number of deaths, in \p deaths */
int ww_dawn(ww_game_t *g, int victim, bool healed, int poisoned, uint8_t *deaths);

/** \brief Cupidon links \p a and \p b (two different players). */
bool ww_link(ww_game_t *g, int a, int b);

/** \brief The lovers are a wolf and a non wolf (their only way to win: being the last two). */
bool ww_mixed_lovers(const ww_game_t *g);

ww_win_t ww_winner(const ww_game_t *g);

/** \brief Player \p i is among the winners of \p win. */
bool ww_is_winner(const ww_game_t *g, int i, ww_win_t win);

const char *ww_role_name(int role);  /* "?" when unknown */
const char *ww_win_text(ww_win_t win);

#endif /* _WEREWOLF_LOGIC_H */
