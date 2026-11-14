#include <stdio.h>
#include <string.h>

#include "link.h"
#include "tdata.h"

static int fails;

#define CHECK(c)                                                  \
    do {                                                          \
        if (!(c)) {                                               \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); \
            fails++;                                              \
        }                                                         \
    } while (0)

static uint8_t page[256];
static uint8_t flash[256];
static uint8_t last_type;
static uint8_t last_pl[8];
static int sent;
static int bridge_on = 1;
static int writes;
static int burns;
static int verifies;
static uint8_t last_len;

uint8_t linkrx_send(uint8_t type, const uint8_t *payload, uint8_t len) {
    last_type = type;
    memcpy(last_pl, payload, len);
    last_len = len;
    sent++;
    return 1;
}

static void ack(uint8_t c, uint8_t id, uint8_t cell, uint8_t st, uint8_t v) {
    const uint8_t p[LINK_ACK_LEN] = {c, id, cell, st, v};
    td_on_ack(p);
}

static void bridge(void) {
    const tune_table_t *t;
    if (!sent || !bridge_on) {
        sent = 0;
        return;
    }
    sent = 0;
    t = tune_get(last_pl[0]);
    if (last_type == LINK_C_READ) {
        uint8_t len = tune_blob_len(t);
        uint8_t buf[2 + LINK_TABLE_CHUNK];
        for (uint8_t off = 0; off < len; off = (uint8_t)(off + LINK_TABLE_CHUNK)) {
            uint8_t n = (uint8_t)(len - off < LINK_TABLE_CHUNK ? len - off : LINK_TABLE_CHUNK);
            buf[0] = last_pl[0];
            buf[1] = off;
            for (uint8_t i = 0; i < n; i++) {
                buf[2 + i] = page[tune_blob_offset(t, (uint8_t)(off + i))];
            }
            td_on_table(buf, (uint8_t)(2 + n));
        }
        ack(LINK_C_READ, last_pl[0], 0, TUNE_OK, 0);
    } else if (last_type == LINK_C_WRITE) {
        uint8_t cell = last_pl[1];
        uint8_t st = tune_check_write(last_pl[0], cell, page[cell], last_pl[2]);
        if (st == TUNE_OK) {
            page[cell] = last_pl[2];
            writes++;
            if (last_len == 3 || (last_pl[3] & LINK_WF_VERIFY)) {
                verifies++;
            }
        }
        ack(LINK_C_WRITE, last_pl[0], cell, st, page[cell]);
    } else if (last_type == LINK_C_BURN) {
        memcpy(flash, page, sizeof page);
        burns++;
        ack(LINK_C_BURN, last_pl[0], 0, TUNE_OK, 0);
    }
}

static void run(int ticks) {
    for (int i = 0; i < ticks; i++) {
        bridge();
        td_tick();
    }
}

static void setup(void) {
    for (int i = 0; i < 256; i++) {
        page[i] = (uint8_t)(40 + i % 50);
    }
    memcpy(flash, page, sizeof page);
    bridge_on = 1;
    writes = 0;
    burns = 0;
    verifies = 0;
    td_load(TUNE_VE1);
    run(2);
}

static void test_load(void) {
    setup();
    CHECK(td.state == TD_READY);
    CHECK(td.blob[0] == page[0]);
    CHECK(td.blob[143] == page[143]);
    CHECK(td.blob[144] == page[158]);
    CHECK(td.blob[167] == page[181]);
    CHECK(!td_dirty());
}

static void test_edit_and_burn(void) {
    setup();
    CHECK(td_set(5, (uint8_t)(page[5] + 25)));
    CHECK(!td_set(6, 50));
    uint8_t want = (uint8_t)(flash[5] + 25);
    run(20);
    CHECK(page[5] == want);
    CHECK(writes == 3);
    CHECK(verifies == 1);
    CHECK(!td_busy());
    CHECK(td_dirty());
    td_burn();
    run(3);
    CHECK(burns == 1);
    CHECK(flash[5] == want);
    CHECK(!td_dirty());
    CHECK(td.burned);
}

static void test_discard(void) {
    setup();
    uint8_t o5 = page[5];
    uint8_t o9 = page[9];
    td_set(5, (uint8_t)(o5 + 3));
    run(10);
    td_set(9, (uint8_t)(o9 - 12));
    run(10);
    CHECK(page[5] == o5 + 3 && page[9] == o9 - 12);
    td_discard();
    run(20);
    CHECK(page[5] == o5 && page[9] == o9);
    CHECK(!td.discarding && !td_dirty() && !td_busy());
    CHECK(burns == 0);
}

static void test_steps_on_ack(void) {
    setup();
    td_set(5, (uint8_t)(page[5] + 25));
    run(4);
    CHECK(!td_busy());
    CHECK(writes == 3);
}

static void test_retarget_needs_verify(void) {
    setup();
    uint8_t o = page[5];
    td_set(5, (uint8_t)(o + 25));
    td_tick();
    bridge();
    CHECK(page[5] == o + 10 && verifies == 0);
    td_set(5, (uint8_t)(o + 20));
    bridge();
    CHECK(page[5] == o + 20 && verifies == 0);
    CHECK(td_busy());
    bridge();
    CHECK(verifies == 1);
    CHECK(!td_busy());
}

static void test_limits(void) {
    setup();
    td_set(0, 255);
    CHECK(td.tgt_val == 250);
    td_set(0, 0);
    CHECK(td.tgt_val == 10);
}

static void test_timeout(void) {
    setup();
    bridge_on = 0;
    td_set(1, (uint8_t)(page[1] + 1));
    run(200);
    CHECK(td.err == TUNE_E_ECU);
    CHECK(!td_busy());
    CHECK(page[1] == flash[1]);
}

int main(void) {
    test_load();
    test_edit_and_burn();
    test_discard();
    test_steps_on_ack();
    test_retarget_needs_verify();
    test_limits();
    test_timeout();
    if (fails) {
        fprintf(stderr, "%d failures\n", fails);
        return 1;
    }
    puts("ok");
    return 0;
}
