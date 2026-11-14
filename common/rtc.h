#pragma once

#include <stdint.h>

#define RTC_REGS 7u

typedef struct {
    uint8_t yy;
    uint8_t mo;
    uint8_t dd;
    uint8_t hh;
    uint8_t mi;
    uint8_t ss;
} rtc_time_t;

uint8_t rtc_days(uint8_t yy, uint8_t mo);
uint8_t rtc_valid(const rtc_time_t *t);
void rtc_encode(const rtc_time_t *t, uint8_t *regs);
uint8_t rtc_decode(const uint8_t *regs, rtc_time_t *t);
