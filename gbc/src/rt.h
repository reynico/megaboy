#pragma once

#include <stdint.h>

typedef struct {
    uint16_t rpm;
    int16_t adv_x10;
    int16_t map_x10;
    int16_t iat_c_x10;
    int16_t clt_c_x10;
    int16_t tps_x10;
    int16_t batt_x10;
    int16_t afr_x10;
    int16_t boost_psi_x10;
} rt_t;
