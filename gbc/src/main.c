#include <gb/cgb.h>
#include <gb/gb.h>

#include "dash.h"
#include "rt.h"
#include "sim.h"
#include "tiles.h"

#define C_BG RGB(2, 3, 4)
#define C_TRACK RGB(5, 7, 8)
#define C_FG RGB(29, 30, 28)
#define C_DIM RGB(17, 19, 20)
#define C_GREEN RGB(11, 24, 8)
#define C_AMBER RGB(31, 21, 6)
#define C_RED RGB(31, 9, 6)
#define C_BLUE RGB(9, 20, 31)

static const palette_color_t pals[PAL_COUNT * 4] = {
    C_BG, C_TRACK, C_FG, C_FG,
    C_BG, C_TRACK, C_DIM, C_DIM,
    C_BG, C_TRACK, C_GREEN, C_GREEN,
    C_BG, C_TRACK, C_AMBER, C_AMBER,
    C_BG, C_TRACK, C_RED, C_RED,
    C_BG, C_TRACK, C_BLUE, C_BLUE,
};

#define FRAMES_PER_TICK 6u

static rt_t rt;

void main(void) {
    uint8_t frame = 0;
    uint8_t sec_frames = 0;
    uint16_t rec_s = 192;
    int16_t peak = 0;
    uint8_t paused = 0;
    uint8_t prev = 0;

    DISPLAY_OFF;
    if (_cpu == CGB_TYPE) {
        set_bkg_palette(0, PAL_COUNT, pals);
    } else {
        BGP_REG = 0x1bu;
    }
    tiles_load();
    dash_draw_static();
    sim_init();
    sim_step(&rt);
    SHOW_BKG;
    DISPLAY_ON;

    while (1) {
        uint8_t keys;
        wait_vbl_done();

        keys = joypad();
        if ((keys & J_START) && !(prev & J_START)) {
            paused = (uint8_t)!paused;
        }
        if ((keys & J_SELECT) && !(prev & J_SELECT)) {
            peak = 0;
        }
        prev = keys;

        if (++sec_frames >= 60u) {
            sec_frames = 0;
            rec_s++;
        }
        if (++frame < FRAMES_PER_TICK) {
            continue;
        }
        frame = 0;
        if (!paused) {
            sim_step(&rt);
        }
        if (rt.boost_psi_x10 > peak) {
            peak = rt.boost_psi_x10;
        }
        dash_update(&rt, peak, sec_frames < 30u, rec_s);
    }
}
