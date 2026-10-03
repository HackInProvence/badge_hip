/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "ff.h"
#include "screen.h"
#include "sd.h"
#include "video.h"

#ifndef N_
#define N_(text) (text)  /* A text shown: translated when drawn (src/menu/i18n.h, tools/i18n.py) */
#endif


#define FRAME_SIZE ((SCREEN_WIDTH*SCREEN_HEIGHT)/8)
#define HEADER_SIZE 512
#define AUDIO_CHUNK 1024  /* Samples read from the card at once */


typedef enum {
    V_IDLE,
    V_CLEAR,  /* Full refresh to white to start from a clean screen */
    V_START,  /* Enter the multiframe mode (and start the sound) */
    V_PLAYING,
    V_PAUSING,  /* Leave the multiframe mode (should not stay in it while paused) */
    V_PAUSED,
    V_ENDING,  /* Leave the multiframe mode at the end */
    V_FINAL,  /* Full refresh of the last frame, removes ghosting */
    V_CLOSE,
} video_state_t;

static video_state_t state = V_IDLE;
static bool completed = false;  /* The last video was played to its end */
static bool stopping = false;  /* video_stop() was called */
static FIL file;  /* Reads the frames */
static FIL afile;  /* Reads the sound, stored after the frames */
static uint16_t fps = 10;
static uint32_t n_frames = 0;
static uint32_t audio_rate = 0;  /* 0 without sound */
static uint32_t n_samples = 0;
static uint32_t audio_start = 0;  /* First sample played since the last (re)start */
static uint32_t audio_read = 0;  /* Next sample to read */
static char message[96] = "";

static uint8_t buffers[2][FRAME_SIZE];
static uint8_t *prev = buffers[0];  /* Last frame sent to the screen */
static uint8_t *next = buffers[1];  /* Next frame to send */
static bool next_loaded = false;
static uint32_t i_frame = 0;  /* Index of the next frame to show */
static absolute_time_t next_ts = 0;  /* When to show the next frame (without sound) */

/* Statistics */
static absolute_time_t play_ts = 0;
static uint32_t late_frames = 0;
static uint32_t skipped_frames = 0;
static int64_t read_us_max = 0;


static bool fail(const char *msg) {
    snprintf(message, sizeof(message), "%s", msg);
    printf("video: %s\n", msg);
    return false;
}


static bool open_video(const char *path) {
    int fr = sd_mount();
    if (fr != FR_OK) {
        printf("video: mount failed, FatFs error %d, card detect pin = %d\n", fr, sd_detect());
        return fail(fr == FR_NO_FILESYSTEM ? N_("Carte SD non formatée (FAT/exFAT)") : N_("Pas de carte SD"));
    }

    /* Without path: VIDEO.EPV, or the first *.EPV of the root */
    static char name[SD_NAME_MAX];
    if (path) {
        snprintf(name, sizeof(name), "%s", path);
    } else if (f_stat("VIDEO.EPV", NULL) == FR_OK) {
        snprintf(name, sizeof(name), "VIDEO.EPV");
    } else if (sd_list_files("", ".EPV", &name, 1) == 0) {
        sd_unmount();  /* The card may have been changed */
        return fail(N_("Pas de fichier .EPV sur la carte"));
    }

    if (f_open(&file, name, FA_READ) != FR_OK) {
        sd_unmount();
        return fail(N_("Impossible d'ouvrir la vidéo"));
    }

    uint8_t header[28];
    UINT n;
    bool v2 = false;
    if (f_read(&file, header, sizeof(header), &n) != FR_OK || n != sizeof(header)
            || (memcmp(header, "EPVIDEO1", 8) && ! (v2 = ! memcmp(header, "EPVIDEO2", 8)))) {
        f_close(&file);
        return fail(N_("Fichier .EPV invalide"));
    }
    uint16_t width = header[8] | header[9] << 8;
    uint16_t height = header[10] | header[11] << 8;
    fps = header[12] | header[13] << 8;
    uint16_t bpp = header[14] | header[15] << 8;
    n_frames = header[16] | header[17] << 8 | header[18] << 16 | (uint32_t)header[19] << 24;
    audio_rate = v2 ? header[20] | header[21] << 8 | header[22] << 16 | (uint32_t)header[23] << 24 : 0;
    n_samples = v2 ? header[24] | header[25] << 8 | header[26] << 16 | (uint32_t)header[27] << 24 : 0;
    if (width != SCREEN_WIDTH || height != SCREEN_HEIGHT || bpp != 1 || fps == 0 || n_frames == 0) {
        f_close(&file);
        return fail(N_("Format de vidéo non supporté"));
    }
    f_lseek(&file, HEADER_SIZE);

    /* A second file handle reads the sound independently */
    if (audio_rate && n_samples && f_open(&afile, name, FA_READ) != FR_OK) {
        printf("video: cannot open the sound, playing without\n");
        audio_rate = 0;
    }
    if (! n_samples)
        audio_rate = 0;

    snprintf(message, sizeof(message), "%s", name);
    printf("video: playing %s, %lu frames @ %u fps (%.1fs), %s\n", name, (unsigned long)n_frames, fps,
           n_frames/(float)fps, audio_rate ? "with sound" : "without sound");
    return true;
}

