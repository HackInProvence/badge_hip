/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "ff.h"
#include "gfx.h"
#include "i18n.h"
#include "rsvp.h"
#include "screen.h"
#include "sd.h"
#include "store.h"


#define TOKEN_MAX 64  /* Bytes of a word (longer words are truncated) */
#define WORD_MAX 128  /* Bytes of a displayed word (with the attached punctuation) */
#define CHUNK_CPS 12  /* Long words are split in chunks of this number of characters... */
#define LONG_WORD_CPS 13  /* ...when they are longer than this */
#define MIN_WORD_MS 100  /* The e-Paper can't draw faster (10 fps waveform) */
#define OVERLAY_MS 1500  /* Messages (speed, seek) shown on top of the words */
#define ORP_X 80  /* Fixed position of the recognition point, left of the center */
#define WORD_Y 82
#define RETICLE_TOP 62
#define RETICLE_BOTTOM 124
#define AVG_BYTES_PER_WORD 6  /* Initial estimate, for the backward seek */


/* ------ Reading the file ------ */

static FIL file;
static bool file_open = false;
static FSIZE_t file_size = 0;
static uint8_t buf[512];
static UINT buf_len = 0, buf_pos = 0;
static FSIZE_t buf_file_pos = 0;  /* Offset of buf[0] in the file */
static bool latin1 = false;  /* Windows-1252 instead of UTF-8 */
static FSIZE_t text_start = 0;  /* After the UTF-8 byte order mark */

static void seek_file(FSIZE_t offset) {
    f_lseek(&file, offset);
    buf_file_pos = offset;
    buf_len = buf_pos = 0;
}

static FSIZE_t tell_file(void) {
    return buf_file_pos + buf_pos;
}

static int get_byte(void) {
    if (buf_pos >= buf_len) {
        buf_file_pos += buf_len;
        UINT n = 0;
        if (f_read(&file, buf, sizeof(buf), &n) != FR_OK)
            n = 0;
        buf_len = n;
        buf_pos = 0;
        if (n == 0)
            return -1;
    }
    return buf[buf_pos++];
}

/* Windows-1252 characters 0x80-0x9F: the typographic ones used in texts */
static int cp1252(int b) {
    switch (b) {
    case 0x85: return 0x2026;  /* … */
    case 0x8C: return 0x0152;  /* Œ */
    case 0x91: case 0x92: return 0x2019;  /* ’ */
    case 0x93: case 0x94: return 0x201C;  /* “ ” */
    case 0x96: case 0x97: return 0x2014;  /* – — */
    case 0x9C: return 0x0153;  /* œ */
    default: return b >= 0xA0 ? b : '?';  /* Latin-1 */
    }
}

/* Next character (code point), -1 at the end */
static int get_cp(void) {
    int b = get_byte();
    if (b < 0x80)
        return b;
    if (latin1)
        return cp1252(b);
    int n = (b & 0xE0) == 0xC0 ? 1 : (b & 0xF0) == 0xE0 ? 2 : (b & 0xF8) == 0xF0 ? 3 : 0;
    int cp = b & (0x3F >> n);
    for (int i = 0; i < n; ++i) {
        int c = get_byte();
        if (c < 0 || (c & 0xC0) != 0x80)
            return '?';
        cp = (cp << 6) | (c & 0x3F);
    }
    return n ? cp : '?';
}

