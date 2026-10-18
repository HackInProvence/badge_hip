/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* File system helpers shared by the applications (video, music, file browser) */

#include <string.h>
#include <strings.h>

#include "pico/time.h"

#include "ff.h"
#include "log.h"
#include "sd.h"


static FATFS fs;
static bool mounted = false;


int sd_mount(void) {
    if (mounted)
        return FR_OK;
    FRESULT fr = f_mount(&fs, "", 1);
    if (fr == FR_OK)
        mounted = true;
    else
        log_warning("sd: mount failed, FatFs error %d", fr);
    return fr;
}


void sd_unmount(void) {
    if (mounted)
        f_unmount("");
    mounted = false;
}


static bool ends_with(const char *name, const char *ext) {
    size_t n = strlen(name), e = strlen(ext);
    return n >= e && strcasecmp(name + n - e, ext) == 0;
}


/* Lists files (ext != NULL) or directories (ext == NULL) */
static size_t list(const char *dir, const char *ext, char (*names)[SD_NAME_MAX], size_t max) {
    if (sd_mount() != FR_OK)
        return 0;

    DIR d;
    FILINFO info;
    size_t n = 0;
    if (f_opendir(&d, dir) != FR_OK)
        return 0;
    while (n < max && f_readdir(&d, &info) == FR_OK && info.fname[0]) {
        if ((info.fattrib & (AM_HID | AM_SYS)) || info.fname[0] == '.')
            continue;
        if (ext ? (info.fattrib & AM_DIR) || ! ends_with(info.fname, ext) : ! (info.fattrib & AM_DIR))
            continue;
        if (strlen(info.fname) >= SD_NAME_MAX) {
            log_warning("sd: name too long, ignored: %s", info.fname);
            continue;
        }
        /* Insertion sort */
        size_t i = n++;
        while (i > 0 && strcasecmp(names[i-1], info.fname) > 0) {
            strcpy(names[i], names[i-1]);
            --i;
        }
        strcpy(names[i], info.fname);
    }
    f_closedir(&d);
    return n;
}


size_t sd_list_files(const char *dir, const char *ext, char (*names)[SD_NAME_MAX], size_t max) {
    return list(dir, ext, names, max);
}


size_t sd_list_dirs(const char *dir, char (*names)[SD_NAME_MAX], size_t max) {
    return list(dir, NULL, names, max);
}


/* ------ A folder read a page at a time ------ */

/* The order of the entries: the folders, then the files, each by name (or by its key, sd_page_t.key) */
static int entry_cmp(const sd_page_t *p, bool a_dir, const char *a, bool b_dir, const char *b) {
    if (a_dir != b_dir)
        return a_dir ? -1 : 1;
    return p->keys ? strcmp(a, b) : strcasecmp(a, b);
}

static bool page_dir(const sd_page_t *p, int i) {
    return p->is_dir && p->is_dir[i];
}

/* What an entry of the page is sorted by: its key, or its name */
static const char *page_sort(const sd_page_t *p, int i) {
    return p->keys ? p->keys[i] : p->names[i];
}

static void page_move(sd_page_t *p, int to, int from) {
    strcpy(p->names[to], p->names[from]);
    if (p->is_dir)
        p->is_dir[to] = p->is_dir[from];
    if (p->keys)
        strcpy(p->keys[to], p->keys[from]);
}

