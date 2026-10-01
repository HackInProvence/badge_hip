/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Host tests of the gamebooks (gamebook_parse.c): the format (CRLF, BOM, comments, items, die, endings), the
 * missing sections, the long lines and texts, the typography, the wrapping, and the built-in book: same text as
 * docs/sd/LIVRES (copied as book.txt by run_tests.py), every link valid, random games always reach an ending. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gamebook_builtin.h"
#include "gamebook_parse.h"
#include "gfx.h"
#include "test.h"

/* A book in memory, read by chunks of at most max_chunk bytes (to test the cache) */
typedef struct {
    const char *text;
    uint32_t len;
    int max_chunk;
} mem_t;

static int read_mem(void *ctx, uint32_t offset, void *buf, int len) {
    mem_t *m = ctx;
    if (offset >= m->len)
        return 0;
    if (m->max_chunk && len > m->max_chunk)
        len = m->max_chunk;
    if ((uint32_t)len > m->len - offset)
        len = m->len - offset;
    memcpy(buf, m->text + offset, len);
    return len;
}

static int read_fail(void *ctx, uint32_t offset, void *buf, int len) {
    (void)ctx; (void)offset; (void)buf; (void)len;
    return -1;
}

static gb_book_t book;
static gb_section_t sec;
static gb_src_t src;

static bool open_mem(mem_t *m, const char *text, int chunk) {
    m->text = text;
    m->len = strlen(text);
    m->max_chunk = chunk;
    gb_src_init(&src, read_mem, m);
    return gb_index(&book, &src);
}

static const char SAMPLE[] =
    "\xEF\xBB\xBF" "Le Test\r\n"
    "Une autrice\r\n"
    "Une description ignor\xC3\xA9" "e\r\n"
    "// un commentaire\r\n"
    "\r\n"
    "== 1\r\n"
    "Premier paragraphe,\r\n"
    "  sur deux lignes.\r\n"
    "\r\n"
    "\xC2\xAB Bonjour \xC2\xBB, dit l\xE2\x80\x99" "ab\xC5\x93" "il\xE2\x80\xA6\r\n"
    "\xE2\x80\x94 Un dialogue !\r\n"
    "+[Cl\xC3\xA9]\r\n"
    "-> 2 : Aller au 2\r\n"
    "-> 3 [cl\xC3\xA9] : Avec la cl\xC3\xA9\r\n"
    "-> 4 [!CL\xC3\x89] : Sans la cl\xC3\xA9\r\n"
    "\xE2\x86\x92 5\r\n"
    "== 2 ==\r\n"
    "Le d\xC3\xA9.\r\n"
    "-> 6 [d\xC3\xA9 1-3] : Petit\r\n"
    "-> 7 [de 6 - 4] : Grand\r\n"
    "-> 99 : Section absente\r\n"
    "== 3 FIN gagn\xC3\xA9\r\n"
    "Victoire.\r\n"
    "== 4 fin PERDU\r\n"
    "D\xE9" "faite.\r\n"  /* Windows-1252 é */
    "== 5 FIN\r\n"
    "== 6\r"  /* Old Mac line ends */
    "Sans choix.\r"
    "-[cl\xC3\xA9] +[corde]\r"
    "== 7\n"
    "-> 1 [inconnu] : Jamais\n"
    "-> 1 [!inconnu] : Toujours\n";

