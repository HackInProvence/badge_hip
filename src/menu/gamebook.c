/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Livres-jeux: the "livres dont vous êtes le héros". The books are text files (format in gamebook_parse.h and
 * docs/fr/livres_jeux.md): the ones built in the firmware (gamebook_builtin.c) and the .txt files of the LIVRES
 * folder of the SD card. A book is indexed once (offsets of its sections), then only the current section is read.
 *
 * The page of a section: its text cut in lines, then the notes (die, items), then the choices, all paginated by 8
 * lines; the flanks go through the pages and the choices, D takes the selected choice, G goes to the menu of the book.
 * The progress (book, section, items) is saved in the store: "Continuer" at the top of the list of the books. */

#include <stdio.h>
#include <string.h>

#include "pico/rand.h"

#include "achievements.h"
#include "app.h"
#include "ff.h"
#include "gamebook_builtin.h"
#include "gamebook_parse.h"
#include "i18n.h"
#include "sd.h"
#include "store.h"

#define BOOK_DIR "LIVRES"
#define MAX_SD_BOOKS 16
#define MAX_BUILTIN 4
#define MAX_BOOKS (MAX_BUILTIN + MAX_SD_BOOKS)
#define LIST_TITLE_LEN 40

/* The reading page (tools/gamebook_check.py --pages uses the same numbers) */
#define HEADER_H 20
#define TEXT_X 4
#define TEXT_W (GFX_WIDTH - 2 * TEXT_X)
#define INDENT 10  /* First line of a paragraph */
#define CHOICE_X 16  /* After the "> " of a choice */
#define LINE_H 19
#define TEXT_Y (HEADER_H + 3)
#define PAGE_LINES 8
#define MAX_LINES 160
#define MAX_PAGES 40
#define NOTE_LEN 160

/* ------ The books ------ */

typedef struct {
    int8_t builtin;  /* Index in gamebook_builtins, -1: the SD card */
    int8_t sd;  /* Index in sd_names */
    uint32_t key;  /* 24 bits hash of its path: the saved progress (store.h) */
    char title[LIST_TITLE_LEN];
} book_entry_t;

static book_entry_t books[MAX_BOOKS];
static int n_books = 0;
static char sd_names[MAX_SD_BOOKS][SD_NAME_MAX];

static gb_src_t src;
static FIL file;
static bool file_open = false;

static gb_book_t book;  /* The index of the book open */
static int cur_book = -1;  /* Its index in books */
static gb_section_t sec;  /* The section shown */
static bool sec_valid = false;  /* sec is a section of the book (not an error page) */
static uint8_t items = 0;  /* Bit n: item n of the book held */
static uint8_t roll = 0;  /* Die of the section, 0 = none */
static uint16_t last_good = 0, prev_good = 0;  /* The section shown before */
static char note[NOTE_LEN];

static int read_sd(void *ctx, uint32_t offset, void *buf, int len) {
    (void)ctx;
    UINT n = 0;
    if (f_lseek(&file, offset) != FR_OK || f_read(&file, buf, len, &n) != FR_OK)
        return -1;
    return (int)n;
}

static int read_builtin(void *ctx, uint32_t offset, void *buf, int len) {
    const gamebook_builtin_t *b = ctx;
    if (offset >= b->len)
        return 0;
    if ((uint32_t)len > b->len - offset)
        len = b->len - offset;
    memcpy(buf, b->text + offset, len);
    return len;
}

static void book_path(int i, char *path, size_t len) {
    if (books[i].builtin >= 0)
        snprintf(path, len, "%s", gamebook_builtins[books[i].builtin].key);
    else
        snprintf(path, len, "%s/%s", BOOK_DIR, sd_names[books[i].sd]);
}

/* The file is only open while reading (the card may be removed between two sections) */
static bool open_source(int i) {
    if (books[i].builtin >= 0) {
        gb_src_init(&src, read_builtin, (void *)&gamebook_builtins[books[i].builtin]);
        return true;
    }
    char path[SD_NAME_MAX + 8];
    book_path(i, path, sizeof(path));
    if (sd_mount() != FR_OK)
        return false;
    if (f_open(&file, path, FA_READ) != FR_OK) {
        sd_unmount();
        return false;
    }
    file_open = true;
    gb_src_init(&src, read_sd, NULL);
    return true;
}

