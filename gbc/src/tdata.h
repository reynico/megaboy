#pragma once

#include <stdint.h>

#include "tune.h"

#define TD_IDLE 0u
#define TD_LOADING 1u
#define TD_READY 2u
#define TD_ERROR 3u

#define TD_BLOB_MAX 168u
#define TD_CELLS_MAX 144u
#define TD_NO_CELL 0xffu

typedef struct {
    uint8_t id;
    const tune_table_t *t;
    uint8_t state;
    uint8_t err;
    uint8_t blob[TD_BLOB_MAX];
    uint8_t orig[TD_CELLS_MAX];
    uint8_t tgt_cell;
    uint8_t tgt_val;
    uint8_t discarding;
    uint8_t burned;
    uint8_t changed;
} tdata_t;

extern tdata_t td;

void td_load(uint8_t id);
uint8_t td_busy(void);
uint8_t td_dirty(void);
uint8_t td_value(uint8_t cell);
uint8_t td_set(uint8_t cell, uint8_t val);
void td_burn(void);
void td_discard(void);
void td_on_table(const uint8_t *p, uint8_t len);
void td_on_ack(const uint8_t *p);
void td_tick(void);
