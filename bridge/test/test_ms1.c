#include <stdio.h>
#include <string.h>

#include "ms1.h"

static int fails;

#define CHECK(c)                                                  \
    do {                                                          \
        if (!(c)) {                                               \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); \
            fails++;                                              \
        }                                                         \
    } while (0)

static void test_sig(void) {
    const char *q = "MSnS-extra format 029q *********";
    const char *y = "MS1/Extra format 029y3 *********";
    const char *m = "MS2Extra comms332m2\0\0\0\0\0\0\0\0\0\0\0\0\0";
    CHECK(ms1_check_sig((const uint8_t *)q, 32));
    CHECK(ms1_check_sig((const uint8_t *)y, 32));
    CHECK(!ms1_check_sig((const uint8_t *)q, 31));
    CHECK(!ms1_check_sig((const uint8_t *)m, 32));
}

static void test_decode(void) {
    uint8_t d[MS1_RT_LEN] = {0};
    d[2] = 1;
    d[3] = 100;
    d[4] = 150;
    d[5] = 100;
    d[6] = 100;
    d[7] = 128;
    d[8] = 119;
    d[9] = 128;
    d[13] = 35;
    d[24] = 57;
    ecu_rt_t rt;
    CHECK(!ms1_decode_rt(d, MS1_RT_LEN - 1, MS1_MAP_4250, &rt));
    CHECK(ms1_decode_rt(d, sizeof d, MS1_MAP_4250, &rt));
    CHECK(rt.rpm == 3500);
    CHECK(rt.engine == 1);
    CHECK(rt.adv_x10 == 101);
    CHECK(rt.baro_x10 == 1060);
    CHECK(rt.clt_c_x10 == 372);
    CHECK(rt.iat_c_x10 == 372);
    CHECK(rt.tps_x10 == 500);
    CHECK(rt.batt_x10 == 140);
    CHECK(rt.afr_x10 == 150);
    CHECK(rt.boost_psi_x10 == ecu_boost_psi_x10(rt.map_x10, rt.baro_x10));
    CHECK(rt.map_x10 > rt.baro_x10);
}

static void test_cold(void) {
    uint8_t d[MS1_RT_LEN] = {0};
    d[6] = 254;
    ecu_rt_t rt;
    CHECK(ms1_decode_rt(d, sizeof d, MS1_MAP_4115, &rt));
    CHECK(rt.clt_c_x10 == -400);
}

int main(void) {
    test_sig();
    test_decode();
    test_cold();
    if (fails) {
        fprintf(stderr, "%d failures\n", fails);
        return 1;
    }
    puts("ok");
    return 0;
}
