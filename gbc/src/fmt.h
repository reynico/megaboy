#pragma once

#include <stdint.h>

int16_t fmt_round_x10(int16_t v);
void fmt_int(char *out, uint8_t w, int16_t v, char suffix);
void fmt_x10(char *out, uint8_t w, int16_t v, uint8_t plus);
void fmt_time(char *out, uint16_t sec);
