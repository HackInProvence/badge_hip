/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Reading of the books of the gamebooks (format in gamebook_parse.h and docs/fr/livres_jeux.md).
 * No dynamic memory: the caller gives the index and the section buffers, the file is read through a 256 byte cache. */

#include <string.h>

#include "gamebook_parse.h"
#include "i18n.h"


/* ------ Reading the file ------ */

void gb_src_init(gb_src_t *src, gb_read_t read, void *ctx) {
    memset(src, 0, sizeof(*src));
    src->read = read;
    src->ctx = ctx;
}

static int src_peek(gb_src_t *s) {
    if (s->pos < s->buf_start || s->pos >= s->buf_start + (uint32_t)s->buf_len) {
        if (s->error)
            return -1;
        int n = s->read(s->ctx, s->pos, s->buf, sizeof(s->buf));
        s->buf_start = s->pos;
        s->buf_len = n > 0 ? n : 0;
        if (n < 0)
            s->error = true;
        if (n <= 0)
            return -1;
    }
    return s->buf[s->pos - s->buf_start];
}

/* Skips the UTF-8 byte order mark at the beginning of the file (Windows Notepad writes one) */
static void src_rewind(gb_src_t *s) {
    s->pos = 0;
    s->buf_len = 0;  /* The file may have changed */
    s->error = false;
    if (src_peek(s) == 0xEF) {
        s->pos = 1;
        if (src_peek(s) == 0xBB) {
            s->pos = 2;
            if (src_peek(s) == 0xBF) {
                s->pos = 3;
                return;
            }
        }
        s->pos = 0;
    }
}

/* Reads a line without its end ("\n", "\r\n" or "\r") in \p line, returns its length, -1 at the end of the file.
 * A line longer than the buffer is cut between two UTF-8 characters: *more is set and the next call gives the rest. */
static int read_line(gb_src_t *s, char *line, int size, bool *more) {
    int n = 0, c;
    *more = false;
    if (src_peek(s) < 0)
        return -1;
    while ((c = src_peek(s)) >= 0) {
        if (c == '\n' || c == '\r') {
            ++s->pos;
            if (c == '\r' && src_peek(s) == '\n')
                ++s->pos;
            break;
        }
        if ((n >= size - 4 && (c & 0xC0) != 0x80) || n >= size - 1) {
            *more = true;
            break;
        }
        line[n++] = (char)c;
        ++s->pos;
    }
    if (! *more)
        while (n && (line[n - 1] == ' ' || line[n - 1] == '\t'))
            --n;
    line[n] = 0;
    return n;
}


/* ------ Text adapted to the fonts ------ */

/* Windows-1252 (a file saved by an old editor): the typographic characters of 0x80-0x9F, Latin-1 above */
static int cp1252(uint8_t b) {
    switch (b) {
    case 0x85: return 0x2026;
    case 0x8C: return 0x0152;
    case 0x91: case 0x92: return 0x2019;
    case 0x93: case 0x94: return 0x201C;
    case 0x96: case 0x97: return 0x2014;
    case 0x9C: return 0x0153;
    default: return b >= 0xA0 ? b : '?';
    }
}

/* Next character of \p p (\p n bytes left), returns the bytes used */
static int decode(const uint8_t *p, int n, int *cp) {
    uint8_t b = p[0];
    if (b < 0x80) {
        *cp = b;
        return 1;
    }
    int len = (b & 0xE0) == 0xC0 ? 2 : (b & 0xF0) == 0xE0 ? 3 : (b & 0xF8) == 0xF0 ? 4 : 0;
    if (len && len <= n) {
        int v = b & (0x7F >> len);
        int k = 1;
        for (; k < len && (p[k] & 0xC0) == 0x80; ++k)
            v = (v << 6) | (p[k] & 0x3F);
        if (k == len) {
            *cp = v;
            return len;
        }
    }
    *cp = cp1252(b);  /* Not UTF-8 */
    return 1;
}

