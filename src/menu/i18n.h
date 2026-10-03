/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file i18n.h
 *
 * \brief Translation of the texts shown on the screen (docs/fr/traduction.md).
 *
 * The texts are written in French in the code and marked for tools/i18n.py:
 * - N_("Réglages"): the text as it is (a static table, a label...). It is translated when it is drawn: gfx_text(),
 *   gfx_text_width() and the ui_*() functions translate what they get. So a text stored or sent by radio stays in
 *   French, and each badge shows it in its own language.
 * - _("Sonnerie %d / %d"): translated at once, for a format given to snprintf() (the result would not be found).
 * The translations are in src/menu/lang/<code>.po, compiled into i18n_table.c by "python tools/i18n.py gen".
 * A text without translation is shown in French.
 * */

#ifndef _I18N_H
#define _I18N_H

#include <stdbool.h>
#include <stdint.h>

#define N_(text) (text)
#define _(text) tr(text)

typedef struct {
    const char *code;  /* "fr", "en"... */
    const char *name;  /* In the language itself: "Français", "English" */
} i18n_lang_t;

typedef struct {
    uint32_t hash;  /* FNV-1a of the French text */
    const char *fr;
} i18n_text_t;

/* i18n_table.c (generated) */
extern const int I18N_N_LANGS;  /* French first */
extern const i18n_lang_t I18N_LANGS[];
extern const int I18N_N_TEXTS;
extern const i18n_text_t I18N_TEXTS[];  /* Sorted by hash */
extern const char *const *const I18N_TRANSLATIONS[];  /* [language - 1][text], NULL: not translated */

/** \brief The text in the current language: its translation, or \p fr itself (French, not found, NULL). */
const char *tr(const char *fr);

/** \brief The current language (index in I18N_LANGS, 0 = French). */
int i18n_current(void);

/** \brief Changes the language (an invalid index: French), and saves it in the store if \p save. */
void i18n_set(int lang, bool save);

/** \brief The language saved in the store (at the start). */
void i18n_init(void);

/** \brief The index of the language \p code ("en"), -1 if unknown. */
int i18n_find(const char *code);

#endif /* _I18N_H */