/* The texts are UTF-8 unless the beginning of the file is not valid UTF-8 */
static void detect_encoding(void) {
    uint8_t probe[2048];
    UINT n = 0;
    f_lseek(&file, 0);
    f_read(&file, probe, sizeof(probe), &n);
    latin1 = false;
    for (UINT i = 0; i < n; ) {
        uint8_t b = probe[i];
        int len = b < 0x80 ? 0 : (b & 0xE0) == 0xC0 ? 1 : (b & 0xF0) == 0xE0 ? 2 : (b & 0xF8) == 0xF0 ? 3 : -1;
        if (len < 0) {
            latin1 = true;
            break;
        }
        for (int k = 1; k <= len && i + k < n; ++k)
            if ((probe[i + k] & 0xC0) != 0x80)
                latin1 = true;
        if (latin1)
            break;
        i += 1 + len;
    }
    /* Skip the UTF-8 byte order mark */
    text_start = ! latin1 && n >= 3 && probe[0] == 0xEF && probe[1] == 0xBB && probe[2] == 0xBF ? 3 : 0;
    seek_file(text_start);
}

static bool is_space(int cp) {
    return cp == ' ' || cp == '\t' || cp == '\r' || cp == '\n' || cp == 0xA0 || cp == 0x202F || cp == 0x2009;
}

static bool is_letter(int cp) {
    return (cp < 0x80 && isalnum(cp)) || (cp >= 0xC0 && cp <= 0x24F) || cp == 0x152 || cp == 0x153;
}

static int put_utf8(char *dst, int cp) {
    if (cp < 0x80) {
        dst[0] = cp;
        return 1;
    }
    if (cp < 0x800) {
        dst[0] = 0xC0 | (cp >> 6);
        dst[1] = 0x80 | (cp & 0x3F);
        return 2;
    }
    dst[0] = 0xE0 | (cp >> 12);
    dst[1] = 0x80 | ((cp >> 6) & 0x3F);
    dst[2] = 0x80 | (cp & 0x3F);
    return 3;
}


/* ------ Words ------ */

typedef struct {
    char text[TOKEN_MAX];  /* UTF-8, as in the file */
    FSIZE_t offset;  /* Position of the first character in the file */
    uint8_t newlines_before;  /* 2 or more: new paragraph */
    bool punct_only;  /* No letter nor digit */
    bool opening;  /* Opening quote, bracket or dialogue dash: goes with the next word */
} token_t;

static token_t lookahead;
static bool lookahead_valid = false;
static uint8_t carried_newlines = 0;  /* The newline that ended the previous token */

static bool read_token(token_t *t) {
    int cp;
    t->newlines_before = carried_newlines;
    carried_newlines = 0;
    while ((cp = get_cp()) >= 0 && is_space(cp))
        if (cp == '\n' && t->newlines_before < 255)
            ++t->newlines_before;
    if (cp < 0)
        return false;
    /* The character just read started at... (they are at most 4 bytes long) */
    FSIZE_t end = tell_file();
    int len = 0;
    bool letters = false;
    int first = cp;
    t->offset = end - (latin1 || cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4);
    do {
        if (is_letter(cp))
            letters = true;
        if (len < TOKEN_MAX - 4)
            len += put_utf8(t->text + len, cp);
    } while ((cp = get_cp()) >= 0 && ! is_space(cp));
    if (cp == '\n')
        carried_newlines = 1;  /* Counted for the next token */
    t->text[len] = 0;
    t->punct_only = ! letters;
    t->opening = ! letters && (first == 0xAB || first == 0x201C || first == '(' || first == '[' || first == 0x2014
                               || first == '-' || first == '"');
    return true;
}

static bool pull_token(token_t *t) {
    if (lookahead_valid) {
        *t = lookahead;
        lookahead_valid = false;
        return true;
    }
    return read_token(t);
}

static token_t *peek_token(void) {
    if (! lookahead_valid)
        lookahead_valid = read_token(&lookahead);
    return lookahead_valid ? &lookahead : NULL;
}

/* Appends " src" to dst (bounded) */
static void append_word(char *dst, const char *src) {
    size_t n = strlen(dst);
    if (n + 1 + strlen(src) < WORD_MAX) {
        if (n)
            dst[n++] = ' ';
        strcpy(dst + n, src);
    }
}

