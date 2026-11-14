#pragma once

#include <stdint.h>

void clockset_enter(void);
void clockset_tick(void);
void clockset_input(uint8_t pressed);
