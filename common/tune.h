#pragma once

#include <stdint.h>

#define TUNE_VE1 0u
#define TUNE_ADV1 1u
#define TUNE_AFR1 2u
#define TUNE_LIMITS 3u
#define TUNE_BCT 4u
#define TUNE_BCD 5u
#define TUNE_COUNT 6u

#define TUNE_ECU_MS1 1u
#define TUNE_ECU_MS2 2u

#define TUNE_CELLS_MAX 256u
#define TUNE_BLOB_MAX (TUNE_CELLS_MAX + 32u)

#define TUNE_U08 0u
#define TUNE_S16 1u
#define TUNE_U16 2u

#define TUNE_F_INT 0u
#define TUNE_F_X10 1u
#define TUNE_F_X100 2u
#define TUNE_F_MS1_ADV 3u
#define TUNE_F_MS1_AFR 4u
#define TUNE_F_MS1_DUTY 5u

#define TUNE_A_RAW 0u
#define TUNE_A_X100 1u
#define TUNE_A_DIV10 2u
#define TUNE_A_MS1_TPS 3u

#define TUNE_OK 0u
#define TUNE_E_TABLE 1u
#define TUNE_E_CELL 2u
#define TUNE_E_RANGE 3u
#define TUNE_E_DELTA 4u
#define TUNE_E_ECU 5u
#define TUNE_E_VERIFY 6u

typedef struct {
    uint8_t page;
    uint16_t off;
} tune_item_t;

typedef struct {
    uint8_t page;
    uint8_t rows;
    uint8_t cols;
    uint8_t type;
    uint8_t xtype;
    uint8_t ytype;
    uint16_t val_off;
    uint16_t x_off;
    uint16_t y_off;
    int16_t min;
    int16_t max;
    int16_t delta;
    int16_t step;
    uint8_t fmt;
    uint8_t xaxis;
    uint8_t yaxis;
    uint8_t y_tps;
    const tune_item_t *items;
} tune_table_t;

const tune_table_t *tune_get(uint8_t ecu, uint8_t id);
uint16_t tune_cells(const tune_table_t *t);
uint16_t tune_blob_len(const tune_table_t *t);
uint8_t tune_size(uint8_t type);
void tune_loc(const tune_table_t *t, uint16_t i, uint8_t *page, uint16_t *off, uint8_t *type);
int16_t tune_decode(uint8_t type, const uint8_t *p);
void tune_encode(uint8_t type, int16_t v, uint8_t *p);
int16_t tune_axis(uint8_t axis, int16_t raw);
uint8_t tune_check_write(const tune_table_t *t, uint16_t cell, int16_t cur, int16_t val);
