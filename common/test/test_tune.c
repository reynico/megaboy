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

static int at(const tune_table_t *t, uint16_t i, uint8_t page, uint16_t off, uint8_t type) {
    uint8_t p;
    uint16_t o;
    uint8_t ty;
    tune_loc(t, i, &p, &o, &ty);
    return p == page && o == off && ty == type;
}

static void test_get(void) {
    CHECK(tune_get(TUNE_ECU_MS1, TUNE_COUNT) == 0);
    CHECK(tune_get(0, TUNE_VE1) == 0);
    CHECK(tune_get(3, TUNE_VE1) == 0);
    CHECK(tune_get(TUNE_ECU_MS1, TUNE_VE1) != tune_get(TUNE_ECU_MS2, TUNE_VE1));
}

static void test_ms1_layout(void) {
    const tune_table_t *ve = tune_get(TUNE_ECU_MS1, TUNE_VE1);
    const tune_table_t *adv = tune_get(TUNE_ECU_MS1, TUNE_ADV1);
    const tune_table_t *afr = tune_get(TUNE_ECU_MS1, TUNE_AFR1);
    const tune_table_t *lim = tune_get(TUNE_ECU_MS1, TUNE_LIMITS);
    const tune_table_t *bct = tune_get(TUNE_ECU_MS1, TUNE_BCT);
    const tune_table_t *bcd = tune_get(TUNE_ECU_MS1, TUNE_BCD);
    CHECK(tune_blob_len(ve) == 168 && tune_blob_len(afr) == 80 && tune_blob_len(lim) == 4);
    CHECK(at(ve, 0, 1, 0, TUNE_U08) && at(ve, 143, 1, 143, TUNE_U08));
    CHECK(at(ve, 144, 1, 158, TUNE_U08) && at(ve, 155, 1, 169, TUNE_U08));
    CHECK(at(ve, 156, 1, 170, TUNE_U08) && at(ve, 167, 1, 181, TUNE_U08));
    CHECK(at(adv, 144, 3, 144, TUNE_U08) && at(adv, 167, 3, 167, TUNE_U08));
    CHECK(at(afr, 64, 6, 64, TUNE_U08) && at(afr, 79, 6, 79, TUNE_U08));
    CHECK(at(lim, 0, 0, 2, TUNE_U08) && at(lim, 1, 0, 6, TUNE_U08));
    CHECK(at(lim, 2, 0, 20, TUNE_U08) && at(lim, 3, 0, 128, TUNE_U08));
    CHECK(at(bct, 35, 7, 35, TUNE_U08) && at(bct, 36, 7, 36, TUNE_U08) && at(bct, 42, 7, 42, TUNE_U08));
    CHECK(at(bcd, 0, 7, 48, TUNE_U08) && at(bcd, 35, 7, 83, TUNE_U08));
    CHECK(at(bcd, 36, 7, 84, TUNE_U08) && at(bcd, 47, 7, 95, TUNE_U08));
    for (uint8_t id = 0; id < TUNE_COUNT; id++) {
        const tune_table_t *t = tune_get(TUNE_ECU_MS1, id);
        for (uint16_t i = 0; i < tune_blob_len(t); i++) {
            uint8_t p;
            uint16_t o;
            uint8_t ty;
            tune_loc(t, i, &p, &o, &ty);
            CHECK(o < 189 && ty == TUNE_U08);
        }
    }
}

static void test_ms2_layout(void) {
    const tune_table_t *ve = tune_get(TUNE_ECU_MS2, TUNE_VE1);
    const tune_table_t *adv = tune_get(TUNE_ECU_MS2, TUNE_ADV1);
    const tune_table_t *afr = tune_get(TUNE_ECU_MS2, TUNE_AFR1);
    const tune_table_t *lim = tune_get(TUNE_ECU_MS2, TUNE_LIMITS);
    const tune_table_t *bct = tune_get(TUNE_ECU_MS2, TUNE_BCT);
    const tune_table_t *bcd = tune_get(TUNE_ECU_MS2, TUNE_BCD);
    CHECK(tune_cells(ve) == 256 && tune_blob_len(ve) == 288 && tune_blob_len(ve) <= TUNE_BLOB_MAX);
    CHECK(at(ve, 0, 0x09, 0, TUNE_U08) && at(ve, 255, 0x09, 255, TUNE_U08));
    CHECK(at(ve, 256, 0x09, 768, TUNE_U16) && at(ve, 271, 0x09, 798, TUNE_U16));
    CHECK(at(ve, 272, 0x09, 864, TUNE_S16) && at(ve, 287, 0x09, 894, TUNE_S16));
    CHECK(at(adv, 0, 0x0a, 0, TUNE_S16) && at(adv, 143, 0x0a, 286, TUNE_S16));
    CHECK(at(adv, 144, 0x0a, 576, TUNE_U16) && at(adv, 155, 0x0a, 598, TUNE_U16));
    CHECK(at(adv, 156, 0x0a, 624, TUNE_S16) && at(adv, 167, 0x0a, 646, TUNE_S16));
    CHECK(at(afr, 0, 0x04, 48, TUNE_U08) && at(afr, 143, 0x04, 191, TUNE_U08));
    CHECK(at(afr, 144, 0x04, 374, TUNE_U16) && at(afr, 156, 0x04, 422, TUNE_S16));
    CHECK(at(lim, 0, 0x04, 656, TUNE_S16) && at(lim, 1, 0x0a, 678, TUNE_S16));
    CHECK(at(lim, 2, 0x0a, 685, TUNE_S16) && at(lim, 3, 0x0a, 691, TUNE_S16));
    CHECK(at(bct, 0, 0x05, 76, TUNE_S16) && at(bct, 63, 0x05, 202, TUNE_S16));
    CHECK(at(bct, 64, 0x05, 220, TUNE_U16) && at(bct, 72, 0x05, 204, TUNE_S16));
    CHECK(at(bcd, 0, 0x05, 236, TUNE_U08) && at(bcd, 63, 0x05, 299, TUNE_U08));
    CHECK(at(bcd, 64, 0x05, 316, TUNE_U16) && at(bcd, 79, 0x05, 314, TUNE_S16));
    for (uint8_t id = 0; id < TUNE_COUNT; id++) {
        const tune_table_t *t = tune_get(TUNE_ECU_MS2, id);
        CHECK(tune_blob_len(t) <= TUNE_BLOB_MAX);
        for (uint16_t i = 0; i < tune_blob_len(t); i++) {
            uint8_t p;
            uint16_t o;
            uint8_t ty;
            tune_loc(t, i, &p, &o, &ty);
            CHECK(o + tune_size(ty) <= 1024);
        }
    }
}

