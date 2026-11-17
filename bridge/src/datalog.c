#include "datalog.h"

#include <stdio.h>
#include <string.h>

#include "ff.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/sync.h"
#include "pico/util/queue.h"

#define MAX_NUM 9999u
#define ROW_MAX 96
#define ROW_Q 64
#define CMD_Q 4
#define SIG_MAX 33
#define PROBE_MS 5000u
#define SYNC_MS 1000u
#define IDLE_US 500u

enum { OP_START, OP_STOP };

typedef struct {
    uint32_t ms;
    ecu_rt_t rt;
} row_t;

typedef struct {
    uint8_t op;
    uint8_t have_t;
    uint32_t ms;
    rtc_time_t t;
    char sig[SIG_MAX];
} cmd_t;

static queue_t row_q;
static queue_t cmd_q;
static critical_section_t cs;
static link_log_t pub;
static rtc_time_t clk;
static uint8_t clk_ok;
static uint8_t want;
static uint32_t dropped;

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

static void w_probe(void) {
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

static void w_start(const char *sig, const rtc_time_t *t, uint32_t now_ms) {
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

static void w_stop(void) {
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

static void w_row(const ecu_rt_t *r, uint32_t now_ms) {
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

static void w_second(uint32_t now_ms) {
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

static void publish(void) {
    link_log_t l = {state, num, 0, 0, 0, dated, start};
    if (state == LINK_LOG_REC) {
        uint32_t secs = (last_ms - start_ms) / 1000u;
        l.secs = secs > 0xffffu ? 0xffffu : (uint16_t)secs;
        l.bytes = f_size(&fil);
        l.hz = hz;
    }
    critical_section_enter_blocking(&cs);
    pub = l;
    critical_section_exit(&cs);
}

static void core1_main(void) {
    uint32_t last_sec = to_ms_since_boot(get_absolute_time());
    uint32_t last_probe = last_sec - PROBE_MS;
    multicore_lockout_victim_init();
    for (;;) {
        cmd_t c;
        row_t r;
        uint8_t work = 0;
        uint32_t now;
        while (queue_try_remove(&cmd_q, &c)) {
            if (c.op == OP_START) {
                w_start(c.sig, c.have_t ? &c.t : 0, c.ms);
            } else {
                w_stop();
            }
            publish();
            work = 1;
        }
        while (queue_try_remove(&row_q, &r)) {
            w_row(&r.rt, r.ms);
            work = 1;
        }
        now = to_ms_since_boot(get_absolute_time());
        if (now - last_sec >= SYNC_MS) {
            last_sec = now;
            w_second(now);
            publish();
        }
        if (state != LINK_LOG_REC && !mounted && now - last_probe >= PROBE_MS) {
            last_probe = now;
            w_probe();
            publish();
        }
        if (!work) {
            sleep_us(IDLE_US);
        }
    }
}

void log_init(void) {
    critical_section_init(&cs);
    queue_init(&row_q, sizeof(row_t), ROW_Q);
    queue_init(&cmd_q, sizeof(cmd_t), CMD_Q);
    pub.state = LINK_LOG_NOCARD;
    multicore_launch_core1(core1_main);
}

void log_start(const char *sig, const rtc_time_t *t, uint32_t now_ms) {
    cmd_t c = {OP_START, t != 0, now_ms, {0}, {0}};
    if (t) {
        c.t = *t;
    }
    snprintf(c.sig, sizeof c.sig, "%s", sig);
    want = 1;
    queue_try_add(&cmd_q, &c);
}

void log_stop(void) {
    cmd_t c = {OP_STOP, 0, 0, {0}, {0}};
    want = 0;
    queue_try_add(&cmd_q, &c);
}

void log_row(const ecu_rt_t *r, uint32_t now_ms) {
    row_t row = {now_ms, *r};
    if (want && !queue_try_add(&row_q, &row)) {
        dropped++;
    }
}

uint32_t log_dropped(void) {
    return dropped;
}

void log_clock(const rtc_time_t *t, uint8_t ok) {
    critical_section_enter_blocking(&cs);
    clk = *t;
    clk_ok = ok;
    critical_section_exit(&cs);
}

DWORD get_fattime(void) {
    rtc_time_t t;
    uint8_t ok;
    critical_section_enter_blocking(&cs);
    t = clk;
    ok = clk_ok;
    critical_section_exit(&cs);
    if (!ok) {
        return (DWORD)(FF_NORTC_YEAR - 1980) << 25 | (DWORD)FF_NORTC_MON << 21 | (DWORD)FF_NORTC_MDAY << 16;
    }
    return (DWORD)(t.yy + 20u) << 25 | (DWORD)t.mo << 21 | (DWORD)t.dd << 16 | (DWORD)t.hh << 11 | (DWORD)t.mi << 5 |
           (DWORD)(t.ss / 2u);
}

void log_status(link_log_t *out) {
    critical_section_enter_blocking(&cs);
    *out = pub;
    critical_section_exit(&cs);
}

uint8_t log_active(void) {
    uint8_t rec;
    critical_section_enter_blocking(&cs);
    rec = pub.state == LINK_LOG_REC;
    critical_section_exit(&cs);
    return rec;
}
