#pragma once

void linkrx_init(void);
#include <stdint.h>

void linkrx_poll(void);
uint8_t linkrx_send(uint8_t type, const uint8_t *payload, uint8_t len);
