#pragma once

#include <stdint.h>

#define TUNE_VE1 0u
#define TUNE_ADV1 1u
#define TUNE_AFR1 2u
#define TUNE_COUNT 3u

#define TUNE_MAX_DELTA 10u

#define TUNE_OK 0u
#define TUNE_E_TABLE 1u
#define TUNE_E_CELL 2u
#define TUNE_E_RANGE 3u
#define TUNE_E_DELTA 4u
#define TUNE_E_ECU 5u
#define TUNE_E_VERIFY 6u

typedef struct {
    uint8_t page_cmd;
    uint8_t rows;
    uint8_t cols;
    uint8_t val_off;
    uint8_t rpm_off;
    uint8_t map_off;
    uint8_t min;
    uint8_t max;
} tune_table_t;

extern const tune_table_t tune_tables[TUNE_COUNT];

const tune_table_t *tune_get(uint8_t id);
uint8_t tune_cells(const tune_table_t *t);
uint8_t tune_blob_len(const tune_table_t *t);
uint8_t tune_blob_offset(const tune_table_t *t, uint8_t i);
uint8_t tune_check_write(uint8_t id, uint8_t cell, uint8_t cur, uint8_t val);
