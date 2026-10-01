/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/binary_info.h"

#include "audio.h"
#include "log.h"
#include "pinouts.h"


#define PWM_WRAP 255  /* 8 bit samples */
#define PWM_CLKDIV 8  /* 125MHz/8/256 = 61kHz carrier: inaudible, and slow enough for the MOSFET gate */
#define UNDERRUN_SILENCE 256  /* Samples of silence queued when the producer is late */

/* The DMA reads the ring buffer in a loop: it must be aligned on its size */
static uint16_t ring[AUDIO_RING_SIZE] __attribute__((aligned(AUDIO_RING_SIZE * sizeof(uint16_t))));
static int dma_chan = -1;
static int dma_timer = -1;
static uint slice = 0;
static uint32_t written = 0;  /* Samples written since open */
static uint32_t silence = 0;  /* Samples of silence inserted by this module (start, underruns) */
#define SILENCE_AHEAD 512  /* Silence written after the samples: a late producer plays silence, not old samples */
static uint8_t volume = AUDIO_VOLUME_MAX * 3 / 4;
static volatile bool muted = false;  /* The samples still play (the players use them as a clock), silently */

/* The radio output (pirate radio): its own ring, read at the same index by a second DMA channel */
static uint8_t outputs = AUDIO_OUT_SPEAKER;
static uint16_t radio_ring[AUDIO_RING_SIZE] __attribute__((aligned(AUDIO_RING_SIZE * sizeof(uint16_t))));
static int radio_chan = -1;
static int radio_timer = -1;
static uint radio_slice = 0;
#define RADIO_CENTER ((AUDIO_RADIO_WRAP + 1) / 2)


/* The PWM level of a sample: the volume scales the whole signal (also its DC part, which only heats the buzzer) */
static inline uint16_t level(uint8_t sample) {
    return muted || ! (outputs & AUDIO_OUT_SPEAKER) ? 0 : (uint16_t)((sample * volume) / AUDIO_VOLUME_MAX);
}

/* The duty cycle of the radio output: the full scale, 128 = 50 % */
static inline uint16_t radio_level(uint8_t sample) {
    return (uint16_t)(RADIO_CENTER + ((int)sample - 128) * (int)RADIO_CENTER / 128);
}

/* A sample at the index \p i of the rings */
static inline void put(uint32_t i, uint8_t sample) {
    ring[i % AUDIO_RING_SIZE] = level(sample);
    if (radio_chan >= 0)
        radio_ring[i % AUDIO_RING_SIZE] = radio_level(sample);
}


static void radio_dma_stop(void) {
    if (radio_chan >= 0) {
        dma_channel_abort(radio_chan);
        dma_channel_unclaim(radio_chan);
        radio_chan = -1;
    }
    if (radio_timer >= 0) {
        dma_timer_unclaim(radio_timer);
        radio_timer = -1;
    }
    if (outputs & AUDIO_OUT_RADIO)
        pwm_set_chan_level(radio_slice, pwm_gpio_to_channel(BADGE_RADIO_GDO0), RADIO_CENTER);  /* Unmodulated */
}


void audio_set_outputs(uint8_t o) {
    if (! o)
        o = AUDIO_OUT_SPEAKER;
    if ((o & AUDIO_OUT_RADIO) && ! (outputs & AUDIO_OUT_RADIO)) {
        /* GDO0 to a fast PWM at 50 % (only channel A or B of the slice is connected to a pin) */
        radio_slice = pwm_gpio_to_slice_num(BADGE_RADIO_GDO0);
        pwm_config pc = pwm_get_default_config();
        pwm_config_set_clkdiv_int(&pc, 1);
        pwm_config_set_wrap(&pc, AUDIO_RADIO_WRAP);
        pwm_init(radio_slice, &pc, false);
        pwm_set_chan_level(radio_slice, pwm_gpio_to_channel(BADGE_RADIO_GDO0), RADIO_CENTER);
        pwm_set_enabled(radio_slice, true);
        gpio_set_function(BADGE_RADIO_GDO0, GPIO_FUNC_PWM);
    } else if (! (o & AUDIO_OUT_RADIO) && (outputs & AUDIO_OUT_RADIO)) {
        radio_dma_stop();
        pwm_set_enabled(radio_slice, false);
        gpio_init(BADGE_RADIO_GDO0);  /* Input again (the CC1101 drives it in the other modes) */
    }
    outputs = o;
}


