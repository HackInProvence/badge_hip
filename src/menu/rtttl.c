/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Sonneries (Médias): an RTTTL ringtone player (docs/fr/sonneries.md). The tunes of the firmware (rtttl_parse.c),
 * then the ones of the .txt / .rtttl / .rtx files of the folder SONNERIES of the SD card: one tune per line, the lines
 * starting with '#' are comments. The SD files are read and checked when the application opens: only the name and
 * the place of each tune are kept, the line is read again to play it.
 *
 * Playing: a square wave synthesized like the chorus (chorus.c), ~150 ms written ahead in the audio ring, so the
 * main loop never waits; the note shown and the LEDs follow audio_played() (what is heard, not what is written).
 * The mute mode is respected by the audio (silent samples) and app_leds(). */

#include <stdio.h>
#include <string.h>

#include "achievements.h"
#include "app.h"
#include "audio.h"
#include "ff.h"
#include "rtttl_parse.h"
#include "sd.h"

#define RATE 16000
#define CHUNK 256
#define AHEAD_SAMPLES 2400  /* 150 ms of sound written in advance */
#define AMPLITUDE 100
#define ATTACK_SAMPLES 32  /* 2 ms */
#define GAP_MAX_SAMPLES 480  /* Silence at the end of a note (articulation): 1/8 of it, at most 30 ms */
#define QUEUE 64  /* Notes written but not heard yet (for the page and the LEDs) */
#define REDRAW_MS 150  /* The e-Paper can't follow every note */

#define SD_DIR "SONNERIES"
#define MAX_FILES 24
#define MAX_SD_TUNES 128
#define LINE_MAX 2048  /* Bytes of a line (a tune) of a file */

enum { M_LIST, M_PLAY, M_ERROR };

typedef struct {
    char name[RTTTL_NAME_MAX];
    uint32_t offset;  /* Of the line in the file */
    uint16_t line;  /* Number of the line, from 1 */
    uint16_t err_pos;
    uint8_t file;
    uint8_t err;  /* rtttl_err_t */
} sd_tune_t;

static char files[MAX_FILES][SD_NAME_MAX];
static int n_files = 0;
static sd_tune_t sd_tunes[MAX_SD_TUNES];
static int n_sd = 0;
static char line[LINE_MAX + 1];  /* A line while scanning, then the text of the tune played */
static char status[48] = "";

static int mode = M_LIST;
static int sel = 0;

/* The tune played (or in error) */
static int cur = -1;
static rtttl_t song;
static rtttl_err_t song_err = RTTTL_OK;
static uint32_t song_ms = 0;

/* Synthesis */
static bool own_audio = false;
static bool gen_done = false;
static uint32_t gen_len = 0, gen_left = 0;
static uint32_t phase = 0, phase_inc = 0;
static uint32_t written = 0;  /* Samples written since audio_open() */
static uint32_t last_played = 0;

/* Notes written, waiting to be heard */
typedef struct {
    uint32_t at;  /* Sample of the start */
    uint32_t len;
    rtttl_note_t note;
} queued_t;
static queued_t queue[QUEUE];
static uint32_t q_in = 0, q_out = 0;

/* The note heard */
static queued_t heard;
static bool have_heard = false;
static bool leds_set = false, leds_dim = false;
static uint8_t led_rgb[3] = {0, 0, 0};
static bool redraw_pending = false;
static absolute_time_t redraw_ts = 0;
static uint32_t shown_s = 0;

static int n_tunes(void) {
    return RTTTL_N_BUILTIN + n_sd;
}


/* ------ The files of the SD card ------ */

