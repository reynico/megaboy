#pragma once

#include <stdint.h>

#include "rt.h"

typedef struct {
    int16_t clt_warn_c;
    int16_t afr_lean_x10;
    int16_t rpm_shift;
    int16_t boost_warn_x10;
    int16_t auto_log;
} settings_t;

typedef struct {
    rt_t rt;
    settings_t set;
    int16_t peak_x10;
    uint16_t uptime_s;
    uint8_t blink;
    uint8_t logging;
    uint16_t log_s;
    uint16_t log_num;
    uint32_t log_bytes;
} app_t;

#define LOG_BYTES_PER_S 3200u

extern app_t app;

void app_init(void);
void app_log_start(void);
void app_log_stop(void);
void app_tick(void);
void app_second(void);