uint8_t audio_get_outputs(void) {
    return outputs;
}


/* Finds X/Y (16 bits each) so that clk_sys*X/Y is the closest to the sample rate */
static void timer_fraction(uint32_t rate, uint16_t *x, uint16_t *y) {
    uint32_t clk = clock_get_hz(clk_sys);
    uint64_t best_err = UINT64_MAX;
    for (uint32_t i = 1; i < 0x10000; ++i) {
        uint64_t j = ((uint64_t)clk * i + rate / 2) / rate;
        if (j > 0xFFFF)
            break;
        uint64_t f = (uint64_t)clk * i / j;  /* Achieved rate */
        uint64_t err = f > rate ? f - rate : rate - f;
        if (err < best_err) {
            best_err = err;
            *x = i;
            *y = j;
        }
        if (err == 0)
            break;
    }
}


bool audio_open(uint32_t sample_rate) {
    if (dma_chan >= 0)
        audio_close();

    bi_decl_if_func_used(bi_1pin_with_func(BADGE_BUZZER, GPIO_FUNC_PWM));
    dma_chan = dma_claim_unused_channel(false);
    dma_timer = dma_claim_unused_timer(false);
    if (dma_chan < 0 || dma_timer < 0) {
        log_warning("audio: no DMA channel or timer available");
        audio_close();
        return false;
    }

    /* PWM on the buzzer, starts silent */
    for (size_t i = 0; i < AUDIO_RING_SIZE; ++i)
        ring[i] = level(128);
    slice = pwm_gpio_to_slice_num(BADGE_BUZZER);
    pwm_config pc = pwm_get_default_config();
    pwm_config_set_clkdiv_int(&pc, PWM_CLKDIV);
    pwm_config_set_wrap(&pc, PWM_WRAP);
    pwm_init(slice, &pc, true);
    pwm_set_gpio_level(BADGE_BUZZER, 0);
    gpio_set_function(BADGE_BUZZER, GPIO_FUNC_PWM);

    /* DMA timer at the sample rate */
    uint16_t x = 1, y = 0xFFFF;
    timer_fraction(sample_rate, &x, &y);
    dma_timer_set_fraction(dma_timer, x, y);

    /* DMA: ring buffer -> PWM compare register (16 bit writes are replicated on both channels A and B,
     *  B is not connected to its pin) */
    dma_channel_config dc = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dc, DMA_SIZE_16);
    channel_config_set_read_increment(&dc, true);
    channel_config_set_write_increment(&dc, false);
    channel_config_set_ring(&dc, false, AUDIO_RING_BITS + 1);  /* Wrap the read address, the ring is 2^(bits+1) bytes */
    channel_config_set_dreq(&dc, dma_get_timer_dreq(dma_timer));
    dma_channel_configure(dma_chan, &dc, &pwm_hw->slice[slice].cc, ring, 0xFFFFFFFF, false);

    uint32_t mask = 1u << dma_chan;
    if (outputs & AUDIO_OUT_RADIO) {
        /* The same for the radio output: its own timer with the same fraction, started with the buzzer (the two
         * channels read the same index of their rings) */
        radio_chan = dma_claim_unused_channel(false);
        radio_timer = dma_claim_unused_timer(false);
        if (radio_chan < 0 || radio_timer < 0) {
            log_warning("audio: no DMA channel or timer available for the radio output");
            audio_close();
            return false;
        }
        for (size_t i = 0; i < AUDIO_RING_SIZE; ++i)
            radio_ring[i] = RADIO_CENTER;
        dma_timer_set_fraction(radio_timer, x, y);
        dma_channel_config rc = dma_channel_get_default_config(radio_chan);
        channel_config_set_transfer_data_size(&rc, DMA_SIZE_16);
        channel_config_set_read_increment(&rc, true);
        channel_config_set_write_increment(&rc, false);
        channel_config_set_ring(&rc, false, AUDIO_RING_BITS + 1);
        channel_config_set_dreq(&rc, dma_get_timer_dreq(radio_timer));
        dma_channel_configure(radio_chan, &rc, &pwm_hw->slice[radio_slice].cc, radio_ring, 0xFFFFFFFF, false);
        mask |= 1u << radio_chan;
    }

    written = UNDERRUN_SILENCE;  /* Start with a bit of silence */
    silence = UNDERRUN_SILENCE;
    dma_start_channel_mask(mask);
    log_info("audio: open at %lu Hz (timer %u/%u)%s", (unsigned long)sample_rate, x, y,
             (outputs & AUDIO_OUT_RADIO) ? ", radio output" : "");
    return true;
}


