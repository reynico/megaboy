#include "ms1.h"

#include <string.h>

#include "ms1_tables.h"

static const char *const sigs[] = {"MSnS-extra format ", "MS1/Extra format "};

bool ms1_check_sig(const uint8_t *data, size_t len) {
    if (len < MS1_SIG_LEN) {
        return false;
    }
    for (size_t i = 0; i < sizeof sigs / sizeof sigs[0]; i++) {
        if (memcmp(data, sigs[i], strlen(sigs[i])) == 0) {
            return true;
        }
    }
    return false;
}

static int16_t temp_c_x10(uint16_t f_plus_40) {
    return ecu_f_to_c_x10(((int32_t)f_plus_40 - 40) * 10);
}

bool ms1_decode_rt(const uint8_t *data, size_t len, ms1_map_t map, ecu_rt_t *rt) {
    if (len < MS1_RT_LEN) {
        return false;
    }
    const uint16_t *kpa = map == MS1_MAP_4115 ? ms1_kpa4115 : ms1_kpa4250;
    rt->rpm = (uint16_t)(data[13] * 100u);
    rt->engine = data[2];
    rt->adv_x10 = (int16_t)((data[24] * 352 + 50) / 100 - 100);
    rt->baro_x10 = (int16_t)(kpa[data[3]] * 10);
    rt->map_x10 = (int16_t)(kpa[data[4]] * 10);
    rt->iat_c_x10 = temp_c_x10(ms1_mat[data[5]]);
    rt->clt_c_x10 = temp_c_x10(ms1_therm[data[6]]);
    rt->tps_x10 = (int16_t)(ms1_tps[data[7]] * 10);
    rt->batt_x10 = (int16_t)((data[8] * 300 + 127) / 255);
    rt->afr_x10 = (int16_t)(100 + (data[9] * 100 + 127) / 255);
    rt->boost_psi_x10 = ecu_boost_psi_x10(rt->map_x10, rt->baro_x10);
    return true;
}
