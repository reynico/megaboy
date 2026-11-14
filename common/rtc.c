#include "rtc.h"

static const uint8_t month_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

uint8_t rtc_days(uint8_t yy, uint8_t mo) {
    if (mo < 1u || mo > 12u) {
        return 0;
    }
    if (mo == 2u && (yy & 3u) == 0u) {
        return 29;
    }
    return month_days[mo - 1u];
}

uint8_t rtc_valid(const rtc_time_t *t) {
    return t->yy <= 99u && t->dd >= 1u && t->dd <= rtc_days(t->yy, t->mo) && t->hh <= 23u && t->mi <= 59u &&
           t->ss <= 59u;
}

static uint8_t to_bcd(uint8_t v) {
    return (uint8_t)((v / 10u) << 4 | v % 10u);
}

static uint8_t from_bcd(uint8_t v) {
    return (uint8_t)((v >> 4) * 10u + (v & 0x0fu));
}

void rtc_encode(const rtc_time_t *t, uint8_t *regs) {
    regs[0] = to_bcd(t->ss);
    regs[1] = to_bcd(t->mi);
    regs[2] = to_bcd(t->hh);
    regs[3] = 1;
    regs[4] = to_bcd(t->dd);
    regs[5] = to_bcd(t->mo);
    regs[6] = to_bcd(t->yy);
}

uint8_t rtc_decode(const uint8_t *regs, rtc_time_t *t) {
    uint8_t h = regs[2];
    t->ss = from_bcd(regs[0] & 0x7fu);
    t->mi = from_bcd(regs[1] & 0x7fu);
    if (h & 0x40u) {
        t->hh = (uint8_t)(from_bcd(h & 0x1fu) % 12u + ((h & 0x20u) ? 12u : 0u));
    } else {
        t->hh = from_bcd(h & 0x3fu);
    }
    t->dd = from_bcd(regs[4] & 0x3fu);
    t->mo = from_bcd(regs[5] & 0x1fu);
    t->yy = from_bcd(regs[6]);
    return rtc_valid(t);
}