/* A line of a file: a tune unless it is empty or a comment */
static void add_line(int f, uint32_t offset, uint16_t line_no, size_t len, bool too_long) {
    line[len] = 0;
    size_t i = 0;
    if (len >= 3 && (uint8_t)line[0] == 0xEF && (uint8_t)line[1] == 0xBB && (uint8_t)line[2] == 0xBF)
        i = 3;  /* UTF-8 byte order mark */
    while (i < len && (line[i] == ' ' || line[i] == '\t'))
        ++i;
    if (i >= len || line[i] == '#')
        return;
    if (n_sd >= MAX_SD_TUNES) {
        printf("rtttl: %s line %u: too many tunes, ignored\n", files[f], line_no);
        return;
    }
    sd_tune_t *s = &sd_tunes[n_sd++];
    rtttl_t t;
    rtttl_err_t e = rtttl_open(&t, line, len);
    uint32_t notes = 0, ms = 0;
    if (e == RTTTL_OK)
        e = rtttl_check(&t, &notes, &ms);
    if (too_long) {
        e = RTTTL_ERR_TOO_LONG;
        t.err_pos = LINE_MAX;
    }
    snprintf(s->name, sizeof(s->name), "%s", t.name[0] ? t.name : "Sans nom");
    s->offset = offset;
    s->line = line_no;
    s->file = (uint8_t)f;
    s->err = (uint8_t)e;
    s->err_pos = (uint16_t)(t.err_pos > 0xFFFF ? 0xFFFF : t.err_pos);
    if (e == RTTTL_OK)
        printf("rtttl: %s line %u: \"%s\", %lu notes, %lu ms\n", files[f], line_no, s->name, (unsigned long)notes,
               (unsigned long)ms);
    else
        printf("rtttl: %s line %u: error %d (%s) at column %u\n", files[f], line_no, e, rtttl_error_text(e),
               s->err_pos + 1);
}

static void scan_file(int f) {
    char path[sizeof(SD_DIR) + SD_NAME_MAX + 1];
    snprintf(path, sizeof(path), "%s/%s", SD_DIR, files[f]);
    FIL fil;
    FRESULT fr = f_open(&fil, path, FA_READ);
    if (fr != FR_OK) {
        printf("rtttl: %s: can't open (FatFs error %d)\n", path, fr);
        return;
    }
    uint8_t buf[256];
    UINT n = 0;
    size_t len = 0;
    uint32_t offset = 0, start = 0;
    uint16_t line_no = 1;
    bool too_long = false;
    while (f_read(&fil, buf, sizeof(buf), &n) == FR_OK && n > 0) {
        for (UINT i = 0; i < n; ++i, ++offset) {
            char c = (char)buf[i];
            if (c == '\n' || c == '\r') {
                add_line(f, start, line_no, len, too_long);
                if (c == '\n')
                    ++line_no;
                len = 0;
                too_long = false;
                start = offset + 1;
            } else if (len < LINE_MAX) {
                line[len++] = c;
            } else {
                too_long = true;
            }
        }
    }
    add_line(f, start, line_no, len, too_long);
    f_close(&fil);
}

static void load_sd(void) {
    static const char *const EXTS[] = {".TXT", ".RTTTL", ".RTX"};
    n_files = n_sd = 0;
    status[0] = 0;
    for (size_t e = 0; e < sizeof(EXTS) / sizeof(EXTS[0]); ++e)
        n_files += (int)sd_list_files(SD_DIR, EXTS[e], files + n_files, MAX_FILES - n_files);
    for (int f = 0; f < n_files; ++f)
        scan_file(f);
    printf("rtttl: /%s: %d file(s), %d tune(s)\n", SD_DIR, n_files, n_sd);
    if (! n_files) {
        sd_unmount();  /* Mount again next time, the card may be changed */
        snprintf(status, sizeof(status), sd_is_ready() ? "Rien dans /" SD_DIR : "Pas de carte SD");
    }
}