static void close_source(void) {
    if (file_open)
        f_close(&file);
    file_open = false;
}

static uint32_t key_of(const char *path) {
    uint32_t h = gb_hash(path) & 0xFFFFFF;
    return h ? h : 1;  /* 0 = no progress */
}

static void scan_books(void) {
    char path[SD_NAME_MAX + 8];
    n_books = 0;
    for (int i = 0; i < gamebook_builtin_count && i < MAX_BUILTIN; ++i) {
        book_entry_t *b = &books[n_books++];
        b->builtin = i;
        b->sd = -1;
        b->key = key_of(gamebook_builtins[i].key);
        gb_src_init(&src, read_builtin, (void *)&gamebook_builtins[i]);
        if (! gb_read_title(&src, b->title, sizeof(b->title)))
            snprintf(b->title, sizeof(b->title), _("Livre intégré %d"), i + 1);
    }
    int n_sd = sd_mount() == FR_OK ? (int)sd_list_files(BOOK_DIR, ".TXT", sd_names, MAX_SD_BOOKS) : 0;
    for (int i = 0; i < n_sd; ++i) {
        book_entry_t *b = &books[n_books];
        b->builtin = -1;
        b->sd = i;
        book_path(n_books, path, sizeof(path));
        b->key = key_of(path);
        b->title[0] = 0;
        if (open_source(n_books)) {
            gb_read_title(&src, b->title, sizeof(b->title));
            close_source();
        }
        if (! b->title[0]) {
            /* No title: the name of the file without ".txt" */
            gb_normalize(sd_names[i], strlen(sd_names[i]), b->title, sizeof(b->title));  /* Not cut in a letter */
            char *dot = strrchr(b->title, '.');
            if (dot)
                *dot = 0;
        }
        ++n_books;
    }
    printf("gamebook: %d books (%d on the SD card)\n", n_books, n_sd);
}

/* The book of the saved progress, -1 when none */
static int saved_book(void) {
    const store_t *s = store_get();
    if (! s->book_section)
        return -1;
    for (int i = 0; i < n_books; ++i)
        if (books[i].key == (s->book_hash & 0xFFFFFF))
            return i;
    return -1;
}

static bool index_book(int i) {
    if (cur_book == i)
        return true;
    cur_book = -1;
    sec_valid = false;
    bool ok = open_source(i) && gb_index(&book, &src);
    close_source();
    if (! ok) {
        printf("gamebook: can't open \"%s\"\n", books[i].title);
        return false;
    }
    cur_book = i;
    printf("gamebook: open \"%s\" by \"%s\": %u sections%s, start %u, %u items\n", book.title, book.author,
           book.n_sections, book.too_many ? " (too many)" : "", book.start, book.n_items);
    return true;
}

static void save_progress(void) {
    store_t *s = store_get();
    uint32_t h = books[cur_book].key | (uint32_t)items << 24;
    if (s->book_hash != h || s->book_section != sec.number) {
        s->book_hash = h;
        s->book_section = sec.number;
        store_changed();
    }
}


/* ------ The reading page ------ */

enum { K_TEXT, K_NOTE, K_GAP, K_OPT, K_OPT_CONT, K_END };  /* Kinds of lines */
enum { OPT_CHOICE, OPT_BACK, OPT_RESTART, OPT_LIST };  /* Kinds of options */

typedef struct {
    uint8_t kind;
    uint8_t choice;  /* OPT_CHOICE: index in sec.choices */
    uint16_t target;  /* OPT_BACK */
} opt_t;

static opt_t opts[GB_MAX_CHOICES + 2];
static int n_opts = 0;
static gb_line_t lines[MAX_LINES];
static uint8_t line_kind[MAX_LINES];
static int8_t line_opt[MAX_LINES];
static int n_lines = 0;
static uint8_t page_start[MAX_PAGES + 1];
static int n_pages = 0, page = 0, sel = -1;

