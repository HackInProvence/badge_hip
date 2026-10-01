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

/** \brief Start playing at this sample rate (silence until samples are written), on the outputs chosen by
 * audio_set_outputs().
 * \return false when no DMA channel or timer is available */
bool audio_open(uint32_t sample_rate);

/** \brief Stop playing and release the buzzer pin (driven low). */
void audio_close(void);

bool audio_is_open(void);

/** \brief A square wave of \p hz on the buzzer, straight from the PWM (above the sample rates, e.g. 19 kHz that
 * a phone hears but hardly a human), 0 to stop. Closes the audio playback; silent in mute mode. */
void audio_pwm_tone(uint32_t hz);

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


/* ------ Outputs: the buzzer and/or the radio (pirate radio, see menu/pirate_radio.c) ------
 * The radio output is a fast PWM (clk_sys / (AUDIO_RADIO_WRAP + 1), ~122 kHz at 125 MHz) on the GDO0 pin of the
 * CC1101 (its asynchronous TX data input in 2-FSK: high = f0 + deviation, low = f0 - deviation): its duty cycle
 * follows the samples (0 to 255 -> 0 to 100 %), so that the average frequency follows the sound: narrow FM.
 * A second DMA channel and timer copy the samples to it, in step with the buzzer. Neither the volume nor the mute mode
 * change it (the deviation is the level of the transmission). While the audio is closed, the duty cycle stays at
 * 50 % (the carrier on f0, without modulation). Default: the buzzer only, as before. */

#define AUDIO_OUT_SPEAKER 0x01
#define AUDIO_OUT_RADIO 0x02
#define AUDIO_RADIO_WRAP 1023  /* 10 bit duty cycle: 128 (silence) = 512 = 50 % */

/** \brief Chooses the outputs (AUDIO_OUT_* flags, 0 = the buzzer), call it while the audio is closed: the radio
 * output takes GDO0 at once (PWM at 50 %), leaving it releases GDO0 (input). The buzzer stays silent without
 * AUDIO_OUT_SPEAKER (the samples still give the clock). */
void audio_set_outputs(uint8_t outputs);
uint8_t audio_get_outputs(void);

#endif /* _AUDIO_H */
