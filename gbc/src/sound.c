#include "sound.h"

#include <gb/gb.h>

#include "melody.h"

#define TONE_HI 1975u
#define TONE_LO 1899u
#define HALF_FRAMES 10u
#define VOICES 2u

typedef struct {
    uint16_t note;
    uint8_t left;
    uint8_t done;
} voice_t;

static uint8_t active;
static uint8_t t;
static uint8_t playing;
static voice_t voices[VOICES];

static void ch2_tone(uint16_t f) {
    NR21_REG = 0x80u;
    NR22_REG = 0xf0u;
    NR23_REG = (uint8_t)f;
    NR24_REG = (uint8_t)(0x80u | (f >> 8));
}

static void ch1_tone(uint16_t f) {
    NR10_REG = 0x00u;
    NR11_REG = 0x40u;
    NR12_REG = 0xa0u;
    NR13_REG = (uint8_t)f;
    NR14_REG = (uint8_t)(0x80u | (f >> 8));
}

static void voice_out(uint8_t i, uint16_t f) {
    if (f == 0u) {
        if (i == 0u) {
            NR22_REG = 0x00u;
        } else {
            NR12_REG = 0x00u;
        }
    } else if (i == 0u) {
        ch2_tone(f);
    } else {
        ch1_tone(f);
    }
}

static void music_stop(void) {
    playing = 0;
    NR12_REG = 0x00u;
    NR22_REG = 0x00u;
}

void sound_init(void) {
    NR52_REG = 0x80u;
    NR50_REG = 0x77u;
    NR51_REG = 0x33u;
    NR12_REG = 0x00u;
    NR22_REG = 0x00u;
}

void sound_alarm(uint8_t on) {
    if (playing) {
        music_stop();
    }
    active = on;
    t = 0;
    if (!on) {
        NR22_REG = 0x00u;
    }
}

static void voice_start(uint8_t i) {
    const melody_note_t *n = &melody_voices[i].notes[voices[i].note];
    voices[i].left = n->frames;
    voice_out(i, n->f);
}

void sound_music(uint8_t on) {
    uint8_t i;
    if (!on) {
        if (playing) {
            music_stop();
        }
        return;
    }
    if (active) {
        return;
    }
    playing = 1;
    for (i = 0; i < VOICES; i++) {
        voices[i].note = 0;
        voices[i].done = i >= melody_voice_count || melody_voices[i].len == 0u;
        if (!voices[i].done) {
            voice_start(i);
        }
    }
}

static void music_tick(void) {
    uint8_t i;
    uint8_t any = 0;
    for (i = 0; i < VOICES; i++) {
        voice_t *v = &voices[i];
        if (v->done) {
            continue;
        }
        any = 1;
        if (--v->left) {
            continue;
        }
        if (++v->note >= melody_voices[i].len) {
            v->done = 1;
            voice_out(i, 0u);
            continue;
        }
        voice_start(i);
    }
    if (!any) {
        music_stop();
    }
}

void sound_tick(void) {
    if (!active) {
        if (playing) {
            music_tick();
        }
        return;
    }
    if (t == 0u) {
        ch2_tone(TONE_HI);
    } else if (t == HALF_FRAMES) {
        ch2_tone(TONE_LO);
    }
    if (++t >= 2u * HALF_FRAMES) {
        t = 0;
    }
}
