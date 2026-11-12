#pragma once

#include <stdint.h>

void sound_init(void);
void sound_alarm(uint8_t on);
void sound_music(uint8_t on);
void sound_key(uint8_t pressed);
void sound_tick(void);
