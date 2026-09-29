/* FatFs stand-in on stdio, for the tests of rsvp.c */
#pragma once
#include <stdio.h>
#include <stdint.h>
typedef unsigned int UINT;
typedef uint64_t FSIZE_t;
typedef enum { FR_OK = 0, FR_NO_FILE = 4, FR_NO_FILESYSTEM = 13 } FRESULT;
typedef struct { FILE *f; } FIL;
#define FA_READ 1
static inline FRESULT f_open(FIL *fp, const char *p, int m) { (void)m; fp->f = fopen(p, "rb"); return fp->f ? FR_OK : FR_NO_FILE; }
static inline FRESULT f_read(FIL *fp, void *b, UINT n, UINT *r) { *r = (UINT)fread(b, 1, n, fp->f); return FR_OK; }
static inline FRESULT f_lseek(FIL *fp, FSIZE_t o) { fseek(fp->f, (long)o, SEEK_SET); return FR_OK; }
static inline FRESULT f_close(FIL *fp) { fclose(fp->f); return FR_OK; }
static inline FSIZE_t f_size(FIL *fp) { long c = ftell(fp->f); fseek(fp->f, 0, SEEK_END); long s = ftell(fp->f); fseek(fp->f, c, SEEK_SET); return (FSIZE_t)s; }
