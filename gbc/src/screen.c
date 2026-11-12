#include "screen.h"

#include "dash.h"

static const screen_t screens[SCR_COUNT] = {
    {dash_enter, dash_tick, dash_input},
};

static uint8_t current;

void screen_go(uint8_t id) {
    current = id;
    screens[current].enter();
    screens[current].tick();
}

uint8_t screen_current(void) {
    return current;
}

void screen_tick(void) {
    screens[current].tick();
}

void screen_input(uint8_t pressed) {
    if (pressed) {
        screens[current].input(pressed);
    }
}
