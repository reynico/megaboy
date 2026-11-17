#include "save.h"

#include <gb/gb.h>
#include <string.h>

#define SRAM ((uint8_t *)0xa000u)
#define MAGIC0 'M'
#define MAGIC1 'B'
#define VERSION 5u
#define HDR 3u

static uint8_t sum(const uint8_t *p, uint8_t n) {
    uint8_t s = 0x5au;
    uint8_t i;
    for (i = 0; i < n; i++) {
        s = (uint8_t)((s << 1 | s >> 7) ^ p[i]);
    }
    return s;
}

uint8_t save_load(settings_t *out) {
    settings_t tmp;
    uint8_t ok;
    ENABLE_RAM;
    SWITCH_RAM(0);
    ok = SRAM[0] == MAGIC0 && SRAM[1] == MAGIC1 && SRAM[2] == VERSION;
    if (ok) {
        memcpy(&tmp, SRAM + HDR, sizeof tmp);
        ok = SRAM[HDR + sizeof tmp] == sum((const uint8_t *)&tmp, sizeof tmp);
    }
    DISABLE_RAM;
    if (ok) {
        *out = tmp;
    }
    return ok;
}

void save_store(const settings_t *in) {
    ENABLE_RAM;
    SWITCH_RAM(0);
    SRAM[0] = MAGIC0;
    SRAM[1] = MAGIC1;
    SRAM[2] = VERSION;
    memcpy(SRAM + HDR, in, sizeof *in);
    SRAM[HDR + sizeof *in] = sum((const uint8_t *)in, sizeof *in);
    DISABLE_RAM;
}
