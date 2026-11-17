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
#define ITEM_Q 64
#define PEND_MAX 4
#define BATCH 8
#define SIG_MAX 33
#define PROBE_MS 5000u
#define SYNC_MS 1000u
#define IDLE_US 500u

enum { IT_ROW, IT_START, IT_STOP };

typedef struct {
    uint8_t kind;
    uint8_t have_t;
    uint32_t ms;
    uint32_t seq;
    union {
        ecu_rt_t rt;
        struct {
            rtc_time_t t;
            char sig[SIG_MAX];
        } start;
    } u;
} item_t;

static queue_t item_q;
static item_t pend[PEND_MAX];
static uint8_t pend_head;
static uint8_t pend_n;
static critical_section_t cs;
static link_log_t pub;
static rtc_time_t clk;
static uint8_t clk_ok;
static uint8_t want;
static uint32_t dropped;
static uint32_t sent_seq;
static uint8_t sent_kind = IT_STOP;
static uint32_t done_seq;

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

static int x10(char *o, size_t n, int16_t v, char end) {
    int32_t a = v < 0 ? -(int32_t)v : v;
    return snprintf(o, n, "%s%ld.%ld%c", v < 0 ? "-" : "", (long)(a / 10), (long)(a % 10), end);
}

static void w_row(const ecu_rt_t *r, uint32_t now_ms) {
    char line[ROW_MAX];
    const int16_t v[] = {r->map_x10, r->tps_x10,  r->afr_x10, r->clt_c_x10,
                         r->iat_c_x10, r->batt_x10, r->adv_x10, r->boost_psi_x10};
    size_t used;
    uint32_t ms;
    if (state != LINK_LOG_REC || (int32_t)(now_ms - start_ms) < 0) {
        return;
    }
    ms = now_ms - start_ms;
    used = (size_t)snprintf(line, sizeof line, "%lu.%03lu\t%u\t", (unsigned long)(ms / 1000u),
                            (unsigned long)(ms % 1000u), r->rpm);
    for (size_t k = 0; k < sizeof v / sizeof v[0] && used < sizeof line; k++) {
        used += (size_t)x10(line + used, sizeof line - used, v[k], k + 1 < sizeof v / sizeof v[0] ? '\t' : '\n');
    }
    if (used >= sizeof line) {
        return;
    }
    if (put(line, (UINT)used)) {
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

static void publish(uint8_t done, uint32_t seq) {
    link_log_t l = {state, num, 0, 0, 0, dated, start};
    if (state == LINK_LOG_REC) {
        uint32_t secs = (last_ms - start_ms) / 1000u;
        l.secs = secs > 0xffffu ? 0xffffu : (uint16_t)secs;
        l.bytes = f_size(&fil);
        l.hz = hz;
    }
    critical_section_enter_blocking(&cs);
    pub = l;
    if (done) {
        done_seq = seq;
    }
    critical_section_exit(&cs);
}

static uint32_t last_sec;
static uint32_t last_probe;

static uint8_t worker_step(void) {
    item_t it;
    uint8_t n = 0;
    uint8_t cmd = 0;
    uint32_t now;
    while (n < BATCH && queue_try_remove(&item_q, &it)) {
        n++;
        if (it.kind == IT_ROW) {
            w_row(&it.u.rt, it.ms);
        } else if (it.kind == IT_START) {
            w_start(it.u.start.sig, it.have_t ? &it.u.start.t : 0, it.ms);
        } else {
            w_stop();
        }
        if (it.kind != IT_ROW) {
            publish(1, it.seq);
        }
    }
    now = to_ms_since_boot(get_absolute_time());
    if (now - last_sec >= SYNC_MS) {
        last_sec = now;
        w_second(now);
        cmd = 1;
    }
    if (state != LINK_LOG_REC && !mounted && now - last_probe >= PROBE_MS) {
        last_probe = now;
        w_probe();
        cmd = 1;
    }
    if (cmd) {
        publish(0, 0);
    }
    return n;
}

static void core1_main(void) {
    last_sec = to_ms_since_boot(get_absolute_time());
    last_probe = last_sec - PROBE_MS;
    multicore_lockout_victim_init();
    for (;;) {
        if (!worker_step()) {
            sleep_us(IDLE_US);
        }
    }
}

void log_init(void) {
    critical_section_init(&cs);
    queue_init(&item_q, sizeof(item_t), ITEM_Q);
    pub.state = LINK_LOG_NOCARD;
    multicore_launch_core1(core1_main);
}

void log_poll(void) {
    while (pend_n) {
        pend[pend_head].seq = sent_seq + 1u;
        if (!queue_try_add(&item_q, &pend[pend_head])) {
            break;
        }
        sent_seq++;
        sent_kind = pend[pend_head].kind;
        pend_head = (uint8_t)((pend_head + 1u) % PEND_MAX);
        pend_n--;
    }
}

uint8_t log_active(void) {
    uint8_t rec;
    critical_section_enter_blocking(&cs);
    rec = pub.state == LINK_LOG_REC;
    critical_section_exit(&cs);
    return rec;
}

static uint8_t push_cmd(const item_t *it) {
    uint32_t done;
    uint8_t rec;
    uint8_t last;
    critical_section_enter_blocking(&cs);
    done = done_seq;
    rec = pub.state == LINK_LOG_REC;
    critical_section_exit(&cs);
    if (pend_n) {
        last = pend[(pend_head + pend_n - 1u) % PEND_MAX].kind;
    } else if (done != sent_seq) {
        last = sent_kind;
    } else {
        last = rec ? IT_START : IT_STOP;
    }
    if (it->kind == last) {
        return 1;
    }
    if (pend_n == PEND_MAX) {
        if (it->kind == IT_STOP) {
            pend_n--;
            return 1;
        }
        printf("error: log start refused\n");
        return 0;
    }
    pend[(pend_head + pend_n) % PEND_MAX] = *it;
    pend_n++;
    log_poll();
    return 1;
}

void log_start(const char *sig, const rtc_time_t *t, uint32_t now_ms) {
    item_t it = {.kind = IT_START, .have_t = t != 0, .ms = now_ms};
    if (t) {
        it.u.start.t = *t;
    }
    snprintf(it.u.start.sig, sizeof it.u.start.sig, "%s", sig);
    if (push_cmd(&it)) {
        want = 1;
    }
}

void log_stop(void) {
    item_t it = {.kind = IT_STOP};
    if (push_cmd(&it)) {
        want = 0;
    }
}

void log_row(const ecu_rt_t *r, uint32_t now_ms) {
    item_t it = {.kind = IT_ROW, .ms = now_ms};
    if (!want) {
        return;
    }
    it.u.rt = *r;
    if (pend_n || !queue_try_add(&item_q, &it)) {
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

