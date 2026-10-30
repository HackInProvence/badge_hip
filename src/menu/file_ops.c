/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Renaming and deleting a file of the SD card (file_ops.h) */

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "ff.h"
#include "sd.h"
#include "file_ops.h"

#define PATH_LEN (2 * SD_NAME_MAX + 16)

static bool exists(const char *path) {
    FILINFO info;
    return f_stat(path, &info) == FR_OK;
}

file_result_t file_rename(const char *path, const char *base, char *new_path, size_t len) {
    char name[SD_NAME_MAX];
    snprintf(name, sizeof(name), "%s", base);
    if (! file_name_clean(name))
        return FILE_BAD_NAME;
    if (sd_mount() != FR_OK)
        return FILE_ERROR;
    file_renamed_path(path, name, new_path, len);
    if (! strcmp(new_path, path))
        return FILE_OK;  /* The same name */
    char other[PATH_LEN], other_new[PATH_LEN];
    bool has_other = file_companion(path, other, sizeof(other)) && exists(other);
    if (has_other)
        file_renamed_path(other, name, other_new, sizeof(other_new));
    /* A name taken (FAT: whatever the case), unless it is the same file with other capitals */
    if ((strcasecmp(new_path, path) && exists(new_path)) || (has_other && strcasecmp(other_new, other) && exists(other_new)))
        return FILE_EXISTS;
    FRESULT fr = f_rename(path, new_path);
    printf("file: rename %s -> %s: %s\n", path, new_path, fr == FR_OK ? "OK" : "error");
    if (fr != FR_OK)
        return FILE_ERROR;
    if (has_other) {
        fr = f_rename(other, other_new);
        printf("file: rename %s -> %s: %s\n", other, other_new, fr == FR_OK ? "OK" : "error");
    }
    return FILE_OK;
}

file_result_t file_delete(const char *path) {
    if (sd_mount() != FR_OK)
        return FILE_ERROR;
    FRESULT fr = f_unlink(path);
    printf("file: delete %s: %s\n", path, fr == FR_OK ? "OK" : "error");
    if (fr != FR_OK)
        return FILE_ERROR;
    char other[PATH_LEN];
    if (file_companion(path, other, sizeof(other)) && exists(other)) {
        fr = f_unlink(other);
        printf("file: delete %s: %s\n", other, fr == FR_OK ? "OK" : "error");
    }
    return FILE_OK;
}
