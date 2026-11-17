#pragma once

#include <stdint.h>

typedef struct {
    uint8_t p;
    uint8_t frames;
} melody_note_t;

typedef struct {
    const melody_note_t *notes;
    uint16_t len;
} melody_voice_t;

extern const uint16_t melody_pitch[];
extern const melody_voice_t melody_voices[];
extern const uint8_t melody_voice_count;
