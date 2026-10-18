/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Badge > Badge conférence: the program of any event, from the SD card. CONFERENCES/ holds a folder per event, with
 * text files (format in docs/fr/badge_conference.md, examples in docs/sd/CONFERENCES):
 * - infos.txt: nom, dates, lieu, adresse, site, description;
 * - conferences.txt, workshops.txt: jour (YYYY-MM-DD), heure, fin, salle, titre, orateur, resume, lien;
 * - exposants.txt: nom, stand, description, lien.
 * Blocks of "key: value" lines separated by an empty line, '#' lines are comments.
 *
 * The pages: the events (the "nom" of their infos.txt), the files of an event, the items of a file (offsets of the
 * blocks, read once), an item (its block read again from the card), the QR code of its link. Everything lives in
 * shared_buf (the page is alone on the screen); a file is open only while being read. */

#include <stdio.h>
#include <string.h>

#include "app.h"
#include "ff.h"
#include "i18n.h"
#include "score_code.h"
#include "sd.h"
#include "shared_buf.h"

#define CONF_DIR "CONFERENCES"
#define MAX_EVENTS 16
#define MAX_ITEMS 100
#define LABEL_LEN 48
#define LINE_LEN 480
#define BLOCK_LEN 1280
#define N_FILES 4
#define PATH_LEN (sizeof(CONF_DIR) + SD_NAME_MAX + 20)

static const char *const FILES[N_FILES] = {"infos.txt", "conferences.txt", "workshops.txt", "exposants.txt"};
static const char *const FILE_NAMES[N_FILES] = {N_("Infos"), N_("Conférences (%d)"), N_("Workshops (%d)"),
                                                N_("Exposants (%d)")};

typedef struct {
    FIL file;
    char chunk[512];  /* The reader of lines */
    uint32_t chunk_pos;  /* Offset of chunk[0] in the file */
    int chunk_len, chunk_at;
    char line[LINE_LEN];
    char dirs[MAX_EVENTS][SD_NAME_MAX];
    char names[MAX_EVENTS][LABEL_LEN];
    uint32_t offsets[MAX_ITEMS];  /* The blocks of the file shown */
    char labels[MAX_ITEMS][LABEL_LEN];
    char block[BLOCK_LEN];  /* The item shown: its lines, each one ended by a 0 */
    int block_len;
} conf_t;

_Static_assert(sizeof(conf_t) <= SHARED_BUF_SIZE, "conf_t does not fit in shared_buf");
#define C ((conf_t *)shared_buf)

typedef enum { V_EVENTS, V_EVENT, V_LIST, V_ITEM, V_QR } view_t;

static view_t view;
static bool no_card;
static int n_events, event_sel;
static int counts[N_FILES];  /* Blocks of each file of the event, -1: no file */
static int rows[N_FILES], n_rows, row_sel;  /* The files present */
static int cur_file, n_items, item_sel;
static view_t qr_back;

/* ------ Reading the card ------ */

static void path_of(char *path, int event, int f) {
    snprintf(path, PATH_LEN, "%s/%s/%s", CONF_DIR, C->dirs[event], FILES[f]);
}

static bool reader_open(const char *path) {
    if (sd_mount() != FR_OK)
        return false;
    if (f_open(&C->file, path, FA_READ) != FR_OK)
        return false;
    C->chunk_pos = 0;
    C->chunk_len = C->chunk_at = 0;
    return true;
}

static void reader_seek(uint32_t offset) {
    f_lseek(&C->file, offset);
    C->chunk_pos = offset;
    C->chunk_len = C->chunk_at = 0;
}

/* The next line into C->line (without "\r\n", cut to LINE_LEN - 1 bytes): its length, -1 at the end of the file;
 * *start: its offset */
static int reader_line(uint32_t *start) {
    int n = 0;
    bool any = false;
    *start = C->chunk_pos + C->chunk_at;
    for (;;) {
        if (C->chunk_at >= C->chunk_len) {
            C->chunk_pos += C->chunk_len;
            UINT got = 0;
            if (f_read(&C->file, C->chunk, sizeof(C->chunk), &got) != FR_OK || ! got)
                break;
            C->chunk_len = got;
            C->chunk_at = 0;
        }
        char c = C->chunk[C->chunk_at++];
        any = true;
        if (c == '\n')
            break;
        if (c != '\r' && n < LINE_LEN - 1)
            C->line[n++] = c;
    }
    C->line[n] = 0;
    if (! strncmp(C->line, "\xEF\xBB\xBF", 3)) {  /* UTF-8 BOM */
        memmove(C->line, C->line + 3, n - 2);
        n -= 3;
    }
    return any ? n : -1;
}

