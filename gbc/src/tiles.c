#include "tiles.h"

#include <gb/gb.h>

#include "font.h"

static const char big_chars[] = "0123456789.+-CWARNIG ";
#define BIG_COUNT 21u

uint8_t tile_small(char c) {
    uint8_t u = (uint8_t)c;
    if (u < FONT_FIRST || u >= FONT_FIRST + FONT_COUNT) {
        return T_SPACE;
    }
    return (uint8_t)(u - FONT_FIRST);
}

uint8_t tile_big_base(char c) {
    uint8_t i;
    if (c >= '0' && c <= '9') {
        return (uint8_t)(T_BIG + (uint8_t)(c - '0') * 4u);
    }
    for (i = 10u; i < BIG_COUNT; i++) {
        if (big_chars[i] == c) {
            return (uint8_t)(T_BIG + i * 4u);
        }
    }
    return (uint8_t)(T_BIG + (BIG_COUNT - 1u) * 4u);
}

static uint8_t dbl(uint8_t nib) {
    uint8_t out = 0;
    uint8_t i;
    for (i = 0; i < 4u; i++) {
        if (nib & (uint8_t)(8u >> i)) {
            out |= (uint8_t)(0xc0u >> (i * 2u));
        }
    }
    return out;
}

static void load_font(void) {
    uint8_t tile[16];
    uint8_t g;
    uint8_t r;
    for (g = 0; g < FONT_COUNT; g++) {
        for (r = 0; r < 8u; r++) {
            uint8_t b = font_1bpp[g * 8u + r];
            tile[r * 2u] = b;
            tile[r * 2u + 1u] = b;
        }
        set_bkg_data(g, 1, tile);
    }
}

static void load_big(void) {
    uint8_t tile[16];
    uint8_t i;
    uint8_t q;
    uint8_t r;
    for (i = 0; i < BIG_COUNT; i++) {
        const uint8_t *g = &font_1bpp[(uint8_t)(big_chars[i] - FONT_FIRST) * 8u];
        for (q = 0; q < 4u; q++) {
            uint8_t qy = q >> 1;
            uint8_t qx = q & 1u;
            for (r = 0; r < 8u; r++) {
                uint8_t src = g[qy * 4u + (r >> 1)];
                uint8_t b = dbl(qx ? (uint8_t)(src & 0x0fu) : (uint8_t)(src >> 4));
                tile[r * 2u] = b;
                tile[r * 2u + 1u] = b;
            }
            set_bkg_data((uint8_t)(T_BIG + i * 4u + q), 1, tile);
        }
    }
}

static void load_shapes(void) {
    uint8_t tile[16];
    uint8_t l;
    uint8_t r;

    for (r = 0; r < 16u; r++) {
        tile[r] = 0;
    }
    tile[3 * 2] = 0xffu;
    set_bkg_data(T_DIV, 1, tile);

    for (l = 0; l < T_BAR_LEVELS; l++) {
        uint8_t fill = l ? (uint8_t)(0xffu << (8u - l)) : 0u;
        for (r = 0; r < 8u; r++) {
            if (r >= 2u && r <= 5u) {
                tile[r * 2u] = (uint8_t)(0xfeu & (uint8_t)~fill);
                tile[r * 2u + 1u] = fill;
            } else {
                tile[r * 2u] = 0;
                tile[r * 2u + 1u] = 0;
            }
        }
        set_bkg_data((uint8_t)(T_BAR0 + l), 1, tile);
    }

    for (r = 0; r < 8u; r++) {
        tile[r * 2u] = 0;
        tile[r * 2u + 1u] = (r >= 2u && r <= 5u) ? 0x3cu : 0u;
    }
    set_bkg_data(T_DOT, 1, tile);

    for (r = 0; r < 16u; r++) {
        tile[r] = (r >= 6u && r <= 9u) ? 0xffu : 0u;
    }
    set_bkg_data(T_STRIPE, 1, tile);
}

void tiles_load(void) {
    load_font();
    load_big();
    load_shapes();
}
