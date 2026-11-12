#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MS2_CAN_ID 0x00
#define MS2_TABLE_RT 0x07
#define MS2_TABLE_SIG 0x0f
#define MS2_SIG "MS2Extra comms332m2"
#define MS2_SIG_LEN 20
#define MS2_RT_LEN 30
#define MS2_MAX_PAYLOAD 260
#define MS2_FRAME_MAX (2 + MS2_MAX_PAYLOAD + 4)

typedef struct {
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
} ms2_rt_t;

typedef enum {
    MS2_P_NEED,
    MS2_P_OK,
    MS2_P_ERR_LEN,
    MS2_P_ERR_CRC,
} ms2_pstat_t;

typedef struct {
    uint8_t buf[MS2_FRAME_MAX];
    size_t n;
    size_t want;
} ms2_parser_t;

uint32_t ms2_crc32(const uint8_t *d, size_t n);
size_t ms2_frame(uint8_t *out, const uint8_t *payload, size_t n);
size_t ms2_req_read(uint8_t *out, uint8_t table, uint16_t off, uint16_t cnt);

void ms2_parser_reset(ms2_parser_t *p);
ms2_pstat_t ms2_parser_feed(ms2_parser_t *p, uint8_t b, const uint8_t **payload, size_t *len);

bool ms2_resp_ok(const uint8_t *payload, size_t len);
bool ms2_check_sig(const uint8_t *data, size_t len);
bool ms2_decode_rt(const uint8_t *data, size_t len, ms2_rt_t *rt);