/* The value of a "key: value" line, NULL when the line has another key */
static const char *value_of(const char *line, const char *key) {
    size_t n = strlen(key);
    if (strncmp(line, key, n) || line[n] != ':')
        return NULL;
    line += n + 1;
    while (*line == ' ')
        ++line;
    return line;
}

/* Copy cut on a UTF-8 character */
static void copy_text(char *dst, size_t len, const char *src) {
    size_t n = strlen(src);
    if (n >= len) {
        n = len - 1;
        while (n && ((unsigned char)src[n] & 0xC0) == 0x80)
            --n;
    }
    memcpy(dst, src, n);
    dst[n] = 0;
}

/* The blocks of a file: their number (-1: no file); with \p index, their offsets and labels into C */
static int scan_file(int event, int f, bool index) {
    char path[PATH_LEN];
    path_of(path, event, f);
    if (! reader_open(path))
        return -1;
    int n = 0;
    bool in_block = false;
    char day[6] = "", hour[6] = "", title[LABEL_LEN] = "";
    uint32_t start;
    for (;;) {
        int len = reader_line(&start);
        if (len > 0 && C->line[0] == '#')
            continue;
        if (len > 0 && ! in_block) {
            in_block = true;
            day[0] = hour[0] = title[0] = 0;
            if (index && n < MAX_ITEMS)
                C->offsets[n] = start;
        }
        if (len > 0 && index) {
            const char *v;
            if ((v = value_of(C->line, "jour")) && strlen(v) >= 10)
                snprintf(day, sizeof(day), "%.2s/%.2s", v + 8, v + 5);  /* YYYY-MM-DD -> DD/MM */
            else if ((v = value_of(C->line, "heure")))
                copy_text(hour, sizeof(hour), v);
            else if ((v = value_of(C->line, "titre")) || (v = value_of(C->line, "nom")))
                copy_text(title, sizeof(title), v);
        }
        if (len <= 0 && in_block) {  /* The end of a block */
            in_block = false;
            if (index && n < MAX_ITEMS) {
                char label[LABEL_LEN * 2];
                snprintf(label, sizeof(label), "%s%s%s%s%s", day, day[0] ? " " : "", hour, hour[0] ? " " : "", title);
                copy_text(C->labels[n], LABEL_LEN, label);
            }
            if (n < MAX_ITEMS)
                ++n;
        }
        if (len < 0)
            break;
    }
    f_close(&C->file);
    return n;
}

/* The block of item \p i of the file shown into C->block */
static bool load_item(int i) {
    char path[PATH_LEN];
    path_of(path, event_sel, cur_file);
    C->block_len = 0;
    if (! reader_open(path))
        return false;
    reader_seek(C->offsets[i]);
    uint32_t start;
    int len;
    while ((len = reader_line(&start)) > 0) {
        if (C->line[0] == '#')
            continue;
        if (C->block_len + len + 1 > BLOCK_LEN)
            break;
        memcpy(C->block + C->block_len, C->line, len + 1);
        C->block_len += len + 1;
    }
    f_close(&C->file);
    return true;
}

/* A field of the item shown, "" when it has none */
static const char *field(const char *key) {
    for (int at = 0; at < C->block_len; at += strlen(C->block + at) + 1) {
        const char *v = value_of(C->block + at, key);
        if (v)
            return v;
    }
    return "";
}

static void scan_events(void) {
    n_events = 0;
    no_card = sd_mount() != FR_OK;
    if (no_card)
        return;
    n_events = (int)sd_list_dirs(CONF_DIR, C->dirs, MAX_EVENTS);
    for (int i = 0; i < n_events; ++i) {
        copy_text(C->names[i], LABEL_LEN, C->dirs[i]);  /* No infos.txt or no "nom": the name of the folder */
        char path[PATH_LEN];
        path_of(path, i, 0);
        if (! reader_open(path))
            continue;
        uint32_t start;
        const char *v;
        while (reader_line(&start) >= 0)
            if ((v = value_of(C->line, "nom")) && *v) {
                copy_text(C->names[i], LABEL_LEN, v);
                break;
            }
        f_close(&C->file);
    }
}