typedef struct {
    char text[WORD_MAX];  /* Displayed text (typography adapted to the font) */
    FSIZE_t offset;
    bool paragraph_end;
    bool sentence_end;
    bool clause_end;
} word_t;

/* Characters missing in the font */
static void normalize(char *dst, const char *src) {
    char out[WORD_MAX];
    int n = 0;
    while (*src && n < WORD_MAX - 5) {
        const uint8_t *p = (const uint8_t *)src;
        int cp, len;
        if (p[0] < 0x80) { cp = p[0]; len = 1; }
        else if ((p[0] & 0xE0) == 0xC0 && p[1]) { cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F); len = 2; }
        else if ((p[0] & 0xF0) == 0xE0 && p[1] && p[2]) { cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); len = 3; }
        else { cp = '?'; len = 1; }
        src += len;
        switch (cp) {
        case 0x2018: case 0x2019: case 0x2032: out[n++] = '\''; break;
        case 0xAB: case 0xBB: case 0x201C: case 0x201D: case 0x201E: out[n++] = '"'; break;
        case 0x2026: memcpy(out + n, "...", 3); n += 3; break;
        case 0x2013: case 0x2014: case 0x2010: out[n++] = '-'; break;
        case 0x153: memcpy(out + n, "oe", 2); n += 2; break;
        case 0x152: memcpy(out + n, "OE", 2); n += 2; break;
        case 0xA0: case 0x202F: case 0x2009: out[n++] = ' '; break;
        default: n += put_utf8(out + n, cp); break;
        }
    }
    out[n] = 0;
    strcpy(dst, out);
}

/* Last "meaningful" character, ignoring the closing quotes and brackets */
static char last_punct(const char *s) {
    for (int i = strlen(s) - 1; i >= 0; --i) {
        char c = s[i];
        if (c == '"' || c == ')' || c == ']' || c == '\'' || c == ' ')
            continue;
        return c;
    }
    return 0;
}

/* Next word, with the punctuation that goes with it (French typography separates : ; ! ? and quotes) */
static bool next_word(word_t *w) {
    token_t t;
    if (! pull_token(&t))
        return false;
    char raw[WORD_MAX] = "";
    w->offset = t.offset;
    append_word(raw, t.text);
    /* Opening punctuation goes with the next word */
    token_t *n;
    while (t.opening && (n = peek_token()) && n->newlines_before < 2) {
        pull_token(&t);
        append_word(raw, t.text);
    }
    /* Closing punctuation goes with the previous word */
    while ((n = peek_token()) && n->punct_only && ! n->opening && n->newlines_before < 2) {
        pull_token(&t);
        append_word(raw, t.text);
    }
    normalize(w->text, raw);
    n = peek_token();
    w->paragraph_end = ! n || n->newlines_before >= 2;
    char c = last_punct(w->text);
    w->sentence_end = c == '.' || c == '!' || c == '?';
    w->clause_end = c == ',' || c == ';' || c == ':';
    return true;
}


/* ------ Display ------ */

static uint8_t frames[2][GFX_FB_SIZE];
static uint8_t *shown = frames[0], *drawing = frames[1];
static word_t word;  /* Current word */
static bool have_word = false;
static int chunk = 0;  /* Current chunk of a long word */
static char overlay[32] = "";
static absolute_time_t overlay_ts = 0;
static bool at_end = false;

static int count_cps(const char *s) {
    int n = 0;
    for (; *s; ++s)
        n += (*s & 0xC0) != 0x80;
    return n;
}

/* Byte offset of the code point \p i */
static int cp_offset(const char *s, int i) {
    int n = 0, b = 0;
    for (; s[b]; ++b)
        if ((s[b] & 0xC0) != 0x80 && n++ == i)
            return b;
    return b;
}

/* Whether the code point \p i of the UTF-8 string is a letter or a digit */
static bool is_letter_at(const char *s, int i) {
    const uint8_t *p = (const uint8_t *)s + cp_offset(s, i);
    return isalnum(*p) || *p >= 0xC0;  /* Lead bytes of the accented letters */
}

