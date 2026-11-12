#include "ui.h"

#include <gb/cgb.h>
#include <gb/gb.h>
#include <string.h>

#include "tiles.h"

static uint8_t row[20];

void ui_attr(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t pal) {
    uint8_t i;
    if (_cpu != CGB_TYPE || w == 0u) {
        return;
    }
    for (i = 0; i < w; i++) {
        row[i] = pal;
    }
    VBK_REG = VBK_ATTRIBUTES;
    for (i = 0; i < h; i++) {
        set_bkg_tiles(x, (uint8_t)(y + i), w, 1, row);
    }
    VBK_REG = VBK_TILES;
}

void ui_clear(void) {
    uint8_t y;
    memset(row, T_SPACE, sizeof row);
    for (y = 0; y < 18u; y++) {
        set_bkg_tiles(0, y, 20, 1, row);
    }
    ui_attr(0, 0, 20, 18, PAL_TEXT);
}

void ui_tile(uint8_t x, uint8_t y, uint8_t tile, uint8_t pal) {
    row[0] = tile;
    set_bkg_tiles(x, y, 1, 1, row);
    ui_attr(x, y, 1, 1, pal);
}

void ui_textn(uint8_t x, uint8_t y, const char *s, uint8_t n, uint8_t pal) {
    uint8_t i;
    for (i = 0; i < n; i++) {
        row[i] = tile_small(s[i]);
    }
    set_bkg_tiles(x, y, n, 1, row);
    ui_attr(x, y, n, 1, pal);
}

void ui_text(uint8_t x, uint8_t y, const char *s, uint8_t pal) {
    ui_textn(x, y, s, (uint8_t)strlen(s), pal);
}

void ui_big(uint8_t x, uint8_t y, const char *s, uint8_t n, uint8_t pal) {
    uint8_t i;
    uint8_t half;
    for (half = 0; half < 2u; half++) {
        for (i = 0; i < n; i++) {
            row[i * 2u] = tile_big(s[i], (uint8_t)(half * 2u));
            row[i * 2u + 1u] = tile_big(s[i], (uint8_t)(half * 2u + 1u));
        }
        set_bkg_tiles(x, (uint8_t)(y + half), (uint8_t)(n * 2u), 1, row);
    }
    ui_attr(x, y, (uint8_t)(n * 2u), 2, pal);
}

void ui_bar(uint8_t x, uint8_t y, uint8_t w, int32_t value, int32_t max) {
    uint8_t i;
    int16_t px;
    if (value < 0) {
        value = 0;
    }
    if (value > max) {
        value = max;
    }
    px = (int16_t)(value * (int32_t)(w * 7u) / max);
    for (i = 0; i < w; i++) {
        int16_t l = (int16_t)(px - (int16_t)i * 7);
        if (l < 0) {
            l = 0;
        }
        if (l > 7) {
            l = 7;
        }
        row[i] = (uint8_t)(T_BAR0 + (uint8_t)l);
    }
    set_bkg_tiles(x, y, w, 1, row);
}

void ui_bar_zones(uint8_t x, uint8_t y, uint8_t w, uint8_t amber_from, uint8_t red_from) {
    ui_attr(x, y, amber_from, 1, PAL_GREEN);
    ui_attr((uint8_t)(x + amber_from), y, (uint8_t)(red_from - amber_from), 1, PAL_AMBER);
    ui_attr((uint8_t)(x + red_from), y, (uint8_t)(w - red_from), 1, PAL_RED);
}

void ui_divider(uint8_t y) {
    uint8_t i;
    for (i = 0; i < 20u; i++) {
        row[i] = T_DIV;
    }
    set_bkg_tiles(0, y, 20, 1, row);
    ui_attr(0, y, 20, 1, PAL_TEXT);
}
