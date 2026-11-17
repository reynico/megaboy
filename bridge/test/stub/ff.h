#pragma once

#include <stdint.h>

#define FF_NORTC_YEAR 2026
#define FF_NORTC_MON 1
#define FF_NORTC_MDAY 1
#define FA_WRITE 0x02
#define FA_CREATE_NEW 0x04

typedef unsigned int UINT;
typedef uint32_t DWORD;
typedef enum { FR_OK = 0, FR_DISK_ERR, FR_EXIST = 8, FR_NO_FILE = 4 } FRESULT;

typedef struct {
    int unused;
} FATFS;

typedef struct {
    int idx;
} FIL;

typedef struct {
    int unused;
} DIR;

typedef struct {
    char fname[33];
} FILINFO;

FRESULT f_mount(FATFS *fs, const char *path, uint8_t opt);
FRESULT f_open(FIL *f, const char *name, uint8_t mode);
FRESULT f_write(FIL *f, const void *buf, UINT n, UINT *bw);
FRESULT f_sync(FIL *f);
FRESULT f_close(FIL *f);
DWORD f_size(FIL *f);
FRESULT f_opendir(DIR *d, const char *path);
FRESULT f_readdir(DIR *d, FILINFO *fi);
FRESULT f_closedir(DIR *d);
