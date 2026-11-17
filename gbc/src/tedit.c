#include "tedit.h"

#include <gb/gb.h>

#include "app.h"
#include "fmt.h"
#include "hdr.h"
#include "screen.h"
#include "tdata.h"
#include "tiles.h"
#include "ui.h"

#define VIS_COLS 4u
#define VIS_ROWS 12u
#define CELL_W 4u
#define GRID_X 4u
#define BINS_Y 2u
#define GRID_Y 3u
#define STATUS_Y 16u
#define KEYS_Y 17u
#define NONE 0xffu

enum { M_NAV, M_EDIT, M_EXIT, M_LEAVE };

static const char *const titles[TUNE_COUNT] = {"VE1", "SPARK1", "AFR1", "LIMITS", "BOOST TGT", "BOOST DUTY"};
static const char *const ms1_limits[] = {"SOFT LIMIT", "HARD LIMIT", "LAUNCH", "FLAT SHIFT"};
static const char *const ms2_limits[] = {"REV LIMIT", "LAUNCH", "FLAT ARM", "FLAT SHIFT"};
#define LIST_Y0 3u
#define LIST_VAL_X 14u

static uint8_t emode;
static uint8_t cur_r;
static uint8_t cur_c;
static uint8_t view_c;
static uint8_t view_r;
static uint8_t live_r;
static uint8_t live_c;
static uint8_t shown_state;
static int16_t edit_val;
static uint8_t drawn_view = NONE;
static uint8_t drawn_view_r = NONE;
static int16_t drawn_v[VIS_ROWS][VIS_COLS];
static uint8_t drawn_p[VIS_ROWS][VIS_COLS];
static const char *shown_line[2];
static uint8_t shown_pal[2];

static uint8_t rows(void) {
    return td.t->rows;
}

static uint8_t cols(void) {
    return td.t->cols;
}

static uint8_t is_list(void) {
    return td.t->items != 0;
}

static uint8_t is_tps(void) {
    return td.t->y_tps;
}

static uint16_t cell_at(uint8_t r, uint8_t c) {
    if (is_list()) {
        return r;
    }
    return (uint16_t)((rows() - 1u - r) * cols() + c);
}

static int16_t rpm_bin(uint8_t c) {
    return tune_axis(td.t->xaxis, td.blob[tune_cells(td.t) + c]);
}

static int16_t y_bin(uint8_t r) {
    return tune_axis(td.t->yaxis, td.blob[tune_cells(td.t) + cols() + (rows() - 1u - r)]);
}

static void fmt_x10_cell(char *s, int16_t v) {
    if (v <= -100 || v >= 1000) {
        fmt_int(s, CELL_W, fmt_round_x10(v), 0);
    } else {
        fmt_x10(s, CELL_W, v, 0);
    }
}

static void fmt_cell(char *s, int16_t raw) {
    switch (td.t->fmt) {
    case TUNE_F_X10:
        fmt_x10_cell(s, raw);
        break;
    case TUNE_F_MS1_ADV:
        fmt_x10_cell(s, (int16_t)(((int32_t)raw * 352 + 50) / 100 - 100));
        break;
    case TUNE_F_MS1_AFR:
        fmt_x10(s, CELL_W, (int16_t)(100 + ((int32_t)raw * 392 + 500) / 1000), 0);
        break;
    case TUNE_F_MS1_DUTY:
        fmt_x10_cell(s, (int16_t)(((int32_t)raw * 392 + 50) / 100));
        break;
    default:
        fmt_int(s, CELL_W, raw, 0);
        break;
    }
}

