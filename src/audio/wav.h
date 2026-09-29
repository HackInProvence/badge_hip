/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file wav.h
 *
 * \brief WAV player: plays PCM WAV files from the SD card on the buzzer (see audio.h).
 *
 * Supports PCM 8 bit (unsigned) or 16 bit (signed), mono or stereo (mixed), up to 48kHz.
 * Use audio2wav.py to convert MP3 and other formats: 8 bit mono 16kHz is enough for the buzzer.
 *
 * Usage: wav_start("MUSIQUE/song.wav"), then call wav_task() in the main loop while it returns true.
 * */

#ifndef _WAV_H
#define _WAV_H

#include <stdbool.h>
#include <stdint.h>

/** \brief Mount the card, open the file and start playing. \return false on error, see wav_message() */
bool wav_start(const char *path);

/** \brief Feeds the sound, to call in the main loop. \return true while playing (or paused) */
bool wav_task(void);

void wav_stop(void);
void wav_toggle_pause(void);
bool wav_is_paused(void);

/** \brief Position and duration, in seconds. */
uint32_t wav_position_s(void);
uint32_t wav_duration_s(void);

/** \brief The error, when wav_start() failed. */
const char *wav_message(void);

#endif /* _WAV_H */
