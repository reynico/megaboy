#include "ms2.h"

#include <string.h>

uint32_t ms2_crc32(const uint8_t *d, size_t n) {
    uint32_t c = 0xffffffffu;
    for (size_t i = 0; i < n; i++) {
        c ^= d[i];
        for (int k = 0; k < 8; k++) {
            c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1u)));
        }
    }
    return ~c;
}

static void put16(uint8_t *o, uint16_t v) {
    o[0] = (uint8_t)(v >> 8);
    o[1] = (uint8_t)v;
}

static void put32(uint8_t *o, uint32_t v) {
    o[0] = (uint8_t)(v >> 24);
    o[1] = (uint8_t)(v >> 16);
    o[2] = (uint8_t)(v >> 8);
    o[3] = (uint8_t)v;
}

static uint16_t get16(const uint8_t *d) {
    return (uint16_t)((d[0] << 8) | d[1]);
}

static uint32_t get32(const uint8_t *d) {
    return ((uint32_t)d[0] << 24) | ((uint32_t)d[1] << 16) | ((uint32_t)d[2] << 8) | d[3];
}

size_t ms2_frame(uint8_t *out, const uint8_t *payload, size_t n) {
    if (n == 0 || n > MS2_MAX_PAYLOAD) {
        return 0;
    }
    put16(out, (uint16_t)n);
    memcpy(out + 2, payload, n);
    put32(out + 2 + n, ms2_crc32(payload, n));
    return n + 6;
}

size_t ms2_req_read(uint8_t *out, uint8_t table, uint16_t off, uint16_t cnt) {
    uint8_t p[7] = {'r', MS2_CAN_ID, table};
    put16(p + 3, off);
    put16(p + 5, cnt);
    return ms2_frame(out, p, sizeof p);
}

void ms2_parser_reset(ms2_parser_t *p) {
    p->n = 0;
    p->want = 0;
}

ms2_pstat_t ms2_parser_feed(ms2_parser_t *p, uint8_t b, const uint8_t **payload, size_t *len) {
    p->buf[p->n++] = b;
    if (p->n == 2) {
        size_t l = get16(p->buf);
        if (l == 0 || l > MS2_MAX_PAYLOAD) {
            ms2_parser_reset(p);
            return MS2_P_ERR_LEN;
        }
        p->want = l + 6;
    }
    if (p->want == 0 || p->n < p->want) {
        return MS2_P_NEED;
    }
    size_t l = p->want - 6;
    uint32_t crc = get32(p->buf + 2 + l);
    ms2_parser_reset(p);
    if (crc != ms2_crc32(p->buf + 2, l)) {
        return MS2_P_ERR_CRC;
    }
    *payload = p->buf + 2;
    *len = l;
    return MS2_P_OK;
}

bool ms2_resp_ok(const uint8_t *payload, size_t len) {
    return len >= 1 && payload[0] < 0x80;
}

bool ms2_check_sig(const uint8_t *data, size_t len) {
    size_t sl = strlen(MS2_SIG);
    return len >= sl && memcmp(data, MS2_SIG, sl) == 0;
}

bool ms2_decode_rt(const uint8_t *data, size_t len, ecu_rt_t *rt) {
    if (len < MS2_RT_LEN) {
        return false;
    }
    rt->rpm = get16(data + 6);
    rt->adv_x10 = (int16_t)get16(data + 8);
    rt->engine = data[11];
    rt->baro_x10 = (int16_t)get16(data + 16);
    rt->map_x10 = (int16_t)get16(data + 18);
    rt->iat_c_x10 = ecu_f_to_c_x10((int16_t)get16(data + 20));
    rt->clt_c_x10 = ecu_f_to_c_x10((int16_t)get16(data + 22));
    rt->tps_x10 = (int16_t)get16(data + 24);
    rt->batt_x10 = (int16_t)get16(data + 26);
    rt->afr_x10 = (int16_t)get16(data + 28);
    rt->boost_psi_x10 = ecu_boost_psi_x10(rt->map_x10, rt->baro_x10);
    return true;
}