static void test_codec(void) {
    uint8_t b[2];
    tune_encode(TUNE_S16, -100, b);
    CHECK(b[0] == 0xff && b[1] == 0x9c && tune_decode(TUNE_S16, b) == -100);
    tune_encode(TUNE_U16, 7500, b);
    CHECK(b[0] == 0x1d && b[1] == 0x4c && tune_decode(TUNE_U16, b) == 7500);
    tune_encode(TUNE_U08, 200, b);
    CHECK(b[0] == 200 && tune_decode(TUNE_U08, b) == 200);
    CHECK(tune_axis(TUNE_A_X100, 84) == 8400);
    CHECK(tune_axis(TUNE_A_DIV10, 1005) == 100);
    CHECK(tune_axis(TUNE_A_MS1_TPS, 255) == 100);
    CHECK(tune_axis(TUNE_A_RAW, 6500) == 6500);
}

static void test_checks(void) {
    const tune_table_t *ve = tune_get(TUNE_ECU_MS1, TUNE_VE1);
    const tune_table_t *adv = tune_get(TUNE_ECU_MS1, TUNE_ADV1);
    const tune_table_t *afr = tune_get(TUNE_ECU_MS1, TUNE_AFR1);
    const tune_table_t *lim = tune_get(TUNE_ECU_MS1, TUNE_LIMITS);
    const tune_table_t *bcd = tune_get(TUNE_ECU_MS1, TUNE_BCD);
    const tune_table_t *ve2 = tune_get(TUNE_ECU_MS2, TUNE_VE1);
    const tune_table_t *adv2 = tune_get(TUNE_ECU_MS2, TUNE_ADV1);
    const tune_table_t *lim2 = tune_get(TUNE_ECU_MS2, TUNE_LIMITS);
    CHECK(tune_check_write(0, 0, 50, 51) == TUNE_E_TABLE);
    CHECK(tune_check_write(ve, 144, 50, 51) == TUNE_E_CELL);
    CHECK(tune_check_write(afr, 64, 150, 151) == TUNE_E_CELL);
    CHECK(tune_check_write(ve, 0, 50, 60) == TUNE_OK);
    CHECK(tune_check_write(ve, 0, 50, 61) == TUNE_E_DELTA);
    CHECK(tune_check_write(ve, 0, 15, 9) == TUNE_E_RANGE);
    CHECK(tune_check_write(adv, 0, 150, 157) == TUNE_E_RANGE);
    CHECK(tune_check_write(lim, 4, 50, 51) == TUNE_E_CELL);
    CHECK(tune_check_write(lim, 1, 85, 91) == TUNE_E_RANGE);
    CHECK(tune_check_write(bcd, 0, 240, 243) == TUNE_E_RANGE);
    CHECK(tune_check_write(ve2, 255, 50, 55) == TUNE_OK);
    CHECK(tune_check_write(ve2, 256, 50, 55) == TUNE_E_CELL);
    CHECK(tune_check_write(adv2, 0, -90, -60) == TUNE_OK);
    CHECK(tune_check_write(adv2, 0, -90, -59) == TUNE_E_DELTA);
    CHECK(tune_check_write(adv2, 0, -95, -101) == TUNE_E_RANGE);
    CHECK(tune_check_write(lim2, 0, 6500, 7500) == TUNE_OK);
    CHECK(tune_check_write(lim2, 0, 6500, 7600) == TUNE_E_DELTA);
    CHECK(tune_check_write(lim2, 3, -30000, 9000) == TUNE_E_DELTA);
}

int main(void) {
    test_get();
    test_ms1_layout();
    test_ms2_layout();
    test_codec();
    test_checks();
    if (fails) {
        fprintf(stderr, "%d failures\n", fails);
        return 1;
    }
    puts("ok");
    return 0;
}
