/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file gamebook_parse.h
 *
 * \brief The books of the gamebooks ("livres dont vous êtes le héros", gamebook.c): a text format easy to write by
 * hand (docs/fr/livres_jeux.md), read without loading the whole book in RAM.
 *
 * \code
 * Le Trésor du capitaine Cigalon       <- title (first line)
 * Les cigales de Hack In Provence      <- author (second line, optional)
 * // a comment
 * == 1                                 <- a section (the first one of the file is the start)
 * Text, paragraphs separated by an empty line.
 * +[antenne]                           <- gives an item (-[antenne] takes it)
 * -> 2 : Ouvrir la porte               <- a choice
 * -> 3 [antenne] : Écouter la radio    <- only with the item ([!antenne]: only without it)
 * -> 4 [dé 1-3] : Pas de chance        <- a die is rolled when the section is shown
 * == 4 FIN perdu                       <- an ending (FIN, FIN gagné or FIN perdu)
 * \endcode
 *
 * The file is read through a callback (SD card or built-in text in the flash): gb_index() scans it once and keeps the
 * offsets of the sections, gb_load() reads one section in a bounded buffer. Pure C, tested on the PC
 * (tests/host/test_gamebook.c).
 * */

#ifndef _GAMEBOOK_PARSE_H
#define _GAMEBOOK_PARSE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GB_MAX_SECTIONS 400  /* Classic gamebooks have 400 sections */
#define GB_MAX_ITEMS 8  /* Items of a book: saved as 8 bits with the progress */
#define GB_ITEM_LEN 24
#define GB_TITLE_LEN 48
#define GB_TEXT_MAX 2048  /* Bytes of the text of a section (UTF-8), the rest is cut */
#define GB_MAX_CHOICES 8
#define GB_CHOICE_LEN 96
#define GB_LINE_MAX 256  /* Longer lines of the file are read in pieces */

/* Reads \p len bytes at \p offset of the book, returns the number of bytes read (0 at the end), < 0 on error */
typedef int (*gb_read_t)(void *ctx, uint32_t offset, void *buf, int len);

/* A book file being read: a small cache in front of the callback */
typedef struct {
    gb_read_t read;
    void *ctx;
    uint32_t pos;  /* Offset of the next byte */
    uint32_t buf_start;  /* Offset of buf[0] */
    int buf_len;
    bool error;  /* The callback failed */
    uint8_t buf[256];
} gb_src_t;

typedef enum {
    GB_END_NONE,  /* Not an ending: choices */
    GB_END_NEUTRAL,  /* "== 12 FIN", or a section without any choice */
    GB_END_WIN,  /* "== 12 FIN gagné" */
    GB_END_LOSE,  /* "== 12 FIN perdu" */
} gb_end_t;

typedef struct {
    char title[GB_TITLE_LEN];
    char author[GB_TITLE_LEN];
    uint16_t start;  /* Number of the first section of the file, 0 = no section */
    uint16_t n_sections;
    bool too_many;  /* More than GB_MAX_SECTIONS: the others are missing */
    uint8_t n_items;
    char items[GB_MAX_ITEMS][GB_ITEM_LEN];  /* In the order of their first use in the file */
    uint16_t number[GB_MAX_SECTIONS];
    uint32_t offset[GB_MAX_SECTIONS];  /* Of the "==" line */
} gb_book_t;

typedef struct {
    uint16_t target;
    int8_t item;  /* Condition: index in gb_book_t.items, -1 = none (or an unknown item) */
    bool negate;  /* [!item]: only without the item */
    bool unknown_item;  /* The condition names an item that is not in the book (never held) */
    uint8_t dice_min, dice_max;  /* [dé 1-3]: shown when the die gives 1 to 3; 0 = no die */
    char label[GB_CHOICE_LEN];  /* UTF-8, adapted to the fonts */
} gb_choice_t;

typedef struct {
    uint16_t number;
    uint8_t end;  /* gb_end_t */
    bool text_cut;  /* The text was longer than GB_TEXT_MAX */
    bool has_dice;  /* A choice depends on a die roll */
    uint8_t gain, lose;  /* Items (bits) given and taken when the section is shown */
    uint8_t n_choices;
    char text[GB_TEXT_MAX];  /* Paragraphs separated by '\n', UTF-8 adapted to the fonts */
    gb_choice_t choices[GB_MAX_CHOICES];
} gb_section_t;

/** \brief Prepares the reading of a book through \p read. */
void gb_src_init(gb_src_t *src, gb_read_t read, void *ctx);

/** \brief Scans the whole book: title, author, items and offsets of the sections.
 * \return false when there is no section (or a read error) */
bool gb_index(gb_book_t *book, gb_src_t *src);

/** \brief Only the title (first line), for the list of the books. \return false when there is none */
bool gb_read_title(gb_src_t *src, char *title, size_t len);

/** \brief Index of section \p number in \p book, -1 when missing. */
int gb_find(const gb_book_t *book, uint16_t number);

/** \brief Reads section \p number. \return false when it is missing (or a read error) */
bool gb_load(const gb_book_t *book, gb_src_t *src, uint16_t number, gb_section_t *sec);

/** \brief Whether the choice is shown with these \p items (bits) and this die roll (1..6, 0 = not rolled). */
bool gb_choice_available(const gb_choice_t *c, uint8_t items, uint8_t roll);

/** \brief Index of an item of the book by its name (case insensitive), -1 when unknown. */
int gb_item_index(const gb_book_t *book, const char *name);

/** \brief Hash of a string (FNV-1a): identifies a book in the saved progress. */
uint32_t gb_hash(const char *s);

/** \brief Copies \p n bytes of UTF-8 text adapted to the fonts of the badge (quotes, dashes, ellipsis, oe...),
 * Windows-1252 bytes accepted. Never writes more than \p cap - 1 bytes, nor half a character. \return bytes written */
int gb_normalize(const char *src, int n, char *dst, int cap);

/* A line of a wrapped text */
typedef struct {
    uint16_t start;  /* Offset in the text */
    uint16_t len;  /* Bytes */
    bool first;  /* First line of a paragraph */
} gb_line_t;

/* Width in pixels of \p n bytes of \p s */
typedef int (*gb_measure_t)(const char *s, int n, void *ctx);

/** \brief Cuts \p text (paragraphs separated by '\n') in lines of at most \p width pixels, the first line of each
 * paragraph \p indent pixels narrower. A word too long for a line is cut. \return the number of lines (at most \p max) */
int gb_wrap(const char *text, int width, int indent, gb_measure_t measure, void *ctx, gb_line_t *lines, int max);

#endif /* _GAMEBOOK_PARSE_H */