/* Reads the line of an SD tune in line[] */
static bool read_sd_line(const sd_tune_t *s) {
    char path[sizeof(SD_DIR) + SD_NAME_MAX + 1];
    snprintf(path, sizeof(path), "%s/%s", SD_DIR, files[s->file]);
    FIL fil;
    FRESULT fr = sd_mount();
    if (fr == FR_OK)
        fr = f_open(&fil, path, FA_READ);
    if (fr != FR_OK && fr != FR_NO_FILE && fr != FR_NO_PATH) {
        sd_unmount();  /* The card was removed or changed: mount it again */
        fr = sd_mount();
        if (fr == FR_OK)
            fr = f_open(&fil, path, FA_READ);
    }
    if (fr != FR_OK) {
        printf("rtttl: %s: can't open (FatFs error %d)\n", path, fr);
        return false;
    }
    UINT n = 0;
    if (f_lseek(&fil, s->offset) != FR_OK || f_read(&fil, line, LINE_MAX, &n) != FR_OK)
        n = 0;
    f_close(&fil);
    line[n] = 0;
    line[strcspn(line, "\r\n")] = 0;
    return n > 0;
}


/* ------ Playing ------ */

static void leds(uint8_t r, uint8_t g, uint8_t b) {
    app_leds(r, g, b);
    leds_set = r || g || b;
}

/* A color per note: the 12 semitones around the color wheel */
static void note_leds(const rtttl_note_t *n, bool dim) {
    static const uint8_t WHEEL[12][3] = {
        {255, 0, 0}, {255, 64, 0}, {255, 140, 0}, {255, 220, 0}, {160, 255, 0}, {0, 255, 0},
        {0, 255, 140}, {0, 220, 255}, {0, 100, 255}, {40, 0, 255}, {160, 0, 255}, {255, 0, 160},
    };
    if (! n->rest)
        memcpy(led_rgb, WHEEL[n->semitone % 12], 3);
    else if (! leds_set)
        return;
    dim = dim || n->rest;
    uint8_t d = dim ? 12 : 1;
    leds(led_rgb[0] / d, led_rgb[1] / d, led_rgb[2] / d);
    leds_dim = dim;
}

static void stop_audio(void) {
    if (own_audio && audio_is_open())
        audio_close();
    own_audio = false;
}

static void stop_playing(const char *why) {
    if (mode == M_PLAY)
        printf("rtttl: %s \"%s\"\n", why, song.name);
    stop_audio();
    if (leds_set)
        leds(0, 0, 0);  /* Back to the LED animation of the badge */
    mode = M_LIST;
}

/* Starts the next note at the sample \p at: 1, 0 at the end, -1 when the queue is full */
static int next_note(uint32_t at) {
    if (q_in - q_out >= QUEUE)
        return -1;
    rtttl_note_t n;
    rtttl_err_t e = rtttl_next(&song, &n);
    if (e != RTTTL_OK) {
        if (e != RTTTL_END)
            printf("rtttl: error %d (%s) at column %u\n", e, rtttl_error_text(e), (unsigned)song.err_pos + 1);
        gen_done = true;
        return 0;
    }
    gen_len = gen_left = n.ms * (RATE / 1000);  /* n.ms >= 1 */
    phase = 0;
    phase_inc = n.rest ? 0 : (uint32_t)(((uint64_t)n.freq_mhz << 32) / (RATE * 1000ull));
    queued_t *q = &queue[q_in++ % QUEUE];
    q->at = at;
    q->len = gen_len;
    q->note = n;
    return 1;
}

/* Square wave, a short attack then a slow decay, a gap at the end of the note */
static uint8_t sample(void) {
    int amp = 0;
    if (phase_inc) {
        uint32_t t = gen_len - gen_left;
        uint32_t gap = gen_len / 8 < GAP_MAX_SAMPLES ? gen_len / 8 : GAP_MAX_SAMPLES;
        if (gen_left > gap)
            amp = t < ATTACK_SAMPLES ? AMPLITUDE * (int)t / ATTACK_SAMPLES
                                     : AMPLITUDE - (int)((uint64_t)(AMPLITUDE / 3) * t / gen_len);
        phase += phase_inc;
    }
    --gen_left;
    return (uint8_t)(128 + ((phase & 0x80000000u) ? amp : -amp));
}

