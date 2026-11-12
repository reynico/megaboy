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

typedef enum {
    MS1_MAP_4250,
    MS1_MAP_4115,
} ms1_map_t;

bool ms1_check_sig(const uint8_t *data, size_t len);
bool ms1_decode_rt(const uint8_t *data, size_t len, ms1_map_t map, ecu_rt_t *rt);
