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

static int16_t ram[TUNE_BLOB_MAX];
static int16_t flash[TUNE_BLOB_MAX];
static uint8_t ecu;
static uint8_t last_type;
static uint8_t last_pl[8];
static uint8_t last_len;
static int sent;
static int bridge_on = 1;
static int writes;
static int verifies;
static int burns;
static int drop_acks;
static int fail_verify;

uint8_t linkrx_send(uint8_t type, const uint8_t *payload, uint8_t len) {
    last_type = type;
    memcpy(last_pl, payload, len);
    last_len = len;
    sent++;
    return 1;
}

static void ack(uint8_t c, uint8_t id, uint8_t cell, uint8_t st, int16_t v) {
    uint8_t p[LINK_ACK_LEN] = {c, id, cell, st};
    if (drop_acks && c != LINK_C_READ) {
        return;
    }
    link_put_s16(p + 4, v);
    td_on_ack(p);
}

static void bridge(void) {
    const tune_table_t *t;
    if (!sent || !bridge_on) {
        sent = 0;
        return;
    }
    sent = 0;
    t = tune_get(ecu, last_pl[0]);
    if (last_type == LINK_C_READ) {
        uint16_t len = tune_blob_len(t);
        uint8_t buf[LINK_TABLE_HDR + LINK_TABLE_VALS * 2u];
        for (uint16_t off = 0; off < len; off = (uint16_t)(off + LINK_TABLE_VALS)) {
            uint16_t n = (uint16_t)(len - off < LINK_TABLE_VALS ? len - off : LINK_TABLE_VALS);
            buf[0] = last_pl[0];
            buf[1] = (uint8_t)off;
            buf[2] = (uint8_t)(off >> 8);
            for (uint16_t i = 0; i < n; i++) {
                link_put_s16(buf + LINK_TABLE_HDR + i * 2u, ram[off + i]);
            }
            td_on_table(buf, (uint8_t)(LINK_TABLE_HDR + n * 2u));
        }
        ack(LINK_C_READ, last_pl[0], 0, TUNE_OK, 0);
    } else if (last_type == LINK_C_WRITE) {
        uint8_t cell = last_pl[1];
        int16_t v = link_get_s16(last_pl + 2);
        uint8_t st = last_len == LINK_WRITE_LEN ? tune_check_write(t, cell, ram[cell], v) : TUNE_E_TABLE;
        if (st == TUNE_OK) {
            st = tune_check_order(t, ram, cell, v);
        }
    if (st == TUNE_OK) {
            int16_t old = ram[cell];
            ram[cell] = v;
            writes++;
            verifies += (last_pl[4] & LINK_WF_VERIFY) != 0;
            if (fail_verify && (last_pl[4] & LINK_WF_VERIFY)) {
                ack(LINK_C_WRITE, last_pl[0], cell, TUNE_E_ECU, old);
                return;
            }
        }
        ack(LINK_C_WRITE, last_pl[0], cell, st, ram[cell]);
    } else if (last_type == LINK_C_BURN) {
        memcpy(flash, ram, sizeof ram);
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

static void setup(uint8_t e, uint8_t id, int16_t base) {
    for (unsigned i = 0; i < TUNE_BLOB_MAX; i++) {
        ram[i] = (int16_t)(base + i % 50);
    }
    memcpy(flash, ram, sizeof ram);
    ecu = e;
    bridge_on = 1;
    drop_acks = 0;
    fail_verify = 0;
    writes = 0;
    verifies = 0;
    burns = 0;
    td_load(e, id);
    run(2);
}

static void test_load(void) {
    setup(TUNE_ECU_MS1, TUNE_VE1, 40);
    CHECK(td.state == TD_READY);
    CHECK(td.blob[0] == ram[0] && td.blob[143] == ram[143] && td.blob[167] == ram[167]);
    CHECK(!td_dirty());
    setup(TUNE_ECU_MS2, TUNE_VE1, 40);
    CHECK(td.state == TD_READY);
    CHECK(td.blob[255] == ram[255] && td.blob[287] == ram[287]);
    setup(TUNE_ECU_MS2, TUNE_ADV1, -90);
    CHECK(td.state == TD_READY && td.blob[0] == -90);
}

static void test_edit_and_burn(void) {
    setup(TUNE_ECU_MS1, TUNE_VE1, 40);
    int16_t want = (int16_t)(flash[5] + 25);
    CHECK(td_set(5, want));
    CHECK(!td_set(6, 50));
    run(20);
    CHECK(ram[5] == want && writes == 3 && verifies == 1);
    CHECK(!td_busy() && td_dirty());
    td_burn();
    run(3);
    CHECK(burns == 1 && flash[5] == want && !td_dirty() && td.burned);
}

static void test_ms2_last_cell(void) {
    setup(TUNE_ECU_MS2, TUNE_VE1, 40);
    int16_t want = (int16_t)(ram[255] + 12);
    CHECK(td_set(255, want));
    run(20);
    CHECK(ram[255] == want && writes == 2 && verifies == 1 && !td_busy());
    CHECK(!td_set(256, 50));
}

static void test_ms2_spark(void) {
    setup(TUNE_ECU_MS2, TUNE_ADV1, -90);
    CHECK(td_set(0, 0));
    run(20);
    CHECK(ram[0] == 0 && writes == 3 && verifies == 1);
    CHECK(td_set(0, -200) && td.tgt_val == -100);
    CHECK(td_set(0, 600) && td.tgt_val == 500);
}

static void test_discard(void) {
    setup(TUNE_ECU_MS1, TUNE_VE1, 40);
    int16_t o5 = ram[5];
    int16_t o9 = ram[9];
    td_set(5, (int16_t)(o5 + 3));
    run(10);
    td_set(9, (int16_t)(o9 - 12));
    run(10);
    CHECK(ram[5] == o5 + 3 && ram[9] == o9 - 12);
    td_discard();
    run(20);
    CHECK(ram[5] == o5 && ram[9] == o9);
    CHECK(!td.discarding && !td_dirty() && !td_busy() && burns == 0);
}

static void test_retarget_needs_verify(void) {
    setup(TUNE_ECU_MS1, TUNE_VE1, 40);
    int16_t o = ram[5];
    td_set(5, (int16_t)(o + 25));
    td_tick();
    bridge();
    CHECK(ram[5] == o + 10 && verifies == 0);
    td_set(5, (int16_t)(o + 20));
    bridge();
    CHECK(ram[5] == o + 20 && verifies == 0 && td_busy());
    bridge();
    CHECK(verifies == 1 && !td_busy());
}

static void set_limits(int16_t soft, int16_t hard, int16_t launch, int16_t flat) {
    ram[0] = soft;
    ram[1] = hard;
    ram[2] = launch;
    ram[3] = flat;
    memcpy(flash, ram, sizeof ram);
    writes = 0;
    td_load(TUNE_ECU_MS1, TUNE_LIMITS);
    run(2);
}

static void test_order(void) {
    ecu = TUNE_ECU_MS1;
    bridge_on = 1;
    set_limits(60, 70, 40, 65);
    CHECK(!td_set(0, 71));
    run(10);
    CHECK(td.err == TUNE_E_ORDER && ram[0] == 60 && writes == 0 && !td_busy());
    CHECK(td_set(0, 70));
    run(10);
    CHECK(ram[0] == 70 && td.err == TUNE_OK);
}

static void test_discard_order(void) {
    ecu = TUNE_ECU_MS1;
    bridge_on = 1;
    set_limits(60, 70, 40, 65);
    td_set(0, 50);
    run(10);
    td_set(3, 55);
    run(10);
    td_set(1, 56);
    run(10);
    CHECK(ram[0] == 50 && ram[1] == 56 && ram[3] == 55 && td.err == TUNE_OK);
    td_discard();
    run(40);
    CHECK(ram[0] == 60 && ram[1] == 70 && ram[3] == 65);
    CHECK(!td.discarding && !td_dirty() && td.err == TUNE_OK);
}

static void test_limits(void) {
    setup(TUNE_ECU_MS1, TUNE_VE1, 40);
    td_set(0, 255);
    CHECK(td.tgt_val == 250);
    td_set(0, 0);
    CHECK(td.tgt_val == 10);
}

static void test_table_bounds(void) {
    setup(TUNE_ECU_MS1, TUNE_VE1, 40);
    td_load(TUNE_ECU_MS1, TUNE_VE1);
    int16_t before = td.blob[0];
    uint8_t p[LINK_TABLE_HDR + 2] = {TUNE_VE1, 0xff, 0xff, 0x34, 0x12};
    td_on_table(p, sizeof p);
    CHECK(td.blob[0] == before);
    uint8_t q[LINK_TABLE_HDR + 4] = {TUNE_VE1, 167, 0, 1, 0, 2, 0};
    td_on_table(q, sizeof q);
    CHECK(td.blob[167] == ram[167]);
    run(2);
}

static void test_burn_while_writing(void) {
    setup(TUNE_ECU_MS1, TUNE_VE1, 40);
    int16_t want = (int16_t)(ram[5] + 25);
    CHECK(td_set(5, want));
    td_burn();
    CHECK(td_busy());
    run(20);
    CHECK(ram[5] == want && burns == 1 && flash[5] == want && !td_busy() && td.burned);
}

static void test_lost_ack(void) {
    setup(TUNE_ECU_MS1, TUNE_VE1, 40);
    int16_t o = ram[5];
    drop_acks = 1;
    CHECK(td_set(5, (int16_t)(o + 1)));
    run(200);
    CHECK(ram[5] == o + 1 && td.err == TUNE_E_ECU && td.unsure && !td_busy());
    CHECK(!td_set(6, 50));
    td_burn();
    CHECK(!td_busy());
    drop_acks = 0;
    td_resync();
    run(2);
    CHECK(td.state == TD_READY && !td.unsure && td.blob[5] == o + 1 && td.orig[5] == o && td_dirty());
    td_discard();
    run(20);
    CHECK(ram[5] == o && !td_dirty() && burns == 0);
}

static void test_failed_readback(void) {
    setup(TUNE_ECU_MS1, TUNE_VE1, 40);
    int16_t o = ram[5];
    fail_verify = 1;
    CHECK(td_set(5, (int16_t)(o + 1)));
    run(10);
    CHECK(ram[5] == o + 1 && td.err == TUNE_E_ECU && td.unsure);
    CHECK(!td_set(6, 50));
    fail_verify = 0;
    td_resync();
    run(2);
    CHECK(!td.unsure && td.blob[5] == o + 1 && td.orig[5] == o && td_dirty());
}

static void test_lost_burn(void) {
    setup(TUNE_ECU_MS1, TUNE_VE1, 40);
    int16_t o = ram[5];
    td_set(5, (int16_t)(o + 1));
    run(10);
    drop_acks = 1;
    td_burn();
    run(200);
    CHECK(flash[5] == o + 1 && td.flash_unsure && td.err == TUNE_E_ECU);
    drop_acks = 0;
    td_resync();
    run(2);
    CHECK(td.flash_unsure && td_dirty());
    td_discard();
    run(30);
    CHECK(ram[5] == o && flash[5] == o && !td.flash_unsure && !td_dirty());
}

static void test_no_table(void) {
    td_load(0, TUNE_VE1);
    CHECK(td.state == TD_ERROR && td.err == TUNE_E_TABLE);
}

static void test_timeout(void) {
    setup(TUNE_ECU_MS1, TUNE_VE1, 40);
    bridge_on = 0;
    td_set(1, (int16_t)(ram[1] + 1));
    run(200);
    CHECK(td.err == TUNE_E_ECU && !td_busy() && ram[1] == flash[1]);
}

int main(void) {
    test_load();
    test_edit_and_burn();
    test_ms2_last_cell();
    test_ms2_spark();
    test_discard();
    test_retarget_needs_verify();
    test_limits();
    test_order();
    test_discard_order();
    test_table_bounds();
    test_burn_while_writing();
    test_lost_ack();
    test_failed_readback();
    test_lost_burn();
    test_no_table();
    test_timeout();
    if (fails) {
        fprintf(stderr, "%d failures\n", fails);
        return 1;
    }
    puts("ok");
    return 0;
}
