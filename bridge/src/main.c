#include <stdio.h>

#include "hardware/uart.h"
#include "pico/stdlib.h"

#include "ms2.h"

#define MS2_UART uart0
#define MS2_TX_PIN 0
#define MS2_RX_PIN 1
#define MS2_BAUD 115200
#define POLL_MS 50
#define RESP_TIMEOUT_US 100000

static ms2_parser_t parser;

static bool transact(const uint8_t *req, size_t n, const uint8_t **data, size_t *len) {
    while (uart_is_readable(MS2_UART)) {
        uart_getc(MS2_UART);
    }
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

static bool wait_signature(void) {
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
    uart_init(MS2_UART, MS2_BAUD);
    gpio_set_function(MS2_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(MS2_RX_PIN, GPIO_FUNC_UART);

    while (!wait_signature()) {
        sleep_ms(1000);
    }

    uint8_t req[MS2_FRAME_MAX];
    size_t n = ms2_req_read(req, MS2_TABLE_RT, 0, MS2_RT_LEN);
    int errors = 0;
    while (true) {
        absolute_time_t next = make_timeout_time_ms(POLL_MS);
        const uint8_t *d;
        size_t l;
        ecu_rt_t rt;
        if (transact(req, n, &d, &l) && ms2_decode_rt(d, l, &rt)) {
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