int sd_page_read(sd_page_t *p, const char *dir, int way) {
    /* The bound: the last entry of the page (next page) or its first one (previous page) */
    char bound[SD_KEY_LEN];
    bool bound_dir = false;
    if (way && p->n) {
        int k = way > 0 ? p->n - 1 : 0;
        strcpy(bound, page_sort(p, k));
        bound_dir = page_dir(p, k);
    } else {
        way = 0;
    }
    p->n = 0;
    p->prev = p->next = false;
    if (sd_mount() != FR_OK)
        return 0;
    DIR d;
    FILINFO info;
    if (f_opendir(&d, dir) != FR_OK)
        return 0;
    bool more = false;  /* Entries beyond the page, in the direction read */
    bool other = false;  /* Entries on the other side of the bound */
    char name[SD_NAME_MAX], sort[SD_KEY_LEN];
    while (f_readdir(&d, &info) == FR_OK && info.fname[0]) {
        bool is_dir = info.fattrib & AM_DIR;
        if ((info.fattrib & (AM_HID | AM_SYS)) || info.fname[0] == '.' || strlen(info.fname) >= SD_NAME_MAX)
            continue;
        if (is_dir ? ! p->dirs : ! (p->accept ? p->accept(info.fname) : p->ext && ends_with(info.fname, p->ext)))
            continue;
        if (! p->is_dir)
            is_dir = false;  /* Folders only (or files only): one kind, the order of the names */
        strcpy(name, info.fname);
        if (p->keys)
            p->key(dir, name, sort, sizeof(sort));  /* It may read the card (the directory stays open) */
        else
            strcpy(sort, name);
        int c = way ? entry_cmp(p, is_dir, sort, bound_dir, bound) : 0;
        if ((way > 0 && c <= 0) || (way < 0 && c >= 0)) {
            other = true;
            continue;
        }
        if (p->n == p->max) {
            more = true;
            if (way >= 0) {  /* Keep the first entries */
                if (entry_cmp(p, is_dir, sort, page_dir(p, p->n - 1), page_sort(p, p->n - 1)) > 0)
                    continue;
                --p->n;
            } else {  /* Keep the last ones */
                if (entry_cmp(p, is_dir, sort, page_dir(p, 0), page_sort(p, 0)) < 0)
                    continue;
                for (int i = 0; i < p->n - 1; ++i)
                    page_move(p, i, i + 1);
                --p->n;
            }
        }
        /* Insertion sort */
        int i = p->n++;
        while (i > 0 && entry_cmp(p, page_dir(p, i - 1), page_sort(p, i - 1), is_dir, sort) > 0) {
            page_move(p, i, i - 1);
            --i;
        }
        strcpy(p->names[i], name);
        if (p->is_dir)
            p->is_dir[i] = is_dir;
        if (p->keys)
            strcpy(p->keys[i], sort);
    }
    f_closedir(&d);
    p->prev = way >= 0 ? other : more;
    p->next = way >= 0 ? more : other;
    return p->n;
}


/* ------ The date of the files written ------ */

static int64_t clock_offset = 0;  /* Local time (seconds since 1970, as if UTC) - seconds since boot; 0: not set */

void sd_set_time(uint32_t local) {
    clock_offset = (int64_t)local - (int64_t)(time_us_64() / 1000000);
}


uint32_t sd_time(void) {
    return clock_offset ? (uint32_t)(clock_offset + (int64_t)(time_us_64() / 1000000)) : 0;
}


/* Days since 1970-01-01 <-> year, month, day (Howard Hinnant's algorithms) */
static int32_t days_from_civil(int y, int m, int d) {
    y -= m <= 2;
    int era = y / 400;
    int yoe = y - era * 400;
    int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static void civil_from_days(int32_t z, int *y, int *m, int *d) {
    z += 719468;
    int era = z / 146097;
    int doe = z - era * 146097;
    int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int mp = (5 * doy + 2) / 153;
    *d = doy - (153 * mp + 2) / 5 + 1;
    *m = mp + (mp < 10 ? 3 : -9);
    *y = yoe + era * 400 + (*m <= 2);
}


uint32_t sd_fat_datetime(uint32_t local) {
    int y, m, d;
    civil_from_days((int32_t)(local / 86400), &y, &m, &d);
    if (y < 1980)
        return (uint32_t)(1 << 21 | 1 << 16);  /* 1980-01-01, the first FAT date */
    uint32_t s = local % 86400;
    return (uint32_t)(y - 1980) << 25 | (uint32_t)m << 21 | (uint32_t)d << 16 | (s / 3600) << 11 | (s / 60 % 60) << 5 |
           (s % 60) / 2;
}


uint32_t sd_local_time(uint16_t fdate, uint16_t ftime) {
    int32_t days = days_from_civil(1980 + (fdate >> 9), (fdate >> 5) & 15, fdate & 31);
    return (uint32_t)days * 86400 + (ftime >> 11) * 3600 + ((ftime >> 5) & 63) * 60 + (ftime & 31) * 2;
}


/* FatFs: the date of the files created or written. Without a clock set by the PC (or the phone, the ESP32...):
 * October 1st, 2026 */
DWORD get_fattime(void) {
    uint32_t t = sd_time();
    return t ? sd_fat_datetime(t) : (DWORD)(2026 - 1980) << 25 | 10 << 21 | 1 << 16;
}