static void synthesize(void) {
    uint8_t buf[CHUNK];
    while (! gen_done && audio_queued() < AHEAD_SAMPLES && audio_free() >= CHUNK) {
        int k = 0;
        for (; k < CHUNK; ++k) {
            if (! gen_left && next_note(written + k) <= 0)
                break;
            buf[k] = sample();
        }
        if (k) {
            audio_write(buf, k);
            written += k;
        }
        if (k < CHUNK)
            break;  /* The end, or the queue is full */
    }
}

/* Loads the tune \p i in song (built-in, or read again from the card) */
static rtttl_err_t load_tune(int i) {
    if (i < RTTTL_N_BUILTIN)
        return rtttl_open(&song, RTTTL_BUILTIN[i], strlen(RTTTL_BUILTIN[i]));
    const sd_tune_t *s = &sd_tunes[i - RTTTL_N_BUILTIN];
    if (s->err != RTTTL_OK) {
        rtttl_open(&song, "", 0);
        snprintf(song.name, sizeof(song.name), "%s", s->name);
        song.err_pos = s->err_pos;
        return (rtttl_err_t)s->err;
    }
    if (! read_sd_line(s)) {
        rtttl_open(&song, "", 0);
        snprintf(song.name, sizeof(song.name), "%s", s->name);
        return RTTTL_ERR_EMPTY;
    }
    return rtttl_open(&song, line, LINE_MAX);
}

static void start_playing(int i) {
    stop_audio();
    cur = i;
    uint32_t notes = 0;
    song_err = load_tune(i);
    if (song_err == RTTTL_OK)
        song_err = rtttl_check(&song, &notes, &song_ms);
    if (song_err != RTTTL_OK) {
        printf("rtttl: \"%s\": error %d (%s) at column %u\n", song.name, song_err, rtttl_error_text(song_err),
               (unsigned)song.err_pos + 1);
        if (leds_set)
            leds(0, 0, 0);
        mode = M_ERROR;
        return;
    }
    app_tone(0, 0);  /* Stops a chime of the menu */
    if (! audio_open(RATE)) {
        snprintf(status, sizeof(status), "Son indisponible");
        mode = M_LIST;
        return;
    }
    own_audio = true;
    written = last_played = 0;
    q_in = q_out = 0;
    gen_len = gen_left = 0;
    gen_done = false;
    have_heard = false;
    redraw_pending = false;
    shown_s = 0;
    mode = M_PLAY;
    printf("rtttl: playing \"%s\" (%s), %lu notes, %lu ms, d=%u o=%u b=%u\n", song.name,
           i < RTTTL_N_BUILTIN ? "built-in" : files[sd_tunes[i - RTTTL_N_BUILTIN].file], (unsigned long)notes,
           (unsigned long)song_ms, song.duration, song.octave, song.bpm);
    achv_unlock(ACHV_RTTTL);
    achv_add(ACHV_CNT_RINGTONES, 1);
    synthesize();
}

/* The next (\p delta = 1) or previous (-1) valid tune */
static void play_next(int delta) {
    int n = n_tunes();
    int i = cur;
    for (int k = 0; k < n; ++k) {
        i = (i + delta + n) % n;
        if (i < RTTTL_N_BUILTIN || sd_tunes[i - RTTTL_N_BUILTIN].err == RTTTL_OK)
            break;
    }
    sel = i;
    start_playing(i);
}

/* Another user of the sound (a chime of a notification) closed or reopened the audio: continue */
static void resync_audio(void) {
    if (! audio_is_open()) {
        if (! audio_open(RATE)) {
            stop_playing("sound lost, stopped");
            return;
        }
        printf("rtttl: sound taken by another feature, resumed\n");
        written = 0;
    } else {
        printf("rtttl: sound shared with another feature\n");
        written = audio_played() + (uint32_t)audio_queued();
    }
    last_played = audio_played();
    q_out = q_in;  /* The notes written are lost */
}


