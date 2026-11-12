#pragma once

#include <stdint.h>

#if defined(__AVR__)
#include <avr/pgmspace.h>
#define MS1_TABLE PROGMEM
#define MS1_RD(t, i) ((uint16_t)pgm_read_word(&(t)[i]))
#else
#define MS1_TABLE
#define MS1_RD(t, i) ((t)[i])
#endif

extern const uint16_t ms1_therm[256] MS1_TABLE;
extern const uint16_t ms1_mat[256] MS1_TABLE;
extern const uint16_t ms1_kpa4250[256] MS1_TABLE;
extern const uint16_t ms1_kpa4115[256] MS1_TABLE;
extern const uint16_t ms1_tps[256] MS1_TABLE;
