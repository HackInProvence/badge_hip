/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file sd_list.h
 *
 * \brief A folder of the SD card of any size in a list (ui_list): a page of entries read by sd_page_read(), with the
 * rows "< Précédents" before them and "Suivants >" after them when the folder has more (as the ringtones).
 * Every page where a file is chosen uses it: the list of a page is never cut. */

#ifndef _SD_LIST_H
#define _SD_LIST_H

#include <stdbool.h>
#include <stddef.h>

#include "sd.h"

typedef struct {
    sd_page_t page;  /* Its buffers, max, dirs, ext / accept are set by the page */
    int sel;  /* The row selected */
    int way;  /* The page to read: 0 the first one, 1 the next one, -1 the previous one */
} sd_list_t;

#define SD_LIST_PREV (-1)  /* sd_list_entry(): the row "< Précédents" */
#define SD_LIST_NEXT (-2)  /* The row "Suivants >" */

/** \brief Reads the page asked (l->way, then back to 0) of \p dir, selects its first entry (its last one after
 * going back a page). \return the number of entries of the page */
int sd_list_read(sd_list_t *l, const char *dir);

/** \brief Rows of the list: the entries and the rows to the other pages. */
int sd_list_rows(const sd_list_t *l);

/** \brief The entry (index in page.names) of a row, SD_LIST_PREV or SD_LIST_NEXT. */
int sd_list_entry(const sd_list_t *l, int row);

/** \brief The text of a row: the name ("Name/" for a folder), or "< Précédents" / "Suivants >". */
void sd_list_label(const sd_list_t *l, int row, char *buf, size_t len);

/** \brief Y / X: the row before / after (wrapping). */
void sd_list_move(sd_list_t *l, int delta);

/** \brief When the row selected leads to another page: sets l->way and returns true (the page then calls
 * sd_list_read(), when the card can be read). */
bool sd_list_turn(sd_list_t *l);

#endif /* _SD_LIST_H */
