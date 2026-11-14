#include "clockset.h"

#include <gb/gb.h>

#include "app.h"
#include "hdr.h"
#include "link.h"
#include "linkrx.h"
#include "rtc.h"
#include "screen.h"
#include "tiles.h"
#include "ui.h"

#define F_COUNT 6u
#define DATE_Y 6u
#define TIME_Y 10u
#define NOW_Y 13u
#define MSG_Y 17u

typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t min;
    uint8_t max;
} field_t;

static const field_t fields[F_COUNT] = {
    {9, DATE_Y, 0, 99},
    {12, DATE_Y, 1, 12},
    {15, DATE_Y, 1, 31},
    {9, TIME_Y, 0, 23},
    {12, TIME_Y, 0, 59},
    {15, TIME_Y, 0, 59},
};

static rtc_time_t t;
static uint8_t f;

static uint8_t *field_ptr(uint8_t i) {
    switch (i) {
    case 0:
        return &t.yy;
    case 1:
        return &t.mo;
    case 2:
        return &t.dd;
    case 3:
        return &t.hh;
    case 4:
        return &t.mi;
    default:
        return &t.ss;
    }
}

static void two(char *s, uint8_t v) {
    s[0] = (char)('0' + v / 10u);
    s[1] = (char)('0' + v % 10u);
}

static void draw_fields(void) {
    uint8_t i;
    char s[2];
    for (i = 0; i < F_COUNT; i++) {
        two(s, *field_ptr(i));
        ui_textn(fields[i].x, fields[i].y, s, 2, i == f ? PAL_AMBER : PAL_TEXT);
    }
}

static void draw_now(void) {
    char s[8];
    if (app.clk_state != LINK_CLK_OK) {
        ui_text(9, NOW_Y, "UNSET   ", PAL_AMBER);
        return;
    }
    two(s, app.clk.hh);
    s[2] = ':';
    two(s + 3, app.clk.mi);
    s[5] = ':';
    two(s + 6, app.clk.ss);
    ui_textn(9, NOW_Y, s, 8, PAL_DIM);
}

static void clamp_day(void) {
    uint8_t d = rtc_days(t.yy, t.mo);
    if (t.dd > d) {
        t.dd = d;
    }
}

void clockset_enter(void) {
    if (app.clk_state == LINK_CLK_OK) {
        t = app.clk;
    } else {
        t.yy = 26;
        t.mo = 1;
        t.dd = 1;
        t.hh = 12;
        t.mi = 0;
        t.ss = 0;
    }
    f = 0;
    ui_clear();
    hdr_draw("CLOCK");
    ui_text(1, DATE_Y, "DATE", PAL_DIM);
    ui_text(7, DATE_Y, "20  -  -", PAL_TEXT);
    ui_text(1, TIME_Y, "TIME", PAL_DIM);
    ui_text(11, TIME_Y, ":  :", PAL_TEXT);
    ui_text(1, NOW_Y, "RTC", PAL_DIM);
    ui_divider(15);
    ui_text(1, 16, "A:SET  B:BACK", PAL_DIM);
    draw_fields();
    draw_now();
}

void clockset_tick(void) {
    hdr_tick();
    draw_now();
}

void clockset_input(uint8_t pressed) {
    uint8_t *v = field_ptr(f);
    const field_t *fd = &fields[f];
    uint8_t max = f == 2u ? rtc_days(t.yy, t.mo) : fd->max;
    uint8_t pl[LINK_TIME_SET_LEN];
    if (pressed & J_LEFT) {
        f = (uint8_t)((f + F_COUNT - 1u) % F_COUNT);
    } else if (pressed & J_RIGHT) {
        f = (uint8_t)((f + 1u) % F_COUNT);
    } else if (pressed & J_UP) {
        *v = *v >= max ? fd->min : (uint8_t)(*v + 1u);
        clamp_day();
    } else if (pressed & J_DOWN) {
        *v = *v <= fd->min ? max : (uint8_t)(*v - 1u);
        clamp_day();
    } else if (pressed & J_A) {
        if (!rtc_valid(&t)) {
            ui_text(1, MSG_Y, "BAD DATE", PAL_RED);
            return;
        }
        link_put_time(pl, &t);
        if (!linkrx_send(LINK_C_TIME, pl, LINK_TIME_SET_LEN)) {
            ui_text(1, MSG_Y, "BUSY    ", PAL_RED);
            return;
        }
        screen_go(SCR_SETTINGS);
        return;
    } else if (pressed & J_B) {
        screen_go(SCR_SETTINGS);
        return;
    }
    draw_fields();
}
