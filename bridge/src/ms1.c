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

bool ms1_parse_config(const uint8_t *page, size_t len, ms1_cfg_t *cfg) {
    if (len < MS1_PAGE_LEN) {
        return false;
    }
    uint8_t c = page[MS1_CONFIG1];
    cfg->map = (ms1_map_t)(c & 0x03);
    cfg->two_stroke = (c & 0x04) != 0;
    cfg->cyl = (uint8_t)((c >> 4) + 1);
    return true;
}

uint16_t ms1_map_kpa(ms1_map_t map) {
    static const uint16_t kpa[] = {115, 250, 300, 400};
    return kpa[map & 0x03];
}

static int16_t kpa_x10(ms1_map_t map, uint8_t adc) {
    switch (map) {
    case MS1_MAP_115:
        return (int16_t)(ms1_kpa4115[adc] * 10);
    case MS1_MAP_300:
        return (int16_t)(((int64_t)adc * 12136750 + 18569228 + 500000) / 1000000);
    case MS1_MAP_400:
        return (int16_t)(((int64_t)adc * 16197783 + 34776640 + 500000) / 1000000);
    case MS1_MAP_250:
    default:
        return (int16_t)(ms1_kpa4250[adc] * 10);
    }
}

uint16_t ms1_rpm(const uint8_t *data, const ms1_cfg_t *cfg) {
    uint32_t itime = (uint32_t)data[37] << 16 | (uint32_t)data[22] << 8 | data[23];
    if (itime == 0 || cfg->cyl == 0) {
        return 0;
    }
    uint64_t den = (uint64_t)itime * cfg->cyl;
    uint64_t rpm = ((cfg->two_stroke ? 60000000ull : 120000000ull) + den / 2) / den;
    if (rpm <= 20) {
        return 0;
    }
    return rpm > UINT16_MAX ? UINT16_MAX : (uint16_t)rpm;
}

bool ms1_decode_rt(const uint8_t *data, size_t len, const ms1_cfg_t *cfg, ecu_rt_t *rt) {
    if (len < MS1_RT_LEN) {
        return false;
    }
    rt->rpm = ms1_rpm(data, cfg);
    rt->engine = data[2];
    rt->adv_x10 = (int16_t)((data[24] * 352 + 50) / 100 - 100);
    rt->baro_x10 = kpa_x10(cfg->map, data[3]);
    rt->map_x10 = kpa_x10(cfg->map, data[4]);
    rt->iat_c_x10 = temp_c_x10(ms1_mat[data[5]]);
    rt->clt_c_x10 = temp_c_x10(ms1_therm[data[6]]);
    rt->tps_x10 = (int16_t)(ms1_tps[data[7]] * 10);
    rt->batt_x10 = (int16_t)((data[8] * 300 + 127) / 255);
    rt->afr_x10 = (int16_t)(100 + (data[9] * 100 + 127) / 255);
    rt->boost_psi_x10 = ecu_boost_psi_x10(rt->map_x10, rt->baro_x10);
    return true;
}
