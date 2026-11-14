#pragma once

#include <stdint.h>

void ui_clear(void);
void ui_attr(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t pal);
void ui_tile(uint8_t x, uint8_t y, uint8_t tile, uint8_t pal);
void ui_textn(uint8_t x, uint8_t y, const char *s, uint8_t n, uint8_t pal);
void ui_text(uint8_t x, uint8_t y, const char *s, uint8_t pal);
void ui_big(uint8_t x, uint8_t y, const char *s, uint8_t n, uint8_t pal);
void ui_bar(uint8_t x, uint8_t y, uint8_t w, int16_t value, uint16_t max);
void ui_bar_zones(uint8_t x, uint8_t y, uint8_t w, uint8_t amber_from, uint8_t red_from);
void ui_divider(uint8_t y);
