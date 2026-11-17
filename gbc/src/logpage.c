#include "logpage.h"

#include <gb/gb.h>

#include "app.h"
#include "fmt.h"
#include "hdr.h"
#include "link.h"
#include "tiles.h"
#include "ui.h"

void logpage_enter(void) {
    ui_clear();
    hdr_draw("LOG");
    ui_text(1, 3, "FILE", PAL_DIM);
    ui_text(1, 5, "TIME", PAL_DIM);
    ui_text(1, 7, "RATE", PAL_DIM);
    ui_text(1, 9, "SIZE", PAL_DIM);
    ui_text(1, 11, "CARD", PAL_DIM);
    ui_divider(15);
}

static void two(char *s, uint8_t v) {
    s[0] = (char)('0' + v / 10u);
    s[1] = (char)('0' + v % 10u);
}

static void draw_card(void) {
    const char *t;
    uint8_t pal = PAL_AMBER;
    switch (app.log_state) {
    case LINK_LOG_IDLE:
        t = "OK       ";
        pal = PAL_GREEN;
        break;
    case LINK_LOG_REC:
        t = "WRITING  ";
        pal = PAL_GREEN;
        break;
    case LINK_LOG_NOCARD:
        t = "NO CARD  ";
        break;
    case LINK_LOG_FULL:
        t = "FULL     ";
        pal = PAL_RED;
        break;
    default:
        t = "ERROR    ";
        pal = PAL_RED;
        break;
    }
    ui_text(8, 11, t, pal);
}

enum { L_FILE, L_TIME, L_RATE, L_SIZE, L_CARD, L_KEYS };

static void draw_file(void) {
    char s[11];
    if (app.log_dated) {
        two(s, app.log_start.mo);
        s[2] = '-';
        two(s + 3, app.log_start.dd);
        s[5] = ' ';
        two(s + 6, app.log_start.hh);
        s[8] = ':';
        two(s + 9, app.log_start.mi);
    } else {
        s[0] = 'L';
        s[1] = 'O';
        s[2] = 'G';
        s[3] = (char)('0' + (app.log_num / 1000u) % 10u);
        s[4] = (char)('0' + (app.log_num / 100u) % 10u);
        s[5] = (char)('0' + (app.log_num / 10u) % 10u);
        s[6] = (char)('0' + app.log_num % 10u);
        s[7] = '.';
        s[8] = 'M';
        s[9] = 'S';
        s[10] = 'L';
    }
    ui_textn(8, 3, s, 11, app.logging ? PAL_TEXT : PAL_DIM);
}

static void draw_size(uint32_t kb) {
    char s[7];
    uint16_t mb;
    if (kb < 1024u) {
        fmt_int(s, 4, (int16_t)kb, 0);
        s[5] = 'K';
    } else {
        mb = kb >> 10 > 3000u ? 3000u : (uint16_t)(kb >> 10);
        fmt_x10(s, 4, (int16_t)(mb * 10u + (((uint16_t)kb & 1023u) * 10u >> 10)), 0);
        s[5] = 'M';
    }
    s[4] = ' ';
    s[6] = 'B';
    ui_textn(8, 9, s, 7, PAL_TEXT);
}

static void draw_rate(void) {
    char s[6];
    fmt_int(s, 3, app.log_hz, 0);
    s[3] = ' ';
    s[4] = 'H';
    s[5] = 'Z';
    ui_textn(8, 7, s, 6, app.logging ? PAL_TEXT : PAL_DIM);
}

void logpage_tick(void) {
    char s[5];
    uint32_t kb = app.log_bytes >> 10;
    int16_t file = app.log_dated ? (int16_t)(0x4000u | (uint16_t)app.log_start.dd << 11 |
                                                      (uint16_t)app.log_start.hh << 6 | app.log_start.mi)
                                           : (int16_t)app.log_num;

    hdr_tick();
    if (ui_changed(L_FILE, file, (uint8_t)(app.logging | app.log_start.mo << 1))) {
        draw_file();
    }
    if (ui_changed(L_TIME, (int16_t)app.log_s, 0)) {
        fmt_time(s, app.log_s);
        ui_textn(8, 5, s, 5, PAL_TEXT);
    }
    if (ui_changed(L_RATE, app.log_hz, app.logging)) {
        draw_rate();
    }
    if (ui_changed(L_SIZE, (int16_t)(uint16_t)kb, 0)) {
        draw_size(kb);
    }
    if (ui_changed(L_CARD, app.log_state, 0)) {
        draw_card();
    }
    if (ui_changed(L_KEYS, 0, app.logging)) {
        ui_text(1, 16, app.logging ? "A:STOP " : "A:START", PAL_DIM);
    }
}

void logpage_input(uint8_t pressed) {
    if (pressed & J_A) {
        if (app.logging) {
            app_log_stop();
        } else {
            app_log_start();
        }
        logpage_tick();
    }
}
