#pragma once

#include <stdint.h>

typedef struct {
    uint16_t f;
    uint8_t frames;
} melody_note_t;

typedef struct {
    const melody_note_t *notes;
    uint16_t len;
} melody_voice_t;

extern const melody_voice_t melody_voices[];
extern const uint8_t melody_voice_count;
