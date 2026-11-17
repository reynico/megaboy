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

static const ms1_cfg_t cfg4250 = {MS1_MAP_250, 4, false};
static const ms1_cfg_t cfg4115 = {MS1_MAP_115, 4, false};

static void test_config(void) {
    uint8_t pg[MS1_PAGE_LEN] = {0};
    ms1_cfg_t c;
    pg[MS1_CONFIG1] = 0x51;
    CHECK(!ms1_parse_config(pg, MS1_PAGE_LEN - 1, &c));
    CHECK(ms1_parse_config(pg, sizeof pg, &c));
    CHECK(c.cyl == 6 && !c.two_stroke && c.map == MS1_MAP_250);
    pg[MS1_CONFIG1] = 0xf6;
    CHECK(ms1_parse_config(pg, sizeof pg, &c));
    CHECK(c.cyl == 16 && c.two_stroke && c.map == MS1_MAP_300);
    CHECK(ms1_map_kpa(c.map) == 300);
}

static void test_linear_map(void) {
    uint8_t d[MS1_RT_LEN] = {0};
    ms1_cfg_t c3 = {MS1_MAP_300, 4, false};
    ms1_cfg_t c4 = {MS1_MAP_400, 4, false};
    ecu_rt_t rt;
    d[4] = 100;
    CHECK(ms1_decode_rt(d, sizeof d, &c3, &rt));
    CHECK(rt.map_x10 == 1232);
    CHECK(ms1_decode_rt(d, sizeof d, &c4, &rt));
    CHECK(rt.map_x10 == 1655);
}

static void test_rpm(void) {
    uint8_t d[MS1_RT_LEN] = {0};
    ms1_cfg_t c6 = {MS1_MAP_250, 6, false};
    ms1_cfg_t c1t = {MS1_MAP_250, 1, true};
    ms1_cfg_t c0 = {MS1_MAP_250, 0, false};
    CHECK(ms1_rpm(d, &cfg4250) == 0);
    d[22] = 19556 >> 8;
    d[23] = 19556 & 0xff;
    CHECK(ms1_rpm(d, &cfg4250) == 1534);
    CHECK(ms1_rpm(d, &c6) == 1023);
    CHECK(ms1_rpm(d, &c0) == 0);
    d[22] = 0;
    d[23] = 1;
    CHECK(ms1_rpm(d, &c1t) == UINT16_MAX);
    d[23] = 0;
    d[37] = 0xff;
    CHECK(ms1_rpm(d, &cfg4250) == 0);
}

static void test_decode(void) {
    uint8_t d[MS1_RT_LEN] = {0};
    d[0] = 77;
    d[2] = 1;
    d[3] = 100;
    d[4] = 150;
    d[5] = 100;
    d[6] = 100;
    d[7] = 128;
    d[8] = 119;
    d[9] = 128;
    d[22] = 8571 >> 8;
    d[23] = 8571 & 0xff;
    d[24] = 57;
    ecu_rt_t rt;
    CHECK(!ms1_decode_rt(d, MS1_RT_LEN - 1, &cfg4250, &rt));
    CHECK(ms1_decode_rt(d, sizeof d, &cfg4250, &rt));
    CHECK(rt.rpm == 3500);
    CHECK(rt.secs == 77);
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

static void test_restart(void) {
    CHECK(!ecu_restarted(10, 10, 256));
    CHECK(!ecu_restarted(10, 11, 256));
    CHECK(!ecu_restarted(255, 0, 256));
    CHECK(!ecu_restarted(254, 3, 256));
    CHECK(ecu_restarted(100, 2, 256));
    CHECK(ecu_restarted(10, 9, 256));
    CHECK(!ecu_restarted(65535, 0, 65536));
    CHECK(ecu_restarted(3000, 1, 65536));
    CHECK(!ecu_restarted(3000, 3001, 65536));
}

static void test_cold(void) {
    uint8_t d[MS1_RT_LEN] = {0};
    d[6] = 254;
    ecu_rt_t rt;
    CHECK(ms1_decode_rt(d, sizeof d, &cfg4115, &rt));
    CHECK(rt.clt_c_x10 == -400);
}

int main(void) {
    test_restart();
    test_sig();
    test_config();
    test_linear_map();
    test_rpm();
    test_decode();
    test_cold();
    if (fails) {
        fprintf(stderr, "%d failures\n", fails);
        return 1;
    }
    puts("ok");
    return 0;
}
