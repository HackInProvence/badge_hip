/* FatFs stand-in on stdio and dirent, for the test of flipper.c */
#pragma once
#include <dirent.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef unsigned int UINT;
typedef uint64_t FSIZE_t;
typedef enum { FR_OK = 0, FR_NO_FILE = 4, FR_NO_PATH = 5, FR_NO_FILESYSTEM = 13 } FRESULT;
typedef struct { FILE *f; } FIL;
typedef struct { DIR *d; } FF_DIR;
#define DIR FF_DIR  /* The DIR of FatFs, after the one of dirent.h */
#define FA_READ 1
#define AM_DIR 0x10
typedef struct { char fname[256]; uint8_t fattrib; } FILINFO;

static inline FRESULT f_open(FIL *fp, const char *p, int m) {
    (void)m;
    fp->f = fopen(p, "rb");
    return fp->f ? FR_OK : FR_NO_FILE;
}
static inline FRESULT f_read(FIL *fp, void *b, UINT n, UINT *r) { *r = (UINT)fread(b, 1, n, fp->f); return FR_OK; }
static inline FRESULT f_lseek(FIL *fp, FSIZE_t o) { return fseek(fp->f, (long)o, SEEK_SET) ? FR_NO_FILE : FR_OK; }
static inline FRESULT f_close(FIL *fp) {
    if (fp->f)
        fclose(fp->f);
    fp->f = NULL;
    return FR_OK;
}
static inline FRESULT f_opendir(FF_DIR *dp, const char *p) {
    dp->d = opendir(p);
    return dp->d ? FR_OK : FR_NO_PATH;
}
static inline FRESULT f_readdir(FF_DIR *dp, FILINFO *fi) {
    struct dirent *e = readdir(dp->d);
    fi->fname[0] = 0;
    fi->fattrib = 0;
    if (e) {
        snprintf(fi->fname, sizeof(fi->fname), "%s", e->d_name);
        if (! strcmp(e->d_name, ".") || ! strcmp(e->d_name, ".."))
            fi->fattrib = AM_DIR;
    }
    return FR_OK;
}
static inline FRESULT f_closedir(FF_DIR *dp) {
    closedir(dp->d);
    return FR_OK;
}
