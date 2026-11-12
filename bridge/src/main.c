#include <stdio.h>

#include "hardware/uart.h"
#include "pico/stdlib.h"

#include "ms1.h"
#include "ms2.h"

#define MS2_UART uart0
#define MS2_TX_PIN 0
#define MS2_RX_PIN 1
#define MS2_BAUD 115200
#define ECU_SEL_PIN 8
#define POLL_MS 50
#define MS1_POLL_MS 100
#define RESP_TIMEOUT_US 100000
#define MS1_TIMEOUT_US 200000

static ms2_parser_t parser;
static const ms1_cfg_t ms1_cfg = {MS1_MAP_4250, 4, false};
static bool is_ms1;

static void flush_rx(void) {
    while (uart_is_readable(MS2_UART)) {
        uart_getc(MS2_UART);
    }
}

static bool ms1_cmd(char cmd, uint8_t *out, size_t n) {
    flush_rx();
    uart_putc_raw(MS2_UART, cmd);
    absolute_time_t deadline = make_timeout_time_us(MS1_TIMEOUT_US);
    size_t got = 0;
    while (got < n && !time_reached(deadline)) {
        if (uart_is_readable(MS2_UART)) {
            out[got++] = (uint8_t)uart_getc(MS2_UART);
        }
    }
    if (got != n) {
        printf("error: timeout, got %u of %u\n", (unsigned)got, (unsigned)n);
        return false;
    }
    return true;
}

static bool transact(const uint8_t *req, size_t n, const uint8_t **data, size_t *len) {
    flush_rx();
    ms2_parser_reset(&parser);
    uart_write_blocking(MS2_UART, req, n);

    absolute_time_t deadline = make_timeout_time_us(RESP_TIMEOUT_US);
    while (!time_reached(deadline)) {
        if (!uart_is_readable(MS2_UART)) {
            continue;
        }
        const uint8_t *pl;
        size_t pll;
        switch (ms2_parser_feed(&parser, (uint8_t)uart_getc(MS2_UART), &pl, &pll)) {
        case MS2_P_NEED:
            break;
        case MS2_P_OK:
            if (!ms2_resp_ok(pl, pll)) {
                printf("error: ecu 0x%02x\n", pl[0]);
                return false;
            }
            *data = pl + 1;
            *len = pll - 1;
            return true;
        case MS2_P_ERR_LEN:
            printf("error: bad length\n");
            return false;
        case MS2_P_ERR_CRC:
            printf("error: bad crc\n");
            return false;
        }
    }
    printf("error: timeout\n");
    return false;
}

static bool ms1_signature(void) {
    uint8_t sig[MS1_SIG_LEN];
    if (!ms1_cmd(MS1_CMD_SIG, sig, sizeof sig)) {
        return false;
    }
    if (!ms1_check_sig(sig, sizeof sig)) {
        printf("error: unexpected signature %.*s\n", (int)sizeof sig, sig);
        return false;
    }
    printf("signature: %.*s\n", (int)sizeof sig, sig);
    return true;
}

static bool ms2_signature(void) {
    uint8_t req[MS2_FRAME_MAX];
    size_t n = ms2_req_read(req, MS2_TABLE_SIG, 0, MS2_SIG_LEN);
    const uint8_t *d;
    size_t l;
    if (!transact(req, n, &d, &l)) {
        return false;
    }
    if (!ms2_check_sig(d, l)) {
        printf("error: unexpected signature %.*s\n", (int)l, d);
        return false;
    }
    printf("signature: %s\n", MS2_SIG);
    return true;
}

static bool wait_signature(void) {
    return is_ms1 ? ms1_signature() : ms2_signature();
}

static bool poll_rt(ecu_rt_t *rt) {
    if (is_ms1) {
        uint8_t d[MS1_RT_LEN];
        return ms1_cmd(MS1_CMD_RT, d, sizeof d) && ms1_decode_rt(d, sizeof d, &ms1_cfg, rt);
    }
    uint8_t req[MS2_FRAME_MAX];
    size_t n = ms2_req_read(req, MS2_TABLE_RT, 0, MS2_RT_LEN);
    const uint8_t *d;
    size_t l;
    return transact(req, n, &d, &l) && ms2_decode_rt(d, l, rt);
}

static void print_rt(const ecu_rt_t *r) {
    printf("rpm %5u  boost %4d.%d psi  adv %3d.%d  clt %4d.%d C  iat %4d.%d C  afr %2d.%d\n",
           r->rpm,
           r->boost_psi_x10 / 10, (r->boost_psi_x10 < 0 ? -r->boost_psi_x10 : r->boost_psi_x10) % 10,
           r->adv_x10 / 10, (r->adv_x10 < 0 ? -r->adv_x10 : r->adv_x10) % 10,
           r->clt_c_x10 / 10, (r->clt_c_x10 < 0 ? -r->clt_c_x10 : r->clt_c_x10) % 10,
           r->iat_c_x10 / 10, (r->iat_c_x10 < 0 ? -r->iat_c_x10 : r->iat_c_x10) % 10,
           r->afr_x10 / 10, r->afr_x10 % 10);
}

int main(void) {
    stdio_init_all();
    gpio_init(ECU_SEL_PIN);
    gpio_set_dir(ECU_SEL_PIN, GPIO_IN);
    gpio_pull_up(ECU_SEL_PIN);
    sleep_ms(10);
    is_ms1 = !gpio_get(ECU_SEL_PIN);

    uart_init(MS2_UART, is_ms1 ? MS1_BAUD : MS2_BAUD);
    gpio_set_function(MS2_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(MS2_RX_PIN, GPIO_FUNC_UART);
    printf("ecu: %s\n", is_ms1 ? "ms1/extra" : "ms2/extra");

    while (!wait_signature()) {
        sleep_ms(1000);
    }

    uint32_t poll_ms = is_ms1 ? MS1_POLL_MS : POLL_MS;
    int errors = 0;
    while (true) {
        absolute_time_t next = make_timeout_time_ms(poll_ms);
        ecu_rt_t rt;
        if (poll_rt(&rt)) {
            errors = 0;
            print_rt(&rt);
        } else if (++errors >= 10) {
            while (!wait_signature()) {
                sleep_ms(1000);
            }
            errors = 0;
        }
        sleep_until(next);
    }
}
