/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Parser of the RTTTL ringtones, see rtttl_parse.h. Pure C: no SDK, no allocation. */

#include <stdio.h>
#include <string.h>

#include "i18n.h"
#include "rtttl_parse.h"

#define DEFAULT_DURATION 4
#define DEFAULT_OCTAVE 6
#define DEFAULT_BPM 63
#define OCTAVE_MIN 3
#define OCTAVE_MAX 8
#define BPM_MAX 999
#define NUMBER_DIGITS 4  /* More digits: an error (no overflow) */

/* Frequencies of the octave 8 in millihertz (A8 = 7040 Hz), the lower octaves are halves */
static const uint32_t OCTAVE8_MHZ[12] = {
    4186009, 4434922, 4698636, 4978032, 5274041, 5587652, 5919911, 6271927, 6644875, 7040000, 7458620, 7902133,
};

/* Semitones of the letters a..h from C (h = b, the German name) */
static const int8_t LETTER_SEMITONE[8] = {9, 11, 0, 2, 4, 5, 7, 11};

static char at(const rtttl_t *t, size_t i) {
    return i < t->len ? t->text[i] : 0;
}

static char lower(char c) {
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

static bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static size_t skip_spaces(const rtttl_t *t, size_t i) {
    while (is_space(at(t, i)))
        ++i;
    return i;
}

/* Reads a number at *i, returns -1 when there is none or it is too long */
static int number(const rtttl_t *t, size_t *i) {
    int v = 0, digits = 0;
    while (is_digit(at(t, *i))) {
        if (++digits > NUMBER_DIGITS)
            return -1;
        v = v * 10 + (at(t, *i) - '0');
        ++*i;
    }
    return digits ? v : -1;
}

static bool valid_duration(int d) {
    return d == 1 || d == 2 || d == 4 || d == 8 || d == 16 || d == 32 || d == 64;
}

static bool valid_octave(int o) {
    return o >= OCTAVE_MIN && o <= OCTAVE_MAX;
}

static rtttl_err_t fail(rtttl_t *t, size_t pos, rtttl_err_t e) {
    t->err_pos = pos < t->len ? pos : t->len;
    return e;
}

/* Copies the name (trimmed), cut at a UTF-8 boundary */
static void copy_name(rtttl_t *t, size_t start, size_t end) {
    while (start < end && is_space(t->text[start]))
        ++start;
    while (end > start && is_space(t->text[end - 1]))
        --end;
    size_t n = end - start;
    if (n > RTTTL_NAME_MAX - 1) {
        n = RTTTL_NAME_MAX - 1;
        while (n > 0 && ((uint8_t)t->text[start + n] & 0xC0) == 0x80)
            --n;  /* Not in the middle of a character */
    }
    memcpy(t->name, t->text + start, n);
    t->name[n] = 0;
}

/* The defaults between \p i and the ':' at \p end */
static rtttl_err_t parse_defaults(rtttl_t *t, size_t i, size_t end) {
    while (i < end) {
        i = skip_spaces(t, i);
        if (i >= end)
            break;
        if (at(t, i) == ',') {
            ++i;  /* Empty item */
            continue;
        }
        size_t item = i;
        char key = lower(at(t, i));
        if (key < 'a' || key > 'z')
            return fail(t, item, RTTTL_ERR_DEFAULT);
        i = skip_spaces(t, i + 1);
        if (at(t, i) != '=')
            return fail(t, i, RTTTL_ERR_DEFAULT);
        i = skip_spaces(t, i + 1);
        size_t value_pos = i;
        int v = number(t, &i);
        if (v < 0)
            return fail(t, value_pos, RTTTL_ERR_DEFAULT);
        if (key == 'd') {
            if (! valid_duration(v))
                return fail(t, value_pos, RTTTL_ERR_DEFAULT);
            t->duration = (uint8_t)v;
        } else if (key == 'o') {
            if (! valid_octave(v))
                return fail(t, value_pos, RTTTL_ERR_DEFAULT);
            t->octave = (uint8_t)v;
        } else if (key == 'b') {
            if (v < 1 || v > BPM_MAX)
                return fail(t, value_pos, RTTTL_ERR_DEFAULT);
            t->bpm = (uint16_t)v;
        }  /* Other keys (l = loop, s = style...): ignored */
        i = skip_spaces(t, i);
        if (i < end && at(t, i) != ',')
            return fail(t, i, RTTTL_ERR_DEFAULT);
        ++i;
    }
    return RTTTL_OK;
}

rtttl_err_t rtttl_open(rtttl_t *t, const char *text, size_t len) {
    memset(t, 0, sizeof(*t));
    t->text = text;
    t->len = text ? strnlen(text, len) : 0;
    t->duration = DEFAULT_DURATION;
    t->octave = DEFAULT_OCTAVE;
    t->bpm = DEFAULT_BPM;
    size_t i = 0;
    /* UTF-8 byte order mark */
    if (t->len >= 3 && (uint8_t)text[0] == 0xEF && (uint8_t)text[1] == 0xBB && (uint8_t)text[2] == 0xBF)
        i = 3;
    size_t name_start = i;
    while (i < t->len && text[i] != ':')
        ++i;
    if (i >= t->len)
        return fail(t, t->len, RTTTL_ERR_NAME);
    copy_name(t, name_start, i);
    size_t defaults = ++i;
    while (i < t->len && text[i] != ':')
        ++i;
    if (i >= t->len)
        return fail(t, t->len, RTTTL_ERR_SECTION);
    if (skip_spaces(t, defaults) == i) {
        /* "Name: :d=4,o=5:c": an empty part, then the defaults (a section with a '=' before another ':') */
        size_t j = i + 1;
        bool equal = false;
        while (j < t->len && text[j] != ':')
            equal |= text[j++] == '=';
        if (j < t->len && equal) {
            defaults = i + 1;
            i = j;
        }
    }
    rtttl_err_t e = parse_defaults(t, defaults, i);
    if (e != RTTTL_OK)
        return e;
    t->notes_pos = t->pos = i + 1;
    return RTTTL_OK;
}

void rtttl_rewind(rtttl_t *t) {
    t->pos = t->notes_pos;
}

rtttl_err_t rtttl_next(rtttl_t *t, rtttl_note_t *note) {
    size_t i = t->pos;
    /* Spaces and empty notes */
    for (;;) {
        i = skip_spaces(t, i);
        if (at(t, i) != ',')
            break;
        ++i;
    }
    if (! at(t, i)) {
        t->pos = i;
        return RTTTL_END;
    }
    memset(note, 0, sizeof(*note));
    note->pos = (uint16_t)(i > 0xFFFF ? 0xFFFF : i);

    /* Duration */
    int duration = t->duration;
    if (is_digit(at(t, i))) {
        size_t p = i;
        duration = number(t, &i);
        if (! valid_duration(duration))
            return fail(t, p, RTTTL_ERR_DURATION);
        i = skip_spaces(t, i);
    }

    /* The dot or the sharp before the letter, written by some converters: "8.c6", "8#d4" */
    int dots = 0;
    bool sharp = false;
    for (;;) {
        char d = at(t, i);
        if (d == '.' && dots < 2)
            ++dots;
        else if (d == '#' && ! sharp)
            sharp = true;
        else
            break;
        i = skip_spaces(t, i + 1);
    }

    /* Letter and sharp ('_' in some files) */
    char c = lower(at(t, i));
    int semitone = 0;
    if (c == 'p') {
        note->rest = true;
    } else if (c >= 'a' && c <= 'h') {
        semitone = LETTER_SEMITONE[c - 'a'];
    } else {
        return fail(t, i, RTTTL_ERR_NOTE);
    }
    if (note->rest && sharp)
        return fail(t, i, RTTTL_ERR_NOTE);
    i = skip_spaces(t, i + 1);
    if ((at(t, i) == '#' || at(t, i) == '_') && ! sharp) {
        if (note->rest)
            return fail(t, i, RTTTL_ERR_NOTE);
        sharp = true;
        i = skip_spaces(t, i + 1);
    }
    if (sharp)
        ++semitone;

    /* Dots and octave, the dot before or after the octave */
    int octave = t->octave;
    bool have_octave = false;
    for (;;) {
        char d = at(t, i);
        if (d == '.' && dots < 2) {
            ++dots;
        } else if (is_digit(d) && ! have_octave) {
            size_t p = i;
            octave = number(t, &i);
            if (! valid_octave(octave))
                return fail(t, p, RTTTL_ERR_OCTAVE);
            have_octave = true;
            i = skip_spaces(t, i);
            continue;
        } else {
            break;
        }
        i = skip_spaces(t, i + 1);
    }
    if (at(t, i) && at(t, i) != ',')
        return fail(t, i, RTTTL_ERR_SEPARATOR);
    if (at(t, i) == ',')
        ++i;
    t->pos = i;

    /* Sharp of b (or e): the next octave (or f) */
    if (semitone == 12) {
        semitone = 0;
        ++octave;
    }
    note->semitone = (uint8_t)semitone;
    note->octave = (uint8_t)octave;
    if (! note->rest)
        note->freq_mhz = octave <= 8 ? OCTAVE8_MHZ[semitone] >> (8 - octave) : OCTAVE8_MHZ[semitone] << (octave - 8);
    /* A whole note lasts 4 beats; dotted: x 3/2, twice: x 7/4 */
    static const uint32_t DOT_QUARTERS[3] = {4, 6, 7};
    uint32_t ms = (240000u * DOT_QUARTERS[dots] / 4 + (uint32_t)(t->bpm * duration) / 2) / (uint32_t)(t->bpm * duration);
    note->ms = ms ? ms : 1;
    return RTTTL_OK;
}

rtttl_err_t rtttl_check(rtttl_t *t, uint32_t *n_notes, uint32_t *total_ms) {
    rtttl_note_t n;
    uint32_t count = 0, ms = 0;
    rtttl_err_t e;
    rtttl_rewind(t);
    while ((e = rtttl_next(t, &n)) == RTTTL_OK) {
        ++count;
        ms += n.ms;
    }
    rtttl_rewind(t);
    if (n_notes)
        *n_notes = count;
    if (total_ms)
        *total_ms = ms;
    if (e != RTTTL_END)
        return e;
    if (! count)
        return fail(t, t->notes_pos, RTTTL_ERR_EMPTY);
    return RTTTL_OK;
}

/* ------ PICAXE "tune" commands ------ */

#define PICAXE_QUARTER_US 73840  /* A quarter at the speed 1: sound 65.64 ms + silence 8.20 ms */

/* A number of a PICAXE program at *i: $hex, %binary or decimal; -1 when there is none or it is too large */
static long picaxe_number(const rtttl_t *t, size_t *i) {
    int base = 10;
    if (at(t, *i) == '$' || at(t, *i) == '%') {
        base = at(t, *i) == '$' ? 16 : 2;
        ++*i;
    }
    long v = 0;
    int digits = 0;
    for (;; ++*i) {
        char c = lower(at(t, *i));
        int d = is_digit(c) ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : 99;
        if (d >= base)
            break;
        if (++digits > 9)
            return -1;
        v = v * base + d;
    }
    return digits ? v : -1;
}

bool rtttl_is_picaxe(const char *line, size_t len) {
    rtttl_t t = {.text = line, .len = line ? strnlen(line, len) : 0};
    size_t i = skip_spaces(&t, 0);
    static const char TUNE[] = "tune";
    for (size_t k = 0; k < 4; ++k, ++i)
        if (lower(at(&t, i)) != TUNE[k])
            return false;
    return is_space(at(&t, i));
}

rtttl_err_t rtttl_from_picaxe(const char *line, size_t len, const char *name, char *out, size_t out_len,
                              size_t *err_pos) {
    static const char *const NOTES[12] = {"c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b"};
    static const char DURATIONS[4] = {'4', '8', '1', '2'};
    static const char OCTAVES[3] = {'5', '6', '4'};  /* Middle, high, low */
    rtttl_t t = {.text = line, .len = line ? strnlen(line, len) : 0};
    size_t i = 0;
    if (! rtttl_is_picaxe(line, len))
        goto bad;
    i = skip_spaces(&t, skip_spaces(&t, 0) + 4);
    /* The pin, then the speed */
    while (at(&t, i) && at(&t, i) != ',')
        ++i;
    if (at(&t, i) != ',')
        goto bad;
    i = skip_spaces(&t, i + 1);
    long speed = picaxe_number(&t, &i);
    if (speed < 1 || speed > 15)
        goto bad;
    i = skip_spaces(&t, i);
    if (at(&t, i) != ',')
        goto bad;
    /* The LED mask of the M2 chips, if any, before the '(' */
    while (at(&t, i) && at(&t, i) != '(')
        ++i;
    if (at(&t, i) != '(')
        goto bad;
    ++i;

    /* The name and the defaults: bpm = 60 s / (speed x 73.84 ms), rounded */
    uint32_t q = (uint32_t)speed * PICAXE_QUARTER_US;
    size_t n = 0;
    for (const char *c = name ? name : ""; *c && n < RTTTL_NAME_MAX - 1 && n + 1 < out_len; ++c)
        out[n++] = *c == ':' ? ' ' : *c;
    int w = snprintf(out + n, n < out_len ? out_len - n : 0, ":d=4,o=5,b=%lu:",
                     (unsigned long)((60000000u + q / 2) / q));
    if (w < 0 || n + (size_t)w >= out_len)
        return RTTTL_ERR_TOO_LONG;
    n += (size_t)w;

    /* The notes */
    bool first = true;
    for (;;) {
        i = skip_spaces(&t, i);
        if (at(&t, i) == ')')
            break;
        if (! first) {
            if (at(&t, i) != ',')
                goto bad;
            i = skip_spaces(&t, i + 1);
        }
        size_t value = i;
        long v = picaxe_number(&t, &i);
        if (v < 0 || v > 255 || ((v >> 4) & 3) == 3) {
            i = value;
            goto bad;
        }
        char note[8];
        if ((v & 15) >= 12)
            w = snprintf(note, sizeof(note), "%s%cp", first ? "" : ",", DURATIONS[v >> 6]);
        else
            w = snprintf(note, sizeof(note), "%s%c%s%c", first ? "" : ",", DURATIONS[v >> 6], NOTES[v & 15],
                         OCTAVES[(v >> 4) & 3]);
        if (n + (size_t)w >= out_len)
            return RTTTL_ERR_TOO_LONG;
        memcpy(out + n, note, (size_t)w + 1);
        n += (size_t)w;
        first = false;
    }
    if (first)
        goto bad;  /* No note */
    return RTTTL_OK;

bad:
    if (err_pos)
        *err_pos = i < t.len ? i : t.len;
    return RTTTL_ERR_PICAXE;
}

const char *rtttl_error_text(rtttl_err_t e) {
    switch (e) {
    case RTTTL_OK: return "ok";
    case RTTTL_END: return N_("fin");
    case RTTTL_ERR_NAME: return N_("':' manquant après le nom");
    case RTTTL_ERR_SECTION: return N_("':' manquant avant les notes");
    case RTTTL_ERR_DEFAULT: return N_("réglage d, o ou b invalide");
    case RTTTL_ERR_DURATION: return N_("durée invalide");
    case RTTTL_ERR_NOTE: return N_("note invalide");
    case RTTTL_ERR_OCTAVE: return N_("octave invalide");
    case RTTTL_ERR_SEPARATOR: return N_("',' attendue après la note");
    case RTTTL_ERR_EMPTY: return N_("aucune note");
    case RTTTL_ERR_TOO_LONG: return N_("ligne trop longue");
    case RTTTL_ERR_PICAXE: return N_("commande tune PICAXE invalide");
    }
    return N_("erreur");
}

void rtttl_note_label(const rtttl_note_t *n, char *buf, size_t len) {
    static const char *const NAMES[12] = {"Do", "Do#", "Ré", "Ré#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si"};
    if (n->rest)
        snprintf(buf, len, N_("Silence"));
    else
        snprintf(buf, len, "%s%u", NAMES[n->semitone % 12], n->octave);
}


/* ------ The tunes in the firmware: public domain melodies (traditional, or composers dead for long) ------ */

const char *const RTTTL_BUILTIN[] = {
    /* Beethoven, 1810 */
    "Lettre à Élise:d=16,o=5,b=125:e6,d#6,e6,d#6,e6,b,d6,c6,8a,p,c,e,a,8b,p,e,g#,b,8c6,p,e,e6,d#6,e6,d#6,e6,b,d6,c6,"
    "8a,p,c,e,a,8b,p,e,c6,b,4a.",
    /* Beethoven, 9th symphony, 1824 */
    "Ode à la joie:d=4,o=5,b=120:e,e,f,g,g,f,e,d,c,c,d,e,e.,8d,2d,e,e,f,g,g,f,e,d,c,c,d,e,d.,8c,2c,"
    "d,d,e,c,d,8e,8f,e,c,d,8e,8f,e,d,c,d,2g4,e,e,f,g,g,f,e,d,c,c,d,e,d.,8c,2c",
    /* Traditional */
    "Frère Jacques:d=4,o=5,b=120:c,d,e,c,c,d,e,c,e,f,2g,e,f,2g,8g,8a,8g,8f,e,c,8g,8a,8g,8f,e,c,c,g4,2c,c,g4,2c",
    /* Traditional */
    "Au clair de la lune:d=4,o=5,b=112:c,c,c,d,2e,2d,c,e,d,d,1c,c,c,c,d,2e,2d,c,e,d,d,1c,"
    "d,d,d,d,2a4,2a4,d,c,b4,a4,1g4,c,c,c,d,2e,2d,c,e,d,d,1c",
    /* Traditional (Mozart wrote variations on it) */
    "Ah ! vous dirai-je, maman:d=4,o=5,b=120:c,c,g,g,a,a,2g,f,f,e,e,d,d,2c,g,g,f,f,e,e,2d,g,g,f,f,e,e,2d,"
    "c,c,g,g,a,a,2g,f,f,e,e,d,d,2c",
    /* Rouget de Lisle, 1792 */
    "La Marseillaise:d=4,o=5,b=100:8d.,16d,g,g,a,a,d.6,8b,8g.,16g,8b.,16g,e,2c6,8a.,16f#,2g,p,8g.,16a,"
    "b,b,b,8c6.,16b,b,a,p,8a.,16b,c6,c6,c6,8d6.,16c6,2b",
    /* Russian folk song (the Tetris theme) */
    "Korobeiniki:d=4,o=5,b=160:e6,8b,8c6,d6,8c6,8b,a,8a,8c6,e6,8d6,8c6,b.,8c6,d6,e6,c6,a,2a,"
    "8p,d6,8f6,a6,8g6,8f6,e6.,8c6,e6,8d6,8c6,b,8b,8c6,d6,e6,c6,a,2a",
    /* English, 16th century */
    "Greensleeves:d=4,o=5,b=140:a,2c6,d6,e6.,8f6,e6,2d6,b,g.,8a,b,2c6,a,a.,8g#,a,2b,g#,2e,a,"
    "2c6,d6,e6.,8f6,e6,2d6,b,g.,8a,b,c6.,8b,a,g#.,8f#,g#,2a.,2a.",
    /* Mexican folk song */
    "La Cucaracha:d=4,o=5,b=140:8c,8c,8c,f.,a.,8c,8c,8c,f.,a.,8f,8f,8e,8e,8d,8d,2c.,"
    "8c,8c,8c,e.,g.,8c,8c,8c,e.,g.,8c6,8d6,8c6,8a#,8a,8g,2f.",
    /* Mozart, 1787 */
    "Petite musique de nuit:d=4,o=5,b=140:g,8p,8d,g,8p,8d,8g,8d,8g,8b,2d6,c6,8p,8a,c6,8p,8a,8c6,8a,8f#,8a,2d",
    /* Grieg, Peer Gynt, 1875 */
    "Roi de la montagne:d=8,o=5,b=120:b4,c#,d,e,f#,d,4f#,f,c#,4f,e,c,4e,b4,c#,d,e,f#,d,f#,b,a,f#,d,f#,2a",
    /* Pierpont, 1857 */
    "Jingle Bells:d=4,o=5,b=170:e,e,2e,e,e,2e,e,g,c.,8d,1e,f,f,f.,8f,f,e,e,8e,8e,e,d,d,e,2d,2g,"
    "e,e,2e,e,e,2e,e,g,c.,8d,1e,f,f,f,f,f,e,e,8e,8e,g,g,f,d,1c",
};

const int RTTTL_N_BUILTIN = sizeof(RTTTL_BUILTIN) / sizeof(RTTTL_BUILTIN[0]);