/* What the badge draws for \p cp (the fonts have ASCII and the French letters), returns its length (0: dropped) */
static int replacement(int cp, char *out) {
    const char *s = NULL;
    switch (cp) {
    case 0x2018: case 0x2019: case 0x201A: case 0x2032: case 0x02BC: s = "'"; break;
    case 0xAB: case 0xBB: case 0x201C: case 0x201D: case 0x201E: case 0x2039: case 0x203A: s = "\""; break;
    case 0x2026: s = "..."; break;
    case 0x2010: case 0x2011: case 0x2012: case 0x2013: case 0x2014: case 0x2015: case 0x2212: s = "-"; break;
    case 0x2022: case 0xB7: s = "-"; break;
    case 0x153: s = "oe"; break;
    case 0x152: s = "OE"; break;
    case 0xE6: s = "ae"; break;
    case 0xC6: s = "AE"; break;
    case 0xA0: case 0x202F: case 0x2009: case 0x2007: case '\t': s = " "; break;
    case 0xFEFF: return 0;
    default: break;
    }
    if (s) {
        int n = strlen(s);
        memcpy(out, s, n);
        return n;
    }
    if (cp < 0x20 || cp == 0x7F)
        return 0;
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = '?';  /* Emojis: not in the fonts */
    return 1;
}

/* gb_normalize(), also says how many bytes of \p src were used (less than \p n when \p dst is full).
 * French quotes lose their inner spaces ("« Oui »" -> "\"Oui\""), the runs of spaces become one. */
static int normalize(const char *src, int n, char *dst, int cap, int *used) {
    const uint8_t *p = (const uint8_t *)src;
    int i = 0, w = 0;
    bool skip_spaces = false;
    while (i < n) {
        int cp, len = decode(p + i, n - i, &cp);
        char out[4];
        int k = replacement(cp, out);
        bool space = k == 1 && out[0] == ' ';
        if (space && (skip_spaces || (w > 0 && dst[w - 1] == ' '))) {
            i += len;
            continue;
        }
        if (cp == 0xBB || cp == 0x203A)
            while (w > 0 && dst[w - 1] == ' ')
                --w;
        if (w + k > cap - 1)
            break;
        memcpy(dst + w, out, k);
        w += k;
        i += len;
        skip_spaces = cp == 0xAB || cp == 0x2039;
    }
    if (cap > 0)
        dst[w] = 0;
    *used = i;
    return w;
}

int gb_normalize(const char *src, int n, char *dst, int cap) {
    int used;
    return normalize(src, n, dst, cap, &used);
}


/* ------ The lines of the format ------ */

static const char *skip_blank(const char *p) {
    while (*p == ' ' || *p == '\t')
        ++p;
    return p;
}

