/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file werewolf_logic.h
 *
 * \brief The rules of the loup-garou (werewolf.c), after the rules of "Les Loups-garous de Thiercelieux", without
 * the radio nor the screen: the cards dealt (with the 2 cards of the thief), the votes (the captain counts twice),
 * the deaths (the lovers die together), the end of the game. Tested on the PC (tests/host/test_werewolf.c).
 * The players are numbered 0..n-1 (their place in the roster of the party).
 * */

#ifndef _WEREWOLF_LOGIC_H
#define _WEREWOLF_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

#define WW_MAX 20  /* Size of the arrays (the alive masks are 32 bits) */
#define WW_MIN_PLAYERS 8  /* The rules: 8 to 18 players */
#define WW_MAX_PLAYERS 18
#define WW_NONE 0xFF  /* No player (no vote, nobody died...) */

typedef enum {
    WW_VILLAGER,  /* Simple villageois */
    WW_WOLF,
    WW_SEER,  /* Voyante: sees the card of a player each night */
    WW_WITCH,  /* Sorcière: a healing potion and a poison, once each in the game */
    WW_HUNTER,  /* Chasseur: shoots someone when he dies */
    WW_CUPID,  /* Cupidon: links two lovers the first night */
    WW_GIRL,  /* Petite fille: may spy on the wolves, at the risk of her life */
    WW_THIEF,  /* Voleur: may take one of the 2 cards left, the first night */
    WW_ROLES,
} ww_role_t;

/* The options of the game: the roles in play (the wolves and the villagers always are) and the captain */
#define WW_OPT_SEER 0x01
#define WW_OPT_WITCH 0x02
#define WW_OPT_HUNTER 0x04
#define WW_OPT_CUPID 0x08
#define WW_OPT_GIRL 0x10
#define WW_OPT_CAPTAIN 0x20
#define WW_OPT_THIEF 0x40
#define WW_OPT_ALL 0x7F  /* "Classique" */
#define WW_OPT_BEGINNER WW_OPT_SEER  /* "Débutant" */

typedef enum {
    WW_WIN_NONE,  /* The game goes on */
    WW_WIN_VILLAGE,  /* The last wolf is dead */
    WW_WIN_WOLVES,  /* The last villager is dead */
    WW_WIN_LOVERS,  /* A wolf and a villager in love, the last two alive */
    WW_WIN_DRAW,  /* Nobody alive */
} ww_win_t;

typedef struct {
    uint8_t n;
    uint8_t options;
    uint8_t role[WW_MAX];
    uint8_t center[2];  /* The 2 cards left for the thief (WW_NONE without the thief) */
    uint32_t alive;  /* Bit i: player i is alive */
    uint8_t lovers[2];  /* WW_NONE: no couple */
    uint8_t captain;  /* WW_NONE: none (yet) */
    bool heal_used, poison_used;  /* The potions of the witch */
} ww_game_t;

typedef uint32_t (*ww_rand_t)(void);

/** \brief The number of wolves of the rules: 2 from 8 to 11 players, 3 from 12 (1 for the small test games). */
int ww_wolves_for(int n);

/** \brief Deals the cards to \p n players at random: the wolves, the roles of \p options, simple villagers for the
 * others; with the thief, 2 more simple villagers are shuffled with them and the 2 cards left go to g->center.
 * Everybody alive, no lovers, no captain. */
void ww_deal(ww_game_t *g, int n, uint8_t options, ww_rand_t rnd);

static inline bool ww_alive(const ww_game_t *g, int i) {
    return i >= 0 && i < g->n && (g->alive >> i & 1);
}

int ww_count_alive(const ww_game_t *g);
/** \brief The player who has the card \p r (alive or not), -1 when nobody has it (not in play, or left in the center). */
int ww_find(const ww_game_t *g, ww_role_t r);
/** \brief Bit mask of the wolves (alive or not). */
uint32_t ww_wolves(const ww_game_t *g);
/** \brief The other lover of \p i, WW_NONE when \p i is not in love. */
int ww_partner(const ww_game_t *g, int i);

/** \brief \p actor may vote against / attack / poison / shoot \p target: alive, not himself, not his lover. */
bool ww_may_harm(const ww_game_t *g, int actor, int target);

/** \brief The thief takes the card \p k (0 or 1) of the center, or keeps his card (\p k < 0): refused when the two
 * cards are wolves (he must take one). His old card goes to the center. */
bool ww_thief_must_take(const ww_game_t *g);
bool ww_thief_swap(ww_game_t *g, int thief, int k);

/** \brief Counts the votes: \p votes[i] is the vote of player i (WW_NONE: none; the dead do not vote), \p double_voter
 * counts twice (the captain, WW_NONE: nobody). \return the most voted player, or WW_NONE (no vote, or a tie: the tied
 * players in *tied) */
int ww_count_votes(const ww_game_t *g, const uint8_t *votes, int double_voter, uint32_t *tied);

/** \brief One of the players of \p mask at random (WW_NONE when empty). */
int ww_pick(uint32_t mask, ww_rand_t rnd);

/* The vote of the village */
typedef enum {
    WW_DAY_OUT,  /* *out is eliminated */
    WW_DAY_NOBODY,  /* No vote, or a tie again at the second vote */
    WW_DAY_TIEBREAK,  /* A tie, the captain did not vote for one of them: he chooses among *tied */
    WW_DAY_REVOTE,  /* A tie without captain: a second vote among *tied */
} ww_day_t;

/** \brief The result of the vote of the village (\p second: the second vote, after a tie without captain). The
 * captain counts twice; a tie: the one of the tied players the captain voted for. */
ww_day_t ww_day_vote(const ww_game_t *g, const uint8_t *votes, bool second, int *out, uint32_t *tied);

/** \brief The little girl spies on the wolves: she sees one living wolf (\p *seen), and she is caught when \p r is a
 * multiple of 3 (a chance of 1 in 3). \return caught */
bool ww_spy(const ww_game_t *g, uint32_t r, uint8_t *seen);

/** \brief Kills player \p i (if alive) and the other lover, who dies of grief. The dead are appended to \p deaths
 * (\p n_deaths of them already, WW_MAX at most). \return the new number of deaths */
int ww_kill(ww_game_t *g, int i, uint8_t *deaths, int n_deaths);

/** \brief The end of a night: the victim of the wolves (WW_NONE: nobody) dies unless \p healed, the target of the
 * poison (WW_NONE: none) dies: both potions can be used the same night. The potions used are marked.
 * \return the number of deaths, in \p deaths */
int ww_dawn(ww_game_t *g, int victim, bool healed, int poisoned, uint8_t *deaths);

/** \brief Cupidon links \p a and \p b (two different players). */
bool ww_link(ww_game_t *g, int a, int b);

/** \brief The lovers are a wolf and a non wolf (their goal: being the last two). */
bool ww_mixed_lovers(const ww_game_t *g);

ww_win_t ww_winner(const ww_game_t *g);

/** \brief Player \p i is among the winners of \p win. */
bool ww_is_winner(const ww_game_t *g, int i, ww_win_t win);

const char *ww_role_name(int role);  /* "?" when unknown */
const char *ww_win_text(ww_win_t win);

#endif /* _WEREWOLF_LOGIC_H */
