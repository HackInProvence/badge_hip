/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file audio.h
 *
 * \brief Audio API: plays 8 bit samples on the buzzer, like a small class D amplifier.
 *
 * The buzzer is driven by a MOSFET (see the schematic), so its PWM duty cycle can follow the samples:
 * the PWM runs at ~61kHz (inaudible) and a DMA channel, paced by a DMA timer at the sample rate,
 * copies the samples from a ring buffer to the PWM compare register. No CPU is used while playing.
 * The buzzer only renders ~300Hz to ~5kHz: 16kHz mono is enough (see audio2wav.py).
 *
 * The producer writes samples with audio_write() when audio_free() allows it.
 * When the producer is late, silence is played (no garbage).
 *
 * The buzzer pin is shared with the cicada (noise_gen) and the melodies (music): only use one at a time.
 * */

#ifndef _AUDIO_H
#define _AUDIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AUDIO_RING_BITS 13  /* 8192 samples: 0.5s at 16kHz */
#define AUDIO_RING_SIZE (1u << AUDIO_RING_BITS)
#define AUDIO_VOLUME_MAX 8

/** \brief Start playing at this sample rate (silence until samples are written).
 * \return false when no DMA channel or timer is available */
bool audio_open(uint32_t sample_rate);

/** \brief Stop playing and release the buzzer pin (driven low). */
void audio_close(void);

bool audio_is_open(void);

/** \brief Number of samples that can be written now. */
size_t audio_free(void);

/** \brief Queue 8 bit unsigned samples (128 = silence), at most audio_free(). */
void audio_write(const uint8_t *samples, size_t n);

/** \brief Number of written samples played since audio_open() (the clock for synchronization, silences excluded). */
uint32_t audio_played(void);

/** \brief Number of samples written but not played yet. */
size_t audio_queued(void);

void audio_set_volume(uint8_t volume);
uint8_t audio_get_volume(void);

/* Mute (the samples still play silently: the players keep their clock) */
void audio_set_mute(bool muted);

#endif /* _AUDIO_H */