static void open_event(int i) {
    event_sel = i;
    n_rows = 0;
    for (int f = 0; f < N_FILES; ++f) {
        counts[f] = scan_file(i, f, false);
        if (counts[f] > 0)
            rows[n_rows++] = f;
    }
    row_sel = 0;
    view = V_EVENT;
}

static void open_file(int f) {
    cur_file = f;
    if (f == 0) {  /* Infos: a single item */
        n_items = scan_file(event_sel, f, true);
        item_sel = 0;
        if (n_items > 0 && load_item(0))
            view = V_ITEM;
        return;
    }
    n_items = scan_file(event_sel, f, true);
    item_sel = 0;
    if (n_items > 0)
        view = V_LIST;
}

/* ------ The page ------ */

static const char *link_of_item(void) {
    return cur_file == 0 ? field("site") : field("lien");
}

static void conf_start(absolute_time_t now) {
    (void)now;
    view = V_EVENTS;
    event_sel = 0;
    scan_events();
}

static void move(int *sel, int count, const app_buttons_t *b) {
    if (! count)
        return;
    if (b->pressed & UI_BTN_Y)
        *sel = (*sel + count - 1) % count;
    if (b->pressed & UI_BTN_X)
        *sel = (*sel + 1) % count;
}

static bool conf_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    switch (view) {
    case V_EVENTS:
        if (b->pressed & UI_BTN_A)
            return false;
        move(&event_sel, n_events, b);
        if ((b->pressed & UI_BTN_B) && n_events)
            open_event(event_sel);
        break;
    case V_EVENT:
        if (b->pressed & UI_BTN_A)
            view = V_EVENTS;
        move(&row_sel, n_rows, b);
        if ((b->pressed & UI_BTN_B) && n_rows)
            open_file(rows[row_sel]);
        break;
    case V_LIST:
        if (b->pressed & UI_BTN_A)
            view = V_EVENT;
        move(&item_sel, n_items, b);
        if ((b->pressed & UI_BTN_B) && n_items && load_item(item_sel))
            view = V_ITEM;
        break;
    case V_ITEM:
        if (b->pressed & UI_BTN_A)
            view = cur_file == 0 ? V_EVENT : V_LIST;
        if (cur_file && (b->pressed & (UI_BTN_X | UI_BTN_Y))) {  /* The previous or next item */
            move(&item_sel, n_items, b);
            load_item(item_sel);
        }
        if ((b->pressed & UI_BTN_B) && link_of_item()[0]) {
            qr_back = view;
            view = V_QR;
        }
        break;
    case V_QR:
        if (b->pressed & (UI_BTN_A | UI_BTN_B))
            view = qr_back;
        break;
    }
    return true;
}

static void event_label(int i, char *buf, size_t len) {
    snprintf(buf, len, "%s", C->names[i]);
}

static void row_label(int i, char *buf, size_t len) {
    int f = rows[i];
    snprintf(buf, len, tr(FILE_NAMES[f]), counts[f]);
}

static void item_label(int i, char *buf, size_t len) {
    snprintf(buf, len, "%s", C->labels[i]);
}