/* ------ The application ------ */

static void rtttl_start(absolute_time_t now) {
    (void)now;
    mode = M_LIST;
    load_sd();
    if (sel >= n_tunes())
        sel = 0;
}

static void rtttl_stop(void) {
    stop_playing("stopped");
}

/* Flanks in the list: one step, repeated while held */
static int list_step(const app_buttons_t *b, absolute_time_t now) {
    static absolute_time_t repeat_ts = 0;
    for (int w = 0; w < 2; ++w) {
        uint8_t bit = w ? UI_BTN_X : UI_BTN_Y;
        int sign = w ? 1 : -1;
        if (b->pressed & bit) {
            repeat_ts = delayed_by_ms(now, 400);
            return sign;
        }
        if ((b->held & bit) && absolute_time_diff_us(repeat_ts, now) >= 0) {
            repeat_ts = delayed_by_ms(now, 90);
            return sign;
        }
    }
    return 0;
}

static bool rtttl_buttons(const app_buttons_t *b, absolute_time_t now) {
    if (b->pressed)
        status[0] = 0;
    switch (mode) {
    case M_PLAY:
        if (b->pressed & UI_BTN_A) {
            sel = cur;
            stop_playing("stopped");
        } else if (b->pressed & UI_BTN_X)
            play_next(1);
        else if (b->pressed & UI_BTN_Y)
            play_next(-1);
        else if (b->pressed & UI_BTN_B)
            start_playing(cur);  /* From the start */
        return true;
    case M_ERROR:
        if (b->pressed & (UI_BTN_A | UI_BTN_B))
            mode = M_LIST;
        return true;
    default:
        break;
    }
    if (b->pressed & UI_BTN_A)
        return false;
    int step = list_step(b, now);
    if (step)
        sel = (sel + step + n_tunes()) % n_tunes();
    if (b->pressed & UI_BTN_B)
        start_playing(sel);
    return true;
}

static bool rtttl_task(absolute_time_t now) {
    if (mode != M_PLAY)
        return false;
    if (! audio_is_open() || audio_played() < last_played)
        resync_audio();
    if (mode != M_PLAY)
        return true;
    synthesize();
    uint32_t played = audio_played();
    last_played = played;

    /* The notes heard now */
    bool new_note = false;
    while (q_out != q_in && queue[q_out % QUEUE].at <= played) {
        heard = queue[q_out++ % QUEUE];
        have_heard = new_note = true;
    }
    if (new_note) {
        note_leds(&heard.note, false);
        redraw_pending = true;
    } else if (have_heard && ! leds_dim && played - heard.at > heard.len / 2) {
        note_leds(&heard.note, true);  /* A flash on each note */
    }

    /* The end */
    if (gen_done && q_out == q_in && (audio_queued() == 0 || played >= written)) {
        sel = cur;
        stop_playing("end of");
        return true;
    }

    uint32_t s = (uint32_t)((uint64_t)played / RATE);
    if (s != shown_s) {
        shown_s = s;
        redraw_pending = true;
    }
    if (redraw_pending && absolute_time_diff_us(redraw_ts, now) >= REDRAW_MS * 1000) {
        redraw_pending = false;
        redraw_ts = now;
        return true;
    }
    return false;
}

static void tune_label(int i, char *buf, size_t len) {
    if (i < RTTTL_N_BUILTIN) {
        rtttl_t t;
        rtttl_open(&t, RTTTL_BUILTIN[i], strlen(RTTTL_BUILTIN[i]));
        snprintf(buf, len, "%s", t.name);
        return;
    }
    const sd_tune_t *s = &sd_tunes[i - RTTTL_N_BUILTIN];
    char file[SD_NAME_MAX];
    snprintf(file, sizeof(file), "%s", files[s->file]);
    char *dot = strrchr(file, '.');
    if (dot)
        *dot = 0;
    snprintf(buf, len, "%s%s (%s)", s->err != RTTTL_OK ? "(!) " : "", s->name, file);
}

