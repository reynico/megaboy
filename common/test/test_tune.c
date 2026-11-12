#include <stdio.h>

#include "tune.h"

static int fails;

#define CHECK(c)                                                  \
    do {                                                          \
        if (!(c)) {                                               \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); \
            fails++;                                              \
        }                                                         \
    } while (0)

static void test_layout(void) {
    const tune_table_t *ve = tune_get(TUNE_VE1);
    const tune_table_t *adv = tune_get(TUNE_ADV1);
    const tune_table_t *afr = tune_get(TUNE_AFR1);
    CHECK(tune_get(TUNE_COUNT) == 0);
    CHECK(ve->page_cmd == 1 && adv->page_cmd == 3 && afr->page_cmd == 6);
    CHECK(tune_blob_len(ve) == 168);
    CHECK(tune_blob_len(afr) == 80);
    CHECK(tune_blob_offset(ve, 0) == 0);
    CHECK(tune_blob_offset(ve, 143) == 143);
    CHECK(tune_blob_offset(ve, 144) == 158);
    CHECK(tune_blob_offset(ve, 155) == 169);
    CHECK(tune_blob_offset(ve, 156) == 170);
    CHECK(tune_blob_offset(ve, 167) == 181);
    CHECK(tune_blob_offset(adv, 144) == 144);
    CHECK(tune_blob_offset(adv, 167) == 167);
    CHECK(tune_blob_offset(afr, 64) == 64);
    CHECK(tune_blob_offset(afr, 79) == 79);
    for (uint8_t id = 0; id < TUNE_COUNT; id++) {
        const tune_table_t *t = tune_get(id);
        for (uint8_t i = 0; i < tune_blob_len(t); i++) {
            CHECK(tune_blob_offset(t, i) < 189);
        }
    }
}

static void test_checks(void) {
    CHECK(tune_check_write(TUNE_COUNT, 0, 50, 51) == TUNE_E_TABLE);
    CHECK(tune_check_write(TUNE_VE1, 144, 50, 51) == TUNE_E_CELL);
    CHECK(tune_check_write(TUNE_AFR1, 64, 150, 151) == TUNE_E_CELL);
    CHECK(tune_check_write(TUNE_VE1, 0, 50, 51) == TUNE_OK);
    CHECK(tune_check_write(TUNE_VE1, 0, 50, 60) == TUNE_OK);
    CHECK(tune_check_write(TUNE_VE1, 0, 50, 61) == TUNE_E_DELTA);
    CHECK(tune_check_write(TUNE_VE1, 0, 60, 50) == TUNE_OK);
    CHECK(tune_check_write(TUNE_VE1, 0, 15, 9) == TUNE_E_RANGE);
    CHECK(tune_check_write(TUNE_ADV1, 0, 150, 157) == TUNE_E_RANGE);
    CHECK(tune_check_write(TUNE_ADV1, 0, 160, 156) == TUNE_OK);
    CHECK(tune_check_write(TUNE_AFR1, 63, 30, 25) == TUNE_E_RANGE);
}

int main(void) {
    test_layout();
    test_checks();
    if (fails) {
        fprintf(stderr, "%d failures\n", fails);
        return 1;
    }
    puts("ok");
    return 0;
}