/* A line of the small font, centered, scrolling when too wide; returns the y after it */
static int small_line(uint8_t *fb, int y, const char *text) {
    if (! text[0])
        return y;
    ui_scroll_text(fb, GFX_WIDTH / 2, y, GFX_WIDTH - 8, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
    return y + gfx_font_small.height + 1;
}

/* The rest of the page for a long text (it scrolls a line at a time) */
static void small_text(uint8_t *fb, int y, const char *text) {
    int lines = (UI_FOOTER_Y - 2 - y) / gfx_font_small.height;
    if (text[0] && lines > 0)
        ui_wrapped(fb, y, &gfx_font_small, text, lines);
}

static void render_item(uint8_t *fb) {
    char head[48];
    int y = UI_TITLE_H + 2;
    if (cur_file == 0) {  /* Infos */
        ui_title(fb, field("nom")[0] ? field("nom") : C->names[event_sel]);
        y = small_line(fb, y, field("dates"));
        y = small_line(fb, y, field("lieu"));
        y = small_line(fb, y, field("adresse"));
        small_text(fb, y + 2, field("description"));
    } else if (cur_file == 3) {  /* An exhibitor */
        const char *stand = field("stand");
        if (stand[0]) {
            snprintf(head, sizeof(head), _("Stand %s"), stand);
            ui_title(fb, head);
        } else {
            ui_title(fb, N_("Exposant"));
        }
        y = ui_wrapped(fb, y, &gfx_font_medium, field("nom"), 2);
        small_text(fb, y + 2, field("description"));
    } else {  /* A talk or a workshop */
        /* The title bar: "DD/MM HH:MM-HH:MM" */
        const char *day = field("jour"), *hour = field("heure"), *end = field("fin");
        if (strlen(day) >= 10)
            snprintf(head, sizeof(head), "%.2s/%.2s %s%s%s", day + 8, day + 5, hour, end[0] ? "-" : "", end);
        else
            snprintf(head, sizeof(head), "%s%s%s", hour, end[0] ? "-" : "", end);
        ui_title(fb, head);
        y = ui_wrapped(fb, y, &gfx_font_medium, field("titre"), 2);
        y = small_line(fb, y + 1, field("orateur"));
        y = small_line(fb, y, field("salle"));
        small_text(fb, y + 2, field("resume"));
    }
    if (cur_file == 0)
        ui_footer(fb, link_of_item()[0] ? N_("A : retour  B : QR code") : N_("A : retour"));
    else
        ui_footer(fb, link_of_item()[0] ? N_("Y X : suivant  A : retour  B : QR") : N_("Y X : suivant  A : retour"));
}

static void render_qr(uint8_t *fb) {
    const char *link = link_of_item();
    ui_title(fb, N_("Lien"));
    int y = UI_TITLE_H + 2;
    int size = score_code_draw(NULL, link, 0, 1);
    int room = UI_FOOTER_Y - 4 - gfx_font_small.height - y;
    int scale = size ? room / size : 0;
    if (scale > 4)
        scale = 4;
    if (scale >= 1)
        score_code_draw(fb, link, y + (room - size * scale) / 2, scale);
    else
        ui_lines(fb, y + 20, &gfx_font_small, N_("(QR code trop grand)"));
    ui_scroll_text(fb, GFX_WIDTH / 2, UI_FOOTER_Y - 2 - gfx_font_small.height, GFX_WIDTH - 8, &gfx_font_small, link,
                   GFX_BLACK, GFX_ALIGN_CENTER);
    ui_footer(fb, N_("A : retour"));
}

static void conf_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    switch (view) {
    case V_EVENTS:
        ui_title(fb, N_("Badge conférence"));
        if (n_events) {
            ui_list(fb, n_events, event_sel, event_label);
            ui_footer(fb, N_("A : retour  B : ouvrir"));
        } else {
            ui_wrapped(fb, UI_TITLE_H + 20, &gfx_font_small, no_card ? N_("Pas de carte SD.") :
                       N_("Aucun événement : copier un dossier par événement dans CONFERENCES sur la carte SD."), 6);
            ui_footer(fb, N_("A : retour"));
        }
        break;
    case V_EVENT:
        ui_title(fb, C->names[event_sel]);
        if (n_rows) {
            ui_list(fb, n_rows, row_sel, row_label);
            ui_footer(fb, N_("A : retour  B : ouvrir"));
        } else {
            ui_wrapped(fb, UI_TITLE_H + 20, &gfx_font_small, N_("Ce dossier ne contient aucun fichier lisible."), 4);
            ui_footer(fb, N_("A : retour"));
        }
        break;
    case V_LIST:
        ui_title(fb, C->names[event_sel]);
        ui_list(fb, n_items, item_sel, item_label);
        ui_footer(fb, N_("A : retour  B : ouvrir"));
        break;
    case V_ITEM:
        render_item(fb);
        break;
    case V_QR:
        render_qr(fb);
        break;
    }
}

const app_t app_conference = {
    .name = N_("Badge conférence"),
    .start = conf_start,
    .buttons = conf_buttons,
    .render = conf_render,
    .no_saver = true,
};
