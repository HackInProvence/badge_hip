/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Host tests of the translation (i18n.c, the generated i18n_table.c): every text of the table is found in every
 * language, a text not in the table or not translated stays as it is, French returns the text itself. */

#include <stdio.h>
#include <string.h>

#include "i18n.h"
#include "test.h"

int main(void) {
    CHECK(I18N_N_LANGS >= 2);
    CHECK_STR(I18N_LANGS[0].code, "fr");
    CHECK_EQ(i18n_find("fr"), 0);
    CHECK(i18n_find("en") > 0);
    CHECK_EQ(i18n_find("xx"), -1);
    /* Sorted by hash */
    for (int i = 1; i < I18N_N_TEXTS; ++i)
        CHECK(I18N_TEXTS[i - 1].hash <= I18N_TEXTS[i].hash);
    /* French: the text itself */
    i18n_set(0, false);
    for (int i = 0; i < I18N_N_TEXTS; ++i)
        CHECK(tr(I18N_TEXTS[i].fr) == I18N_TEXTS[i].fr);
    /* Every language: each text of the table gives its translation, or itself */
    int translated = 0;
    for (int l = 1; l < I18N_N_LANGS; ++l) {
        i18n_set(l, false);
        CHECK_EQ(i18n_current(), l);
        for (int i = 0; i < I18N_N_TEXTS; ++i) {
            const char *t = I18N_TRANSLATIONS[l - 1][i];
            CHECK(tr(I18N_TEXTS[i].fr) == (t ? t : I18N_TEXTS[i].fr));
            /* The same text at another address is found too (it is compared, not its pointer) */
            char copy[512];
            snprintf(copy, sizeof(copy), "%s", I18N_TEXTS[i].fr);
            CHECK_STR(tr(copy), t ? t : I18N_TEXTS[i].fr);
            translated += t != NULL;
        }
        printf("%s (%s): %d / %d texts translated\n", I18N_LANGS[l].code, I18N_LANGS[l].name, translated,
               I18N_N_TEXTS);
    }
    /* Not in the table, empty, NULL: as they are */
    i18n_set(i18n_find("en"), false);
    CHECK_STR(tr("Pas un texte de la table, ni traduit"), "Pas un texte de la table, ni traduit");
    CHECK_STR(tr(""), "");
    CHECK(tr(NULL) == NULL);
    /* An invalid language: French */
    i18n_set(99, false);
    CHECK_EQ(i18n_current(), 0);
    TEST_END();
}
