/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdio.h>
#include <string.h>

#include "pico/time.h"

#include "app.h"
#include "ui.h"

const char UI_CHARSET_TEXT[] = " abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789@.-_+/:'!?#&()";
const char UI_CHARSET_PHONE[] = " 0123456789+";
const char UI_CHARSET_UPPER[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";


void ui_title(uint8_t *fb, const char *title) {
    ui_check_width(&gfx_font_medium, title, GFX_WIDTH - 2, "title");
    gfx_fill_rect(fb, 0, 0, GFX_WIDTH, UI_TITLE_H, GFX_BLACK);
    gfx_text(fb, GFX_WIDTH/2, (UI_TITLE_H - gfx_font_medium.height)/2, &gfx_font_medium, title, GFX_WHITE, GFX_ALIGN_CENTER);
}


void ui_footer(uint8_t *fb, const char *text) {
    ui_check_width(&gfx_font_small, text, GFX_WIDTH - 2, "footer");
    gfx_fill_rect(fb, 0, UI_FOOTER_Y - 2, GFX_WIDTH, 1, GFX_BLACK);
    gfx_text(fb, GFX_WIDTH/2, UI_FOOTER_Y, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
}


/* The check of the texts (debug, key U on the serial port): the texts cut or drawn under the footer are traced,
 * to find them all by going through every page (tools/badge_screens.py) */
bool ui_check = false;

void ui_check_width(const gfx_font_t *font, const char *text, int width, const char *what) {
    if (ui_check && gfx_text_width(font, text) > width)
        printf("uicheck: %s too wide (%d > %d px): \"%s\"\n", what, gfx_text_width(font, text), width, text);
}

void ui_check_bottom(int bottom, const char *text) {
    if (ui_check && bottom > UI_FOOTER_Y - 2)
        printf("uicheck: under the footer (y %d): \"%s\"\n", bottom, text);
}


void ui_fit(const gfx_font_t *font, char *dst, size_t len, const char *src, int width) {
    snprintf(dst, len, "%s", src);
    size_t n = strlen(dst);
    if (ui_check && (strlen(src) >= len || gfx_text_width(font, dst) > width))
        printf("uicheck: cut \"%s\"\n", src);
    while (n > 3 && gfx_text_width(font, dst) > width) {
        /* Remove a whole UTF-8 character before the ellipsis */
        do
            --n;
        while (n > 0 && (dst[n] & 0xC0) == 0x80);
        if (n + 4 > len)
            break;
        strcpy(dst + n, "...");
    }
}


/* Next line of a '\n' separated text, returns the rest */
static const char *next_line(const char *text, char *line, size_t len) {
    const char *end = strchr(text, '\n');
    size_t n = end ? (size_t)(end - text) : strlen(text);
    if (n >= len)
        n = len - 1;
    memcpy(line, text, n);
    line[n] = 0;
    return end ? end + 1 : text + strlen(text);
}


int ui_lines(uint8_t *fb, int y, const gfx_font_t *font, const char *text) {
    char line[64], fitted[64];
    while (*text) {
        text = next_line(text, line, sizeof(line));
        ui_fit(font, fitted, sizeof(fitted), line, GFX_WIDTH - 4);
        gfx_text(fb, GFX_WIDTH/2, y, font, fitted, GFX_BLACK, GFX_ALIGN_CENTER);
        ui_check_bottom(y + font->height, line);
        y += font->height + 3;
    }
    return y;
}


int ui_text(uint8_t *fb, int x, int y, const gfx_font_t *font, const char *text) {
    char line[64], fitted[64];
    while (*text) {
        text = next_line(text, line, sizeof(line));
        ui_fit(font, fitted, sizeof(fitted), line, GFX_WIDTH - x - 2);
        gfx_text(fb, x, y, font, fitted, GFX_BLACK, GFX_ALIGN_LEFT);
        ui_check_bottom(y + font->height, line);
        y += font->height + 3;
    }
    return y;
}


void ui_list(uint8_t *fb, int count, int sel, void (*label)(int, char *, size_t)) {
    int first = sel - UI_VISIBLE_ROWS/2;
    if (first > count - UI_VISIBLE_ROWS)
        first = count - UI_VISIBLE_ROWS;
    if (first < 0)
        first = 0;
    char text[64], fitted[64];
    for (int i = first; i < count && i < first + UI_VISIBLE_ROWS; ++i) {
        int y = UI_TITLE_H + 3 + (i - first)*UI_ROW_H;
        label(i, text, sizeof(text));
        ui_fit(&gfx_font_small, fitted, sizeof(fitted), text, GFX_WIDTH - 20);
        if (i == sel) {
            gfx_fill_rect(fb, 2, y, GFX_WIDTH-8, UI_ROW_H-1, GFX_BLACK);
            gfx_text(fb, 8, y + 1, &gfx_font_small, fitted, GFX_WHITE, GFX_ALIGN_LEFT);
        } else {
            gfx_text(fb, 8, y + 1, &gfx_font_small, fitted, GFX_BLACK, GFX_ALIGN_LEFT);
        }
    }
    /* Scroll bar */
    if (count > UI_VISIBLE_ROWS) {
        int h = UI_VISIBLE_ROWS * UI_ROW_H;
        int bar = h * UI_VISIBLE_ROWS / count;
        gfx_fill_rect(fb, GFX_WIDTH - 4, UI_TITLE_H + 3 + (h - bar) * first / (count - UI_VISIBLE_ROWS), 3, bar, GFX_BLACK);
    }
}


void ui_box(uint8_t *fb, const char *text) {
    int lines = 1;
    for (const char *c = text; *c; ++c)
        lines += *c == '\n';
    int h = lines * (gfx_font_medium.height + 3) + 16;
    int y = UI_TITLE_H + (UI_FOOTER_Y - 2 - UI_TITLE_H - h) / 2;
    gfx_fill_rect(fb, 12, y, GFX_WIDTH - 24, h, GFX_WHITE);
    gfx_rect(fb, 12, y, GFX_WIDTH - 24, h, GFX_BLACK);
    gfx_rect(fb, 13, y + 1, GFX_WIDTH - 26, h - 2, GFX_BLACK);
    ui_lines(fb, y + 8, &gfx_font_medium, text);
}


void ui_gauge(uint8_t *fb, int x, int y, int w, int h, int value, int max) {
    gfx_rect(fb, x, y, w, h, GFX_BLACK);
    if (value < 0)
        value = 0;
    if (value > max)
        value = max;
    if (max > 0)
        gfx_fill_rect(fb, x + 2, y + 2, (w - 4) * value / max, h - 4, GFX_BLACK);
}


/* ------ Text editor ------ */

void ui_edit_start(ui_edit_t *e, const char *text, int max_len, const char *charset) {
    if (max_len > UI_EDIT_MAX)
        max_len = UI_EDIT_MAX;
    e->max_len = max_len;
    e->charset = charset;
    e->cursor = 0;
    memset(e->text, ' ', max_len);
    e->text[max_len] = 0;
    for (int i = 0; i < max_len && text && text[i]; ++i)
        e->text[i] = strchr(charset, text[i]) ? text[i] : ' ';
}


void ui_edit_change(ui_edit_t *e, int delta) {
    int n = strlen(e->charset);
    const char *p = strchr(e->charset, e->text[e->cursor]);
    int i = p ? p - e->charset : 0;
    e->text[e->cursor] = e->charset[((i + delta) % n + n) % n];
}


bool ui_edit_move(ui_edit_t *e, int delta) {
    int c = e->cursor + delta;
    if (c < 0)
        return false;
    if (c >= e->max_len)
        c = e->max_len - 1;
    e->cursor = c;
    return true;
}


void ui_edit_result(const ui_edit_t *e, char *buf, size_t len) {
    snprintf(buf, len, "%s", e->text);
    for (int i = strlen(buf) - 1; i >= 0 && buf[i] == ' '; --i)
        buf[i] = 0;
}


static char typed[32];
static int typed_head = 0, typed_count = 0;

void ui_edit_type(char c) {
    if (typed_count < (int)sizeof(typed)) {
        typed[(typed_head + typed_count) % sizeof(typed)] = c;
        ++typed_count;
    }
}


bool ui_edit_typed_pending(void) {
    return typed_count > 0;
}


void ui_edit_typed_clear(void) {
    typed_count = 0;
}


int ui_edit_apply_typed(ui_edit_t *e) {
    while (typed_count) {
        char c = typed[typed_head];
        typed_head = (typed_head + 1) % sizeof(typed);
        --typed_count;
        if (c == '\r' || c == '\n') {
            typed_count = 0;
            return UI_EDIT_DONE;
        }
        if (c == 0x1B) {
            typed_count = 0;
            return UI_EDIT_CANCEL;
        }
        if (c == '\b' || c == 0x7F) {
            if (e->cursor > 0) {
                /* Erase the previous character, the rest moves left */
                memmove(e->text + e->cursor - 1, e->text + e->cursor, e->max_len - e->cursor);
                e->text[e->max_len - 1] = ' ';
                --e->cursor;
            }
            continue;
        }
        if (e->charset == UI_CHARSET_UPPER && c >= 'a' && c <= 'z')
            c -= 'a' - 'A';
        if (c && strchr(e->charset, c)) {
            e->text[e->cursor] = c;
            if (e->cursor < e->max_len - 1)
                ++e->cursor;
        }
    }
    return UI_EDIT_EDITING;
}


#define EDIT_REPEAT_DELAY_US 400000
#define EDIT_REPEAT_US 90000

int app_edit_buttons(ui_edit_t *e, const app_buttons_t *b) {
    int typed_result = ui_edit_apply_typed(e);
    if (typed_result != UI_EDIT_EDITING)
        return typed_result;
    if (b->long_pressed & UI_BTN_B)
        return UI_EDIT_DONE;
    if (b->released_short & UI_BTN_B)
        ui_edit_move(e, 1);
    if ((b->pressed & UI_BTN_A) && ! ui_edit_move(e, -1))
        return UI_EDIT_CANCEL;
    /* Flanks: the characters, repeated while held */
    uint64_t now = time_us_64();
    for (int f = 0; f < 2; ++f) {
        uint8_t bit = f ? UI_BTN_X : UI_BTN_Y;
        if (b->pressed & bit) {
            ui_edit_change(e, f ? 1 : -1);
            e->repeat_us[f] = now + EDIT_REPEAT_DELAY_US;
        } else if ((b->held & bit) && now >= e->repeat_us[f]) {
            ui_edit_change(e, f ? 1 : -1);
            e->repeat_us[f] = now + EDIT_REPEAT_US;
        }
    }
    return UI_EDIT_EDITING;
}


void ui_edit_render(uint8_t *fb, const ui_edit_t *e, const char *title, const char *prompt) {
    ui_title(fb, title);
    int y = ui_lines(fb, UI_TITLE_H + 4, &gfx_font_small, prompt) + 2;
    /* A window of cells around the cursor, the current one inverted */
    const int cell = 17, n_cells = 11, x0 = (GFX_WIDTH - n_cells * cell) / 2, h = gfx_font_medium.height + 4;
    int first = e->cursor - n_cells / 2;
    if (first > e->max_len - n_cells)
        first = e->max_len - n_cells;
    if (first < 0)
        first = 0;
    for (int i = 0; i < n_cells && first + i < e->max_len; ++i) {
        int x = x0 + i * cell, k = first + i;
        char c[2] = {e->text[k], 0};
        if (k == e->cursor) {
            gfx_fill_rect(fb, x, y, cell - 2, h, GFX_BLACK);
            gfx_text(fb, x + (cell - 2) / 2, y + 2, &gfx_font_medium, c, GFX_WHITE, GFX_ALIGN_CENTER);
        } else {
            gfx_text(fb, x + (cell - 2) / 2, y + 2, &gfx_font_medium, c, GFX_BLACK, GFX_ALIGN_CENTER);
            gfx_fill_rect(fb, x + 2, y + h - 2, cell - 6, 1, GFX_BLACK);
        }
    }
    if (first > 0)
        gfx_text(fb, x0 - 6, y + 2, &gfx_font_small, "<", GFX_BLACK, GFX_ALIGN_CENTER);
    if (first + n_cells < e->max_len)
        gfx_text(fb, x0 + n_cells * cell + 3, y + 2, &gfx_font_small, ">", GFX_BLACK, GFX_ALIGN_CENTER);
    y += h + 4;
    /* The whole text */
    char text[UI_EDIT_MAX + 1], fitted[UI_EDIT_MAX + 4];
    ui_edit_result(e, text, sizeof(text));
    ui_fit(&gfx_font_small, fitted, sizeof(fitted), text[0] ? text : "(vide)", GFX_WIDTH - 8);
    gfx_text(fb, GFX_WIDTH/2, y, &gfx_font_small, fitted, GFX_BLACK, GFX_ALIGN_CENTER);
    y += gfx_font_small.height + 6;
    /* The help, as many lines as fit above the footer (a long prompt leaves less room) */
    static const char *HELP[] = {"Flancs : lettre (maintenir : vite)", "D : suivante  G : précédente",
                                 "Espace = effacer"};
    for (int i = 0; i < (int)(sizeof(HELP) / sizeof(HELP[0])) && y + gfx_font_small.height <= UI_FOOTER_Y - 3; ++i)
        y = ui_lines(fb, y, &gfx_font_small, HELP[i]);
    ui_footer(fb, e->cursor == 0 ? "G : annuler  D long : valider" : "D long : valider");
}
