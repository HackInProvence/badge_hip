/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Host test of the names of the files renamed on the badge (src/menu/file_names.c): the name typed cleaned, the
 * new path, the JPEG of a photo of the camera. */

#include <stdio.h>
#include <string.h>

#include "file_ops.h"

static int fails = 0;
#define CHECK(c, ...) do { if (! (c)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); \
                                        printf("\n"); ++fails; } } while (0)

static void check_split(const char *path, const char *base, const char *ext) {
    char b[80], e[16];
    file_name_split(path, b, sizeof(b), e, sizeof(e));
    CHECK(! strcmp(b, base) && ! strcmp(e, ext), "split %s: [%s] [%s]", path, b, e);
}

static void check_clean(const char *typed, bool ok, const char *result) {
    char n[80];
    snprintf(n, sizeof(n), "%s", typed);
    bool r = file_name_clean(n);
    CHECK(r == ok && (! ok || ! strcmp(n, result)), "clean [%s]: %d [%s]", typed, r, n);
}

static void check_companion(const char *path, const char *other) {
    char o[128] = "";
    bool r = file_companion(path, o, sizeof(o));
    CHECK(other ? r && ! strcmp(o, other) : ! r, "companion %s: %d [%s]", path, r, o);
}

static void check_renamed(const char *path, const char *base, const char *result) {
    char o[128];
    file_renamed_path(path, base, o, sizeof(o));
    CHECK(! strcmp(o, result), "renamed %s %s: [%s]", path, base, o);
}

int main(void) {
    check_split("IMAGES/PHOTOS/IMG_0001.EPI", "IMG_0001", ".EPI");
    check_split("chat.noir.epi", "chat.noir", ".epi");
    check_split("MUSIQUE/sans", "sans", "");
    check_split(".cache", ".cache", "");

    check_clean("  Plage 2026  ", true, "Plage 2026");
    check_clean("...photo.", true, "photo");
    check_clean("   ", false, "");
    check_clean("a/b", false, "");
    check_clean("quoi?", false, "");
    check_clean("Mer (soir) & co", true, "Mer (soir) & co");

    check_companion("IMAGES/PHOTOS/IMG_0003.EPI", "PHOTOS/IMG_0003.JPG");
    check_companion("images/photos/Plage.epi", "PHOTOS/Plage.JPG");
    check_companion("PHOTOS/IMG_0003.JPG", "IMAGES/PHOTOS/IMG_0003.EPI");
    check_companion("IMAGES/IMG_0003.EPI", NULL);
    check_companion("IMAGES/PHOTOS/VACANCES/IMG_0003.EPI", NULL);
    check_companion("VIDEOS/PHOTOS/VID_0001.EPV", NULL);
    check_companion("IMAGES/PHOTOS/IMG_0003.TXT", NULL);

    check_renamed("IMAGES/PHOTOS/IMG_0001.EPI", "Plage", "IMAGES/PHOTOS/Plage.EPI");
    check_renamed("chat.epi", "chien", "chien.epi");
    check_renamed("TEXTES/notes", "idees", "TEXTES/idees");

    if (fails) {
        printf("%d failure(s)\n", fails);
        return 1;
    }
    printf("file_ops: OK\n");
    return 0;
}
