/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The SD card through the USB serial port (tools/sd_files.py): the byte 0x05, then a command line ended by '\n'.
 * The path is always the last argument (it may contain spaces), the data is in base64:
 *
 *   L <dir>                    list:  "@SD E <d|f> <size> <date> <name>" per entry, then "@SD OK <n>"
 *   S <path>                   stat:  "@SD OK <d|f> <size> <date>"
 *   R <offset> <len> <file>    read at most SD_SERIAL_CHUNK bytes: "@SD OK <n> <base64>"
 *   W <offset> <base64> <file> write (offset 0: the file is created or emptied): "@SD OK <n>"; the file stays open
 *                              for the next chunk, until C or another command
 *   C                          close the file written: "@SD OK"
 *   D <path>                   delete a file or an empty folder
 *   M <path>                   make a folder
 *   N <old>\t<new>             rename / move
 *   F                          free space: "@SD OK <free KB> <total KB>"
 *   T <date>                   set the clock (the date of the files created or written from now on)
 *   U <date> <path>            set the date of a file or folder (the date of the original, after a copy)
 *
 * A date: the local time of the sender (the PC, the phone...) in seconds since 1970-01-01, counted as if UTC.
 * An error: "@SD ERR <FatFs code> <text>". The same operations as the FILE_* API of the ESP32 module
 * (docs/etude_module_esp32.md). Everything runs in the main loop, like the other users of the card. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"

#include "ff.h"
#include "sd.h"
#include "sd_serial.h"

#define LINE_MAX 900  /* "W", an offset, the base64 of a chunk, a path */
#define CHAR_TIMEOUT_US 200000

static char line[LINE_MAX];
static uint8_t data[SD_SERIAL_CHUNK];
static FIL wfile;
static bool writing = false;
static char wpath[SD_NAME_MAX * 2];

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Encodes \p n bytes into \p out (4 * ceil(n / 3) + 1 bytes) */
static void to_base64(const uint8_t *p, size_t n, char *out) {
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = p[i] << 16 | (i + 1 < n ? p[i + 1] << 8 : 0) | (i + 2 < n ? p[i + 2] : 0);
        *out++ = B64[v >> 18];
        *out++ = B64[(v >> 12) & 63];
        *out++ = i + 1 < n ? B64[(v >> 6) & 63] : '=';
        *out++ = i + 2 < n ? B64[v & 63] : '=';
    }
    *out = 0;
}

/* Decodes \p s (ended by a space or 0) into data: the number of bytes, -1 when invalid or too long */
static int get_base64(const char *s) {
    uint32_t v = 0;
    int bits = 0, n = 0;
    for (; *s && *s != ' ' && *s != '='; ++s) {
        const char *c = strchr(B64, *s);
        if (! c || ! *c)
            return -1;
        v = v << 6 | (uint32_t)(c - B64);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (n >= SD_SERIAL_CHUNK)
                return -1;
            data[n++] = (uint8_t)(v >> bits);
        }
    }
    return n;
}

static const char *const ERRORS[] = {
    "ok", "disk error", "internal error", "not ready", "no file", "no path", "invalid name", "denied", "exists",
    "invalid object", "write protected", "invalid drive", "not enabled", "no filesystem", "mkfs aborted", "timeout",
    "locked", "not enough core", "too many open files", "invalid parameter"};

static void reply_error(int fr) {
    printf("@SD ERR %d %s\n", fr, fr >= 0 && fr < (int)(sizeof(ERRORS) / sizeof(*ERRORS)) ? ERRORS[fr] : "?");
}

static void close_written(void) {
    if (writing)
        f_close(&wfile);
    writing = false;
}

/* The next space separated argument: a number */
static bool number(char **s, uint32_t *v) {
    char *end;
    *v = strtoul(*s, &end, 10);
    if (end == *s || *end != ' ')
        return false;
    *s = end + 1;
    return true;
}

static void cmd_list(const char *path) {
    DIR d;
    FILINFO info;
    int fr = f_opendir(&d, path);
    if (fr != FR_OK)
        return reply_error(fr);
    int n = 0;
    while (f_readdir(&d, &info) == FR_OK && info.fname[0]) {
        printf("@SD E %c %llu %lu %s\n", info.fattrib & AM_DIR ? 'd' : 'f', (unsigned long long)info.fsize,
               (unsigned long)sd_local_time(info.fdate, info.ftime), info.fname);
        ++n;
    }
    f_closedir(&d);
    printf("@SD OK %d\n", n);
}

static void cmd_stat(const char *path) {
    FILINFO info;
    if (! path[0] || ! strcmp(path, "/")) {  /* The root has no entry */
        printf("@SD OK d 0 0\n");
        return;
    }
    int fr = f_stat(path, &info);
    if (fr != FR_OK)
        return reply_error(fr);
    printf("@SD OK %c %llu %lu\n", info.fattrib & AM_DIR ? 'd' : 'f', (unsigned long long)info.fsize,
           (unsigned long)sd_local_time(info.fdate, info.ftime));
}

