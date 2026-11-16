#pragma once

#include <stdint.h>

void gauge_load(void);
void gauge_enter(void);
void gauge_tick(void);
void gauge_input(uint8_t pressed);