static const char *opt_label(int i) {
    switch (opts[i].kind) {
    case OPT_CHOICE: return sec.choices[opts[i].choice].label;
    case OPT_BACK: return _("Revenir en arrière");
    case OPT_RESTART: return _("Recommencer");
    default: return _("Autres livres");
    }
}

static const char *end_text(int i) {
    if (i == 0)
        return N_("~ FIN ~");
    return sec.end == GB_END_WIN ? N_("Bravo, vous avez gagné !")
           : sec.end == GB_END_LOSE ? N_("Perdu... Réessayez !") : N_("Merci d'avoir joué !");
}

/* The text of a line */
static const char *line_text(int i) {
    switch (line_kind[i]) {
    case K_TEXT: return sec.text + lines[i].start;
    case K_NOTE: return note + lines[i].start;
    case K_OPT: case K_OPT_CONT: return opt_label(line_opt[i]) + lines[i].start;
    case K_END: return end_text(lines[i].start);
    default: return "";
    }
}

static int measure(const char *s, int n, void *ctx) {
    char tmp[GB_CHOICE_LEN + 64];
    if (n > (int)sizeof(tmp) - 1)
        n = sizeof(tmp) - 1;
    memcpy(tmp, s, n);
    tmp[n] = 0;
    return gfx_text_width(ctx, tmp);
}

static void add_wrapped(const char *text, int width, int indent, uint8_t kind, int opt) {
    int n = gb_wrap(text, width, indent, measure, (void *)&gfx_font_small, lines + n_lines, MAX_LINES - n_lines);
    for (int i = 0; i < n; ++i) {
        line_kind[n_lines + i] = kind == K_OPT && i ? K_OPT_CONT : kind;
        line_opt[n_lines + i] = opt;
    }
    n_lines += n;
}

static void add_line(uint8_t kind, uint16_t start) {
    if (n_lines >= MAX_LINES)
        return;
    lines[n_lines].start = start;
    lines[n_lines].len = 0;
    lines[n_lines].first = false;
    line_kind[n_lines] = kind;
    line_opt[n_lines] = -1;
    ++n_lines;
}

/* Lines glued to the previous one (a choice, the end banner) stay on its page */
static bool glued(int i) {
    return line_kind[i] == K_OPT_CONT || (line_kind[i] == K_END && lines[i].start > 0);
}

static void paginate(void) {
    n_pages = 0;
    int i = 0;
    while (i < n_lines && n_pages < MAX_PAGES) {
        while (i < n_lines && line_kind[i] == K_GAP)
            ++i;  /* No empty line at the top of a page */
        if (i >= n_lines)
            break;
        page_start[n_pages++] = i;
        int count = 0;
        while (i < n_lines) {
            int block = 1;
            while (i + block < n_lines && glued(i + block))
                ++block;
            if (count && count + block > PAGE_LINES)
                break;
            if (block > PAGE_LINES)
                block = PAGE_LINES;
            count += block;
            i += block;
        }
    }
    page_start[n_pages] = i;
}

/* The options shown on page \p p, false when none */
static bool page_opts(int p, int *first, int *last) {
    *first = *last = -1;
    for (int i = page_start[p]; i < page_start[p + 1]; ++i)
        if (line_kind[i] == K_OPT) {
            if (*first < 0)
                *first = line_opt[i];
            *last = line_opt[i];
        }
    return *first >= 0;
}

static void set_page(int p, bool from_below) {
    int first, last;
    page = p;
    sel = page_opts(p, &first, &last) ? (from_below ? last : first) : -1;
}

