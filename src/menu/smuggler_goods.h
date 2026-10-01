/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file smuggler_goods.h
 *
 * \brief The goods of the smuggler cicada (Social > Contrebande): their names, rarities, values in doublons and
 * icons (32 x 32, drawn in ASCII art in tools/smuggler_icons.py), and the cargo of the badge.
 *
 * Pure logic (no SDK): also compiled by the host tests (src/tests/host/test_smuggler.c).
 *
 * The cargo is the array store_t.cargo (one byte per good): bits 0-6 = how many, bit 7 = already owned once
 * (the Collection page shows the goods never owned as silhouettes).
 * */

#ifndef _SMUGGLER_GOODS_H
#define _SMUGGLER_GOODS_H

#include <stdbool.h>
#include <stdint.h>

#define SMUGGLER_ICON_SIZE 32
#define SMUGGLER_ICON_BYTES (SMUGGLER_ICON_SIZE * SMUGGLER_ICON_SIZE / 8)
#define SMUGGLER_NO_GOOD 0xFF
#define SMUGGLER_MAX_COUNT 99  /* Per good */
#define SMUGGLER_SEEN 0x80  /* Bit of a cargo byte: owned once */
#define SMUGGLER_COUNT_MASK 0x7F

typedef enum {
    RARITY_COMMON,
    RARITY_RARE,
    RARITY_LEGENDARY,
    RARITIES,
} smuggler_rarity_t;

typedef struct {
    const char *name;  /* UTF-8, fits 196 px in the small font */
    uint8_t rarity;
    uint16_t value;  /* Doublons */
    const char *story;  /* One line about it (detail page), '\n' between the lines */
    const uint8_t *icon;  /* SMUGGLER_ICON_BYTES, rows of 4 bytes, bit 7 = left, 1 = black */
} smuggler_good_t;

extern const smuggler_good_t smuggler_goods[];
extern const int smuggler_n_goods;
#define SMUGGLER_GOODS 26  /* smuggler_n_goods, as a constant (at most STORE_CARGO_ITEMS) */

/** \brief "commun", "rare", "légendaire". */
const char *smuggler_rarity_name(uint8_t rarity);

/* The cargo: \p cargo has SMUGGLER_GOODS bytes */
int cargo_count(const uint8_t *cargo, int good);
bool cargo_seen(const uint8_t *cargo, int good);
/** \brief One more (up to SMUGGLER_MAX_COUNT), marks it as owned once. \return false when full or invalid */
bool cargo_add(uint8_t *cargo, int good);
/** \brief One less. \return false when there is none */
bool cargo_take(uint8_t *cargo, int good);
int cargo_total(const uint8_t *cargo);  /* Goods in the cargo */
int cargo_kinds(const uint8_t *cargo);  /* Different goods in the cargo */
int cargo_seen_kinds(const uint8_t *cargo);  /* Different goods owned once (Collection) */
uint32_t cargo_value(const uint8_t *cargo);  /* Doublons */
/** \brief The index of the \p n th good in the cargo (count > 0), SMUGGLER_NO_GOOD when there are fewer. */
int cargo_nth(const uint8_t *cargo, int n);

/** \brief A random good: \p r1 picks the rarity (commun 75 %, rare 22 %, légendaire 3 %), \p r2 the good in it.
 * \p max_rarity limits it (RARITY_COMMON for the first goods of the cargo). */
int smuggler_random_good(uint32_t r1, uint32_t r2, uint8_t max_rarity);

#endif /* _SMUGGLER_GOODS_H */
