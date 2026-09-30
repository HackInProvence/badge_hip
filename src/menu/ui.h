/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file ui.h
 *
 * \brief Drawing helpers shared by the pages of the menu application (same look as main.c):
 * black title bar, footer with the buttons, lists, centered texts, and a text editor ("clavier") for the 4 buttons.
 * */

#ifndef _UI_H
#define _UI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gfx.h"

#define UI_TITLE_H 28
#define UI_FOOTER_Y 180
#define UI_ROW_H 21
#define UI_VISIBLE_ROWS 7

/* Buttons flags, same as btns_get_state() */
#define UI_BTN_A 0x01  /* Left wing (G) */
#define UI_BTN_B 0x02  /* Right wing (D) */
#define UI_BTN_X 0x04  /* Right flank */
#define UI_BTN_Y 0x08  /* Left flank */

void ui_title(uint8_t *fb, const char *title);
void ui_footer(uint8_t *fb, const char *text);

/** \brief Text truncated with "..." to fit in \p width pixels. */
void ui_fit(const gfx_font_t *font, char *dst, size_t len, const char *src, int width);

/** \brief Centered lines ('\n' separated) from \p y, returns the y after the last one. */
int ui_lines(uint8_t *fb, int y, const gfx_font_t *font, const char *text);

/** \brief Left aligned lines ('\n' separated, truncated to the width) from \p y, returns the y after the last one. */
int ui_text(uint8_t *fb, int x, int y, const gfx_font_t *font, const char *text);

/** \brief A scrolling list below the title: \p label gives the text of each row, \p sel is highlighted. */
void ui_list(uint8_t *fb, int count, int sel, void (*label)(int i, char *buf, size_t len));

/** \brief A framed message box in the middle of the page. */
void ui_box(uint8_t *fb, const char *text);

/** \brief Draws \p text centered, cut in lines at the spaces to fit the width (at most \p max_lines), from \p y.
 * \return the y after the last line. */
int ui_wrapped(uint8_t *fb, int y, const gfx_font_t *font, const char *text, int max_lines);

/* Check of the texts (debug, key U of the serial port): the cut texts, the too wide ones and the ones under the
 * footer are traced ("uicheck: ...") */
extern bool ui_check;
void ui_check_width(const gfx_font_t *font, const char *text, int width, const char *what);
void ui_check_bottom(int bottom, const char *text);

/** \brief Horizontal gauge (e.g. a signal strength) of \p value / \p max. */
void ui_gauge(uint8_t *fb, int x, int y, int w, int h, int value, int max);


/* ------ Text editor with the 4 buttons ------
 * Flanks: previous / next character (held: repeated by the caller), right wing: next position,
 * left wing: previous position (leave from the first one), long press on the right wing: done. */

#define UI_EDIT_MAX 56

typedef struct {
    char text[UI_EDIT_MAX + 1];
    int max_len;  /* Characters (the text is ASCII in the editor) */
    int cursor;
    const char *charset;
    uint64_t repeat_us[2];  /* Next repetition of the held flank (left, right), time_us_64() */
} ui_edit_t;

extern const char UI_CHARSET_TEXT[];  /* Letters, digits, punctuation for names, e-mails, URLs */
extern const char UI_CHARSET_PHONE[];  /* Digits, +, space */
extern const char UI_CHARSET_UPPER[];  /* A-Z 0-9 space, for the answers of the challenges */
extern const char UI_CHARSET_LONG[];  /* Letters with the French accents, digits, punctuation: messages, URLs */

void ui_edit_start(ui_edit_t *e, const char *text, int max_len, const char *charset);

/** \brief Changes the character under the cursor (\p delta = -1 / +1, more when held). */
void ui_edit_change(ui_edit_t *e, int delta);

/** \brief Moves the cursor; returns false when leaving from the first position (cancel). */
bool ui_edit_move(ui_edit_t *e, int delta);

/** \brief The edited text, without the trailing spaces. */
void ui_edit_result(const ui_edit_t *e, char *buf, size_t len);

/* A real keyboard (tools/badge_remote.py, keyboard mode): the characters typed on the PC, received by the USB
 * serial port, go to the editor on the screen. '\r' = done, 0x1B = cancel, '\b' = erase the previous character. */
void ui_edit_type(char c);
bool ui_edit_typed_pending(void);
void ui_edit_typed_clear(void);  /* Nobody edits: the characters are dropped */
/** \brief Applies the typed characters to \p e; returns UI_EDIT_DONE, UI_EDIT_CANCEL or UI_EDIT_EDITING. */
int ui_edit_apply_typed(ui_edit_t *e);

enum { UI_EDIT_EDITING, UI_EDIT_DONE, UI_EDIT_CANCEL };

/** \brief Draws the editor: \p prompt (1-2 lines), the text around the cursor, the usage. */
void ui_edit_render(uint8_t *fb, const ui_edit_t *e, const char *title, const char *prompt);

#endif /* _UI_H */