static void source_name(char *buf, size_t len) {
    if (cur < RTTTL_N_BUILTIN)
        snprintf(buf, len, "Sonnerie du badge");
    else
        snprintf(buf, len, "%s", files[sd_tunes[cur - RTTTL_N_BUILTIN].file]);
}

static void render_play(uint8_t *fb) {
    char text[64], fitted[64];
    snprintf(text, sizeof(text), "Sonnerie %d / %d", cur + 1, n_tunes());
    ui_title(fb, text);
    ui_wrapped(fb, UI_TITLE_H + 6, &gfx_font_medium, song.name, 2);
    source_name(text, sizeof(text));
    ui_fit_preview(&gfx_font_small, fitted, sizeof(fitted), text, GFX_WIDTH - 8);
    gfx_text(fb, GFX_WIDTH/2, 76, &gfx_font_small, fitted, GFX_BLACK, GFX_ALIGN_CENTER);
    if (have_heard) {
        rtttl_note_label(&heard.note, text, sizeof(text));
        gfx_text(fb, GFX_WIDTH/2, 100, &gfx_font_large, text, GFX_BLACK, GFX_ALIGN_CENTER);
    }
    uint32_t ms = (uint32_t)((uint64_t)last_played * 1000 / RATE);
    if (ms > song_ms)
        ms = song_ms;
    ui_gauge(fb, 14, 138, GFX_WIDTH - 28, 12, (int)ms, song_ms ? (int)song_ms : 1);
    snprintf(text, sizeof(text), "%lu:%02lu / %lu:%02lu", (unsigned long)(ms / 60000), (unsigned long)(ms / 1000 % 60),
             (unsigned long)(song_ms / 60000), (unsigned long)(song_ms / 1000 % 60));
    gfx_text(fb, GFX_WIDTH/2, 156, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
    ui_footer(fb, "G : stop  Flancs : préc./suiv.");
}

static void render_error(uint8_t *fb) {
    char text[80], fitted[64];
    ui_title(fb, "Sonnerie invalide");
    int y = ui_wrapped(fb, UI_TITLE_H + 6, &gfx_font_medium, song.name, 2) + 4;
    source_name(text, sizeof(text));
    ui_fit_preview(&gfx_font_small, fitted, sizeof(fitted), text, GFX_WIDTH - 8);
    gfx_text(fb, GFX_WIDTH/2, y, &gfx_font_small, fitted, GFX_BLACK, GFX_ALIGN_CENTER);
    y += 26;
    if (cur >= RTTTL_N_BUILTIN)
        snprintf(text, sizeof(text), "Ligne %u, colonne %u :", sd_tunes[cur - RTTTL_N_BUILTIN].line,
                 (unsigned)song.err_pos + 1);
    else
        snprintf(text, sizeof(text), "Colonne %u :", (unsigned)song.err_pos + 1);
    gfx_text(fb, GFX_WIDTH/2, y, &gfx_font_small, text, GFX_BLACK, GFX_ALIGN_CENTER);
    ui_wrapped(fb, y + 18, &gfx_font_small, rtttl_error_text(song_err), 2);
    ui_footer(fb, "G : retour");
}

static void rtttl_render(uint8_t *fb, absolute_time_t now) {
    (void)now;
    if (mode == M_PLAY) {
        render_play(fb);
    } else if (mode == M_ERROR) {
        render_error(fb);
    } else {
        ui_title(fb, "Sonneries");
        ui_list(fb, n_tunes(), sel, tune_label);
        ui_footer(fb, status[0] ? status : "G : retour  D : jouer");
    }
}

static bool rtttl_calm(void) {
    return mode != M_PLAY;
}

const app_t app_rtttl = {
    .name = "Sonneries",
    .start = rtttl_start,
    .buttons = rtttl_buttons,
    .task = rtttl_task,
    .render = rtttl_render,
    .calm = rtttl_calm,
    .stop = rtttl_stop,
};
