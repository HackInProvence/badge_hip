/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file tug_logic.h
 *
 * \brief The rules of the tug of war (tug.c) without the radio nor the screen, tested on the PC
 * (src/tests/host/test_party_games.c): the teams, the pulls, the knot of the rope, the winner.
 * */

#ifndef _TUG_LOGIC_H
#define _TUG_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

enum { TUG_CIGALES, TUG_FOURMIS, TUG_REFEREE };

/** \brief The teams of the \p n players of the roster (\p team[i] for the player i), the same on every badge for the
 * same \p seed (party_seed()) and roster: a shuffle, the first half are the Cigales, the second half the Fourmis;
 * with an odd number of players, the last one of the shuffle is the referee. */
void tug_teams(uint32_t seed, int n, uint8_t *team);

/** \brief Pull detector: a press of the left wing then of the right wing is one pull; holding a wing or pressing
 * both at once does nothing. */
typedef struct {
    bool armed;  /* The left wing was pressed, the right one is expected */
} tug_pull_t;

/** \brief The buttons pressed since the last call (\p left, \p right). \return true for a pull */
bool tug_pull(tug_pull_t *p, bool left, bool right);

/** \brief The team that wins with these totals: TUG_CIGALES, TUG_FOURMIS, or -1 for a draw. */
int tug_winner(uint32_t cigales, uint32_t fourmis);

/** \brief The pulls ahead (Cigales - Fourmis) that reach a mark: the rope is won before the end of the time. */
int tug_margin(int team_size);

/** \brief Offset of the knot from the middle (pixels, negative: towards the Cigales on the left): \p half at the mark,
 * clamped to \p max. */
int tug_knot(int32_t diff, int margin, int half, int max);

#endif /* _TUG_LOGIC_H */
