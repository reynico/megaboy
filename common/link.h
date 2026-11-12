#pragma once

#include <stdint.h>

#define LINK_SYNC0 0x4du
#define LINK_SYNC1 0x42u
#define LINK_MAX_PAYLOAD 32u
#define LINK_FRAME_MAX (LINK_MAX_PAYLOAD + 5u)

#define LINK_T_RT 0x01u
#define LINK_T_STATUS 0x02u
#define LINK_T_TABLE 0x03u
#define LINK_T_ACK 0x04u

#define LINK_C_READ 0x10u
#define LINK_C_WRITE 0x11u
#define LINK_C_BURN 0x12u

#define LINK_TABLE_CHUNK 28u
#define LINK_ACK_LEN 5u

#define LINK_RT_LEN 20u
#define LINK_STATUS_LEN 2u

#define LINK_ECU_NONE 0u
#define LINK_ECU_MS1 1u
#define LINK_ECU_MS2 2u

#define LINK_ST_WAIT 0u
#define LINK_ST_OK 1u

typedef struct {
    uint16_t rpm;
    int16_t adv_x10;
    int16_t map_x10;
    int16_t iat_c_x10;
    int16_t clt_c_x10;
    int16_t tps_x10;
    int16_t batt_x10;
    int16_t afr_x10;
    int16_t boost_psi_x10;
    uint8_t engine;
    uint8_t flags;
} link_rt_t;

typedef struct {
    uint8_t state;
    uint8_t ecu;
} link_status_t;

typedef struct {
    uint8_t step;
    uint8_t type;
    uint8_t len;
    uint8_t n;
    uint8_t sum;
    uint8_t buf[LINK_MAX_PAYLOAD];
} link_parser_t;

uint8_t link_encode(uint8_t *out, uint8_t type, const uint8_t *payload, uint8_t len);
uint8_t link_put_rt(uint8_t *out, const link_rt_t *rt);
void link_get_rt(const uint8_t *in, link_rt_t *rt);
uint8_t link_put_status(uint8_t *out, const link_status_t *st);
void link_get_status(const uint8_t *in, link_status_t *st);

void link_parser_reset(link_parser_t *p);
uint8_t link_feed(link_parser_t *p, uint8_t b);
