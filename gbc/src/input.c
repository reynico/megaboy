#include "input.h"

#include <gb/gb.h>

#define REPEAT_KEYS (J_UP | J_DOWN | J_LEFT | J_RIGHT)
#define REPEAT_DELAY 20u
#define REPEAT_RATE 5u

static uint8_t prev;
static uint8_t held_frames;

uint8_t input_poll(void) {
    uint8_t keys = joypad();
    uint8_t pressed = (uint8_t)(keys & (uint8_t)~prev);
    uint8_t rep = (uint8_t)(keys & REPEAT_KEYS);

    if (rep && rep == (uint8_t)(prev & REPEAT_KEYS)) {
        if (++held_frames >= REPEAT_DELAY) {
            held_frames = (uint8_t)(REPEAT_DELAY - REPEAT_RATE);
            pressed |= rep;
        }
    } else {
        held_frames = 0;
    }
    prev = keys;
    return pressed;
}
