/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file rtttl_parse.h
 *
 * \brief Parser of the RTTTL ringtones (Nokia Ring Tone Text Transfer Language), pure C (host tests: test_rtttl.c).
 *
 * "name:d=4,o=5,b=63:16e6,16d#6,8p,4a.,2c.7"
 * - name: anything up to the first ':' (trimmed);
 * - defaults, comma separated, any order, each optional: d = duration (1, 2, 4, 8, 16, 32, 64; default 4),
 *   o = octave (3..8; default 6), b = beats (quarter notes) per minute (1..999; default 63). Other keys "x=n"
 *   (e.g. l = loop in some files) are ignored;
 * - notes, comma separated: [duration] letter [#] [.] [octave] [.]: letters c d e f g a b (h = b) and p (pause),
 *   '#' = sharp, '.' = dotted (x 1.5, twice x 1.75), before or after the octave.
 * Case insensitive, spaces anywhere between the elements, empty notes (",,") ignored.
 * Variants of some converters, accepted too: '_' for the sharp ("f_5"), the sharp or the dot before the letter
 * ("8#d4", "8.c6"), an empty part between the name and the defaults ("Name: :d=4,o=5:c").
 * A4 = 440 Hz (scientific pitch notation: "a5" = 880 Hz).
 *
 * Nothing is allocated: the text is read where it is, the notes are given one by one by rtttl_next().
 * Errors give the offset of the faulty character in the text.
 * */

#ifndef _RTTTL_PARSE_H
#define _RTTTL_PARSE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RTTTL_NAME_MAX 40  /* Bytes of the name, with the final 0 (longer names are cut, at a UTF-8 boundary) */

typedef enum {
    RTTTL_OK = 0,
    RTTTL_END,  /* rtttl_next(): no more notes */
    RTTTL_ERR_NAME,  /* No ':' after the name */
    RTTTL_ERR_SECTION,  /* No ':' between the defaults and the notes */
    RTTTL_ERR_DEFAULT,  /* A default is not "key=number" or its value is out of range */
    RTTTL_ERR_DURATION,  /* Duration of a note not in 1, 2, 4... 64 */
    RTTTL_ERR_NOTE,  /* Not a note letter */
    RTTTL_ERR_OCTAVE,  /* Octave not in 3..8 */
    RTTTL_ERR_SEPARATOR,  /* Something after a note before the ',' */
    RTTTL_ERR_EMPTY,  /* No note */
    RTTTL_ERR_TOO_LONG,  /* (used by the readers: the line is too long for their buffer) */
    RTTTL_ERR_PICAXE,  /* rtttl_from_picaxe(): not a valid PICAXE tune command */
} rtttl_err_t;

typedef struct {
    bool rest;
    uint8_t semitone;  /* 0 = C (Do) .. 11 = B (Si), after the sharp ("b#5" = C6) */
    uint8_t octave;
    uint32_t freq_mhz;  /* Frequency in millihertz, 0 for a pause */
    uint32_t ms;  /* Duration */
    uint16_t pos;  /* Offset of the note in the text */
} rtttl_note_t;

typedef struct {
    char name[RTTTL_NAME_MAX];
    uint8_t duration;  /* Defaults */
    uint8_t octave;
    uint16_t bpm;
    const char *text;
    size_t len;
    size_t notes_pos;  /* Offset of the notes */
    size_t pos;  /* Next note for rtttl_next() */
    size_t err_pos;  /* Offset of the error */
} rtttl_t;

/** \brief Parses the name and the defaults of \p text (\p len bytes, or up to a 0), ready for rtttl_next().
 * The text must stay in place while the notes are read.
 * \return RTTTL_OK, or an error (offset in t->err_pos) */
rtttl_err_t rtttl_open(rtttl_t *t, const char *text, size_t len);

/** \brief The next note. \return RTTTL_OK, RTTTL_END after the last one, or an error (offset in t->err_pos) */
rtttl_err_t rtttl_next(rtttl_t *t, rtttl_note_t *note);

/** \brief Back to the first note. */
void rtttl_rewind(rtttl_t *t);

/** \brief Checks all the notes (after rtttl_open()), then rewinds.
 * \param n_notes Receives the number of notes and pauses (NULL: not needed)
 * \param total_ms Receives the total duration (NULL: not needed)
 * \return RTTTL_OK, or the first error (offset in t->err_pos), RTTTL_ERR_EMPTY without notes */
rtttl_err_t rtttl_check(rtttl_t *t, uint32_t *n_notes, uint32_t *total_ms);

/** \brief Whether \p line (\p len bytes, or up to a 0) is a PICAXE "tune" command (BASIC of the PICAXE chips). */
bool rtttl_is_picaxe(const char *line, size_t len);

/** \brief Converts a PICAXE "tune pin, speed, [mask,] ($xx, ...)" command (.bas files) into an RTTTL text.
 * The note bytes (PICAXE manual 2, "tune"): bits 7-6 the duration (00 = 1/4, 01 = 1/8, 10 = 1, 11 = 1/2), bits 5-4
 * the octave (00 = middle, 01 = high, 10 = low; middle C = 523 Hz: "c5" here), bits 3-0 the note (0 = C .. 11 = B,
 * 12-15 = pause). The speed (1-15): a quarter lasts speed x 73.84 ms. Values in hexadecimal ($), binary (%) or
 * decimal.
 * \param name Name of the tune (a ':' in it is replaced)
 * \param out Receives "name:d=4,o=5,b=bpm:notes"
 * \return RTTTL_OK, RTTTL_ERR_PICAXE (with the offset in *err_pos when not NULL) or RTTTL_ERR_TOO_LONG */
rtttl_err_t rtttl_from_picaxe(const char *line, size_t len, const char *name, char *out, size_t out_len,
                              size_t *err_pos);

/** \brief Short French text of an error, for the screen (e.g. "durée invalide"). */
const char *rtttl_error_text(rtttl_err_t e);

/** \brief French name of a note with its octave ("La#5", "Do6"), or "Silence" for a pause. */
void rtttl_note_label(const rtttl_note_t *n, char *buf, size_t len);

/** \brief The tunes in the firmware (public domain melodies), checked by the host tests. */
extern const char *const RTTTL_BUILTIN[];
extern const int RTTTL_N_BUILTIN;

#endif /* _RTTTL_PARSE_H */
