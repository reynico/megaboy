#pragma once

#include <stdint.h>

typedef struct {
    void (*enter)(void);
    void (*tick)(void);
    void (*input)(uint8_t pressed);
} screen_t;

enum {
    SCR_DASH,
    SCR_BOOST,
    SCR_LOG,
    SCR_MENU,
    SCR_SETTINGS,
    SCR_WARN,
    SCR_TUNE,
    SCR_ABOUT,
    SCR_TEDIT,
    SCR_COUNT
};

void screen_go(uint8_t id);
void screen_back(void);
void screen_return(void);
uint8_t screen_current(void);
void screen_tick(void);
void screen_input(uint8_t pressed);