static int n_chunks(void) {
    int n = count_cps(word.text);
    return n > LONG_WORD_CPS ? (n + CHUNK_CPS - 1) / CHUNK_CPS : 1;
}

/* The text of the current chunk: the chunks of a long word have the same size (no 2 letters left for the last one) */
static void chunk_text(char *dst) {
    int total = n_chunks();
    if (total == 1) {
        strcpy(dst, word.text);
        return;
    }
    int size = (count_cps(word.text) + total - 1) / total;
    int a = cp_offset(word.text, chunk * size), b = cp_offset(word.text, (chunk + 1) * size);
    memcpy(dst, word.text + a, b - a);
    dst[b - a] = 0;
    if (chunk < total - 1)
        strcat(dst, "-");
}

/* Optimal Recognition Point: position of the letter to look at, from the length of the word (Spritz) */
static int orp_index(int letters) {
    if (letters <= 1) return 0;
    if (letters <= 5) return 1;
    if (letters <= 9) return 2;
    if (letters <= 13) return 3;
    return 4;
}

static void draw_frame(const char *top, const char *bottom1, const char *bottom2) {
    char text[WORD_MAX];
    gfx_clear(drawing, GFX_WHITE);

    /* Reticle: 2 lines with marks on the recognition point */
    gfx_fill_rect(drawing, 10, RETICLE_TOP, GFX_WIDTH - 20, 1, GFX_BLACK);
    gfx_fill_rect(drawing, 10, RETICLE_BOTTOM, GFX_WIDTH - 20, 1, GFX_BLACK);
    gfx_fill_rect(drawing, ORP_X, RETICLE_TOP, 1, 8, GFX_BLACK);
    gfx_fill_rect(drawing, ORP_X, RETICLE_BOTTOM - 7, 1, 8, GFX_BLACK);

    if (have_word) {
        chunk_text(text);
        /* The biggest font that fits */
        const gfx_font_t *font = &gfx_font_large;
        if (gfx_text_width(font, text) > GFX_WIDTH - 8)
            font = &gfx_font_medium;
        if (gfx_text_width(font, text) > GFX_WIDTH - 8)
            font = &gfx_font_small;

        /* The recognition point is the k-th letter (the punctuation and apostrophes don't count) */
        int n = count_cps(text), letters = 0;
        for (int i = 0; i < n; ++i)
            letters += is_letter_at(text, i);
        int target = orp_index(letters), orp = 0;
        for (int i = 0, k = 0; i < n; ++i)
            if (is_letter_at(text, i) && k++ == target) {
                orp = i;
                break;
            }

        /* Align the middle of the ORP letter on the marks */
        char prefix[WORD_MAX], letter[8];
        int a = cp_offset(text, orp), b = cp_offset(text, orp + 1);
        memcpy(prefix, text, a);
        prefix[a] = 0;
        memcpy(letter, text + a, b - a);
        letter[b - a] = 0;
        int prefix_w = gfx_text_width(font, prefix), letter_w = gfx_text_width(font, letter);
        int x = ORP_X - prefix_w - letter_w / 2;
        int width = gfx_text_width(font, text);
        if (x + width > GFX_WIDTH - 4)
            x = GFX_WIDTH - 4 - width;
        if (x < 4)
            x = 4;
        int y = WORD_Y + (gfx_font_large.height - font->height) / 2;
        gfx_text(drawing, x, y, font, text, GFX_BLACK, GFX_ALIGN_LEFT);
        /* Underline the ORP letter */
        gfx_fill_rect(drawing, x + prefix_w, y + font->height - 2, letter_w > 2 ? letter_w : 3, 3, GFX_BLACK);
    }

    if (top && top[0])
        gfx_text(drawing, GFX_WIDTH / 2, 20, &gfx_font_medium, top, GFX_BLACK, GFX_ALIGN_CENTER);
    if (bottom1)
        gfx_text(drawing, GFX_WIDTH / 2, 136, &gfx_font_small, bottom1, GFX_BLACK, GFX_ALIGN_CENTER);
    if (bottom2)
        gfx_text(drawing, GFX_WIDTH / 2, 156, &gfx_font_small, bottom2, GFX_BLACK, GFX_ALIGN_CENTER);

    /* Progress */
    int progress = file_size ? (int)((GFX_WIDTH - 20) * (uint64_t)word.offset / file_size) : 0;
    gfx_fill_rect(drawing, 10, GFX_HEIGHT - 8, GFX_WIDTH - 20, 1, GFX_BLACK);
    gfx_fill_rect(drawing, 10, GFX_HEIGHT - 10, progress, 5, GFX_BLACK);
}