static void test_sample(int chunk) {
    mem_t m;
    CHECK(open_mem(&m, SAMPLE, chunk));
    CHECK_STR(book.title, "Le Test");
    CHECK_STR(book.author, "Une autrice");
    CHECK_EQ(book.n_sections, 7);
    CHECK_EQ(book.start, 1);
    CHECK_EQ(book.n_items, 3);  /* In the order of the file: Clé (= CLÉ), corde, inconnu */
    CHECK_STR(book.items[0], "Cl\xC3\xA9");
    CHECK_EQ(gb_item_index(&book, "corde"), 1);
    CHECK_EQ(gb_item_index(&book, "INCONNU"), 2);
    CHECK_EQ(gb_find(&book, 99), -1);

    CHECK(gb_load(&book, &src, 1, &sec));
    CHECK_STR(sec.text, "Premier paragraphe, sur deux lignes.\n\"Bonjour\", dit l'aboeil...\n- Un dialogue !");
    CHECK_EQ(sec.end, GB_END_NONE);
    CHECK_EQ(sec.gain, 1);
    CHECK_EQ(sec.n_choices, 4);
    CHECK_EQ(sec.choices[0].target, 2);
    CHECK_STR(sec.choices[0].label, "Aller au 2");
    CHECK_EQ(sec.choices[1].item, 0);
    CHECK(! sec.choices[1].negate);
    CHECK_EQ(sec.choices[2].item, 0);
    CHECK(sec.choices[2].negate);
    CHECK_EQ(sec.choices[3].target, 5);
    CHECK_STR(sec.choices[3].label, "Continuer");
    CHECK(gb_choice_available(&sec.choices[0], 0, 0));
    CHECK(! gb_choice_available(&sec.choices[1], 0, 0));
    CHECK(gb_choice_available(&sec.choices[1], 1, 0));
    CHECK(gb_choice_available(&sec.choices[2], 0, 0));
    CHECK(! gb_choice_available(&sec.choices[2], 1, 0));

    CHECK(gb_load(&book, &src, 2, &sec));
    CHECK(sec.has_dice);
    CHECK_EQ(sec.choices[0].dice_min, 1);
    CHECK_EQ(sec.choices[0].dice_max, 3);
    CHECK_EQ(sec.choices[1].dice_min, 4);
    CHECK_EQ(sec.choices[1].dice_max, 6);
    CHECK(gb_choice_available(&sec.choices[0], 0, 2));
    CHECK(! gb_choice_available(&sec.choices[0], 0, 4));
    CHECK(gb_choice_available(&sec.choices[1], 0, 6));
    CHECK(gb_choice_available(&sec.choices[2], 0, 6));
    CHECK(! gb_load(&book, &src, sec.choices[2].target, &sec));  /* Missing: no crash, false */

    CHECK(gb_load(&book, &src, 3, &sec));
    CHECK_EQ(sec.end, GB_END_WIN);
    CHECK_STR(sec.text, "Victoire.");
    CHECK(gb_load(&book, &src, 4, &sec));
    CHECK_EQ(sec.end, GB_END_LOSE);
    CHECK_STR(sec.text, "D\xC3\xA9" "faite.");
    CHECK(gb_load(&book, &src, 5, &sec));
    CHECK_EQ(sec.end, GB_END_NEUTRAL);
    CHECK_STR(sec.text, "");
    CHECK(gb_load(&book, &src, 6, &sec));
    CHECK_EQ(sec.end, GB_END_NEUTRAL);  /* No choice: an ending */
    CHECK_STR(sec.text, "Sans choix.");
    CHECK_EQ(sec.lose, 1);
    CHECK_EQ(sec.gain, 2);
    CHECK(gb_load(&book, &src, 7, &sec));
    CHECK(! gb_choice_available(&sec.choices[0], 3, 0));
    CHECK(gb_choice_available(&sec.choices[1], 3, 0));
}

/* More than GB_MAX_ITEMS items: the others are never held */
static void test_items(void) {
    mem_t m;
    CHECK(open_mem(&m, "T\n== 1\n+[a] +[b] +[c] +[d]\n+[e] +[f] +[g] +[h] +[i]\n-> 2 [i] : Jamais\n-> 2 [!i] : Toujours\n"
                       "== 2\n", 0));
    CHECK_EQ(book.n_items, GB_MAX_ITEMS);
    CHECK(gb_load(&book, &src, 1, &sec));
    CHECK_EQ(sec.gain, 0xFF);
    CHECK(sec.choices[0].unknown_item);
    CHECK(! gb_choice_available(&sec.choices[0], 0xFF, 0));
    CHECK(gb_choice_available(&sec.choices[1], 0xFF, 0));
    /* Not effects: a line of text */
    CHECK(open_mem(&m, "T\n== 1\n+[a] et du texte\n-[b]-\n", 0));
    CHECK(gb_load(&book, &src, 1, &sec));
    CHECK_STR(sec.text, "+[a] et du texte -[b]-");
    CHECK_EQ(sec.gain, 0);
}

static void test_errors(void) {
    mem_t m;
    CHECK(! open_mem(&m, "Titre\nAuteur\nPas de section.\n", 0));
    CHECK(! open_mem(&m, "", 0));
    CHECK(! open_mem(&m, "== 0\n== abc\n=== \n", 0));  /* Not sections */
    gb_src_init(&src, read_fail, NULL);
    CHECK(! gb_index(&book, &src));
    char title[GB_TITLE_LEN];
    CHECK(! gb_read_title(&src, title, sizeof(title)));
    /* Read error after the index: false */
    CHECK(open_mem(&m, "T\n== 1\nTexte\n", 0));
    gb_src_init(&src, read_fail, NULL);
    CHECK(! gb_load(&book, &src, 1, &sec));
    /* Title only, and no title (the same source: the cache is dropped at each read of the beginning) */
    gb_src_init(&src, read_mem, &m);
    m.text = "\n// c\n  Mon titre  \n== 1\n";
    m.len = strlen(m.text);
    CHECK(gb_read_title(&src, title, sizeof(title)));
    CHECK_STR(title, "Mon titre");
    m.text = "== 1\nTexte\n";
    m.len = strlen(m.text);
    CHECK(! gb_read_title(&src, title, sizeof(title)));
    /* Too many sections */
    static char many[(GB_MAX_SECTIONS + 10) * 16];
    int n = 0;
    for (int i = 1; i <= GB_MAX_SECTIONS + 5; ++i)
        n += sprintf(many + n, "== %d\n-> %d\n", i, i + 1);
    CHECK(open_mem(&m, many, 0));
    CHECK(book.too_many);
    CHECK_EQ(book.n_sections, GB_MAX_SECTIONS);
    CHECK(gb_load(&book, &src, GB_MAX_SECTIONS, &sec));
    CHECK(! gb_load(&book, &src, GB_MAX_SECTIONS + 1, &sec));
}