static char lower(char c) {
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

/* Case insensitive (ASCII) prefix */
static bool starts_with(const char *p, const char *prefix) {
    for (; *prefix; ++p, ++prefix)
        if (lower(*p) != lower(*prefix))
            return false;
    return true;
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

/* A number of section: 1..65535, returns false otherwise */
static bool parse_number(const char **pp, uint16_t *number) {
    const char *p = *pp;
    uint32_t v = 0;
    if (! is_digit(*p))
        return false;
    while (is_digit(*p)) {
        v = v * 10 + (*p++ - '0');
        if (v > 65535)
            return false;
    }
    if (v == 0)
        return false;
    *number = (uint16_t)v;
    *pp = p;
    return true;
}

static bool is_comment(const char *p) {
    return p[0] == '/' && p[1] == '/';
}

/* "== 12", "== 12 ==", "== 12 FIN", "== 12 FIN gagné", "== 12 FIN perdu" */
static bool parse_header(const char *p, uint16_t *number, uint8_t *end) {
    p = skip_blank(p);
    if (p[0] != '=' || p[1] != '=')
        return false;
    while (*p == '=')
        ++p;
    p = skip_blank(p);
    if (! parse_number(&p, number))
        return false;
    p = skip_blank(p);
    while (*p == '=')
        ++p;
    p = skip_blank(p);
    *end = GB_END_NONE;
    if (starts_with(p, "fin") && (p[3] == 0 || p[3] == ' ' || p[3] == '\t')) {
        p = skip_blank(p + 3);
        *end = starts_with(p, "gagn") ? GB_END_WIN : starts_with(p, "perdu") ? GB_END_LOSE : GB_END_NEUTRAL;
    }
    return true;
}

/* "-> 12 [cond] : label" (or the arrow "→"): \p cond receives what is between the brackets */
static bool parse_choice(const char *p, uint16_t *target, char *cond, int cond_len, const char **label) {
    p = skip_blank(p);
    if (p[0] == '-' && p[1] == '>')
        p += 2;
    else if ((uint8_t)p[0] == 0xE2 && (uint8_t)p[1] == 0x86 && (uint8_t)p[2] == 0x92)
        p += 3;
    else
        return false;
    p = skip_blank(p);
    if (! parse_number(&p, target))
        return false;
    p = skip_blank(p);
    cond[0] = 0;
    if (*p == '[') {
        const char *close = strchr(p, ']');
        int n = close ? (int)(close - p - 1) : (int)strlen(p + 1);
        if (n > cond_len - 1)
            n = cond_len - 1;
        memcpy(cond, p + 1, n);
        cond[n] = 0;
        p = close ? close + 1 : p + strlen(p);
        p = skip_blank(p);
    }
    if (*p == ':')
        p = skip_blank(p + 1);
    *label = p;
    return true;
}

/* Calls \p fn for each "+[item]" / "-[item]" of the line; false (and no call) when the line is something else */
typedef void (*effect_fn)(void *ctx, const char *name, int n, bool gain);

static bool parse_effects(const char *p, effect_fn fn, void *ctx) {
    for (int pass = 0; pass < 2; ++pass) {
        const char *q = skip_blank(p);
        if (! *q)
            return false;
        while (*q) {
            if ((q[0] != '+' && q[0] != '-') || q[1] != '[')
                return false;
            const char *close = strchr(q + 2, ']');
            if (! close)
                return false;
            if (pass)
                fn(ctx, q + 2, (int)(close - q - 2), q[0] == '+');
            q = skip_blank(close + 1);
        }
    }
    return true;
}

/* Dialogue lines ("— Bonjour !") start a new line of the text */
static bool is_dialogue(const char *p) {
    return (p[0] == '-' && p[1] == ' ') || ((uint8_t)p[0] == 0xE2 && (uint8_t)p[1] == 0x80
                                            && ((uint8_t)p[2] == 0x94 || (uint8_t)p[2] == 0x93));
}


/* ------ Items ------ */

/* The name of an item as kept: adapted to the fonts, without the spaces around */
static void item_name(const char *name, int n, char *out) {
    while (n > 0 && (*name == ' ' || *name == '\t')) {
        ++name;
        --n;
    }
    while (n > 0 && (name[n - 1] == ' ' || name[n - 1] == '\t'))
        --n;
    gb_normalize(name, n, out, GB_ITEM_LEN);
}

/* Lower case of the byte after a 0xC3 lead byte (U+00C0..U+00DE, the accented capitals, but the sign x) */
static uint8_t lower_latin1(uint8_t b) {
    return b >= 0x80 && b <= 0x9E && b != 0x97 ? b + 0x20 : b;
}

/* Case insensitive, accented letters included ("Clé" = "CLÉ") */
static bool same_name(const char *a, const char *b) {
    bool latin1 = false;  /* The previous byte was 0xC3 */
    for (; *a && *b; ++a, ++b) {
        if (latin1 ? lower_latin1(*a) != lower_latin1(*b) : lower(*a) != lower(*b))
            return false;
        latin1 = (uint8_t)*a == 0xC3;
    }
    return *a == *b;
}

int gb_item_index(const gb_book_t *book, const char *name) {
    for (int i = 0; i < book->n_items; ++i)
        if (same_name(book->items[i], name))
            return i;
    return -1;
}

static int find_item(const gb_book_t *book, const char *name, int n) {
    char clean[GB_ITEM_LEN];
    item_name(name, n, clean);
    return clean[0] ? gb_item_index(book, clean) : -1;
}

static void add_item(gb_book_t *book, const char *name, int n) {
    char clean[GB_ITEM_LEN];
    item_name(name, n, clean);
    if (clean[0] && gb_item_index(book, clean) < 0 && book->n_items < GB_MAX_ITEMS)
        strcpy(book->items[book->n_items++], clean);
}

static void index_effect(void *ctx, const char *name, int n, bool gain) {
    (void)gain;
    add_item((gb_book_t *)ctx, name, n);
}

/* "dé 1-3", "de 4", "dé 5-6": a die condition */
static bool parse_dice(const char *p, uint8_t *lo, uint8_t *hi) {
    p = skip_blank(p);
    if (starts_with(p, "d\xC3\xA9"))
        p += 3;
    else if (starts_with(p, "d\xC3\x89"))
        p += 3;
    else if (starts_with(p, "de"))
        p += 2;
    else
        return false;
    p = skip_blank(p);
    if (! is_digit(*p))
        return false;
    int a = *p++ - '0', b = a;
    p = skip_blank(p);
    int sep = *p == '-' || *p == 'a' ? 1 : starts_with(p, "\xC3\xA0") ? 2 : 0;  /* "1-3", "1 a 3", "1 à 3" */
    if (sep) {
        p = skip_blank(p + sep);
        if (is_digit(*p))
            b = *p - '0';
    }
    if (b < a) {
        int t = a;
        a = b;
        b = t;
    }
    *lo = a < 1 ? 1 : a > 6 ? 6 : a;
    *hi = b < 1 ? 1 : b > 6 ? 6 : b;
    return true;
}

/* Condition of a choice: an item (registered when \p book_add) or a die */
static void parse_cond(gb_book_t *book_add, const gb_book_t *book, const char *cond, gb_choice_t *c) {
    c->item = -1;
    c->negate = false;
    c->unknown_item = false;
    c->dice_min = c->dice_max = 0;
    const char *p = skip_blank(cond);
    if (! *p || parse_dice(p, &c->dice_min, &c->dice_max))
        return;
    if (*p == '!') {
        c->negate = true;
        p = skip_blank(p + 1);
    }
    if (book_add)
        add_item(book_add, p, strlen(p));
    c->item = find_item(book, p, strlen(p));
    c->unknown_item = c->item < 0;
}

bool gb_choice_available(const gb_choice_t *c, uint8_t items, uint8_t roll) {
    if (c->dice_min && (roll < c->dice_min || roll > c->dice_max))
        return false;
    bool held = c->item >= 0 && (items & (1u << c->item));
    if (c->item >= 0 || c->unknown_item)
        return c->negate ? ! held : held;
    return true;
}


/* ------ Index ------ */

/* The lines before the first section: title, then author */
static void header_line(gb_book_t *book, const char *p, int *count) {
    if (*count == 0)
        gb_normalize(p, strlen(p), book->title, sizeof(book->title));
    else if (*count == 1)
        gb_normalize(p, strlen(p), book->author, sizeof(book->author));
    ++*count;
}

bool gb_index(gb_book_t *book, gb_src_t *src) {
    memset(book, 0, sizeof(*book));
    src_rewind(src);
    char line[GB_LINE_MAX], cond[GB_ITEM_LEN + 16];
    bool more = false, cont = false;
    int header_lines = 0;
    for (;;) {
        uint32_t start = src->pos;
        if (read_line(src, line, sizeof(line), &more) < 0)
            break;
        bool rest = cont;  /* The rest of a long line: plain text */
        cont = more;
        if (rest)
            continue;
        const char *p = skip_blank(line);
        uint16_t number, target;
        uint8_t end;
        const char *label;
        if (! *p || is_comment(p))
            continue;
        if (parse_header(p, &number, &end)) {
            if (book->n_sections >= GB_MAX_SECTIONS) {
                book->too_many = true;
                continue;
            }
            if (! book->start)
                book->start = number;
            book->number[book->n_sections] = number;
            book->offset[book->n_sections++] = start;
        } else if (! book->start) {
            header_line(book, p, &header_lines);
        } else if (parse_choice(p, &target, cond, sizeof(cond), &label)) {
            gb_choice_t c;
            parse_cond(book, book, cond, &c);
        } else {
            parse_effects(p, index_effect, book);
        }
    }
    return ! src->error && book->n_sections > 0;
}

bool gb_read_title(gb_src_t *src, char *title, size_t len) {
    src_rewind(src);
    char line[GB_LINE_MAX];
    bool more;
    title[0] = 0;
    for (int lines = 0; lines < 20 && read_line(src, line, sizeof(line), &more) >= 0; ++lines) {
        const char *p = skip_blank(line);
        uint16_t number;
        uint8_t end;
        if (! *p || is_comment(p))
            continue;
        if (parse_header(p, &number, &end))
            return false;
        gb_normalize(p, strlen(p), title, len);
        return title[0] != 0;
    }
    return false;
}

int gb_find(const gb_book_t *book, uint16_t number) {
    for (int i = 0; i < book->n_sections; ++i)
        if (book->number[i] == number)
            return i;
    return -1;
}


/* ------ A section ------ */

typedef struct {
    const gb_book_t *book;
    gb_section_t *sec;
} effect_ctx_t;

static void section_effect(void *ctx, const char *name, int n, bool gain) {
    effect_ctx_t *e = ctx;
    int i = find_item(e->book, name, n);
    if (i < 0)
        return;
    if (gain)
        e->sec->gain |= 1u << i;
    else
        e->sec->lose |= 1u << i;
}

/* Appends to the text of the section (a \p separator as is, '\n' between paragraphs); when full, the text ends
 * with "..." */
#define CUT_MARK "..."
static void text_add(gb_section_t *sec, int *len, const char *s, int n, bool separator) {
    if (sec->text_cut || n <= 0)
        return;
    int cap = GB_TEXT_MAX - (int)sizeof(CUT_MARK) + 1;
    int used = 0;
    if (! separator) {
        *len += normalize(s, n, sec->text + *len, cap - *len, &used);
    } else if (*len + n < cap) {
        memcpy(sec->text + *len, s, n);
        *len += n;
        sec->text[*len] = 0;
        used = n;
    }
    if (used < n) {
        sec->text_cut = true;
        while (*len > 0 && sec->text[*len - 1] == ' ')
            --*len;
        strcpy(sec->text + *len, CUT_MARK);
        *len += strlen(CUT_MARK);
    }
}

bool gb_load(const gb_book_t *book, gb_src_t *src, uint16_t number, gb_section_t *sec) {
    sec->number = number;
    sec->end = GB_END_NONE;
    sec->text_cut = sec->has_dice = false;
    sec->gain = sec->lose = 0;
    sec->n_choices = 0;
    sec->text[0] = 0;
    int i = gb_find(book, number);
    if (i < 0)
        return false;
    src->error = false;
    src->pos = book->offset[i];
    char line[GB_LINE_MAX], cond[GB_ITEM_LEN + 16];
    bool more = false;
    uint16_t found, target;
    uint8_t end;
    if (read_line(src, line, sizeof(line), &more) < 0 || ! parse_header(line, &found, &end) || found != number)
        return false;  /* The file changed since the index */
    sec->end = end;
    int len = 0;
    bool paragraph = false;
    enum { NONE, TEXT, SKIP } rest = more ? SKIP : NONE;  /* What the rest of a long line is */
    effect_ctx_t ectx = {book, sec};
    for (;;) {
        int n = read_line(src, line, sizeof(line), &more);
        if (n < 0)
            break;
        if (rest != NONE) {
            if (rest == TEXT)
                text_add(sec, &len, line, n, false);
            if (! more)
                rest = NONE;
            continue;
        }
        const char *p = skip_blank(line);
        const char *label;
        if (! *p) {
            paragraph = len > 0;
            continue;
        }
        rest = more ? SKIP : NONE;
        if (is_comment(p))
            continue;
        if (parse_header(p, &found, &end))
            break;
        if (parse_choice(p, &target, cond, sizeof(cond), &label)) {
            if (sec->n_choices < GB_MAX_CHOICES) {
                gb_choice_t *c = &sec->choices[sec->n_choices++];
                c->target = target;
                parse_cond(NULL, book, cond, c);
                sec->has_dice |= c->dice_min != 0;
                gb_normalize(label, strlen(label), c->label, sizeof(c->label));
                if (! c->label[0])
                    strcpy(c->label, N_("Continuer"));
            }
            continue;
        }
        if (parse_effects(p, section_effect, &ectx))
            continue;
        if (len > 0)
            text_add(sec, &len, paragraph || is_dialogue(p) ? "\n" : " ", 1, true);
        paragraph = false;
        text_add(sec, &len, p, strlen(p), false);
        if (more)
            rest = TEXT;
    }
    if (src->error)
        return false;
    while (len > 0 && (sec->text[len - 1] == ' ' || sec->text[len - 1] == '\n'))
        sec->text[--len] = 0;
    if (sec->n_choices == 0 && sec->end == GB_END_NONE)
        sec->end = GB_END_NEUTRAL;  /* A dead end is an ending */
    return true;
}


/* ------ Misc ------ */

uint32_t gb_hash(const char *s) {
    uint32_t h = 2166136261u;
    while (*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}

/* French typography puts a space before "!?:;": not a place to cut the line */
static bool breakable(const char *text, int i, int end) {
    if (i + 1 >= end)
        return true;
    char next = text[i + 1];
    return ! (next == '!' || next == '?' || next == ':' || next == ';');
}

int gb_wrap(const char *text, int width, int indent, gb_measure_t measure, void *ctx, gb_line_t *lines, int max) {
    int total = strlen(text), pos = 0, n = 0;
    bool first = true;
    while (pos < total && n < max) {
        int end = pos;
        while (end < total && text[end] != '\n')
            ++end;
        while (pos < end && text[pos] == ' ')
            ++pos;
        int w = width - (first ? indent : 0);
        /* As many words as fit */
        int best = -1, i = pos;
        while (i < end) {
            int j = i;
            while (j < end && ! (text[j] == ' ' && breakable(text, j, end)))
                ++j;
            if (measure(text + pos, j - pos, ctx) > w)
                break;
            best = j;
            i = j;
            while (i < end && text[i] == ' ')
                ++i;
        }
        if (best < 0) {
            /* A word longer than the line: cut between two characters */
            int k = pos;
            while (k < end) {
                int k2 = k + 1;
                while (k2 < end && (text[k2] & 0xC0) == 0x80)
                    ++k2;
                if (k > pos && measure(text + pos, k2 - pos, ctx) > w)
                    break;
                k = k2;
            }
            best = k;
        }
        int len = best - pos;
        while (len > 0 && text[pos + len - 1] == ' ')
            --len;
        lines[n].start = pos;
        lines[n].len = len;
        lines[n].first = first;
        ++n;
        pos = best;
        while (pos < end && text[pos] == ' ')
            ++pos;
        first = pos >= end;
        if (first)
            pos = end + 1;
    }
    return n;
}
