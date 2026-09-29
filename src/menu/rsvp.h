/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file rsvp.h
 *
 * \brief Fast reading (PVSR, Présentation Visuelle Sérielle Rapide, RSVP in English): the words of a text file
 * are shown one at a time at the same place, so that the eyes don't move. See docs/pvsr.md.
 *
 * - Each word is aligned on its Optimal Recognition Point (ORP), the letter the eye recognizes the word from,
 *   shown between the marks of a fixed reticle, and underlined.
 * - Each word stays 60000/wpm ms, longer for long words, before a comma, at the end of a sentence or of a paragraph.
 * - Words longer than 13 characters are split with a hyphen.
 * - The texts are .TXT files (UTF-8 or Latin-1/Windows-1252, detected), read word by word from the SD card:
 *   their size does not matter.
 * - The speed and the position in the last text are saved (store.h).
 *
 * The e-Paper screen is used in multiframe mode (~100ms per word with the 10 fps waveform,
 * the 20 fps one above RSVP_FAST_WPM): the reader owns the screen while it runs.
 * */

#ifndef _RSVP_H
#define _RSVP_H

#include <stdbool.h>

#include "pico/time.h"

#define RSVP_MIN_WPM 100
#define RSVP_MAX_WPM 900
#define RSVP_STEP_WPM 25
#define RSVP_DEFAULT_WPM 250
#define RSVP_FAST_WPM 450  /* Above, the faster waveform (less contrast) */
#define RSVP_SEEK_S 10  /* Long press on a flank: back / forward of this reading time */

/** \brief Open the text and start reading (from the saved position when it is the last text read). */
bool rsvp_start(const char *path);

/** \brief Run the reader, to call in the main loop. \return true while it runs (it owns the screen) */
bool rsvp_task(absolute_time_t now);

void rsvp_toggle_pause(void);

/** \brief Change the speed by \p steps of RSVP_STEP_WPM. */
void rsvp_speed(int steps);

/** \brief Go back (< 0) or forward (> 0) of RSVP_SEEK_S seconds of reading. */
void rsvp_seek(int direction);

/** \brief Stop (keep calling rsvp_task() until it returns false). */
void rsvp_stop(void);

/** \brief The error when rsvp_start() failed. */
const char *rsvp_message(void);

#endif /* _RSVP_H */
