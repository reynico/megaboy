#pragma once

#include <stdint.h>

#include "app.h"

uint8_t save_load(settings_t *out);
void save_store(const settings_t *in);