static void build_page(void) {
    /* The options */
    n_opts = 0;
    if (sec_valid && ! sec.end)
        for (int i = 0; i < sec.n_choices; ++i)
            if (gb_choice_available(&sec.choices[i], items, roll))
                opts[n_opts++] = (opt_t){OPT_CHOICE, i, 0};
    if (! n_opts) {
        /* An ending, a missing section or no choice possible with these items */
        uint16_t back = sec_valid ? prev_good : last_good;
        if (! sec.end && back && back != sec.number)
            opts[n_opts++] = (opt_t){OPT_BACK, 0, back};
        opts[n_opts++] = (opt_t){OPT_RESTART, 0, 0};
        if (sec.end)
            opts[n_opts++] = (opt_t){OPT_LIST, 0, 0};
    }

    /* The lines */
    n_lines = 0;
    add_wrapped(sec.text, TEXT_W, INDENT, K_TEXT, -1);
    if (note[0]) {
        add_line(K_GAP, 0);
        add_wrapped(note, TEXT_W, 0, K_NOTE, -1);
    }
    if (sec.end) {
        add_line(K_GAP, 0);
        add_line(K_END, 0);
        add_line(K_END, 1);
    }
    add_line(K_GAP, 0);
    for (int i = 0; i < n_opts; ++i)
        add_wrapped(opt_label(i), GFX_WIDTH - CHOICE_X - 4, 0, K_OPT, i);
    paginate();
    set_page(0, false);
}

static void append_note(const char *text) {
    size_t n = strlen(note);
    snprintf(note + n, sizeof(note) - n, "%s%s", n ? "\n" : "", text);
}

/* Items of \p bits as "antenne, carte" */
static void item_names(uint8_t bits, char *buf, size_t len) {
    buf[0] = 0;
    for (int i = 0; i < book.n_items; ++i)
        if (bits & (1u << i)) {
            size_t n = strlen(buf);
            snprintf(buf + n, len - n, "%s%s", n ? ", " : "", book.items[i]);
        }
}

/* Shows section \p number; \p by_choice: reached by a choice (an ending counts), not by resuming */
static void enter_section(uint16_t number, bool by_choice) {
    uint16_t from = last_good;
    bool ok = open_source(cur_book);
    ok = ok && gb_load(&book, &src, number, &sec);
    close_source();
    note[0] = 0;
    roll = 0;
    sec_valid = ok;
    if (! ok) {
        bool missing = gb_find(&book, number) < 0;
        snprintf(sec.text, sizeof(sec.text),
                 missing ? _("La section %u est introuvable : le livre est incomplet.")
                         : _("Impossible de lire la section %u. La carte SD a-t-elle été retirée ?"),
                 number);
        sec.number = number;
        sec.end = GB_END_NONE;
        sec.n_choices = 0;
        printf("gamebook: section %u %s\n", number, missing ? "missing" : "unreadable");
    } else {
        prev_good = from;
        last_good = number;
        char names[64], text[96];
        uint8_t got = sec.gain & ~items, lost = sec.lose & items;
        items = (items | sec.gain) & ~sec.lose;
        if (sec.has_dice) {
            roll = 1 + get_rand_32() % 6;
            snprintf(text, sizeof(text), _("Le dé roule... et donne %u."), roll);
            append_note(text);
        }
        if (got) {
            item_names(got, names, sizeof(names));
            snprintf(text, sizeof(text), _("Vous obtenez : %s."), names);
            append_note(text);
        }
        if (lost) {
            item_names(lost, names, sizeof(names));
            snprintf(text, sizeof(text), _("Vous perdez : %s."), names);
            append_note(text);
        }
        if (sec.text_cut)
            append_note(_("(Texte trop long, coupé.)"));
        printf("gamebook: section %u, %u choices, items 0x%02x%s\n", number, sec.n_choices, items,
               roll ? ", die rolled" : "");
        save_progress();
        if (sec.end) {
            printf("gamebook: ending %u (%s)%s\n", number,
                   sec.end == GB_END_WIN ? "won" : sec.end == GB_END_LOSE ? "lost" : "neutral", by_choice ? "" : ", resumed");
            if (by_choice) {
                achv_unlock(ACHV_BOOK_END);
                achv_add(ACHV_CNT_BOOK_ENDS, 1);
                app_tone(sec.end == GB_END_LOSE ? 330 : 1319, 150);
            }
        }
    }
    build_page();
}

