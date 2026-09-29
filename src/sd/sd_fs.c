/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* File system helpers shared by the applications (video, music, file browser) */

#include <string.h>
#include <strings.h>

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