static uint8_t nearest(int16_t want, uint8_t n, uint8_t is_rpm) {
    uint8_t i;
    uint8_t best = 0;
    uint16_t best_d = 0xffffu;
    for (i = 0; i < n; i++) {
        int16_t b = is_rpm ? rpm_bin(i) : y_bin(i);
        uint16_t d = b > want ? (uint16_t)(b - want) : (uint16_t)(want - b);
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

static uint8_t update_live(void) {
    uint8_t r;
    uint8_t c;
    int16_t y;
    if (is_list() || app.src != SRC_LINK || app_ecu_missing()) {
        r = NONE;
        c = NONE;
    } else {
        y = is_tps() ? app.rt.tps_x10 : app.rt.map_x10;
        c = nearest((int16_t)(app.rt.rpm > 30000u ? 30000u : app.rt.rpm), cols(), 1);
        r = nearest((int16_t)(y / 10), rows(), 0);
    }
    if (r == live_r && c == live_c) {
        return 0;
    }
    live_r = r;
    live_c = c;
    return 1;
}

static uint8_t cell_pal(uint8_t r, uint8_t c) {
    if (r == cur_r && c == cur_c) {
        return emode == M_EDIT ? PAL_RED : PAL_AMBER;
    }
    if (r == live_r && c == live_c) {
        return PAL_LIVE;
    }
    return (c & 1u) ? PAL_BLUE : PAL_TEXT;
}

static uint8_t vis_rows(void) {
    return rows() < VIS_ROWS ? rows() : VIS_ROWS;
}

static void draw_axes(void) {
    char s[CELL_W];
    uint8_t k;
    uint8_t j;
    uint8_t c;
    for (k = 0; k < VIS_COLS; k++) {
        c = (uint8_t)(view_c + k);
        if (c < cols()) {
            fmt_int(s, CELL_W, (int16_t)(rpm_bin(c) / 100), 0);
            ui_textn((uint8_t)(GRID_X + k * CELL_W), BINS_Y, s, CELL_W, PAL_DIM);
        } else {
            ui_text((uint8_t)(GRID_X + k * CELL_W), BINS_Y, "    ", PAL_DIM);
        }
        for (j = 0; j < vis_rows(); j++) {
            drawn_p[j][k] = NONE;
            if (c >= cols()) {
                ui_text((uint8_t)(GRID_X + k * CELL_W), (uint8_t)(GRID_Y + j), "    ", PAL_TEXT);
            }
        }
    }
    for (j = 0; j < vis_rows(); j++) {
        fmt_int(s, 3, y_bin((uint8_t)(view_r + j)), 0);
        ui_textn(0, (uint8_t)(GRID_Y + j), s, 3, PAL_DIM);
    }
    drawn_view = view_c;
    drawn_view_r = view_r;
}

static void draw_grid(void) {
    char s[CELL_W];
    uint8_t j;
    uint8_t r;
    uint8_t k;
    uint8_t c;
    int16_t v;
    uint8_t p;
    if (drawn_view != view_c || drawn_view_r != view_r) {
        draw_axes();
    }
    for (j = 0; j < vis_rows(); j++) {
        r = (uint8_t)(view_r + j);
        for (k = 0; k < VIS_COLS; k++) {
            c = (uint8_t)(view_c + k);
            if (c >= cols()) {
                continue;
            }
            v = (emode == M_EDIT && r == cur_r && c == cur_c) ? edit_val : td_value(cell_at(r, c));
            p = cell_pal(r, c);
            if (v == drawn_v[j][k] && p == drawn_p[j][k]) {
                continue;
            }
            drawn_v[j][k] = v;
            drawn_p[j][k] = p;
            fmt_cell(s, v);
            ui_textn((uint8_t)(GRID_X + k * CELL_W), (uint8_t)(GRID_Y + j), s, CELL_W, p);
        }
    }
}

static void draw_list(void) {
    char s[5];
    uint8_t r;
    uint8_t y;
    int16_t v;
    const char *const *names = app.ecu == TUNE_ECU_MS2 ? ms2_limits : ms1_limits;
    for (r = 0; r < rows(); r++) {
        y = (uint8_t)(LIST_Y0 + r * 2u);
        v = (emode == M_EDIT && r == cur_r) ? edit_val : td_value(r);
        ui_tile(0, y, r == cur_r ? tile_small(emode == M_EDIT ? '*' : '>') : T_SPACE, PAL_AMBER);
        ui_text(1, y, names[r], PAL_TEXT);
        fmt_int(s, 5, td.t->fmt == TUNE_F_X100 ? (int16_t)(v * 100) : v, 0);
        ui_textn(LIST_VAL_X, y, s, 5, cell_pal(r, 0));
    }
}

static const char *err_text(uint8_t e) {
    switch (e) {
    case TUNE_E_RANGE:
        return "OUT OF RANGE";
    case TUNE_E_DELTA:
        return "STEP TOO BIG";
    case TUNE_E_VERIFY:
        return "VERIFY FAILED";
    case TUNE_E_ECU:
        return "NO ECU ANSWER";
    case TUNE_E_TABLE:
        return "UNKNOWN TABLE";
    case TUNE_E_ORDER:
        return "LIMIT ORDER";
    default:
        return "BAD REQUEST";
    }
}

static void line(uint8_t y, const char *s, uint8_t pal) {
    uint8_t i = (uint8_t)(y - STATUS_Y);
    if (shown_line[i] == s && shown_pal[i] == pal) {
        return;
    }
    shown_line[i] = s;
    shown_pal[i] = pal;
    ui_text(0, y, "                    ", PAL_TEXT);
    ui_text(1, y, s, pal);
}

static void invalidate(void) {
    drawn_view = NONE;
    drawn_view_r = NONE;
    shown_line[0] = 0;
    shown_line[1] = 0;
}

static void draw_status(void) {
    char s[4];
    if (td.state == TD_LOADING) {
        line(STATUS_Y, "LOADING", PAL_AMBER);
    } else if (td.state == TD_ERROR) {
        line(STATUS_Y, err_text(td.err), PAL_RED);
    } else if (emode == M_EXIT) {
        line(STATUS_Y, "BURN? START:STAY", PAL_AMBER);
    } else if (td.err != TUNE_OK) {
        line(STATUS_Y, err_text(td.err), PAL_RED);
    } else if (td.discarding) {
        line(STATUS_Y, "DISCARDING", PAL_AMBER);
    } else if (td_busy()) {
        line(STATUS_Y, "WRITING", PAL_AMBER);
    } else if (td.burned) {
        line(STATUS_Y, "BURNED", PAL_GREEN);
    } else if (td_dirty()) {
        line(STATUS_Y, "IN RAM, NOT BURNED", PAL_AMBER);
    } else {
        if (is_list()) {
            line(STATUS_Y, "VALUES IN RPM", PAL_DIM);
        } else {
            line(STATUS_Y, is_tps() ? "RPM X100   TPS" : "RPM X100   KPA", PAL_DIM);
            fmt_int(s, 2, (int16_t)(rpm_bin(cur_c) / 100), 0);
            ui_textn(10, STATUS_Y, s, 2, PAL_TEXT);
            fmt_int(s, 3, y_bin(cur_r), 0);
            ui_textn(16, STATUS_Y, s, 3, PAL_TEXT);
        }
    }
    if (td.state != TD_READY) {
        line(KEYS_Y, td.state == TD_ERROR ? "A:RETRY  B:BACK" : "B:BACK", PAL_DIM);
    } else if (emode == M_EDIT) {
        line(KEYS_Y, "A:SAVE  B:CANCEL", PAL_DIM);
    } else if (emode == M_EXIT) {
        line(KEYS_Y, "A:BURN  B:DISCARD", PAL_DIM);
    } else if (emode == M_LEAVE) {
        line(KEYS_Y, "WAIT", PAL_DIM);
    } else {
        line(KEYS_Y, td.unsure ? "A:RELOAD  B:BACK" : "A:EDIT  B:BACK", PAL_DIM);
    }
}

static void redraw(void) {
    if (td.state == TD_READY) {
        if (is_list()) {
            draw_list();
        } else {
            draw_grid();
        }
    }
    draw_status();
}

void tedit_enter(void) {
    if (emode != M_LEAVE) {
        emode = M_NAV;
    }
    live_r = NONE;
    live_c = NONE;
    ui_clear();
    hdr_draw(titles[td.id]);
    invalidate();
    shown_state = NONE;
    td.changed = 1;
}

static void leave_done(void) {
    if (td_busy() || td.discarding) {
        return;
    }
    if (td.err != TUNE_OK) {
        emode = M_NAV;
        td.changed = 1;
        return;
    }
    emode = M_NAV;
    screen_go(SCR_TUNE);
}

void tedit_tick(void) {
    uint8_t moved;
    hdr_tick();
    if (emode == M_LEAVE) {
        leave_done();
        if (screen_current() != SCR_TEDIT) {
            return;
        }
    }
    if (td.state != shown_state) {
        shown_state = td.state;
        if (td.state == TD_READY) {
            ui_clear();
            hdr_draw(titles[td.id]);
            invalidate();
        }
        td.changed = 1;
    }
    moved = td.state == TD_READY && update_live();
    if (td.changed || moved) {
        td.changed = 0;
        redraw();
    }
}

static void move(uint8_t pressed) {
    if ((pressed & J_UP) && cur_r > 0u) {
        cur_r--;
    } else if ((pressed & J_DOWN) && cur_r + 1u < rows()) {
        cur_r++;
    } else if ((pressed & J_LEFT) && cur_c > 0u) {
        cur_c--;
    } else if ((pressed & J_RIGHT) && cur_c + 1u < cols()) {
        cur_c++;
    } else {
        return;
    }
    if (cur_c < view_c) {
        view_c = cur_c;
    } else if (cur_c >= view_c + VIS_COLS) {
        view_c = (uint8_t)(cur_c - VIS_COLS + 1u);
    }
    if (cur_r < view_r) {
        view_r = cur_r;
    } else if (cur_r >= view_r + VIS_ROWS) {
        view_r = (uint8_t)(cur_r - VIS_ROWS + 1u);
    }
    td.changed = 1;
}

static void edit(uint8_t pressed) {
    uint16_t cell = cell_at(cur_r, cur_c);
    int16_t st = td.t->step;
    if (pressed & J_UP) {
        edit_val = edit_val > td.t->max - st ? td.t->max : (int16_t)(edit_val + st);
    } else if (pressed & J_DOWN) {
        edit_val = edit_val < td.t->min + st ? td.t->min : (int16_t)(edit_val - st);
    } else if (pressed & J_A) {
        if (edit_val != td_value(cell) && !td_set(cell, edit_val)) {
            return;
        }
        emode = M_NAV;
    } else if (pressed & J_B) {
        emode = M_NAV;
    } else {
        return;
    }
    td.changed = 1;
}

void tedit_input(uint8_t pressed) {
    if (td.state != TD_READY) {
        if ((pressed & J_A) && td.state == TD_ERROR) {
            td_resync();
        } else if (pressed & J_B) {
            screen_go(SCR_TUNE);
        }
        return;
    }
    if (cur_r >= rows()) {
        cur_r = 0;
        view_r = 0;
    }
    if (cur_c >= cols()) {
        cur_c = 0;
        view_c = 0;
    }
    switch (emode) {
    case M_EDIT:
        edit(pressed);
        break;
    case M_EXIT:
        if (pressed & J_A) {
            td_burn();
            emode = M_LEAVE;
        } else if (pressed & J_B) {
            td_discard();
            emode = M_LEAVE;
        } else if (pressed & J_START) {
            emode = M_NAV;
        }
        td.changed = 1;
        break;
    case M_LEAVE:
        break;
    default:
        if ((pressed & J_A) && td.unsure && !td_busy()) {
            td_resync();
        } else if ((pressed & J_B) && td.err == TUNE_E_ECU && !td_busy()) {
            screen_go(SCR_TUNE);
        } else if ((pressed & J_A) && !td_busy()) {
            emode = M_EDIT;
            edit_val = td_value(cell_at(cur_r, cur_c));
            td.err = TUNE_OK;
            td.changed = 1;
        } else if (pressed & J_B) {
            if (td_dirty() || td_busy()) {
                emode = M_EXIT;
                td.changed = 1;
            } else {
                screen_go(SCR_TUNE);
            }
        } else {
            move(pressed);
        }
        break;
    }
}