static void restart(void) {
    items = 0;
    last_good = prev_good = 0;
    enter_section(book.start, true);
}

static void resume(void) {
    const store_t *s = store_get();
    items = (uint8_t)(s->book_hash >> 24);
    last_good = prev_good = 0;
    enter_section(s->book_section, false);
}


/* ------ The pages of the application ------ */

typedef enum { V_LIST, V_MENU, V_READ, V_ITEMS, V_MESSAGE } view_t;
enum { M_RESUME, M_START, M_RESTART, M_ITEMS, M_LIST };  /* Rows of the menu of a book */

static view_t view = V_LIST;
static int list_sel = 0;
static int list_resume = -1;  /* The book of the "Continuer" row (row 0), -1 = no such row */
static uint8_t menu_rows[5];
static int n_menu = 0, menu_sel = 0;
static char message[80];

static int list_count(void) {
    return n_books + (list_resume >= 0);
}

static void list_label(int i, char *buf, size_t len) {
    if (list_resume >= 0) {
        if (i == 0) {
            snprintf(buf, len, _("Continuer : %s"), books[list_resume].title);
            return;
        }
        --i;
    }
    snprintf(buf, len, "%s", books[i].title);
}

static void show_list(void) {
    list_resume = saved_book();
    if (list_sel >= list_count())
        list_sel = 0;
    view = V_LIST;
}

static bool has_progress(void) {
    return saved_book() == cur_book && cur_book >= 0;
}

static void show_menu(void) {
    n_menu = 0;
    if (has_progress()) {
        menu_rows[n_menu++] = M_RESUME;
        menu_rows[n_menu++] = M_RESTART;
    } else {
        menu_rows[n_menu++] = M_START;
    }
    if (book.n_items)
        menu_rows[n_menu++] = M_ITEMS;
    menu_rows[n_menu++] = M_LIST;
    menu_sel = 0;
    view = V_MENU;
}

static const char *menu_label(int row) {
    switch (row) {
    case M_RESUME: return N_("Reprendre la lecture");
    case M_START: return N_("Commencer");
    case M_RESTART: return N_("Recommencer");
    case M_ITEMS: return N_("Objets");
    default: return N_("Autres livres");
    }
}

static void open_failed(void) {
    snprintf(message, sizeof(message), N_("Ce livre est illisible\nou n'a aucune section.\n(une ligne \"== 1\")"));
    view = V_MESSAGE;
}

static void gamebook_start(absolute_time_t now) {
    (void)now;
    cur_book = -1;  /* The card may have changed */
    sec_valid = false;
    list_sel = 0;
    scan_books();
    show_list();
}

static void take_option(int i) {
    switch (opts[i].kind) {
    case OPT_CHOICE:
        enter_section(sec.choices[opts[i].choice].target, true);
        break;
    case OPT_BACK:
        enter_section(opts[i].target, false);
        break;
    case OPT_RESTART:
        restart();
        break;
    default:
        show_list();
        break;
    }
}

static void read_buttons(const app_buttons_t *b) {
    int first, last;
    page_opts(page, &first, &last);
    if (b->pressed & UI_BTN_X) {
        if (sel >= 0 && sel < last)
            ++sel;
        else if (page < n_pages - 1)
            set_page(page + 1, false);
    }
    if (b->pressed & UI_BTN_Y) {
        if (sel >= 0 && sel > first)
            --sel;
        else if (page > 0)
            set_page(page - 1, true);
    }
    if (b->pressed & UI_BTN_B) {
        if (sel >= 0)
            take_option(sel);
        else if (page < n_pages - 1)
            set_page(page + 1, false);
    }
    if (b->released_short & UI_BTN_A)
        show_menu();
}