/* ------ Reader ------ */

typedef enum {
    R_IDLE,
    R_CLEAR,  /* Full refresh to start from a clean screen */
    R_START,  /* Enter the multiframe mode */
    R_READING,
    R_PAUSE_DRAW,  /* Draw the pause page (in multiframe mode) */
    R_PAUSE_END,  /* Leave the multiframe mode while paused */
    R_PAUSED,
    R_STOP_END,  /* Leave the multiframe mode, then stop */
} rsvp_state_t;

static rsvp_state_t state = R_IDLE;
static bool in_multiframe = false;
static bool pause_redraw = false;  /* The pause page changed (seek, speed) */
static bool stopping = false;
static bool first_word = true;  /* The first word after (re)starting stays longer */
static uint16_t wpm = RSVP_DEFAULT_WPM;
static const uint8_t *waveform = NULL;
static absolute_time_t next_ts = 0;
static int pending_seek = 0;
static uint32_t words_read = 0;
static FSIZE_t bytes_read = 0;  /* For the average length of the words */
static uint32_t path_hash = 0;
static char message[48] = "";

static uint32_t hash(const char *s) {
    uint32_t h = 2166136261u;
    while (*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}

static void save_position(void) {
    store_t *s = store_get();
    s->rsvp_wpm = wpm;
    s->rsvp_hash = path_hash;
    s->rsvp_offset = have_word ? word.offset : 0;
    store_changed();
}

static void set_overlay(const char *msg) {
    snprintf(overlay, sizeof(overlay), "%s", msg);
    overlay_ts = get_absolute_time();
}

static const uint8_t *waveform_for_speed(void) {
    return wpm > RSVP_FAST_WPM ? screen_ws_20fps : screen_ws_10fps;
}

/* Display time of the current chunk */
static uint32_t word_ms(void) {
    uint32_t base = 60000 / wpm;
    uint32_t factor = 100;  /* In percent */
    bool last_chunk = chunk == n_chunks() - 1;
    if (count_cps(word.text) > 8)
        factor = 120;
    if (last_chunk) {
        if (word.paragraph_end)
            factor = 200;
        else if (word.sentence_end)
            factor = 160;
        else if (word.clause_end)
            factor = 130;
    }
    if (first_word)
        factor *= 2;
    uint32_t ms = base * factor / 100;
    return ms < MIN_WORD_MS ? MIN_WORD_MS : ms;
}

/* Moves to the next chunk or word, returns false at the end of the text */
static bool advance(void) {
    if (have_word && chunk < n_chunks() - 1) {
        ++chunk;
        return true;
    }
    FSIZE_t before = tell_file();
    have_word = next_word(&word);
    chunk = 0;
    if (have_word) {
        ++words_read;
        bytes_read += tell_file() - before;
    }
    return have_word;
}

/* Moves to the word at this offset (\p exact), or to the first word after it (the offset may be inside a word) */
static void goto_offset(FSIZE_t offset, bool exact) {
    if (offset <= text_start) {
        offset = text_start;
        exact = true;
    }
    seek_file(offset);
    lookahead_valid = false;
    carried_newlines = 0;
    if (offset > 0 && ! exact) {
        /* We may be in the middle of a word: skip to the next space */
        int cp;
        while ((cp = get_cp()) >= 0 && ! is_space(cp))
            ;
    }
    have_word = false;
    advance();
}

static void do_seek(int direction) {
    uint32_t words = (uint32_t)wpm * RSVP_SEEK_S / 60;
    if (direction > 0) {
        for (uint32_t i = 0; i < words && advance(); ++i)
            ;
    } else {
        /* Backward: from the average length of the words read */
        uint32_t avg = words_read > 20 ? (uint32_t)(bytes_read / words_read) : AVG_BYTES_PER_WORD;
        FSIZE_t back = (FSIZE_t)words * avg;
        goto_offset(word.offset > back ? word.offset - back : 0, false);
    }
    at_end = ! have_word;
    char msg[32];
    snprintf(msg, sizeof(msg), direction > 0 ? "+%d s" : "-%d s", RSVP_SEEK_S);
    set_overlay(msg);
    printf("rsvp: seek %s, now at %lu / %lu\n", msg, (unsigned long)word.offset, (unsigned long)file_size);
}


bool rsvp_start(const char *path) {
    if (state != R_IDLE)
        return true;
    int fr = sd_mount();
    if (fr != FR_OK) {
        snprintf(message, sizeof(message), N_("Pas de carte SD"));
        return false;
    }
    if (f_open(&file, path, FA_READ) != FR_OK) {
        sd_unmount();
        snprintf(message, sizeof(message), N_("Impossible d'ouvrir le texte"));
        return false;
    }
    file_open = true;
    file_size = f_size(&file);
    detect_encoding();
    lookahead_valid = false;
    words_read = 0;
    bytes_read = 0;

    store_t *s = store_get();
    wpm = s->rsvp_wpm >= RSVP_MIN_WPM && s->rsvp_wpm <= RSVP_MAX_WPM ? s->rsvp_wpm : RSVP_DEFAULT_WPM;
    path_hash = hash(path);
    FSIZE_t resume = s->rsvp_hash == path_hash && s->rsvp_offset < file_size ? s->rsvp_offset : 0;
    have_word = false;
    goto_offset(resume, true);
    if (! have_word) {
        f_close(&file);
        file_open = false;
        snprintf(message, sizeof(message), N_("Texte vide"));
        return false;
    }
    at_end = false;
    stopping = false;
    first_word = true;
    pending_seek = 0;
    overlay[0] = 0;
    if (resume)
        set_overlay(N_("Reprise"));
    printf("rsvp: %s, %lu bytes, %s, %u wpm, from %lu\n", path, (unsigned long)file_size, latin1 ? "latin-1" : "utf-8",
           wpm, (unsigned long)resume);
    state = R_CLEAR;
    return true;
}


void rsvp_toggle_pause(void) {
    if (state == R_READING) {
        state = R_PAUSE_DRAW;
        save_position();
    } else if (state == R_PAUSED) {
        if (at_end) {
            /* At the end: start again */
            goto_offset(0, true);
            at_end = false;
        }
        first_word = true;
        state = R_START;
    }
}


void rsvp_speed(int steps) {
    int v = wpm + steps * RSVP_STEP_WPM;
    wpm = v < RSVP_MIN_WPM ? RSVP_MIN_WPM : v > RSVP_MAX_WPM ? RSVP_MAX_WPM : v;
    char msg[32];
    snprintf(msg, sizeof(msg), _("%u mots/min"), wpm);
    set_overlay(msg);
    pause_redraw = true;
}


void rsvp_seek(int direction) {
    pending_seek = direction;
    pause_redraw = true;
}


void rsvp_stop(void) {
    stopping = true;
}


const char *rsvp_message(void) {
    return message;
}


static void close_reader(void) {
    save_position();
    if (file_open)
        f_close(&file);
    file_open = false;
    state = R_IDLE;
    printf("rsvp: stopped at %lu\n", (unsigned long)word.offset);
}


bool rsvp_task(absolute_time_t now) {
    if (state == R_IDLE)
        return false;

    /* Seek requests are applied between two words */
    if (pending_seek && ! screen_busy()) {
        do_seek(pending_seek);
        pending_seek = 0;
        first_word = true;
    }

    if (stopping && state != R_STOP_END) {
        if (in_multiframe) {
            state = R_STOP_END;
        } else if (state != R_CLEAR || ! screen_busy()) {
            close_reader();
            return false;
        }
    }

    if (! screen_boot() || screen_busy())
        return true;

    char text[48], text2[48];
    bool overlay_on = overlay[0] && absolute_time_diff_us(overlay_ts, now) < OVERLAY_MS * 1000ll;

    switch (state) {
    case R_CLEAR:
        screen_clear(1);
        memset(shown, 0xFF, GFX_FB_SIZE);  /* Reference of the fast refresh: the screen is now white */
        state = R_START;
        break;
    case R_START:
        screen_clear_image_position();
        waveform = waveform_for_speed();
        screen_push_ws(waveform);
        screen_start_multiframe();
        in_multiframe = true;
        next_ts = now;
        state = R_READING;
        break;
    case R_READING:
        if (absolute_time_diff_us(now, next_ts) > 0)
            break;
        if (! have_word) {
            /* End of the text */
            at_end = true;
            state = R_PAUSE_DRAW;
            break;
        }
        if (waveform != waveform_for_speed()) {
            /* The speed crossed RSVP_FAST_WPM */
            waveform = waveform_for_speed();
            screen_push_ws(waveform);
        }
        draw_frame(overlay_on ? overlay : NULL, NULL, NULL);
        screen_push_rams(drawing, shown, GFX_FB_SIZE);
        screen_draw_multiframe();
        next_ts = delayed_by_ms(now, word_ms());
        first_word = false;
        { uint8_t *o = shown; shown = drawing; drawing = o; }
        advance();
        break;
    case R_PAUSE_DRAW:
        if (! in_multiframe) {
            /* Paused and something changed: come back to the multiframe mode to draw */
            screen_clear_image_position();
            screen_push_ws(screen_ws_10fps);
            screen_start_multiframe();
            in_multiframe = true;
            break;
        }
        snprintf(text, sizeof(text), _("%u mots/min, %u %%"), wpm,
                 file_size ? (unsigned)((uint64_t)word.offset * 100 / file_size) : 0);
        snprintf(text2, sizeof(text2), _("Flancs : vitesse, long: -/+%ds"), RSVP_SEEK_S);
        draw_frame(at_end ? N_("Fin du texte") : N_("Pause"), text, text2);
        gfx_text(drawing, GFX_WIDTH / 2, 174, &gfx_font_small,
                 at_end ? N_("D : relire  G : quitter") : N_("D : reprendre  G : quitter"),
                 GFX_BLACK, GFX_ALIGN_CENTER);
        screen_push_ws(screen_ws_10fps);  /* Best contrast for the page */
        screen_push_rams(drawing, shown, GFX_FB_SIZE);
        screen_draw_multiframe();
        { uint8_t *o = shown; shown = drawing; drawing = o; }
        pause_redraw = false;
        state = R_PAUSE_END;
        break;
    case R_PAUSE_END:
        /* Don't stay in the multiframe mode while paused */
        screen_end_multiframe();
        in_multiframe = false;
        state = R_PAUSED;
        break;
    case R_PAUSED:
        if (pause_redraw)
            state = R_PAUSE_DRAW;
        break;
    case R_STOP_END:
        screen_end_multiframe();
        in_multiframe = false;
        close_reader();
        return false;
    default:
        break;
    }
    return true;
}
