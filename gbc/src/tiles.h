#pragma once

#include <stdint.h>

#define T_SPACE 0u
#define T_DIV 64u
#define T_BAR0 65u
#define T_BAR_LEVELS 8u
#define T_DOT 73u
#define T_BIG 80u

#define PAL_TEXT 0u
#define PAL_DIM 1u
#define PAL_GREEN 2u
#define PAL_AMBER 3u
#define PAL_RED 4u
#define PAL_BLUE 5u
#define PAL_WARN 6u
#define PAL_LIVE 7u
#define PAL_COUNT 8u

#define CH_UP '^'
#define CH_DOWN '_'

uint8_t tile_small(char c);
uint8_t tile_big_base(char c);
void tiles_load(void);
