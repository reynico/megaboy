#pragma once

#include <stdint.h>

#define GAUGE_X 3u
#define GAUGE_Y 2u
#define GAUGE_W 14u
#define GAUGE_H 9u
#define GAUGE_TILE_COUNT 70u
#define GAUGE_STEPS 126u
#define GAUGE_DOTS 9u
#define GAUGE_CX 80u
#define GAUGE_CY 64u
#define GAUGE_RPM_MAX 8000u
#define GAUGE_NONE 0xffu

extern const uint8_t gauge_tiles[GAUGE_TILE_COUNT * 16];
extern const uint8_t gauge_map[GAUGE_W * GAUGE_H];
extern const uint8_t gauge_rpm100[GAUGE_W * GAUGE_H];
extern const int8_t gauge_dir[GAUGE_STEPS * 2];
extern const uint8_t gauge_dot_r[GAUGE_DOTS];
