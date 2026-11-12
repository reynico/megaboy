#include "ecu.h"

static int32_t div_round(int32_t n, int32_t d) {
    return n >= 0 ? (n + d / 2) / d : (n - d / 2) / d;
}

int16_t ecu_boost_psi_x10(int16_t map_x10, int16_t baro_x10) {
    return (int16_t)div_round(((int32_t)map_x10 - baro_x10) * 145038, 1000000);
}

int16_t ecu_f_to_c_x10(int32_t f_x10) {
    return (int16_t)div_round((f_x10 - 320) * 5, 9);
}
