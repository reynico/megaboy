#include "screen.h"

#include <gb/cgb.h>
#include <gb/gb.h>

#include "about.h"
#include "app.h"
#include "boost.h"
#include "clockset.h"
#include "dash.h"
#include "gauge.h"
#include "logpage.h"
#include "menu.h"
#include "settings.h"
#include "tedit.h"
#include "track.h"
#include "tunemenu.h"
#include "warn.h"

static const screen_t screens[SCR_COUNT] = {
    {dash_enter, dash_tick, dash_input},
    {boost_enter, boost_tick, boost_input},
    {logpage_enter, logpage_tick, logpage_input},
    {menu_enter, menu_tick, menu_input},
    {settings_enter, settings_tick, settings_input},
    {warn_enter, warn_tick, warn_input},
    {tunemenu_enter, tunemenu_tick, tunemenu_input},
    {about_enter, about_tick, about_input},
    {tedit_enter, tedit_tick, tedit_input},
    {clockset_enter, clockset_tick, clockset_input},
    {gauge_enter, gauge_tick, gauge_input},
    {track_enter, track_tick, track_input},
};

static const uint8_t pages[] = {SCR_DASH, SCR_TRACK, SCR_GAUGE, SCR_BOOST, SCR_LOG};
#define PAGE_COUNT (sizeof pages / sizeof pages[0])

static uint8_t current;
static uint8_t page;
static uint8_t before_warn;

static uint8_t is_page(void) {
    return current == pages[page];
}

void screen_go(uint8_t id) {
    uint8_t i;
    HIDE_SPRITES;
    current = id;
    for (i = 0; i < PAGE_COUNT; i++) {
        if (pages[i] == id) {
            page = i;
        }
    }
    screens[current].enter();
    screens[current].tick();
}

void screen_back(void) {
    screen_go(pages[page]);
}

uint8_t screen_current(void) {
    return current;
}

void screen_return(void) {
    screen_go(before_warn);
}

void screen_tick(void) {
    uint8_t w = app_warning();
    if (w != W_NONE && current != SCR_WARN) {
        before_warn = current;
        warn_show(w);
        screen_go(SCR_WARN);
        return;
    }
    screens[current].tick();
}

static uint8_t page_ok(uint8_t i) {
    return pages[i] != SCR_GAUGE || _cpu == CGB_TYPE;
}

static void page_step(uint8_t step) {
    uint8_t i = page;
    do {
        i = (uint8_t)((i + step) % PAGE_COUNT);
    } while (!page_ok(i));
    screen_go(pages[i]);
}

static uint8_t page_input(uint8_t pressed) {
    if (pressed & J_RIGHT) {
        page_step(1u);
        return 1;
    }
    if (pressed & J_LEFT) {
        page_step((uint8_t)(PAGE_COUNT - 1u));
        return 1;
    }
    if (pressed & J_START) {
        screen_go(SCR_MENU);
        return 1;
    }
    if (pressed & J_SELECT) {
        if (app.logging) {
            app_log_stop();
        } else {
            app_log_start();
        }
        screens[current].tick();
        return 1;
    }
    return 0;
}

void screen_input(uint8_t pressed) {
    if (!pressed) {
        return;
    }
    if (is_page() && page_input(pressed)) {
        return;
    }
    screens[current].input(pressed);
}
