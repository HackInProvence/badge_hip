/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Translation of the texts shown, see i18n.h. The lookup: FNV-1a hash of the French text, binary search in the
 * table sorted by hash, then the text compared (two texts may share a hash; a text of the user, a file name... is
 * only translated if it is exactly a text of the table). */

#include <stdio.h>
#include <string.h>

#include "i18n.h"
#ifndef I18N_NO_STORE
#include "gfx.h"
#include "store.h"
#endif

static int current = 0;

static uint32_t fnv1a(const char *s) {
    uint32_t h = 0x811C9DC5u;
    while (*s)
        h = (h ^ (uint8_t)*s++) * 0x01000193u;
    return h;
}

const char *tr(const char *fr) {
    if (! current || ! fr || ! *fr)
        return fr;
    uint32_t h = fnv1a(fr);
    int lo = 0, hi = I18N_N_TEXTS;
    while (lo < hi) {  /* The first one with this hash */
        int mid = (lo + hi) / 2;
        if (I18N_TEXTS[mid].hash < h)
            lo = mid + 1;
        else
            hi = mid;
    }
    const char *const *texts = I18N_TRANSLATIONS[current - 1];
    for (int i = lo; i < I18N_N_TEXTS && I18N_TEXTS[i].hash == h; ++i)
        if (! strcmp(I18N_TEXTS[i].fr, fr))
            return texts[i] ? texts[i] : fr;
    return fr;
}

int i18n_current(void) {
    return current;
}

int i18n_find(const char *code) {
    for (int i = 0; i < I18N_N_LANGS; ++i)
        if (! strncmp(I18N_LANGS[i].code, code, 2))
            return i;
    return -1;
}

void i18n_set(int lang, bool save) {
    current = lang > 0 && lang < I18N_N_LANGS ? lang : 0;
    printf("i18n: language %s (%s)\n", I18N_LANGS[current].code, I18N_LANGS[current].name);
#ifndef I18N_NO_STORE
    if (save) {
        memcpy(store_get()->lang, I18N_LANGS[current].code, sizeof(store_get()->lang));
        store_changed();
    }
#else
    (void)save;
#endif
}

void i18n_init(void) {
#ifndef I18N_NO_STORE
    gfx_translate = tr;
    char code[3] = {0};
    memcpy(code, store_get()->lang, 2);
    int lang = i18n_find(code);  /* Never set (0 or 0xFF): French */
    current = lang > 0 ? lang : 0;
    printf("i18n: language %s (%s), %d texts\n", I18N_LANGS[current].code, I18N_LANGS[current].name, I18N_N_TEXTS);
#endif
}