static void cmd_read(char *args) {
    uint32_t offset, len;
    if (! number(&args, &offset) || ! number(&args, &len))
        return reply_error(FR_INVALID_PARAMETER);
    if (len > SD_SERIAL_CHUNK)
        len = SD_SERIAL_CHUNK;
    FIL f;
    int fr = f_open(&f, args, FA_READ);
    if (fr != FR_OK)
        return reply_error(fr);
    UINT got = 0;
    fr = f_lseek(&f, offset);
    if (fr == FR_OK)
        fr = f_read(&f, data, len, &got);
    f_close(&f);
    if (fr != FR_OK)
        return reply_error(fr);
    to_base64(data, got, line);  /* The command line is no longer needed; one printf: the answer is not mixed */
    printf("@SD OK %u %s\n", (unsigned)got, line);
}

static void cmd_write(char *args) {
    uint32_t offset;
    if (! number(&args, &offset))
        return reply_error(FR_INVALID_PARAMETER);
    int n = get_base64(args);
    char *path = strchr(args, ' ');
    if (n < 0 || ! path)
        return reply_error(FR_INVALID_PARAMETER);
    ++path;
    int fr;
    if (offset == 0 || ! writing || strcmp(path, wpath)) {
        close_written();
        fr = f_open(&wfile, path, offset == 0 ? FA_WRITE | FA_CREATE_ALWAYS : FA_WRITE | FA_OPEN_EXISTING);
        if (fr != FR_OK)
            return reply_error(fr);
        writing = true;
        snprintf(wpath, sizeof(wpath), "%s", path);
    }
    UINT done = 0;
    fr = f_lseek(&wfile, offset);
    if (fr == FR_OK)
        fr = f_write(&wfile, data, n, &done);
    if (fr != FR_OK || (int)done != n) {
        close_written();
        return reply_error(fr != FR_OK ? fr : FR_DENIED);  /* Card full */
    }
    printf("@SD OK %u\n", (unsigned)done);
}

static void cmd_free(void) {
    FATFS *fs;
    DWORD clusters;
    int fr = f_getfree("", &clusters, &fs);
    if (fr != FR_OK)
        return reply_error(fr);
    uint64_t kb = (uint64_t)fs->csize * 512 / 1024;
    printf("@SD OK %llu %llu\n", (unsigned long long)(clusters * kb), (unsigned long long)((fs->n_fatent - 2) * kb));
}

/* Reads the command line, 0 when it does not come */
static bool read_line(void) {
    size_t n = 0;
    for (;;) {
        int c = getchar_timeout_us(CHAR_TIMEOUT_US);
        if (c < 0)
            return false;
        if (c == '\n')
            break;
        if (c != '\r' && n < LINE_MAX - 1)
            line[n++] = (char)c;
    }
    line[n] = 0;
    return true;
}

void sd_serial_command(void) {
    if (! read_line()) {
        printf("@SD ERR %d incomplete command\n", FR_INVALID_PARAMETER);
        return;
    }
    char cmd = line[0];
    char *args = line[1] == ' ' ? line + 2 : line + 1;
    if (cmd != 'W')
        close_written();
    if (cmd == 'T') {  /* The clock: no card needed */
        sd_set_time(strtoul(args, NULL, 10));
        printf("@SD OK\n");
        return;
    }
    int fr = sd_mount();
    if (fr != FR_OK) {
        sd_unmount();  /* The card may have been changed: mounted again next time */
        writing = false;
        return reply_error(fr);
    }
    switch (cmd) {
    case 'L': cmd_list(args); break;
    case 'S': cmd_stat(args); break;
    case 'R': cmd_read(args); break;
    case 'W': cmd_write(args); break;
    case 'C': printf("@SD OK\n"); break;
    case 'D': fr = f_unlink(args); fr == FR_OK ? (void)printf("@SD OK\n") : reply_error(fr); break;
    case 'M': fr = f_mkdir(args); fr == FR_OK ? (void)printf("@SD OK\n") : reply_error(fr); break;
    case 'N': {
        char *to = strchr(args, '\t');
        if (! to)
            return reply_error(FR_INVALID_PARAMETER);
        *to++ = 0;
        fr = f_rename(args, to);
        fr == FR_OK ? (void)printf("@SD OK\n") : reply_error(fr);
        break;
    }
    case 'F': cmd_free(); break;
    case 'U': {
        uint32_t t;
        if (! number(&args, &t))
            return reply_error(FR_INVALID_PARAMETER);
        FILINFO info;
        uint32_t fat = sd_fat_datetime(t);
        info.fdate = (WORD)(fat >> 16);
        info.ftime = (WORD)fat;
        fr = f_utime(args, &info);
        fr == FR_OK ? (void)printf("@SD OK\n") : reply_error(fr);
        break;
    }
    default: printf("@SD ERR %d unknown command %c\n", FR_INVALID_PARAMETER, cmd); break;
    }
}
