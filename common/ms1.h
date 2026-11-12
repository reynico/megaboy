#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ecu.h"

#define MS1_BAUD 9600
#define MS1_CMD_SIG 'S'
#define MS1_CMD_RT 'R'
#define MS1_SIG_LEN 32
#define MS1_RT_LEN 39
#define MS1_CMD_PAGE 'P'
#define MS1_CMD_READ 'V'
#define MS1_CONFIG_PAGE 1
#define MS1_PAGE_LEN 189
#define MS1_CONFIG1 182

typedef enum {
    MS1_MAP_115,
    MS1_MAP_250,
    MS1_MAP_300,
    MS1_MAP_400,
} ms1_map_t;

bool ms1_check_sig(const uint8_t *data, size_t len);
typedef struct {
    ms1_map_t map;
    uint8_t cyl;
    bool two_stroke;
} ms1_cfg_t;

bool ms1_parse_config(const uint8_t *page, size_t len, ms1_cfg_t *cfg);
uint16_t ms1_map_kpa(ms1_map_t map);
uint16_t ms1_rpm(const uint8_t *data, const ms1_cfg_t *cfg);
bool ms1_decode_rt(const uint8_t *data, size_t len, const ms1_cfg_t *cfg, ecu_rt_t *rt);
