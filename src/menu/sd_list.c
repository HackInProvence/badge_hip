/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* A folder of the SD card in a list, a page at a time (sd_list.h) */

#include <stdio.h>

#include "i18n.h"
#include "sd_list.h"

int sd_list_read(sd_list_t *l, const char *dir) {
    int way = l->way;
    l->way = 0;
    sd_page_read(&l->page, dir, way);
    /* After the next page: its first entry; after the previous one: its last entry */
    l->sel = way < 0 ? l->page.prev + l->page.n - 1 : l->page.prev;
    if (l->sel < 0)
        l->sel = 0;
    return l->page.n;
}

int sd_list_rows(const sd_list_t *l) {
    return l->page.prev + l->page.n + l->page.next;
}

int sd_list_entry(const sd_list_t *l, int row) {
    if (l->page.prev && row == 0)
        return SD_LIST_PREV;
    int i = row - l->page.prev;
    return i >= l->page.n ? SD_LIST_NEXT : i;
}

void sd_list_label(const sd_list_t *l, int row, char *buf, size_t len) {
    int i = sd_list_entry(l, row);
    if (i == SD_LIST_PREV)
        snprintf(buf, len, "%s", tr(N_("< Précédents")));
    else if (i == SD_LIST_NEXT)
        snprintf(buf, len, "%s", tr(N_("Suivants >")));
    else
        snprintf(buf, len, "%s%s", l->page.names[i], l->page.is_dir && l->page.is_dir[i] ? "/" : "");
}

void sd_list_move(sd_list_t *l, int delta) {
    int rows = sd_list_rows(l);
    if (rows)
        l->sel = (l->sel + rows + delta) % rows;
}

bool sd_list_turn(sd_list_t *l) {
    int i = sd_list_entry(l, l->sel);
    if (i >= 0 || ! sd_list_rows(l))
        return false;
    l->way = i == SD_LIST_PREV ? -1 : 1;
    return true;
}
