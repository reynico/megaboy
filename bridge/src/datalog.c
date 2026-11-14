#include "datalog.h"

#include <stdio.h>
#include <string.h>

#include "ff.h"

#define MAX_NUM 9999u
#define ROW_MAX 96

static FATFS fs;
static FIL fil;
static uint8_t mounted;
static uint8_t state = LINK_LOG_NOCARD;
static uint16_t num;
static uint32_t start_ms;
static uint32_t last_ms;
static uint16_t rows;
static uint8_t hz;
static uint8_t dated;
static rtc_time_t start;

static uint8_t mount(void) {
    mounted = f_mount(&fs, "", 1) == FR_OK;
    return mounted;
}

void log_probe(void) {
    if (state == LINK_LOG_REC || mounted) {
        return;
    }
    if (mount()) {
        state = LINK_LOG_IDLE;
        printf("sd: card ok\n");
    }
}

static uint16_t next_num(void) {
    DIR dir;
    FILINFO fi;
    uint16_t max = 0;
    if (f_opendir(&dir, "") != FR_OK) {
        return 0;
    }
    while (f_readdir(&dir, &fi) == FR_OK && fi.fname[0]) {
        const char *n = fi.fname;
        if (strlen(n) == 11 && !strncmp(n, "LOG", 3) && !strcmp(n + 7, ".MSL")) {
            uint16_t v = 0;
            uint8_t ok = 1;
            for (int i = 3; i < 7; i++) {
                if (n[i] < '0' || n[i] > '9') {
                    ok = 0;
                    break;
                }
                v = (uint16_t)(v * 10u + (uint16_t)(n[i] - '0'));
            }
            if (ok && v > max) {
                max = v;
            }
        }
    }
    f_closedir(&dir);
    return (uint16_t)(max + 1u);
}

static void fail(uint8_t st, const char *why) {
    printf("sd: %s\n", why);
    if (state == LINK_LOG_REC) {
        f_close(&fil);
    }
    f_mount(0, "", 0);
    mounted = 0;
    state = st;
}

static uint8_t put(const char *s, UINT n) {
    UINT bw;
    FRESULT r = f_write(&fil, s, n, &bw);
    if (r == FR_OK && bw == n) {
        return 1;
    }
    fail(r == FR_OK ? LINK_LOG_FULL : LINK_LOG_ERR, r == FR_OK ? "card full" : "write error");
    return 0;
}

static FRESULT open_log(const rtc_time_t *t, char *name, size_t n) {
    FRESULT r;
    if (t) {
        snprintf(name, n, "20%02u-%02u-%02u_%02u.%02u.%02u.msl", t->yy, t->mo, t->dd, t->hh, t->mi, t->ss);
        r = f_open(&fil, name, FA_WRITE | FA_CREATE_NEW);
        if (r != FR_EXIST) {
            dated = 1;
            start = *t;
            return r;
        }
    }
    dated = 0;
    snprintf(name, n, "LOG%04u.MSL", num);
    return f_open(&fil, name, FA_WRITE | FA_CREATE_NEW);
}

void log_start(const char *sig, const rtc_time_t *t, uint32_t now_ms) {
    char name[32];
    char line[ROW_MAX];
    int n;
    if (state == LINK_LOG_REC) {
        return;
    }
    if (!mount()) {
        fail(LINK_LOG_NOCARD, "no card");
        return;
    }
    num = next_num();
    if (num == 0 || num > MAX_NUM) {
        fail(num ? LINK_LOG_FULL : LINK_LOG_ERR, "no file number");
        return;
    }
    if (open_log(t, name, sizeof name) != FR_OK) {
        fail(LINK_LOG_ERR, "open failed");
        return;
    }
    state = LINK_LOG_REC;
    start_ms = now_ms;
    last_ms = now_ms;
    rows = 0;
    hz = 0;
    n = snprintf(line, sizeof line, "\"%s\"\n", sig);
    if (!put(line, (UINT)n)) {
        return;
    }
    if (t) {
        n = snprintf(line, sizeof line, "\"Capture Date: 20%02u-%02u-%02u %02u:%02u:%02u\"\n", t->yy, t->mo, t->dd,
                     t->hh, t->mi, t->ss);
        if (!put(line, (UINT)n)) {
            return;
        }
    }
    static const char cols[] = "Time\tRPM\tMAP\tTPS\tAFR\tCLT\tMAT\tBatt V\tAdvance\tBoost\n"
                               "s\tRPM\tkPa\t%\tAFR\tC\tC\tV\tdeg\tpsi\n";
    if (put(cols, sizeof cols - 1) && f_sync(&fil) == FR_OK) {
        printf("sd: recording %s\n", name);
    }
}

void log_stop(void) {
    if (state != LINK_LOG_REC) {
        return;
    }
    state = f_close(&fil) == FR_OK ? LINK_LOG_IDLE : LINK_LOG_ERR;
    printf("sd: stopped\n");
}

static char *x10(char *o, int16_t v) {
    if (v < 0) {
        *o++ = '-';
        v = (int16_t)-v;
    }
    return o + sprintf(o, "%d.%d", v / 10, v % 10);
}

void log_row(const ecu_rt_t *r, uint32_t now_ms) {
    char line[ROW_MAX];
    char *o = line;
    uint32_t ms;
    if (state != LINK_LOG_REC) {
        return;
    }
    ms = now_ms - start_ms;
    o += sprintf(o, "%lu.%03lu\t%u\t", (unsigned long)(ms / 1000u), (unsigned long)(ms % 1000u), r->rpm);
    o = x10(o, r->map_x10);
    *o++ = '\t';
    o = x10(o, r->tps_x10);
    *o++ = '\t';
    o = x10(o, r->afr_x10);
    *o++ = '\t';
    o = x10(o, r->clt_c_x10);
    *o++ = '\t';
    o = x10(o, r->iat_c_x10);
    *o++ = '\t';
    o = x10(o, r->batt_x10);
    *o++ = '\t';
    o = x10(o, r->adv_x10);
    *o++ = '\t';
    o = x10(o, r->boost_psi_x10);
    *o++ = '\n';
    if (put(line, (UINT)(o - line))) {
        rows++;
    }
}

void log_second(uint32_t now_ms) {
    if (state != LINK_LOG_REC) {
        return;
    }
    hz = rows > 255u ? 255u : (uint8_t)rows;
    rows = 0;
    last_ms = now_ms;
    if (f_sync(&fil) != FR_OK) {
        fail(LINK_LOG_ERR, "sync error");
    }
}

void log_status(link_log_t *out) {
    out->state = state;
    out->num = num;
    out->secs = 0;
    out->bytes = 0;
    out->hz = 0;
    out->dated = dated;
    out->start = start;
    if (state == LINK_LOG_REC) {
        uint32_t s = (last_ms - start_ms) / 1000u;
        out->secs = s > 0xffffu ? 0xffffu : (uint16_t)s;
        out->bytes = f_size(&fil);
        out->hz = hz;
    }
}

uint8_t log_active(void) {
    return state == LINK_LOG_REC;
}
