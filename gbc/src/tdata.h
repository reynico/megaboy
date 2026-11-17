#pragma once

#include <stdint.h>

#include "tune.h"

#define TD_IDLE 0u
#define TD_LOADING 1u
#define TD_READY 2u
#define TD_ERROR 3u

#define TD_NO_CELL 0xffffu

typedef struct {
    uint8_t id;
    const tune_table_t *t;
    uint8_t state;
    uint8_t err;
    int16_t blob[TUNE_BLOB_MAX];
    int16_t orig[TUNE_CELLS_MAX];
    uint16_t tgt_cell;
    int16_t tgt_val;
    uint8_t discarding;
    uint8_t burned;
    uint8_t changed;
} tdata_t;

extern tdata_t td;

void td_load(uint8_t ecu, uint8_t id);
uint8_t td_busy(void);
uint8_t td_dirty(void);
int16_t td_value(uint16_t cell);
uint8_t td_set(uint16_t cell, int16_t val);
void td_burn(void);
void td_discard(void);
void td_on_table(const uint8_t *p, uint8_t len);
void td_on_ack(const uint8_t *p);
void td_tick(void);
