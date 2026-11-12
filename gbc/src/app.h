#pragma once

#include <stdint.h>

#include "rt.h"

typedef struct {
    rt_t rt;
    int16_t peak_x10;
    uint16_t uptime_s;
    uint8_t blink;
} app_t;

extern app_t app;

void app_init(void);
void app_tick(void);
void app_second(void);
