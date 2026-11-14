#pragma once

#include <stdint.h>

#include "rt.h"
#include "rtc.h"

typedef struct {
    int16_t clt_warn_c;
    int16_t afr_lean_x10;
    int16_t rpm_shift;
    int16_t boost_warn_x10;
    int16_t auto_log;
    int16_t alarm_snd;
    int16_t key_snd;
    int16_t ecu;
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
    uint8_t log_state;
    uint8_t log_hz;
    uint8_t log_req;
    uint8_t src;
    uint8_t link_age;
    uint8_t rt_age;
    uint8_t ecu;
    uint8_t ecu_state;
    uint8_t rt_new;
    uint8_t clk_state;
    rtc_time_t clk;
} app_t;

#define LOG_BYTES_PER_S 3200u

enum { SRC_SIM, SRC_LINK, SRC_LOST };

#define LINK_LOST_TICKS 20u
#define RT_STALE_TICKS 10u

enum { W_NONE, W_CLT, W_LEAN, W_BOOST, W_COUNT };

extern app_t app;

void app_init(void);
void app_log_start(void);
void app_log_stop(void);
uint8_t app_warn_active(uint8_t id);
uint8_t app_warning(void);
void app_warn_ack(uint8_t id);
void app_tick(void);
void app_second(void);
uint8_t app_ecu_missing(void);
void app_rt_received(void);
