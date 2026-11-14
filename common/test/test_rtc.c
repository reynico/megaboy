#include <stdio.h>
#include <string.h>

#include "link.h"
#include "rtc.h"

static int fails;

#define CHECK(c)                                                  \
    do {                                                          \
        if (!(c)) {                                               \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); \
            fails++;                                              \
        }                                                         \
    } while (0)

static void test_days(void) {
    CHECK(rtc_days(26, 1) == 31);
    CHECK(rtc_days(26, 2) == 28);
    CHECK(rtc_days(28, 2) == 29);
    CHECK(rtc_days(0, 2) == 29);
    CHECK(rtc_days(26, 4) == 30);
    CHECK(rtc_days(26, 12) == 31);
    CHECK(rtc_days(26, 0) == 0);
    CHECK(rtc_days(26, 13) == 0);
}

static void test_valid(void) {
    rtc_time_t t = {26, 11, 13, 23, 59, 59};
    CHECK(rtc_valid(&t));
    t.dd = 31;
    CHECK(!rtc_valid(&t));
    t.dd = 0;
    CHECK(!rtc_valid(&t));
    t = (rtc_time_t){27, 2, 29, 0, 0, 0};
    CHECK(!rtc_valid(&t));
    t = (rtc_time_t){28, 2, 29, 0, 0, 0};
    CHECK(rtc_valid(&t));
    t = (rtc_time_t){26, 1, 1, 24, 0, 0};
    CHECK(!rtc_valid(&t));
    t = (rtc_time_t){100, 1, 1, 0, 0, 0};
    CHECK(!rtc_valid(&t));
}

static void test_bcd(void) {
    rtc_time_t a = {26, 11, 13, 21, 45, 7};
    rtc_time_t b;
    uint8_t r[RTC_REGS];
    rtc_encode(&a, r);
    CHECK(r[0] == 0x07 && r[1] == 0x45 && r[2] == 0x21 && r[4] == 0x13 && r[5] == 0x11 && r[6] == 0x26);
    CHECK(rtc_decode(r, &b));
    CHECK(memcmp(&a, &b, sizeof a) == 0);
}

static void test_decode_12h(void) {
    const uint8_t pm[RTC_REGS] = {0x00, 0x30, 0x40 | 0x20 | 0x09, 1, 0x01, 0x01, 0x26};
    const uint8_t noon[RTC_REGS] = {0x00, 0x00, 0x40 | 0x20 | 0x12, 1, 0x01, 0x01, 0x26};
    const uint8_t midnight[RTC_REGS] = {0x00, 0x00, 0x40 | 0x12, 1, 0x01, 0x01, 0x26};
    rtc_time_t t;
    CHECK(rtc_decode(pm, &t) && t.hh == 21);
    CHECK(rtc_decode(noon, &t) && t.hh == 12);
    CHECK(rtc_decode(midnight, &t) && t.hh == 0);
}

static void test_decode_century_bit(void) {
    const uint8_t r[RTC_REGS] = {0x00, 0x00, 0x00, 1, 0x01, 0x80 | 0x02, 0x26};
    rtc_time_t t;
    CHECK(rtc_decode(r, &t) && t.mo == 2);
}

static void test_decode_bad(void) {
    const uint8_t r[RTC_REGS] = {0x00, 0x00, 0x00, 1, 0x31, 0x02, 0x26};
    rtc_time_t t;
    CHECK(!rtc_decode(r, &t));
}

static void test_link_time(void) {
    rtc_time_t a = {26, 11, 13, 21, 45, 7};
    rtc_time_t b;
    uint8_t pl[LINK_TIME_LEN];
    CHECK(link_put_time(pl, &a) == LINK_TIME_SET_LEN);
    link_get_time(pl, &b);
    CHECK(memcmp(&a, &b, sizeof a) == 0);
}

int main(void) {
    test_days();
    test_valid();
    test_bcd();
    test_decode_12h();
    test_decode_century_bit();
    test_decode_bad();
    test_link_time();
    if (fails) {
        fprintf(stderr, "%d failures\n", fails);
        return 1;
    }
    puts("ok");
    return 0;
}