static void test_long(void) {
    static char text[20000];
    int n = sprintf(text, "Long\n== 1\n");
    /* A line of 1000 bytes with accents (cut in pieces, never in a letter), then a long text */
    for (int i = 0; i < 200; ++i)
        n += sprintf(text + n, "\xC3\xA9t\xC3\xA9 ");
    n += sprintf(text + n, "\n-> 2 : Un choix apr\xC3\xA8s la longue ligne\n");
    n += sprintf(text + n, "== 2\n");
    for (int i = 0; i < 400; ++i)
        n += sprintf(text + n, "Le texte continue encore %d.\n", i);
    n += sprintf(text + n, "-> 1 : Retour\n== 3\nFin.\n");
    mem_t m;
    CHECK(open_mem(&m, text, 0));
    CHECK_EQ(book.n_sections, 3);
    CHECK(gb_load(&book, &src, 1, &sec));
    CHECK(! sec.text_cut);
    CHECK_EQ(strlen(sec.text), 200 * 6 - 1);
    CHECK_EQ(sec.n_choices, 1);
    CHECK_STR(sec.choices[0].label, "Un choix apr\xC3\xA8s la longue ligne");
    CHECK(gb_load(&book, &src, 2, &sec));
    CHECK(sec.text_cut);
    CHECK(strlen(sec.text) < GB_TEXT_MAX);
    CHECK(! strcmp(sec.text + strlen(sec.text) - 3, "..."));
    CHECK_EQ(sec.n_choices, 1);  /* The choices after a cut text are kept */
    CHECK(gb_load(&book, &src, 3, &sec));
    CHECK_STR(sec.text, "Fin.");
    /* A cut never splits a letter */
    char small[8];
    CHECK_EQ(gb_normalize("\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9", 8, small, 6), 4);
    CHECK_STR(small, "\xC3\xA9\xC3\xA9");
}

/* 6 pixels per character */
static int measure_fixed(const char *s, int n, void *ctx) {
    (void)ctx;
    int w = 0;
    for (int i = 0; i < n; ++i)
        w += (s[i] & 0xC0) != 0x80 ? 6 : 0;
    return w;
}

static int measure_font(const char *s, int n, void *ctx) {
    char tmp[512];
    memcpy(tmp, s, n);
    tmp[n] = 0;
    return gfx_text_width(ctx, tmp);
}

static void test_wrap(void) {
    gb_line_t lines[32];
    const char *t = "Un deux trois quatre cinq six sept huit neuf dix\nOui !\nanticonstitutionnellement";
    int n = gb_wrap(t, 60, 12, measure_fixed, NULL, lines, 32);
    /* Width 60 = 10 characters, 8 on the first line of a paragraph */
    /* "Un deux" / "trois" / "quatre" / "cinq six" / "sept huit" / "neuf dix" / "Oui !" / "anticons" / "titutionne" /
     * "llement" */
    CHECK_EQ(n, 10);
    CHECK(lines[0].first && ! lines[1].first);
    CHECK_EQ(lines[0].len, 7);
    for (int i = 0; i < n; ++i)
        CHECK(measure_fixed(t + lines[i].start, lines[i].len, NULL) <= 60 - (lines[i].first ? 12 : 0));
    CHECK(lines[6].first);
    CHECK_EQ(lines[6].len, 5);  /* "Oui !": not cut before the "!" */
    CHECK(lines[7].first);
    CHECK_EQ(lines[7].len, 8);  /* A word too long is cut */
    CHECK_EQ(lines[8].len, 10);
    CHECK_EQ(gb_wrap("", 60, 0, measure_fixed, NULL, lines, 32), 0);
    CHECK_EQ(gb_wrap("a b c d e f g h i j k l m n o p", 6, 0, measure_fixed, NULL, lines, 4), 4);  /* Bounded */
}