/* Reads the next frame in the free buffer, returns false at the end of the video */
static bool load_next(void) {
    if (i_frame >= n_frames)
        return false;
    absolute_time_t t0 = get_absolute_time();
    UINT n;
    FRESULT fr = f_read(&file, next, FRAME_SIZE, &n);
    int64_t dt = absolute_time_diff_us(t0, get_absolute_time());
    if (dt > read_us_max)
        read_us_max = dt;
    if (fr != FR_OK || n != FRAME_SIZE) {
        printf("video: read error at frame %lu (FatFs error %d, read %u bytes)\n", (unsigned long)i_frame, fr, n);
        return false;
    }
    return true;
}

/* Keeps the audio ring buffer full */
static void feed_audio(void) {
    static uint8_t chunk[AUDIO_CHUNK];
    for (int k = 0; k < 2; ++k) {  /* Bounded work per call */
        size_t n = audio_free();
        if (n > AUDIO_CHUNK)
            n = AUDIO_CHUNK;
        if (n > n_samples - audio_read)
            n = n_samples - audio_read;
        if (n < 256 && audio_read < n_samples)  /* Not worth a read yet */
            return;
        if (n == 0)
            return;
        UINT got;
        if (f_read(&afile, chunk, n, &got) != FR_OK || got == 0)
            return;
        audio_write(chunk, got);
        audio_read += got;
    }
}

/* Position of the sound, in samples since the beginning of the video */
static uint32_t audio_position(void) {
    return audio_start + audio_played();
}

static const uint8_t *waveform(void) {
    if (fps <= 10)
        return screen_ws_10fps;
    if (fps <= 20)
        return screen_ws_20fps;
    return screen_ws_30fps;
}


bool video_start(const char *path) {
    completed = false;
    stopping = false;
    if (state != V_IDLE)
        return true;
    if (! open_video(path))
        return false;
    i_frame = 0;
    late_frames = 0;
    skipped_frames = 0;
    read_us_max = 0;
    next_loaded = false;
    state = V_CLEAR;
    return true;
}


void video_stop(void) {
    stopping = true;
    switch (state) {
    case V_CLEAR:
        state = V_CLOSE;
        break;
    case V_START:
    case V_PLAYING:
        state = V_ENDING;
        break;
    case V_PAUSING:
    case V_PAUSED:
        state = V_FINAL;
        break;
    default:
        break;
    }
}


void video_toggle_pause(void) {
    if (state == V_PLAYING)
        state = V_PAUSING;
    else if (state == V_PAUSED)
        state = V_START;
}


