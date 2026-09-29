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


/* The PWM level of a sample: the volume scales the whole signal (also its DC part, which only heats the buzzer) */
static inline uint16_t level(uint8_t sample) {
    return (uint16_t)((sample * volume) / AUDIO_VOLUME_MAX);
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

    written = UNDERRUN_SILENCE;  /* Start with a bit of silence */
    silence = UNDERRUN_SILENCE;
    dma_channel_start(dma_chan);
    log_info("audio: open at %lu Hz (timer %u/%u)", (unsigned long)sample_rate, x, y);
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
    /* Release the buzzer: no current in its coil */
    pwm_set_enabled(slice, false);
    gpio_init(BADGE_BUZZER);
    gpio_put(BADGE_BUZZER, 0);
    gpio_set_dir(BADGE_BUZZER, GPIO_OUT);
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
            ring[(written++) % AUDIO_RING_SIZE] = level(128);
    }
    /* Keep a few samples between the writer and the reader */
    size_t used = written - played;
    return used + SILENCE_AHEAD + 16 < AUDIO_RING_SIZE ? AUDIO_RING_SIZE - used - SILENCE_AHEAD - 16 : 0;
}


void audio_write(const uint8_t *samples, size_t n) {
    for (size_t i = 0; i < n; ++i)
        ring[(written++) % AUDIO_RING_SIZE] = level(samples[i]);
    for (size_t i = 0; i < SILENCE_AHEAD; ++i)
        ring[(written + i) % AUDIO_RING_SIZE] = level(128);
}


void audio_set_volume(uint8_t v) {
    volume = v > AUDIO_VOLUME_MAX ? AUDIO_VOLUME_MAX : v;
}


uint8_t audio_get_volume(void) {
    return volume;
}
