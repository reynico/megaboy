#pragma once

#include <stdint.h>

typedef struct {
    uint16_t secs;
    uint16_t rpm;
    uint8_t engine;
    int16_t adv_x10;
    int16_t baro_x10;
    int16_t map_x10;
    int16_t iat_c_x10;
    int16_t clt_c_x10;
    int16_t tps_x10;
    int16_t batt_x10;
    int16_t afr_x10;
    int16_t boost_psi_x10;
} ecu_rt_t;

int16_t ecu_boost_psi_x10(int16_t map_x10, int16_t baro_x10);
int16_t ecu_f_to_c_x10(int32_t f_x10);
uint8_t ecu_restarted(uint16_t prev, uint16_t now, uint32_t span, uint32_t elapsed_ms);
