#pragma once

#include <stdint.h>

#include "rt.h"

void dash_draw_static(void);
void dash_update(const rt_t *rt, int16_t peak_x10, uint8_t blink, uint16_t rec_s);
