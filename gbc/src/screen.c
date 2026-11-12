#include "screen.h"

#include <gb/gb.h>

#include "app.h"
#include "boost.h"
#include "dash.h"
#include "logpage.h"
#include "menu.h"
#include "settings.h"

static const screen_t screens[SCR_COUNT] = {
    {dash_enter, dash_tick, dash_input},
    {boost_enter, boost_tick, boost_input},
    {logpage_enter, logpage_tick, logpage_input},
    {menu_enter, menu_tick, menu_input},
    {settings_enter, settings_tick, settings_input},
};

static const uint8_t pages[] = {SCR_DASH, SCR_BOOST, SCR_LOG};
#define PAGE_COUNT (sizeof pages / sizeof pages[0])

static uint8_t current;
static uint8_t page;

static uint8_t is_page(void) {
    return current == pages[page];
}

void screen_go(uint8_t id) {
    uint8_t i;
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

void screen_tick(void) {
    screens[current].tick();
}

static uint8_t page_input(uint8_t pressed) {
    if (pressed & J_RIGHT) {
        screen_go(pages[(page + 1u) % PAGE_COUNT]);
        return 1;
    }
    if (pressed & J_LEFT) {
        screen_go(pages[(page + PAGE_COUNT - 1u) % PAGE_COUNT]);
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
