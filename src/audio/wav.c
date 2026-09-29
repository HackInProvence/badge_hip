/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "ff.h"
#include "sd.h"
#include "wav.h"


#define READ_CHUNK 1024  /* Bytes read from the card at once */

static FIL file;
static bool playing = false;
static bool paused = false;
static uint32_t rate = 0;
static uint16_t channels = 0;
static uint16_t bits = 0;
static uint32_t data_left = 0;  /* Bytes of samples not read yet */
static uint32_t data_size = 0;
static FSIZE_t data_offset = 0;  /* Position of the first sample in the file */
static uint32_t start_sample = 0;  /* Samples played before the last (re)start */
static char message[64] = "";


static uint32_t le32(const uint8_t *p) {
    return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint16_t le16(const uint8_t *p) {
    return p[0] | p[1] << 8;
}

static bool fail(const char *msg) {
    snprintf(message, sizeof(message), "%s", msg);
    printf("wav: %s\n", msg);
    return false;
}


/* Parses the RIFF chunks until the data chunk, the file is then positioned on the samples */
static bool parse_header(void) {
    uint8_t buf[16];
    UINT n;
    if (f_read(&file, buf, 12, &n) != FR_OK || n != 12 || memcmp(buf, "RIFF", 4) || memcmp(buf + 8, "WAVE", 4))
        return fail("Fichier WAV invalide");

    bool fmt_found = false;
    while (true) {
        if (f_read(&file, buf, 8, &n) != FR_OK || n != 8)
            return fail("Pas de données dans le WAV");
        uint32_t size = le32(buf + 4);
        if (! memcmp(buf, "fmt ", 4)) {
            if (size < 16 || f_read(&file, buf, 16, &n) != FR_OK || n != 16)
                return fail("Fichier WAV invalide");
            uint16_t format = le16(buf);
            channels = le16(buf + 2);
            rate = le32(buf + 4);
            bits = le16(buf + 14);
            if ((format != 1 && format != 0xFFFE) || (bits != 8 && bits != 16) || channels < 1 || channels > 2
                    || rate < 4000 || rate > 48000)
                return fail("WAV non supporté (PCM 8/16 bits)");
            f_lseek(&file, f_tell(&file) + size - 16 + (size & 1));
            fmt_found = true;
        } else if (! memcmp(buf, "data", 4)) {
            if (! fmt_found)
                return fail("Fichier WAV invalide");
            data_size = data_left = size;
            data_offset = f_tell(&file);
            return true;
        } else {
            f_lseek(&file, f_tell(&file) + size + (size & 1));  /* Chunks are padded to even sizes */
        }
    }
}


bool wav_start(const char *path) {
    wav_stop();
    int fr = sd_mount();
    if (fr != FR_OK)
        return fail(fr == FR_NO_FILESYSTEM ? "Carte SD non formatée (FAT/exFAT)" : "Pas de carte SD");
    if (f_open(&file, path, FA_READ) != FR_OK) {
        sd_unmount();
        return fail("Impossible d'ouvrir le fichier");
    }
    if (! parse_header()) {
        f_close(&file);
        return false;
    }
    if (! audio_open(rate)) {
        f_close(&file);
        return fail("Audio indisponible");
    }
    playing = true;
    paused = false;
    start_sample = 0;
    printf("wav: playing %s, %lu Hz, %u bits, %u channel(s), %lus\n", path, (unsigned long)rate, bits, channels,
           (unsigned long)wav_duration_s());
    return true;
}


void wav_stop(void) {
    if (! playing)
        return;
    audio_close();
    f_close(&file);
    playing = false;
    paused = false;
}


void wav_toggle_pause(void) {
    if (! playing)
        return;
    if (paused) {
        /* The samples still queued were lost when pausing: go back to the played position */
        uint32_t played_bytes = start_sample * channels * (bits / 8);
        data_left = data_size - played_bytes;
        f_lseek(&file, data_offset + played_bytes);
        paused = ! audio_open(rate);
    } else {
        start_sample += audio_played();
        audio_close();
        paused = true;
    }
}


bool wav_is_paused(void) {
    return paused;
}


uint32_t wav_duration_s(void) {
    uint32_t frame = channels * (bits / 8);
    return frame && rate ? data_size / frame / rate : 0;
}


uint32_t wav_position_s(void) {
    return rate ? (start_sample + (paused ? 0 : audio_played())) / rate : 0;
}


const char *wav_message(void) {
    return message;
}


bool wav_task(void) {
    if (! playing)
        return false;
    if (paused)
        return true;

    static uint8_t raw[READ_CHUNK];
    static uint8_t mono[READ_CHUNK];
    uint32_t frame = channels * (bits / 8);  /* Bytes per sample (all channels) */
    for (int k = 0; k < 2; ++k) {  /* Bounded work per call */
        size_t n = audio_free();  /* In samples */
        if (n * frame > READ_CHUNK)
            n = READ_CHUNK / frame;
        if (n * frame > data_left)
            n = data_left / frame;
        if (n == 0) {
            if (data_left < frame && audio_queued() == 0) {
                /* Everything was played */
                wav_stop();
                return false;
            }
            return true;
        }
        if (n < 256 && data_left >= 256 * frame)
            return true;  /* Not worth a read yet */

        UINT got;
        if (f_read(&file, raw, n * frame, &got) != FR_OK || got < frame) {
            wav_stop();
            return false;
        }
        data_left -= got;
        n = got / frame;
        /* Convert to 8 bit unsigned mono */
        for (size_t i = 0; i < n; ++i) {
            int32_t s = 0;
            for (uint16_t c = 0; c < channels; ++c) {
                if (bits == 8)
                    s += raw[i*frame + c] - 128;
                else
                    s += (int16_t)le16(&raw[i*frame + 2*c]) >> 8;
            }
            mono[i] = (uint8_t)(s / channels + 128);
        }
        audio_write(mono, n);
    }
    return true;
}
