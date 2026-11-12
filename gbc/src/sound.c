#include "sound.h"

#include <gb/gb.h>

#define TONE_HI 1975u
#define TONE_LO 1899u
#define HALF_FRAMES 10u

static uint8_t active;
static uint8_t t;

static void tone(uint16_t f) {
    NR21_REG = 0x80u;
    NR22_REG = 0xf0u;
    NR23_REG = (uint8_t)f;
    NR24_REG = (uint8_t)(0x80u | (f >> 8));
}

void sound_init(void) {
    NR52_REG = 0x80u;
    NR50_REG = 0x77u;
    NR51_REG = 0x22u;
    NR22_REG = 0x00u;
}

void sound_alarm(uint8_t on) {
    active = on;
    t = 0;
    if (!on) {
        NR22_REG = 0x00u;
    }
}

void sound_tick(void) {
    if (!active) {
        return;
    }
    if (t == 0u) {
        tone(TONE_HI);
    } else if (t == HALF_FRAMES) {
        tone(TONE_LO);
    }
    if (++t >= 2u * HALF_FRAMES) {
        t = 0;
    }
}