static int pages_of(const gb_section_t *s) {
    static gb_line_t lines[200];
    int n = gb_wrap(s->text, 192, 10, measure_font, (void *)&gfx_font_small, lines, 200);
    for (int i = 0; i < s->n_choices; ++i)
        n += gb_wrap(s->choices[i].label, 180, 0, measure_font, (void *)&gfx_font_small, lines, 200);
    return (n + 1 + (s->end ? 3 : 0) + 7) / 8;
}

static void test_builtin(void) {
    CHECK(gamebook_builtin_count >= 1);
    const gamebook_builtin_t *b = &gamebook_builtins[0];
    /* Same as the file of the SD card (generated by tools/gamebook_check.py --c) */
    FILE *f = fopen("book.txt", "rb");
    CHECK(f != NULL);
    if (f) {
        static char file[65536];
        size_t n = fread(file, 1, sizeof(file), f);
        fclose(f);
        CHECK_EQ(n, b->len);
        CHECK(n == b->len && ! memcmp(file, b->text, n));
        if (n != b->len || memcmp(file, b->text, n))
            printf("gamebook_builtin.c is not up to date: python tools/gamebook_check.py docs/sd/LIVRES/... --c ...\n");
    }
    mem_t m = {b->text, b->len, 0};
    gb_src_init(&src, read_mem, &m);
    CHECK(gb_index(&book, &src));
    printf("built-in: \"%s\", %u sections, %u items\n", book.title, book.n_sections, book.n_items);
    CHECK(book.n_sections >= 25);
    int endings = 0, wins = 0, max_pages = 0;
    for (int i = 0; i < book.n_sections; ++i) {
        CHECK(gb_load(&book, &src, book.number[i], &sec));
        CHECK(! sec.text_cut);
        CHECK(sec.text[0] != 0);
        endings += sec.end != GB_END_NONE;
        wins += sec.end == GB_END_WIN;
        for (int c = 0; c < sec.n_choices; ++c)
            CHECK(gb_find(&book, sec.choices[c].target) >= 0);
        /* Every character is in the font (no '?') */
        CHECK(! strchr(sec.text, '?') || gfx_text_width(&gfx_font_small, "?") > 0);
        int p = pages_of(&sec);
        if (p > max_pages)
            max_pages = p;
    }
    printf("built-in: %d endings (%d won), at most %d pages per section\n", endings, wins, max_pages);
    CHECK(endings >= 3);
    CHECK(wins >= 1);
    CHECK(max_pages <= 4);

    /* Random games: always an ending, never stuck (the items and the die as on the badge) */
    srand(1234);
    int ended = 0, won = 0;
    for (int game = 0; game < 2000; ++game) {
        uint8_t items = 0;
        uint16_t number = book.start;
        for (int step = 0; step < 200; ++step) {
            if (! gb_load(&book, &src, number, &sec)) {
                CHECK(false);
                break;
            }
            items = (items | sec.gain) & ~sec.lose;
            if (sec.end) {
                ++ended;
                won += sec.end == GB_END_WIN;
                break;
            }
            uint8_t roll = sec.has_dice ? 1 + rand() % 6 : 0;
            int avail[GB_MAX_CHOICES], n = 0;
            for (int c = 0; c < sec.n_choices; ++c)
                if (gb_choice_available(&sec.choices[c], items, roll))
                    avail[n++] = c;
            if (! n) {
                printf("stuck in section %u (items 0x%02x, die %u)\n", number, items, roll);
                CHECK(false);
                break;
            }
            number = sec.choices[avail[rand() % n]].target;
        }
    }
    printf("built-in: %d random games, %d ended, %d won\n", 2000, ended, won);
    CHECK_EQ(ended, 2000);
    CHECK(won > 0);
}

/* The texts of gamebook.c fit in the width of the screen */
static void test_ui_texts(void) {
    static const char *const FOOTERS[] = {"D : choisir  G : menu", "Flancs, D : suite  G : menu", "D : ouvrir  G : quitter",
                                          "D : choisir  G : retour", "Bravo, vous avez gagné !",
                                          "Perdu... Réessayez !", "Merci d'avoir joué !",
                                          "Aucun objet pour l'instant.", "Le dé roule... et donne 6.", "Revenir en arrière"};
    for (size_t i = 0; i < sizeof(FOOTERS) / sizeof(FOOTERS[0]); ++i) {
        int w = gfx_text_width(&gfx_font_small, FOOTERS[i]);
        if (w > 196)
            printf("too wide (%d px): %s\n", w, FOOTERS[i]);
        CHECK(w <= 196);
    }
}

int main(void) {
    test_sample(0);
    test_sample(7);  /* Small reads: the cache refilled in the middle of the lines */
    test_items();
    test_errors();
    test_long();
    test_wrap();
    test_builtin();
    test_ui_texts();
    CHECK_EQ(gb_hash("LIVRES/a.txt") == gb_hash("LIVRES/b.txt"), 0);
    TEST_END();
}
