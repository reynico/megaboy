#pragma once

#include <stdint.h>

#include "ecu.h"
#include "link.h"
#include "rtc.h"

void log_probe(void);
void log_start(const char *sig, const rtc_time_t *t, uint32_t now_ms);
void log_stop(void);
void log_row(const ecu_rt_t *r, uint32_t now_ms);
void log_second(uint32_t now_ms);
void log_status(link_log_t *out);
uint8_t log_active(void);
