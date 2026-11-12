#include "sim.h"

#define CYCLE 90u

static uint8_t t;
static int16_t boost;
static uint16_t lfsr;
static uint16_t ticks;
static int16_t heat;
static uint8_t heating;

static int8_t noise(void) {
    uint16_t bit = (uint16_t)(((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1u);
    lfsr = (uint16_t)((lfsr >> 1) | (bit << 15));
    return (int8_t)((int8_t)(lfsr & 0x0fu) - 7);
}

void sim_init(void) {
    t = 22;
    boost = 142;
    lfsr = 0xace1u;
    ticks = 0;
    heat = 0;
    heating = 0;
}

void sim_overheat(void) {
    heating = 1;
}

void sim_step(rt_t *rt) {
    int32_t rpm;
    int16_t tps;
    int16_t target;
    int16_t pos;
    int8_t n = noise();
    uint8_t p = t;

    if (p < 26u) {
        rpm = 3800 + (int32_t)3400 * p / 26;
        tps = 1000;
    } else if (p < 29u) {
        rpm = 7200 - (int32_t)2000 * (p - 26u) / 3;
        tps = 0;
    } else if (p < 56u) {
        rpm = 5200 + (int32_t)2000 * (p - 29u) / 27;
        tps = 1000;
    } else {
        int32_t r = (int32_t)(90u - p);
        rpm = 2900 + (int32_t)4300 * r * r / (34 * 34);
        tps = (p - 56u) < 10u ? 0 : 140;
    }

    if (tps > 500) {
        int32_t b = (rpm - 3200) * 185 / 2400;
        target = (int16_t)(b < -20 ? -20 : (b > 185 ? 185 : b));
    } else {
        target = tps == 0 ? -100 : -60;
    }
    boost = (int16_t)(boost + (int16_t)(((int32_t)target - boost) * 3 / 10));
    pos = boost > 0 ? boost : 0;

    rt->rpm = (uint16_t)(rpm + n * 4);
    rt->tps_x10 = tps;
    rt->boost_psi_x10 = boost;
    rt->map_x10 = (int16_t)(1013 + (int32_t)boost * 6895 / 1000);
    rt->afr_x10 = (int16_t)(tps > 500 ? 116 + n / 3 : (tps == 0 ? 162 + n / 2 : 147 + n / 4));
    rt->adv_x10 = (int16_t)(tps > 500 ? 300 - (int32_t)pos * 9 / 10 : 340 + n);
    if (heating) {
        heat = (int16_t)(heat + 3);
        if (heat >= 250) {
            heating = 0;
        }
    } else if (heat > 0) {
        heat = (int16_t)(heat - 1);
    }
    rt->clt_c_x10 = (int16_t)(880 + (int16_t)((ticks / 20u) % 30u) + heat);
    rt->iat_c_x10 = (int16_t)(380 + (int32_t)pos * 4 / 10);
    rt->batt_x10 = (int16_t)(138 + n / 7);

    t = (uint8_t)((t + 1u) % CYCLE);
    ticks++;
}
