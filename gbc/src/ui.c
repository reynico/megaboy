#include "ui.h"

#include <gb/cgb.h>
#include <gb/gb.h>
#include <string.h>

#include "fmt.h"
#include "tiles.h"

#define COLS 20u
#define ROWS 18u

static uint8_t row[COLS];
static uint8_t sh_tile[ROWS][COLS];
static uint8_t sh_attr[ROWS][COLS];
static int16_t slot_val[UI_SLOTS];
static uint8_t slot_pal[UI_SLOTS];

uint8_t ui_changed(uint8_t slot, int16_t v, uint8_t pal) {
    if (slot_val[slot] == v && slot_pal[slot] == pal) {
        return 0;
    }
    slot_val[slot] = v;
    slot_pal[slot] = pal;
    return 1;
}

static void put_row(uint8_t x, uint8_t y, uint8_t n, const uint8_t *src, uint8_t *sh) {
    uint8_t first = 0;
    uint8_t last;
    sh += x;
    while (first < n && src[first] == sh[first]) {
        first++;
    }
    if (first == n) {
        return;
    }
    last = (uint8_t)(n - 1u);
    while (src[last] == sh[last]) {
        last--;
    }
    memcpy(sh + first, src + first, (uint8_t)(last - first + 1u));
    set_bkg_tiles((uint8_t)(x + first), y, (uint8_t)(last - first + 1u), 1, src + first);
}

static void put_tiles(uint8_t x, uint8_t y, uint8_t n, const uint8_t *src) {
    put_row(x, y, n, src, sh_tile[y]);
}

void ui_attr(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t pal) {
    uint8_t i;
    uint8_t j;
    const uint8_t *a;
    if (_cpu != CGB_TYPE || w == 0u) {
        return;
    }
    for (j = 0; j < h; j++) {
        a = &sh_attr[y + j][x];
        for (i = 0; i < w; i++) {
            if (a[i] != pal) {
                break;
            }
        }
        if (i < w) {
            break;
        }
    }
    if (j == h) {
        return;
    }
    for (i = 0; i < w; i++) {
        row[i] = pal;
    }
    VBK_REG = VBK_ATTRIBUTES;
    for (i = 0; i < h; i++) {
        put_row(x, (uint8_t)(y + i), w, row, sh_attr[y + i]);
    }
    VBK_REG = VBK_TILES;
}

void ui_clear(void) {
    uint8_t y;
    memset(row, T_SPACE, sizeof row);
    memset(sh_tile, T_SPACE, sizeof sh_tile);
    memset(slot_pal, 0xff, sizeof slot_pal);
    for (y = 0; y < ROWS; y++) {
        set_bkg_tiles(0, y, COLS, 1, row);
    }
    if (_cpu == CGB_TYPE) {
        memset(row, PAL_TEXT, sizeof row);
        memset(sh_attr, PAL_TEXT, sizeof sh_attr);
        VBK_REG = VBK_ATTRIBUTES;
        for (y = 0; y < ROWS; y++) {
            set_bkg_tiles(0, y, COLS, 1, row);
        }
        VBK_REG = VBK_TILES;
    }
}

void ui_tile(uint8_t x, uint8_t y, uint8_t tile, uint8_t pal) {
    row[0] = tile;
    put_tiles(x, y, 1, row);
    ui_attr(x, y, 1, 1, pal);
}

void ui_textn(uint8_t x, uint8_t y, const char *s, uint8_t n, uint8_t pal) {
    uint8_t i;
    for (i = 0; i < n; i++) {
        row[i] = tile_small(s[i]);
    }
    put_tiles(x, y, n, row);
    ui_attr(x, y, n, 1, pal);
}

void ui_text(uint8_t x, uint8_t y, const char *s, uint8_t pal) {
    ui_textn(x, y, s, (uint8_t)strlen(s), pal);
}

void ui_big(uint8_t x, uint8_t y, const char *s, uint8_t n, uint8_t pal) {
    uint8_t base[COLS / 2u];
    uint8_t i;
    uint8_t *r;
    for (i = 0; i < n; i++) {
        base[i] = tile_big_base(s[i]);
    }
    r = row;
    for (i = 0; i < n; i++) {
        *r++ = base[i];
        *r++ = (uint8_t)(base[i] + 1u);
    }
    put_tiles(x, y, (uint8_t)(n * 2u), row);
    r = row;
    for (i = 0; i < n; i++) {
        *r++ = (uint8_t)(base[i] + 2u);
        *r++ = (uint8_t)(base[i] + 3u);
    }
    put_tiles(x, (uint8_t)(y + 1u), (uint8_t)(n * 2u), row);
    ui_attr(x, y, (uint8_t)(n * 2u), 2, pal);
}

void ui_bar(uint8_t x, uint8_t y, uint8_t w, int16_t value, uint16_t max) {
    uint8_t i;
    uint8_t px = fmt_bar_px(value, max, w);
    for (i = 0; i < w; i++) {
        if (px >= 7u) {
            row[i] = (uint8_t)(T_BAR0 + 7u);
            px -= 7u;
        } else {
            row[i] = (uint8_t)(T_BAR0 + (uint8_t)px);
            px = 0;
        }
    }
    put_tiles(x, y, w, row);
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
    put_tiles(0, y, COLS, row);
    ui_attr(0, y, COLS, 1, PAL_TEXT);
}