static bool gamebook_buttons(const app_buttons_t *b, absolute_time_t now) {
    (void)now;
    if (b->long_pressed & UI_BTN_A)
        return false;
    bool back = b->released_short & UI_BTN_A;
    switch (view) {
    case V_LIST:
        if (back)
            return false;
        if (b->pressed & UI_BTN_X)
            list_sel = (list_sel + 1) % list_count();
        if (b->pressed & UI_BTN_Y)
            list_sel = (list_sel + list_count() - 1) % list_count();
        if (b->pressed & UI_BTN_B) {
            bool resuming = list_resume >= 0 && list_sel == 0;
            int i = resuming ? list_resume : list_sel - (list_resume >= 0);
            if (! index_book(i))
                open_failed();
            else if (resuming) {
                resume();
                view = V_READ;
            } else
                show_menu();
        }
        break;
    case V_MENU:
        if (back) {
            show_list();
            break;
        }
        if (b->pressed & UI_BTN_X)
            menu_sel = (menu_sel + 1) % n_menu;
        if (b->pressed & UI_BTN_Y)
            menu_sel = (menu_sel + n_menu - 1) % n_menu;
        if (b->pressed & UI_BTN_B) {
            switch (menu_rows[menu_sel]) {
            case M_RESUME:
                if (! sec_valid || sec.number != store_get()->book_section)
                    resume();  /* Otherwise the page is kept where it was */
                view = V_READ;
                break;
            case M_START:
            case M_RESTART:
                restart();
                view = V_READ;
                break;
            case M_ITEMS:
                view = V_ITEMS;
                break;
            default:
                show_list();
                break;
            }
        }
        break;
    case V_READ:
        read_buttons(b);
        break;
    case V_ITEMS:
        if (back || (b->pressed & UI_BTN_B))
            view = V_MENU;
        break;
    case V_MESSAGE:
        if (back || (b->pressed & UI_BTN_B))
            show_list();
        break;
    }
    return true;
}


/* ------ Drawing ------ */

static void draw_slice(uint8_t *fb, int x, int y, const char *s, int len, gfx_color_t color) {
    char buf[GB_CHOICE_LEN + 64];
    if (len > (int)sizeof(buf) - 1)
        len = sizeof(buf) - 1;
    memcpy(buf, s, len);
    buf[len] = 0;
    gfx_text(fb, x, y, &gfx_font_small, buf, color, GFX_ALIGN_LEFT);
}

static void render_read(uint8_t *fb) {
    char text[48], fitted[48];
    /* A thin header: section, title, page */
    gfx_fill_rect(fb, 0, 0, GFX_WIDTH, HEADER_H, GFX_BLACK);
    snprintf(text, sizeof(text), "%u", sec.number);
    gfx_text(fb, 4, 1, &gfx_font_small, text, GFX_WHITE, GFX_ALIGN_LEFT);
    if (n_pages > 1) {
        snprintf(text, sizeof(text), "%d/%d", page + 1, n_pages);
        gfx_text(fb, GFX_WIDTH - 4, 1, &gfx_font_small, text, GFX_WHITE, GFX_ALIGN_RIGHT);
    }
    ui_fit_preview(&gfx_font_small, fitted, sizeof(fitted), book.title, 120);  /* The title of a book: cut expected */
    gfx_text(fb, GFX_WIDTH / 2, 1, &gfx_font_small, fitted, GFX_WHITE, GFX_ALIGN_CENTER);

    for (int i = page_start[page], row = 0; i < page_start[page + 1]; ++i, ++row) {
        int y = TEXT_Y + row * LINE_H;
        const char *s = line_text(i);
        switch (line_kind[i]) {
        case K_TEXT:
            draw_slice(fb, TEXT_X + (lines[i].first ? INDENT : 0), y, s, lines[i].len, GFX_BLACK);
            break;
        case K_NOTE:
            draw_slice(fb, TEXT_X, y, s, lines[i].len, GFX_BLACK);
            break;
        case K_OPT:
        case K_OPT_CONT: {
            bool on = line_opt[i] == sel;
            gfx_color_t color = on ? GFX_WHITE : GFX_BLACK;
            if (on)
                gfx_fill_rect(fb, 0, y - 1, GFX_WIDTH, LINE_H, GFX_BLACK);
            if (line_kind[i] == K_OPT)
                gfx_text(fb, TEXT_X, y, &gfx_font_small, ">", color, GFX_ALIGN_LEFT);
            draw_slice(fb, CHOICE_X, y, s, lines[i].len, color);
            break;
        }
        case K_END:
            gfx_text(fb, GFX_WIDTH / 2, y, &gfx_font_small, s, GFX_BLACK, GFX_ALIGN_CENTER);
            break;
        default:
            break;
        }
    }
    ui_footer(fb, sel >= 0 ? N_("D : choisir  G : menu")
              : page < n_pages - 1 ? N_("Flancs, D : suite  G : menu") : N_("G : menu"));
}