bool video_is_paused(void) {
    return state == V_PAUSING || state == V_PAUSED;
}


bool video_completed(void) {
    return completed;
}


const char *video_message(void) {
    return message;
}


bool video_task(absolute_time_t now) {
    if (state == V_IDLE)
        return false;
    if (state == V_PAUSED)
        return true;

    if (state == V_PLAYING) {
        if (audio_rate)
            feed_audio();

        /* With sound, the sound is the clock: skip frames when the screen is late */
        if (audio_rate && next_loaded) {
            uint32_t due = (uint64_t)audio_position() * fps / audio_rate;
            if (due > i_frame + 1 && due < n_frames) {
                skipped_frames += due - i_frame;
                i_frame = due;
                f_lseek(&file, HEADER_SIZE + (FSIZE_t)i_frame * FRAME_SIZE);
                next_loaded = false;
            }
        }

        /* Prefetch the next frame from the SD card, also while the screen is drawing */
        if (! next_loaded) {
            if (load_next())
                next_loaded = true;
            else
                state = V_ENDING;
        }
    }

    /* Boots (or wakes from deep sleep) the screen */
    if (! screen_boot() || screen_busy())
        return true;

    switch (state) {
    case V_CLEAR:
        screen_clear(1);
        state = V_START;
        break;
    case V_START:
        screen_clear_image_position();
        screen_push_ws(waveform());
        screen_start_multiframe();
        next_ts = now;
        if (i_frame == 0)
            play_ts = now;
        if (audio_rate) {
            /* (Re)start the sound at the position of the next frame */
            audio_start = (uint64_t)i_frame * audio_rate / fps;
            audio_read = audio_start;
            f_lseek(&afile, HEADER_SIZE + (FSIZE_t)n_frames * FRAME_SIZE + audio_start);
            if (! audio_open(audio_rate))
                audio_rate = 0;
        }
        state = V_PLAYING;
        break;
    case V_PLAYING:
        if (! next_loaded)
            break;
        if (audio_rate) {
            if ((uint64_t)audio_position() * fps < (uint64_t)i_frame * audio_rate)
                break;
        } else if (absolute_time_diff_us(now, next_ts) > 0) {
            break;
        }
        /* lsb = next frame, msb = previous frame (the first frame is compared to itself) */
        screen_push_rams(next, i_frame ? prev : next, FRAME_SIZE);
        screen_draw_multiframe();
        uint8_t *o = prev;
        prev = next;
        next = o;
        next_loaded = false;
        ++i_frame;
        /* Without sound: keep the rhythm, but don't try to catch up if we are late */
        next_ts = delayed_by_us(next_ts, 1000000 / fps);
        if (absolute_time_diff_us(next_ts, now) > 0) {
            next_ts = now;
            ++late_frames;
        }
        break;
    case V_PAUSING:
        audio_close();
        screen_end_multiframe();
        state = V_PAUSED;
        printf("video: paused at frame %lu\n", (unsigned long)i_frame);
        break;
    case V_ENDING:
        audio_close();
        screen_end_multiframe();
        state = V_FINAL;
        {
            float s = absolute_time_diff_us(play_ts, now) / 1e6f;
            snprintf(message, sizeof(message), "%lu images en %.0fs", (unsigned long)i_frame, s);
            printf("video: %lu frames in %.1fs (%.1f fps), %lu late, %lu skipped, SD read max %lld us per frame\n",
                   (unsigned long)i_frame, s, i_frame / s, (unsigned long)late_frames, (unsigned long)skipped_frames,
                   read_us_max);
        }
        break;
    case V_FINAL:
        if (i_frame)
            screen_show_image_bw(prev);
        state = V_CLOSE;
        break;
    case V_CLOSE:
        f_close(&file);
        if (audio_rate)
            f_close(&afile);
        state = V_IDLE;
        completed = ! stopping;
        printf("video: done\n");
        return false;
    default:
        break;
    }
    return true;
}
