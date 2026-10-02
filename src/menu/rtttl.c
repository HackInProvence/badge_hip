/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Sonneries (Médias): an RTTTL ringtone player (docs/fr/sonneries.md). The tunes of the firmware (rtttl_parse.c),
 * and the folders SONNERIES and RTTTL of the SD card, browsed like the pirate radio: in a folder, its sub-folders,
 * then the tunes of its .txt / .rtttl / .rtx files (one tune per line, the lines starting with '#' are comments)
 * and of its .bas files (PICAXE programs: each "tune" command is converted to RTTTL by rtttl_from_picaxe(), named by
 * the comment line ' before it).
 * The files are read by pages of MAX_FILES (in the order of their names), so a folder may hold thousands of them.
 * The files of a page are read and checked when it is shown: only the name and the place of each tune are kept, the
 * line is read again to play it. The card shares its SPI bus with the screen: it is read when the display is idle.
 *
 * Playing: a square wave synthesized like the chorus (chorus.c), ~150 ms written ahead in the audio ring, so the
 * main loop never waits; the note shown and the LEDs follow audio_played() (what is heard, not what is written).
 * The mute mode is respected by the audio (silent samples) and app_leds(). */

#include <stdio.h>
#include <string.h>

#include "achievements.h"
#include "app.h"
#include "audio.h"
#include "display.h"
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

#define MAX_FILES 32  /* Files of a page of the list */
#define MAX_DIRS 40  /* Sub-folders shown in a folder (A..Z, 0-9...) */
#define PATH_MAX_LEN (4 * SD_NAME_MAX)  /* The folder shown, from the root: "RTTTL/Films/Western" */
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

static const char *const ROOT_DIRS[] = {"SONNERIES", "RTTTL"};
static char dir[PATH_MAX_LEN] = "";  /* The folder shown, "" for the top (the tunes of the firmware) */
static char dirs[MAX_DIRS][SD_NAME_MAX];
static int n_dirs = 0;
static char files[MAX_FILES][SD_NAME_MAX];  /* The files of the page, sorted */
static int n_files = 0;
/* The page: the first files (page_way 0), the ones after page_key (1) or the ones before it (-1) */
static int page_way = 0;
static char page_key[SD_NAME_MAX];
static bool page_prev = false, page_next = false;  /* Rows to the previous / next page */
static bool list_pending = false;  /* The folder must be read (when the display is idle) */
enum { SEL_KEEP, SEL_FIRST_TUNE, SEL_LAST_TUNE, SEL_DIR };
static int sel_after = SEL_KEEP;  /* The row selected once the folder is read */
static char sel_dir[SD_NAME_MAX];  /* SEL_DIR: the folder left, selected again */
static sd_tune_t sd_tunes[MAX_SD_TUNES];
static int n_sd = 0;
static char line[LINE_MAX + 1];  /* A line while scanning, then the text of the tune played */
static char conv[LINE_MAX + 1];  /* A PICAXE tune converted to RTTTL (.bas files) */
static char bas_name[RTTTL_NAME_MAX];  /* .bas: the name in the comment before the tune command */
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

/* The tunes of the firmware are shown at the top only */
static int n_builtin(void) {
    return dir[0] ? 0 : RTTTL_N_BUILTIN;
}

static int n_tunes(void) {
    return n_builtin() + n_sd;
}

/* The rows of the list: the folders, "previous", the tunes, "next" */
static int first_tune_row(void) {
    return n_dirs + page_prev;
}

static int n_rows(void) {
    return first_tune_row() + n_tunes() + page_next;
}


/* ------ The files of the SD card ------ */

static bool is_bas(const char *name) {
    const char *dot = strrchr(name, '.');
    return dot && ! strcasecmp(dot, ".bas");
}

/* The name of a .bas tune: the comment before it, else the name of the file */
static void bas_tune_name(int f, char *name, size_t len) {
    if (bas_name[0]) {
        snprintf(name, len, "%s", bas_name);
        return;
    }
    snprintf(name, len, "%s", files[f]);
    char *dot = strrchr(name, '.');
    if (dot)
        *dot = 0;
}

/* A line of a file: a tune unless it is empty or a comment. In a .bas file: the tune commands only. */
static void add_line(int f, uint32_t offset, uint16_t line_no, size_t len, bool too_long) {
    line[len] = 0;
    size_t i = 0;
    if (len >= 3 && (uint8_t)line[0] == 0xEF && (uint8_t)line[1] == 0xBB && (uint8_t)line[2] == 0xBF)
        i = 3;  /* UTF-8 byte order mark */
    while (i < len && (line[i] == ' ' || line[i] == '\t'))
        ++i;
    const char *text = line;
    bool bas = is_bas(files[f]);
    rtttl_err_t bas_err = RTTTL_OK;
    if (bas) {
        if (line[i] == '\'') {  /* A comment: the name of the next tune */
            if (! bas_name[0] && i + 1 < len)
                snprintf(bas_name, sizeof(bas_name), "%s", line + i + 1);
            return;
        }
        if (too_long || ! rtttl_is_picaxe(line + i, len - i))
            return;  /* Other BASIC commands */
        char name[RTTTL_NAME_MAX];
        bas_tune_name(f, name, sizeof(name));
        bas_name[0] = 0;
        bas_err = rtttl_from_picaxe(line + i, len - i, name, conv, sizeof(conv), NULL);
        if (bas_err != RTTTL_OK)
            snprintf(conv, sizeof(conv), "%s:", name);  /* The name for the list */
        text = conv;
        len = strlen(conv);
    } else if (i >= len || line[i] == '#') {
        return;
    }
    if (n_sd >= MAX_SD_TUNES) {
        printf("rtttl: %s line %u: too many tunes, ignored\n", files[f], line_no);
        return;
    }
    sd_tune_t *s = &sd_tunes[n_sd++];
    rtttl_t t;
    rtttl_err_t e = rtttl_open(&t, text, len);
    uint32_t notes = 0, ms = 0;
    if (e == RTTTL_OK)
        e = rtttl_check(&t, &notes, &ms);
    if (bas_err != RTTTL_OK) {
        e = bas_err;
        t.err_pos = 0;
    }
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
    char path[PATH_MAX_LEN + SD_NAME_MAX + 1];
    snprintf(path, sizeof(path), "%s/%s", dir, files[f]);
    FIL fil;
    FRESULT fr = f_open(&fil, path, FA_READ);
    if (fr != FR_OK) {
        printf("rtttl: %s: can't open (FatFs error %d)\n", path, fr);
        return;
    }
    uint8_t buf[256];
    UINT n = 0;
    size_t len = 0;
    bas_name[0] = 0;
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

static bool tune_file(const char *name) {
    const char *dot = strrchr(name, '.');
    return dot && (! strcasecmp(dot, ".txt") || ! strcasecmp(dot, ".rtttl") || ! strcasecmp(dot, ".rtx")
                   || ! strcasecmp(dot, ".bas"));
}

/* Inserts \p name in the sorted files[] (not full) */
static void insert_file(const char *name) {
    int i = n_files++;
    while (i > 0 && strcasecmp(files[i - 1], name) > 0) {
        strcpy(files[i], files[i - 1]);
        --i;
    }
    strcpy(files[i], name);
}

/* Reads the page of files of the folder: the MAX_FILES first names (page_way 0), the MAX_FILES first ones after
 * page_key (1) or the MAX_FILES last ones before it (-1), in the order of the names, whatever the order on the card */
static void list_files(void) {
    bool more = false;  /* Files beyond the page, in the direction read */
    bool other = false;  /* Files on the other side of page_key */
    DIR d;
    FILINFO info;
    if (f_opendir(&d, dir) != FR_OK)
        return;
    while (f_readdir(&d, &info) == FR_OK && info.fname[0]) {
        if ((info.fattrib & (AM_DIR | AM_HID | AM_SYS)) || info.fname[0] == '.' || ! tune_file(info.fname)
            || strlen(info.fname) >= SD_NAME_MAX)
            continue;
        int c = page_way ? strcasecmp(info.fname, page_key) : 0;
        if ((page_way > 0 && c <= 0) || (page_way < 0 && c >= 0)) {
            other = true;
            continue;
        }
        if (n_files == MAX_FILES) {
            more = true;
            if (page_way >= 0) {  /* Keep the smallest names */
                if (strcasecmp(info.fname, files[MAX_FILES - 1]) > 0)
                    continue;
                --n_files;
            } else {  /* Keep the largest ones */
                if (strcasecmp(info.fname, files[0]) < 0)
                    continue;
                memmove(files[0], files[1], (MAX_FILES - 1) * SD_NAME_MAX);
                --n_files;
            }
        }
        insert_file(info.fname);
    }
    f_closedir(&d);
    page_prev = page_way >= 0 ? other : more;
    page_next = page_way >= 0 ? more : other;
}

/* Reads the folder shown: its sub-folders (at the top, SONNERIES and RTTTL), then a page of its files */
static void load_list(void) {
    n_dirs = n_files = n_sd = 0;
    page_prev = page_next = false;
    status[0] = 0;
    if (sd_mount() != FR_OK) {
        sd_unmount();  /* Mount again next time, the card may be inserted */
        snprintf(status, sizeof(status), "Pas de carte SD");
        dir[0] = 0;
    } else if (! dir[0]) {
        for (size_t i = 0; i < sizeof(ROOT_DIRS) / sizeof(ROOT_DIRS[0]); ++i) {
            FILINFO info;
            if (f_stat(ROOT_DIRS[i], &info) == FR_OK && (info.fattrib & AM_DIR))
                snprintf(dirs[n_dirs++], SD_NAME_MAX, "%s", ROOT_DIRS[i]);
        }
        if (! n_dirs)
            snprintf(status, sizeof(status), "Ni SONNERIES ni RTTTL");
    } else {
        n_dirs = (int)sd_list_dirs(dir, dirs, MAX_DIRS);
        list_files();
        for (int f = 0; f < n_files; ++f)
            scan_file(f);
        if (! n_dirs && ! n_files)
            snprintf(status, sizeof(status), "Dossier vide");
    }
    printf("rtttl: /%s: %d dir(s), %d file(s), %d tune(s)%s%s\n", dir, n_dirs, n_files, n_sd,
           page_prev ? ", previous page" : "", page_next ? ", next page" : "");

    int rows = n_rows();
    if (sel_after == SEL_FIRST_TUNE)
        sel = first_tune_row();
    else if (sel_after == SEL_LAST_TUNE)
        sel = first_tune_row() + n_tunes() - 1;
    else if (sel_after == SEL_DIR)
        for (int i = 0; i < n_dirs; ++i)
            if (! strcasecmp(dirs[i], sel_dir))
                sel = i;
    sel_after = SEL_KEEP;
    if (sel >= rows || sel < 0)
        sel = 0;
}

/* Reads the folder (or the page) once the display is idle: until then, an empty list */
static void request_list(int select) {
    n_dirs = n_files = n_sd = 0;
    page_prev = page_next = false;
    sel_after = select;
    list_pending = true;
}

static void enter_dir(const char *name) {
    size_t len = strlen(dir);
    if (len + 1 + strlen(name) >= sizeof(dir)) {
        snprintf(status, sizeof(status), "Chemin trop long");
        return;
    }
    snprintf(dir + len, sizeof(dir) - len, "%s%s", len ? "/" : "", name);
    page_way = 0;
    sel = 0;
    request_list(SEL_KEEP);
}

static void dir_up(void) {
    char *slash = strrchr(dir, '/');
    snprintf(sel_dir, sizeof(sel_dir), "%s", slash ? slash + 1 : dir);
    if (slash)
        *slash = 0;
    else
        dir[0] = 0;
    page_way = 0;
    sel = 0;
    request_list(SEL_DIR);
}

/* The previous (\p way -1) or next (1) page of files */
static void change_page(int way) {
    snprintf(page_key, sizeof(page_key), "%s", way > 0 ? files[n_files - 1] : files[0]);
    page_way = way;
    request_list(way > 0 ? SEL_FIRST_TUNE : SEL_LAST_TUNE);
}

/* Reads the line of an SD tune in line[] */
static bool read_sd_line(const sd_tune_t *s) {
    char path[PATH_MAX_LEN + SD_NAME_MAX + 1];
    snprintf(path, sizeof(path), "%s/%s", dir, files[s->file]);
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
    if (n > 0 && is_bas(files[s->file])) {
        /* A PICAXE tune: converted again, with the name found when the file was read */
        const char *p = line + strspn(line, " \t");
        return rtttl_from_picaxe(p, LINE_MAX, s->name, conv, sizeof(conv), NULL) == RTTTL_OK;
    }
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
    if (i < n_builtin())
        return rtttl_open(&song, RTTTL_BUILTIN[i], strlen(RTTTL_BUILTIN[i]));
    const sd_tune_t *s = &sd_tunes[i - n_builtin()];
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
    return rtttl_open(&song, is_bas(files[s->file]) ? conv : line, LINE_MAX);
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
           i < n_builtin() ? "built-in" : files[sd_tunes[i - n_builtin()].file], (unsigned long)notes,
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
        if (i < n_builtin() || sd_tunes[i - n_builtin()].err == RTTTL_OK)
            break;
    }
    sel = first_tune_row() + i;
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
    request_list(SEL_KEEP);  /* The same folder as last time */
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
            sel = first_tune_row() + cur;
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
    if (b->pressed & UI_BTN_A) {
        if (! dir[0])
            return false;
        dir_up();
        return true;
    }
    if (list_pending)
        return true;
    int rows = n_rows();
    int step = list_step(b, now);
    if (step && rows)
        sel = (sel + step + rows) % rows;
    if ((b->pressed & UI_BTN_B) && rows) {
        if (sel < n_dirs)
            enter_dir(dirs[sel]);
        else if (page_prev && sel == n_dirs)
            change_page(-1);
        else if (sel >= first_tune_row() + n_tunes())
            change_page(1);
        else
            start_playing(sel - first_tune_row());
    }
    return true;
}

static bool rtttl_task(absolute_time_t now) {
    if (list_pending && display_is_idle()) {
        list_pending = false;
        load_list();
        return true;
    }
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
        sel = first_tune_row() + cur;
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

static void row_label(int r, char *buf, size_t len) {
    if (r < n_dirs) {
        snprintf(buf, len, "%s/", dirs[r]);
        return;
    }
    if (page_prev && r == n_dirs) {
        snprintf(buf, len, "< Précédents");
        return;
    }
    int i = r - first_tune_row();
    if (i >= n_tunes()) {
        snprintf(buf, len, "Suivants >");
        return;
    }
    if (i < n_builtin()) {
        rtttl_t t;
        rtttl_open(&t, RTTTL_BUILTIN[i], strlen(RTTTL_BUILTIN[i]));
        snprintf(buf, len, "%s", t.name);
        return;
    }
    const sd_tune_t *s = &sd_tunes[i - n_builtin()];
    /* The name of the file, without its extension */
    char file[SD_NAME_MAX];
    snprintf(file, sizeof(file), "%s", files[s->file]);
    char *dot = strrchr(file, '.');
    if (dot)
        *dot = 0;
    snprintf(buf, len, "%s%s (%s)", s->err != RTTTL_OK ? "(!) " : "", s->name, file);
}

static void source_name(char *buf, size_t len) {
    if (cur < n_builtin())
        snprintf(buf, len, "Sonnerie du badge");
    else
        snprintf(buf, len, "%s/%s", dir, files[sd_tunes[cur - n_builtin()].file]);
}

static void render_play(uint8_t *fb) {
    char text[PATH_MAX_LEN + SD_NAME_MAX + 2], fitted[64];
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
    char text[PATH_MAX_LEN + SD_NAME_MAX + 2], fitted[64];
    ui_title(fb, "Sonnerie invalide");
    int y = ui_wrapped(fb, UI_TITLE_H + 6, &gfx_font_medium, song.name, 2) + 4;
    source_name(text, sizeof(text));
    ui_fit_preview(&gfx_font_small, fitted, sizeof(fitted), text, GFX_WIDTH - 8);
    gfx_text(fb, GFX_WIDTH/2, y, &gfx_font_small, fitted, GFX_BLACK, GFX_ALIGN_CENTER);
    y += 26;
    if (cur >= n_builtin())
        snprintf(text, sizeof(text), "Ligne %u, colonne %u :", sd_tunes[cur - n_builtin()].line,
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
        const char *slash = strrchr(dir, '/');
        ui_title(fb, ! dir[0] ? "Sonneries" : slash ? slash + 1 : dir);
        ui_list(fb, n_rows(), sel, row_label);
        ui_footer(fb, list_pending ? "Lecture de la carte..." : status[0] ? status : "G : retour  D : choisir");
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