static void render_menu(uint8_t *fb) {
    char text[64], fitted[64];
    ui_fit_preview(&gfx_font_medium, fitted, sizeof(fitted), book.title, GFX_WIDTH - 4);
    ui_title(fb, fitted);
    int y = UI_TITLE_H + 4;
    if (book.author[0]) {
        snprintf(text, sizeof(text), _("par %s"), book.author);
        ui_fit(&gfx_font_small, fitted, sizeof(fitted), text, GFX_WIDTH - 4);
        gfx_text(fb, GFX_WIDTH / 2, y, &gfx_font_small, fitted, GFX_BLACK, GFX_ALIGN_CENTER);
    }
    y += 20;
    if (has_progress())
        snprintf(text, sizeof(text), _("Section %u"), store_get()->book_section);
    else
        snprintf(text, sizeof(text), _("%u sections"), book.n_sections);
    gfx_text(fb, GFX_WIDTH / 2, y, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
    y += 28;
    for (int i = 0; i < n_menu; ++i, y += 22) {
        const char *label = menu_label(menu_rows[i]);
        if (i == menu_sel) {
            gfx_fill_rect(fb, 4, y - 2, GFX_WIDTH - 8, 21, GFX_BLACK);
            gfx_text(fb, GFX_WIDTH / 2, y, &gfx_font_small, label, GFX_WHITE, GFX_ALIGN_CENTER);
        } else {
            gfx_text(fb, GFX_WIDTH / 2, y, &gfx_font_small, label, GFX_BLACK, GFX_ALIGN_CENTER);
        }
    }
    ui_footer(fb, N_("D : choisir  G : retour"));
}

static void render_items(uint8_t *fb) {
    ui_title(fb, N_("Objets"));
    int y = UI_TITLE_H + 8, shown = 0;
    uint8_t held = has_progress() ? (uint8_t)(store_get()->book_hash >> 24) : 0;  /* Saved at each section */
    for (int i = 0; i < book.n_items && y < UI_FOOTER_Y - 20; ++i)
        if (held & (1u << i)) {
            char fitted[GB_ITEM_LEN + 4];
            ui_fit(&gfx_font_small, fitted, sizeof(fitted), book.items[i], GFX_WIDTH - 8);
            gfx_text(fb, GFX_WIDTH / 2, y, &gfx_font_small, fitted, GFX_BLACK, GFX_ALIGN_CENTER);
            y += 20;
            ++shown;
        }
    if (! shown)
        ui_lines(fb, 80, &gfx_font_small, N_("Aucun objet pour l'instant."));
    ui_footer(fb, N_("G : retour"));
}

static void gamebook_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    switch (view) {
    case V_LIST:
        ui_title(fb, N_("Livres-jeux"));
        ui_list(fb, list_count(), list_sel, list_label);
        ui_footer(fb, N_("D : ouvrir  G : quitter"));
        break;
    case V_MENU:
        render_menu(fb);
        break;
    case V_READ:
        render_read(fb);
        break;
    case V_ITEMS:
        render_items(fb);
        break;
    case V_MESSAGE:
        ui_title(fb, N_("Livres-jeux"));
        ui_lines(fb, 70, &gfx_font_small, message);
        ui_footer(fb, N_("G : retour"));
        break;
    }
}

static void gamebook_stop(void) {
    close_source();
}

const app_t app_gamebook = {
    .name = N_("Livres-jeux"),
    .start = gamebook_start,
    .buttons = gamebook_buttons,
    .render = gamebook_render,
    .stop = gamebook_stop,
};