void audio_close(void) {
    if (dma_chan >= 0) {
        dma_channel_abort(dma_chan);
        dma_channel_unclaim(dma_chan);
        dma_chan = -1;
    }
    if (dma_timer >= 0) {
        dma_timer_unclaim(dma_timer);
        dma_timer = -1;
    }
    radio_dma_stop();  /* The radio output (if any) stays at 50 % */
    /* Release the buzzer: no current in its coil */
    pwm_set_enabled(slice, false);
    gpio_init(BADGE_BUZZER);
    gpio_put(BADGE_BUZZER, 0);
    gpio_set_dir(BADGE_BUZZER, GPIO_OUT);
}


void audio_pwm_tone(uint32_t hz) {
    if (dma_chan >= 0)
        audio_close();
    if (! hz || muted) {
        pwm_set_enabled(slice, false);
        gpio_init(BADGE_BUZZER);
        gpio_put(BADGE_BUZZER, 0);
        gpio_set_dir(BADGE_BUZZER, GPIO_OUT);
        return;
    }
    /* A square wave straight from the PWM: up to ~65k cycles of the system clock per period (no sample rate) */
    slice = pwm_gpio_to_slice_num(BADGE_BUZZER);
    uint32_t wrap = clock_get_hz(clk_sys) / hz - 1;
    if (wrap > 0xFFFF)
        wrap = 0xFFFF;
    pwm_config pc = pwm_get_default_config();
    pwm_config_set_wrap(&pc, wrap);
    pwm_init(slice, &pc, true);
    pwm_set_gpio_level(BADGE_BUZZER, (wrap + 1) / 2);
    gpio_set_function(BADGE_BUZZER, GPIO_FUNC_PWM);
}


bool audio_is_open(void) {
    return dma_chan >= 0;
}


/* All the samples played by the DMA, including the silences */
static uint32_t dma_played(void) {
    return 0xFFFFFFFF - dma_channel_hw_addr(dma_chan)->transfer_count;
}


uint32_t audio_played(void) {
    if (dma_chan < 0)
        return 0;
    int32_t played = (int32_t)(dma_played() - silence);
    return played > 0 ? (uint32_t)played : 0;
}


size_t audio_queued(void) {
    if (dma_chan < 0)
        return 0;
    int32_t q = (int32_t)(written - dma_played());
    return q > 0 ? (size_t)q : 0;
}


size_t audio_free(void) {
    if (dma_chan < 0)
        return 0;
    uint32_t played = dma_played();
    if ((int32_t)(written - played) < 0) {
        /* Underrun: the DMA played silence after the last samples, restart a bit ahead with more silence */
        silence += played - written + UNDERRUN_SILENCE;
        written = played;
        for (int i = 0; i < UNDERRUN_SILENCE; ++i)
            put(written++, 128);
    }
    /* Keep a few samples between the writer and the reader */
    size_t used = written - played;
    return used + SILENCE_AHEAD + 16 < AUDIO_RING_SIZE ? AUDIO_RING_SIZE - used - SILENCE_AHEAD - 16 : 0;
}


void audio_write(const uint8_t *samples, size_t n) {
    for (size_t i = 0; i < n; ++i)
        put(written++, samples[i]);
    for (size_t i = 0; i < SILENCE_AHEAD; ++i)
        put(written + i, 128);
}


void audio_set_volume(uint8_t v) {
    volume = v > AUDIO_VOLUME_MAX ? AUDIO_VOLUME_MAX : v;
}


void audio_set_mute(bool m) {
    muted = m;
}


uint8_t audio_get_volume(void) {
    return volume;
}
