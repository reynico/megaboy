#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/datalog.c"

static int fails;

#define CHECK(c)                                                  \
    do {                                                          \
        if (!(c)) {                                               \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); \
            fails++;                                              \
        }                                                         \
    } while (0)

#define MAX_FILES 8
#define FILE_MAX 65536

uint32_t stub_now_ms = 1000;
static int card = 1;
static int nfiles;
static int open_file = -1;
static int syncs;
static char data[MAX_FILES][FILE_MAX];
static unsigned len[MAX_FILES];

FRESULT f_mount(FATFS *fs, const char *path, uint8_t opt) {
    (void)fs;
    (void)path;
    (void)opt;
    return card ? FR_OK : FR_DISK_ERR;
}

FRESULT f_open(FIL *f, const char *name, uint8_t mode) {
    (void)name;
    (void)mode;
    if (!card || nfiles == MAX_FILES) {
        return FR_DISK_ERR;
    }
    f->idx = nfiles;
    open_file = nfiles;
    len[nfiles] = 0;
    nfiles++;
    return FR_OK;
}

FRESULT f_write(FIL *f, const void *buf, UINT n, UINT *bw) {
    if (!card || f->idx != open_file) {
        return FR_DISK_ERR;
    }
    memcpy(data[f->idx] + len[f->idx], buf, n);
    len[f->idx] += n;
    *bw = n;
    return FR_OK;
}

FRESULT f_sync(FIL *f) {
    (void)f;
    syncs++;
    return card ? FR_OK : FR_DISK_ERR;
}

FRESULT f_close(FIL *f) {
    (void)f;
    open_file = -1;
    return FR_OK;
}

DWORD f_size(FIL *f) {
    return len[f->idx];
}

FRESULT f_opendir(DIR *d, const char *path) {
    (void)d;
    (void)path;
    return FR_OK;
}

FRESULT f_readdir(DIR *d, FILINFO *fi) {
    (void)d;
    fi->fname[0] = 0;
    return FR_OK;
}

FRESULT f_closedir(DIR *d) {
    (void)d;
    return FR_OK;
}

static void reset(void) {
    memset(len, 0, sizeof len);
    nfiles = 0;
    open_file = -1;
    syncs = 0;
    card = 1;
    stub_now_ms = 1000;
    pend_n = 0;
    pend_head = 0;
    want = 0;
    dropped = 0;
    state = LINK_LOG_NOCARD;
    mounted = 0;
    if (item_q.buf) {
        free(item_q.buf);
        item_q.buf = 0;
    }
    log_init();
    last_sec = stub_now_ms;
    last_probe = stub_now_ms - PROBE_MS;
}

static void drain(void) {
    for (int i = 0; i < 1000; i++) {
        log_poll();
        if (!worker_step() && !pend_n && !item_q.n) {
            return;
        }
    }
}

static ecu_rt_t row(uint16_t rpm) {
    ecu_rt_t r;
    memset(&r, 0, sizeof r);
    r.rpm = rpm;
    return r;
}

static int rows_in(int f) {
    int n = 0;
    data[f][len[f]] = 0;
    for (char *p = data[f]; (p = strchr(p, '\n')) != 0; p++) {
        if (p[1] >= '0' && p[1] <= '9') {
            n++;
        }
    }
    return n;
}

static void test_order_across_sessions(void) {
    reset();
    drain();
    ecu_rt_t a = row(1111);
    ecu_rt_t b = row(2222);
    ecu_rt_t c = row(3333);
    log_start("sig", 0, 1000);
    log_row(&a, 1050);
    log_row(&b, 1100);
    log_stop();
    log_start("sig", 0, 1200);
    log_row(&c, 1250);
    drain();
    CHECK(nfiles == 2);
    CHECK(rows_in(0) == 2 && strstr(data[0], "1111") && strstr(data[0], "2222") && !strstr(data[0], "3333"));
    CHECK(rows_in(1) == 1 && strstr(data[1], "0.050\t3333"));
    CHECK(!strstr(data[1], "429496"));
    CHECK(dropped == 0);
}

static void test_stop_with_full_queue(void) {
    reset();
    drain();
    log_start("sig", 0, 1000);
    drain();
    ecu_rt_t r = row(4000);
    for (int i = 0; i < ITEM_Q; i++) {
        log_row(&r, (uint32_t)(1001 + i));
    }
    CHECK(item_q.n == ITEM_Q && dropped == 0);
    log_row(&r, 1999);
    CHECK(dropped == 1);
    log_stop();
    CHECK(pend_n == 1);
    log_row(&r, 2000);
    CHECK(dropped == 1);
    drain();
    CHECK(pend_n == 0 && open_file == -1 && state == LINK_LOG_IDLE);
    CHECK(rows_in(0) == ITEM_Q);
}

static void test_sync_under_load(void) {
    reset();
    drain();
    log_start("sig", 0, 1000);
    drain();
    int before = syncs;
    ecu_rt_t r = row(5000);
    for (int ms = 1000; ms < 6000; ms += 25) {
        stub_now_ms = (uint32_t)ms;
        log_row(&r, (uint32_t)ms);
        log_row(&r, (uint32_t)ms);
        log_poll();
        worker_step();
    }
    CHECK(syncs - before >= 4);
}

static void fill_queue(void) {
    for (int i = 0; i < ITEM_Q; i++) {
        item_t it = {IT_ROW, 0, 0, {{0}}};
        queue_try_add(&item_q, &it);
    }
}

static void test_redundant_commands(void) {
    reset();
    drain();
    fill_queue();
    log_start("sig", 0, 1000);
    log_start("sig", 0, 1000);
    log_start("sig", 0, 1000);
    CHECK(pend_n == 1 && want == 1);
    log_stop();
    log_stop();
    CHECK(pend_n == 2 && want == 0);
}

static void test_stop_always_accepted(void) {
    reset();
    drain();
    log_start("sig", 0, 1000);
    drain();
    CHECK(state == LINK_LOG_REC);
    fill_queue();
    log_stop();
    log_start("sig", 0, 1100);
    log_stop();
    log_start("sig", 0, 1200);
    CHECK(pend_n == PEND_MAX && want == 1);
    log_stop();
    CHECK(want == 0 && pend_n == PEND_MAX - 1);
    log_start("sig", 0, 1300);
    log_start("sig", 0, 1400);
    CHECK(want == 1 && pend_n == PEND_MAX);
    log_start("sig", 0, 1500);
    CHECK(pend_n == PEND_MAX);
    log_stop();
    CHECK(want == 0);
    drain();
    CHECK(pend_n == 0 && state == LINK_LOG_IDLE && open_file == -1);
}

int main(void) {
    test_order_across_sessions();
    test_stop_with_full_queue();
    test_sync_under_load();
    test_redundant_commands();
    test_stop_always_accepted();
    if (fails) {
        fprintf(stderr, "%d failures\n", fails);
        return 1;
    }
    puts("ok");
    return 0;
}
