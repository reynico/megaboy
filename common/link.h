#pragma once

#include <stdint.h>

#include "rtc.h"

#define LINK_SYNC0 0x4du
#define LINK_SYNC1 0x42u
#define LINK_MAX_PAYLOAD 32u
#define LINK_FRAME_MAX (LINK_MAX_PAYLOAD + 5u)

#define LINK_T_RT 0x01u
#define LINK_T_STATUS 0x02u
#define LINK_T_TABLE 0x03u
#define LINK_T_ACK 0x04u
#define LINK_T_TIME 0x05u
#define LINK_T_LOG 0x06u

#define LINK_C_READ 0x10u
#define LINK_C_WRITE 0x11u
#define LINK_C_BURN 0x12u
#define LINK_C_ECU 0x13u
#define LINK_C_TIME 0x14u
#define LINK_C_LOG 0x15u

#define LINK_TABLE_HDR 3u
#define LINK_TABLE_VALS 14u
#define LINK_ACK_LEN 6u
#define LINK_WRITE_LEN 5u
#define LINK_WF_VERIFY 0x01u

#define LINK_RT_LEN 20u
#define LINK_STATUS_LEN 3u
#define LINK_ECU_LEN 2u
#define LINK_HZ_MIN 10u
#define LINK_HZ_MAX 40u
#define LINK_HZ_DEFAULT 20u
#define LINK_TIME_LEN 7u
#define LINK_TIME_SET_LEN 6u
#define LINK_LOG_LEN 17u

#define LINK_ECU_NONE 0u
#define LINK_ECU_MS1 1u
#define LINK_ECU_MS2 2u

#define LINK_ST_WAIT 0u
#define LINK_ST_OK 1u

#define LINK_CLK_NONE 0u
#define LINK_CLK_UNSET 1u
#define LINK_CLK_OK 2u

#define LINK_LOG_IDLE 0u
#define LINK_LOG_REC 1u
#define LINK_LOG_NOCARD 2u
#define LINK_LOG_FULL 3u
#define LINK_LOG_ERR 4u

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
    uint8_t hz;
} link_status_t;

typedef struct {
    uint8_t state;
    uint16_t num;
    uint16_t secs;
    uint32_t bytes;
    uint8_t hz;
    uint8_t dated;
    rtc_time_t start;
} link_log_t;

typedef struct {
    uint8_t step;
    uint8_t type;
    uint8_t len;
    uint8_t n;
    uint8_t sum;
    uint8_t buf[LINK_MAX_PAYLOAD];
} link_parser_t;

uint8_t link_encode(uint8_t *out, uint8_t type, const uint8_t *payload, uint8_t len);
void link_put_s16(uint8_t *out, int16_t v);
int16_t link_get_s16(const uint8_t *in);
uint8_t link_put_rt(uint8_t *out, const link_rt_t *rt);
void link_get_rt(const uint8_t *in, link_rt_t *rt);
uint8_t link_put_status(uint8_t *out, const link_status_t *st);
void link_get_status(const uint8_t *in, link_status_t *st);
uint8_t link_put_time(uint8_t *out, const rtc_time_t *t);
void link_get_time(const uint8_t *in, rtc_time_t *t);
uint8_t link_put_log(uint8_t *out, const link_log_t *l);
void link_get_log(const uint8_t *in, link_log_t *l);

void link_parser_reset(link_parser_t *p);
uint8_t link_feed(link_parser_t *p, uint8_t b);
