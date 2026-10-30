/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The names of the files renamed on the badge (file_ops.h): no card here, tested on a PC */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "file_ops.h"

const char FILE_NAME_CHARS[] = " abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.,()&'!#+@";

/* The photos of the camera (camera.c): the .epi shown by the badge and its JPEG */
#define EPI_DIR "IMAGES/PHOTOS/"
#define JPG_DIR "PHOTOS/"

void file_name_split(const char *path, char *base, size_t blen, char *ext, size_t elen) {
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    const char *dot = strrchr(name, '.');
    if (! dot || dot == name)
        dot = name + strlen(name);
    snprintf(base, blen, "%.*s", (int)(dot - name), name);
    snprintf(ext, elen, "%s", dot);
}

bool file_name_clean(char *name) {
    size_t start = 0, n = strlen(name);
    while (start < n && (name[start] == ' ' || name[start] == '.'))
        ++start;
    while (n > start && (name[n - 1] == ' ' || name[n - 1] == '.'))
        --n;
    memmove(name, name + start, n - start);
    name[n - start] = 0;
    if (! name[0])
        return false;
    for (const char *p = name; *p; ++p)
        if (! strchr(FILE_NAME_CHARS, *p))
            return false;
    return true;
}

static bool starts_with(const char *s, const char *prefix) {
    for (; *prefix; ++s, ++prefix)
        if (toupper((unsigned char)*s) != toupper((unsigned char)*prefix))
            return false;
    return true;
}

static bool same_text(const char *a, const char *b) {
    return strlen(a) == strlen(b) && starts_with(a, b);
}

bool file_companion(const char *path, char *out, size_t len) {
    char base[80], ext[16];
    file_name_split(path, base, sizeof(base), ext, sizeof(ext));
    const char *name = strrchr(path, '/');
    size_t dir_len = name ? (size_t)(name + 1 - path) : 0;
    if (dir_len == strlen(EPI_DIR) && starts_with(path, EPI_DIR) && same_text(ext, ".EPI")) {
        snprintf(out, len, "%s%s.JPG", JPG_DIR, base);
        return true;
    }
    if (dir_len == strlen(JPG_DIR) && starts_with(path, JPG_DIR) && same_text(ext, ".JPG")) {
        snprintf(out, len, "%s%s.EPI", EPI_DIR, base);
        return true;
    }
    return false;
}

void file_renamed_path(const char *path, const char *base, char *out, size_t len) {
    char old_base[80], ext[16];
    file_name_split(path, old_base, sizeof(old_base), ext, sizeof(ext));
    const char *name = strrchr(path, '/');
    int dir_len = name ? (int)(name + 1 - path) : 0;
    snprintf(out, len, "%.*s%s%s", dir_len, path, base, ext);
}
