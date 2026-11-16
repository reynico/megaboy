#include "gauge.h"

#include <gb/cgb.h>
#include <gb/gb.h>

#include "app.h"
#include "fmt.h"
#include "gauge_gfx.h"
#include "hdr.h"
#include "tiles.h"
#include "ui.h"

#define DOT_TILE 250u
#define HUB_TILE 251u
#define HUB_SPR GAUGE_DOTS
#define BANK1 0x08u
#define SPR_OFF_X 4u
#define SPR_OFF_Y 12u

enum { F_RPM, F_CLT, F_BOOST, F_AFR, F_IAT };

static const uint8_t dot[16] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x1c, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t hub[16] = {
    0x00, 0x00, 0x00, 0x00, 0x1c, 0x1c, 0x3e, 0x3e, 0x3e, 0x3e, 0x3e, 0x3e, 0x1c, 0x1c, 0x00, 0x00,
};

static uint8_t shown_step;

void gauge_load(void) {
    if (_cpu != CGB_TYPE) {
        return;
    }
    VBK_REG = VBK_BANK_1;
    set_bkg_data(0, GAUGE_TILE_COUNT, gauge_tiles);
    VBK_REG = VBK_BANK_0;
    set_sprite_data(DOT_TILE, 1, dot);
    set_sprite_data(HUB_TILE, 1, hub);
}

static void draw_face(void) {
    uint8_t x;
    uint8_t y;
    uint8_t i = 0;
    uint8_t pal;
    for (y = 0; y < GAUGE_H; y++) {
        for (x = 0; x < GAUGE_W; x++, i++) {
            if (gauge_map[i] == GAUGE_NONE) {
                continue;
            }
            pal = (uint16_t)gauge_rpm100[i] * 100u >= (uint16_t)app.set.rpm_shift ? PAL_RED : PAL_TEXT;
            ui_tile((uint8_t)(GAUGE_X + x), (uint8_t)(GAUGE_Y + y), gauge_map[i], (uint8_t)(pal | BANK1));
        }
    }
}

void gauge_enter(void) {
    uint8_t i;
    ui_clear();
    hdr_draw("GAUGE");
    if (_cpu != CGB_TYPE) {
        return;
    }
    draw_face();
    ui_text(9, 10, "RPM", PAL_DIM);
    ui_divider(13);
    ui_text(1, 14, "CLT", PAL_DIM);
    ui_text(10, 14, "BST", PAL_DIM);
    ui_text(1, 16, "AFR", PAL_DIM);
    ui_text(10, 16, "IAT", PAL_DIM);
    for (i = 0; i < GAUGE_DOTS; i++) {
        set_sprite_tile(i, DOT_TILE);
        set_sprite_prop(i, 0);
    }
    set_sprite_tile(HUB_SPR, HUB_TILE);
    set_sprite_prop(HUB_SPR, 0);
    move_sprite(HUB_SPR, (uint8_t)(GAUGE_CX + SPR_OFF_X), (uint8_t)(GAUGE_CY + SPR_OFF_Y));
    shown_step = 0xffu;
    SHOW_SPRITES;
}

static void draw_needle(uint8_t step) {
    const int8_t *p = &gauge_needle[(uint16_t)step * (GAUGE_DOTS * 2u)];
    uint8_t i;
    for (i = 0; i < GAUGE_DOTS; i++, p += 2) {
        move_sprite(i, (uint8_t)(GAUGE_CX + SPR_OFF_X + p[0]), (uint8_t)(GAUGE_CY + SPR_OFF_Y + p[1]));
    }
}

static void field(uint8_t f, uint8_t x, uint8_t y, int16_t v, uint8_t pal, uint8_t kind) {
    char s[5];
    if (!ui_changed(f, v, pal)) {
        return;
    }
    if (kind == 'C') {
        fmt_int(s, 4, v, 'C');
        ui_textn(x, y, s, 4, pal);
    } else if (kind == '+') {
        fmt_x10(s, 5, v, 1);
        ui_textn(x, y, s, 5, pal);
    } else {
        fmt_x10(s, 4, v, 0);
        ui_textn(x, y, s, 4, pal);
    }
}

void gauge_tick(void) {
    const rt_t *rt = &app.rt;
    char s[4];
    uint16_t rpm = rt->rpm > 9999u ? 9999u : rt->rpm;
    uint8_t step = (uint8_t)((rpm > GAUGE_RPM_MAX ? GAUGE_RPM_MAX : rpm) >> 6);
    uint8_t pal = rpm >= (uint16_t)app.set.rpm_shift ? PAL_RED : PAL_TEXT;
    int16_t clt = fmt_round_x10(rt->clt_c_x10);

    hdr_tick();
    if (_cpu != CGB_TYPE) {
        return;
    }
    if (step != shown_step) {
        shown_step = step;
        draw_needle(step);
    }
    if (ui_changed(F_RPM, (int16_t)rpm, pal)) {
        fmt_int(s, 4, (int16_t)rpm, 0);
        ui_big(6, 11, s, 4, pal);
    }
    field(F_CLT, 5, 14, clt, clt >= app.set.clt_warn_c ? PAL_RED : PAL_TEXT, 'C');
    field(F_BOOST, 14, 14, rt->boost_psi_x10, rt->boost_psi_x10 >= 0 ? PAL_AMBER : PAL_BLUE, '+');
    field(F_AFR, 5, 16, rt->afr_x10, PAL_TEXT, 'x');
    field(F_IAT, 15, 16, fmt_round_x10(rt->iat_c_x10), PAL_TEXT, 'C');
}

void gauge_input(uint8_t pressed) {
    (void)pressed;
}
