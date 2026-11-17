#include <stdio.h>
#include <string.h>

#include "ms2.h"

static int fails;

#define CHECK(c)                                                  \
    do {                                                          \
        if (!(c)) {                                               \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); \
            fails++;                                              \
        }                                                         \
    } while (0)

static void test_crc(void) {
    CHECK(ms2_crc32((const uint8_t *)"123456789", 9) == 0xcbf43926u);
}

static void test_requests(void) {
    uint8_t out[MS2_FRAME_MAX];
    const uint8_t sig[] = {0x00, 0x07, 0x72, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x14, 0x7e, 0x11, 0x5d, 0xd0};
    const uint8_t rt[] = {0x00, 0x07, 0x72, 0x00, 0x07, 0x00, 0x00, 0x00, 0x1e, 0xae, 0xb4, 0xff, 0x0f};
    CHECK(ms2_req_read(out, MS2_TABLE_SIG, 0, 20) == sizeof sig);
    CHECK(memcmp(out, sig, sizeof sig) == 0);
    CHECK(ms2_req_read(out, MS2_TABLE_RT, 0, MS2_RT_LEN) == sizeof rt);
    CHECK(memcmp(out, rt, sizeof rt) == 0);
}

static size_t sample_frame(uint8_t *out) {
    uint8_t p[1 + MS2_RT_LEN] = {0};
    uint8_t *d = p + 1;
    const struct {
        int off;
        int v;
    } f[] = {{6, 3500}, {8, 215}, {16, 1013}, {18, 1700}, {20, 950},
             {22, 1940}, {24, 455}, {26, 138}, {28, 128}};
    for (size_t i = 0; i < sizeof f / sizeof f[0]; i++) {
        d[f[i].off] = (uint8_t)(f[i].v >> 8);
        d[f[i].off + 1] = (uint8_t)f[i].v;
    }
    return ms2_frame(out, p, sizeof p);
}

static void test_parse_decode(void) {
    uint8_t fr[MS2_FRAME_MAX];
    size_t n = sample_frame(fr);
    ms2_parser_t p;
    ms2_parser_reset(&p);
    const uint8_t *pl = NULL;
    size_t len = 0;
    ms2_pstat_t st = MS2_P_NEED;
    for (size_t i = 0; i < n; i++) {
        st = ms2_parser_feed(&p, fr[i], &pl, &len);
        if (i + 1 < n) {
            CHECK(st == MS2_P_NEED);
        }
    }
    CHECK(st == MS2_P_OK);
    CHECK(ms2_resp_ok(pl, len));
    ecu_rt_t rt;
    CHECK(ms2_decode_rt(pl + 1, len - 1, &rt));
    CHECK(rt.rpm == 3500);
    CHECK(rt.adv_x10 == 215);
    CHECK(rt.map_x10 == 1700);
    CHECK(rt.boost_psi_x10 == 100);
    CHECK(rt.clt_c_x10 == 900);
    CHECK(rt.iat_c_x10 == 350);
    CHECK(rt.tps_x10 == 455);
    CHECK(rt.batt_x10 == 138);
    CHECK(rt.afr_x10 == 128);
}

static void test_write_burn(void) {
    uint8_t out[MS2_FRAME_MAX];
    const uint8_t v2[] = {0xff, 0x9c};
    const uint8_t v1[] = {0x41};
    const uint8_t w2[] = {0x00, 0x09, 'w', 0x00, 0x0a, 0x00, 0x10, 0x00, 0x02, 0xff, 0x9c, 0xdf, 0x42, 0x84, 0x0d};
    const uint8_t w1[] = {0x00, 0x08, 'w', 0x00, 0x09, 0x00, 0x05, 0x00, 0x01, 0x41, 0xcb, 0xa6, 0xa6, 0x5d};
    const uint8_t b[] = {0x00, 0x03, 'b', 0x00, 0x05, 0xc4, 0x78, 0x32, 0xd3};
    CHECK(ms2_req_write(out, 0x0a, 16, v2, 2) == sizeof w2 && memcmp(out, w2, sizeof w2) == 0);
    CHECK(ms2_req_write(out, 0x09, 5, v1, 1) == sizeof w1 && memcmp(out, w1, sizeof w1) == 0);
    CHECK(ms2_req_write(out, 0x09, 5, v1, 0) == 0);
    CHECK(ms2_req_write(out, 0x09, 5, v1, 3) == 0);
    CHECK(ms2_req_burn(out, 0x05) == sizeof b && memcmp(out, b, sizeof b) == 0);
}

static void test_bad_crc(void) {
    uint8_t fr[MS2_FRAME_MAX];
    size_t n = sample_frame(fr);
    fr[10] ^= 0xff;
    ms2_parser_t p;
    ms2_parser_reset(&p);
    const uint8_t *pl;
    size_t len;
    ms2_pstat_t st = MS2_P_NEED;
    for (size_t i = 0; i < n; i++) {
        st = ms2_parser_feed(&p, fr[i], &pl, &len);
    }
    CHECK(st == MS2_P_ERR_CRC);
}

static void test_bad_len(void) {
    ms2_parser_t p;
    ms2_parser_reset(&p);
    const uint8_t *pl;
    size_t len;
    CHECK(ms2_parser_feed(&p, 0xff, &pl, &len) == MS2_P_NEED);
    CHECK(ms2_parser_feed(&p, 0xff, &pl, &len) == MS2_P_ERR_LEN);
    CHECK(p.n == 0);
}

static void test_vacuum(void) {
    uint8_t d[MS2_RT_LEN] = {0};
    d[0] = 0x12;
    d[1] = 0x34;
    d[16] = 1013 >> 8;
    d[17] = 1013 & 0xff;
    d[18] = 350 >> 8;
    d[19] = 350 & 0xff;
    ecu_rt_t rt;
    CHECK(ms2_decode_rt(d, sizeof d, &rt));
    CHECK(rt.secs == 0x1234);
    CHECK(rt.boost_psi_x10 == -96);
    CHECK(rt.clt_c_x10 == -178);
}

int main(void) {
    test_crc();
    test_requests();
    test_parse_decode();
    test_bad_crc();
    test_write_burn();
    test_bad_len();
    test_vacuum();
    if (fails) {
        fprintf(stderr, "%d failures\n", fails);
        return 1;
    }
    puts("ok");
    return 0;
}
