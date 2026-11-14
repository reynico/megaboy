#include <stdio.h>
#include <string.h>

#include "hardware/flash.h"
#include "hardware/i2c.h"
#include "hardware/sync.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"

#include "link.h"
#include "ms1.h"
#include "ms2.h"
#include "rtc.h"
#include "tune.h"

#define ECU_UART uart0
#define ECU_TX_PIN 0
#define ECU_RX_PIN 1
#define MS2_BAUD 115200
#define GB_SCK 26
#define GB_OUT 27
#define GB_IN 28
#define RTC_I2C i2c1
#define RTC_SDA 6
#define RTC_SCL 7
#define RTC_ADDR 0x68
#define RTC_HZ 100000
#define RTC_TIMEOUT_US 5000
#define RTC_REG_STATUS 0x0f
#define RTC_OSF 0x80

#define GB_HALF_US 15
#define GB_GAP_US 150
#define MS1_POLL_MS 40
#define MS2_POLL_MS 50
#define PRINT_MS 500
#define STATUS_MS 1000
#define RETRY_MS 1000
#define CLOCK_PRINT_S 10
#define MS1_TIMEOUT_US 400000
#define MS2_TIMEOUT_US 100000
#define MAX_ERRORS 10
#define BURN_WAIT_MS 200
#define NO_PAGE 0xff

#define CFG_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define CFG_MAGIC0 'M'
#define CFG_MAGIC1 'B'

static uint8_t ecu_type;
static bool ecu_ok;
static uint8_t errors;
static uint32_t last_poll;
static uint32_t last_status;
static uint32_t last_try;
static uint32_t last_print;
static ms1_cfg_t ms1_cfg;
static ms2_parser_t ms2p;
static link_parser_t up;
static uint8_t cmd_type;
static uint8_t cmd_len;
static uint8_t cmd[LINK_TIME_SET_LEN];
static bool cmd_pending;
static uint8_t page_buf[MS1_PAGE_LEN];
static uint8_t page_cached = NO_PAGE;
static uint8_t clk_state;
static rtc_time_t clk;

static uint32_t now_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

static uint8_t cfg_load(void) {
    const uint8_t *c = (const uint8_t *)(XIP_BASE + CFG_OFFSET);
    if (c[0] != CFG_MAGIC0 || c[1] != CFG_MAGIC1 || (uint8_t)~c[2] != c[3]) {
        return LINK_ECU_NONE;
    }
    return c[2] == LINK_ECU_MS1 || c[2] == LINK_ECU_MS2 ? c[2] : LINK_ECU_NONE;
}

static void cfg_save(uint8_t type) {
    uint8_t page[FLASH_PAGE_SIZE];
    memset(page, 0xff, sizeof page);
    page[0] = CFG_MAGIC0;
    page[1] = CFG_MAGIC1;
    page[2] = type;
    page[3] = (uint8_t)~type;
    uint32_t irq = save_and_disable_interrupts();
    flash_range_erase(CFG_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(CFG_OFFSET, page, sizeof page);
    restore_interrupts(irq);
}

static void ecu_open(void) {
    ecu_ok = false;
    page_cached = NO_PAGE;
    if (ecu_type == LINK_ECU_NONE) {
        return;
    }
    uart_deinit(ECU_UART);
    uart_init(ECU_UART, ecu_type == LINK_ECU_MS1 ? MS1_BAUD : MS2_BAUD);
    gpio_set_function(ECU_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(ECU_RX_PIN, GPIO_FUNC_UART);
    printf("ecu: %s\n", ecu_type == LINK_ECU_MS1 ? "ms1/extra" : "ms2/extra");
}

static uint8_t gb_xfer(uint8_t out) {
    uint8_t in = 0;
    for (int i = 0; i < 8; i++) {
        gpio_put(GB_SCK, 0);
        gpio_put(GB_OUT, out & 0x80);
        out <<= 1;
        busy_wait_us_32(GB_HALF_US);
        gpio_put(GB_SCK, 1);
        in = (uint8_t)(in << 1) | (gpio_get(GB_IN) ? 1 : 0);
        busy_wait_us_32(GB_HALF_US);
    }
    busy_wait_us_32(GB_GAP_US);
    return in;
}

static void gb_send(uint8_t type, const uint8_t *payload, uint8_t len) {
    uint8_t fr[LINK_FRAME_MAX];
    uint8_t n = link_encode(fr, type, payload, len);
    for (uint8_t i = 0; i < n; i++) {
        uint8_t b = gb_xfer(fr[i]);
        if (link_feed(&up, b) && !cmd_pending && up.len <= sizeof cmd) {
            cmd_type = up.type;
            cmd_len = up.len;
            memcpy(cmd, up.buf, up.len);
            cmd_pending = true;
        }
    }
}

static void send_status(void) {
    link_status_t st = {ecu_ok ? LINK_ST_OK : LINK_ST_WAIT, ecu_type};
    uint8_t pl[LINK_STATUS_LEN];
    gb_send(LINK_T_STATUS, pl, link_put_status(pl, &st));
}

static bool rtc_read(uint8_t reg, uint8_t *out, size_t n) {
    return i2c_write_timeout_us(RTC_I2C, RTC_ADDR, &reg, 1, true, RTC_TIMEOUT_US) == 1 &&
           i2c_read_timeout_us(RTC_I2C, RTC_ADDR, out, n, false, RTC_TIMEOUT_US) == (int)n;
}

static bool rtc_write(const uint8_t *buf, size_t n) {
    return i2c_write_timeout_us(RTC_I2C, RTC_ADDR, buf, n, false, RTC_TIMEOUT_US) == (int)n;
}

static void rtc_poll(void) {
    uint8_t regs[RTC_REGS];
    uint8_t st;
    if (!rtc_read(0, regs, sizeof regs) || !rtc_read(RTC_REG_STATUS, &st, 1)) {
        clk_state = LINK_CLK_NONE;
        return;
    }
    clk_state = (st & RTC_OSF) || !rtc_decode(regs, &clk) ? LINK_CLK_UNSET : LINK_CLK_OK;
}

static void print_clock(void) {
    if (clk_state == LINK_CLK_OK) {
        printf("rtc: 20%02u-%02u-%02u %02u:%02u:%02u\n", clk.yy, clk.mo, clk.dd, clk.hh, clk.mi, clk.ss);
    } else {
        printf("rtc: %s\n", clk_state == LINK_CLK_NONE ? "not found" : "not set");
    }
}

static void send_time(void) {
    uint8_t pl[LINK_TIME_LEN];
    pl[0] = clk_state;
    link_put_time(pl + 1, &clk);
    gb_send(LINK_T_TIME, pl, LINK_TIME_LEN);
}

static void do_time(const uint8_t *in) {
    rtc_time_t t;
    uint8_t buf[1 + RTC_REGS];
    uint8_t st[2] = {RTC_REG_STATUS, 0};
    link_get_time(in, &t);
    if (!rtc_valid(&t)) {
        printf("rtc: bad time\n");
        return;
    }
    buf[0] = 0;
    rtc_encode(&t, buf + 1);
    if (!rtc_write(buf, sizeof buf) || !rtc_read(RTC_REG_STATUS, &st[1], 1)) {
        printf("rtc: write failed\n");
        return;
    }
    st[1] &= (uint8_t)~RTC_OSF;
    rtc_write(st, sizeof st);
    rtc_poll();
    print_clock();
}

static void send_rt(const ecu_rt_t *r) {
    link_rt_t l = {
        .rpm = r->rpm,
        .adv_x10 = r->adv_x10,
        .map_x10 = r->map_x10,
        .iat_c_x10 = r->iat_c_x10,
        .clt_c_x10 = r->clt_c_x10,
        .tps_x10 = r->tps_x10,
        .batt_x10 = r->batt_x10,
        .afr_x10 = r->afr_x10,
        .boost_psi_x10 = r->boost_psi_x10,
        .engine = r->engine,
        .flags = 0,
    };
    uint8_t pl[LINK_RT_LEN];
    gb_send(LINK_T_RT, pl, link_put_rt(pl, &l));
}

static void flush_rx(void) {
    while (uart_is_readable(ECU_UART)) {
        uart_getc(ECU_UART);
    }
}

static bool ms1_cmd(const uint8_t *c, size_t clen, uint8_t *out, size_t n) {
    flush_rx();
    uart_write_blocking(ECU_UART, c, clen);
    absolute_time_t deadline = make_timeout_time_us(MS1_TIMEOUT_US);
    size_t got = 0;
    while (got < n && !time_reached(deadline)) {
        if (uart_is_readable(ECU_UART)) {
            out[got++] = (uint8_t)uart_getc(ECU_UART);
        }
    }
    if (got != n) {
        printf("error: timeout, got %u of %u\n", (unsigned)got, (unsigned)n);
        return false;
    }
    return true;
}

static bool ms2_transact(const uint8_t *req, size_t n, const uint8_t **data, size_t *len) {
    flush_rx();
    ms2_parser_reset(&ms2p);
    uart_write_blocking(ECU_UART, req, n);
    absolute_time_t deadline = make_timeout_time_us(MS2_TIMEOUT_US);
    while (!time_reached(deadline)) {
        if (!uart_is_readable(ECU_UART)) {
            continue;
        }
        const uint8_t *pl;
        size_t pll;
        switch (ms2_parser_feed(&ms2p, (uint8_t)uart_getc(ECU_UART), &pl, &pll)) {
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

static bool ms1_connect(void) {
    static const uint8_t c_sig[] = {MS1_CMD_SIG};
    static const uint8_t c_page[] = {MS1_CMD_PAGE, MS1_CONFIG_PAGE, MS1_CMD_READ};
    uint8_t *buf = page_buf;
    page_cached = NO_PAGE;
    if (!ms1_cmd(c_sig, sizeof c_sig, buf, MS1_SIG_LEN)) {
        return false;
    }
    if (!ms1_check_sig(buf, MS1_SIG_LEN)) {
        printf("error: unexpected signature %.*s\n", MS1_SIG_LEN, buf);
        return false;
    }
    printf("signature: %.*s\n", MS1_SIG_LEN, buf);
    if (!ms1_cmd(c_page, sizeof c_page, buf, MS1_PAGE_LEN) || !ms1_parse_config(buf, MS1_PAGE_LEN, &ms1_cfg)) {
        return false;
    }
    page_cached = MS1_CONFIG_PAGE;
    printf("config: %u cyl, %s-stroke, map %u kPa\n", ms1_cfg.cyl, ms1_cfg.two_stroke ? "two" : "four",
           ms1_map_kpa(ms1_cfg.map));
    return true;
}

static bool ms2_connect(void) {
    uint8_t req[MS2_FRAME_MAX];
    size_t n = ms2_req_read(req, MS2_TABLE_SIG, 0, MS2_SIG_LEN);
    const uint8_t *d;
    size_t l;
    if (!ms2_transact(req, n, &d, &l)) {
        return false;
    }
    if (!ms2_check_sig(d, l)) {
        printf("error: unexpected signature %.*s\n", (int)l, d);
        return false;
    }
    printf("signature: %s\n", MS2_SIG);
    return true;
}

static bool poll_rt(ecu_rt_t *rt) {
    if (ecu_type == LINK_ECU_MS1) {
        static const uint8_t c_rt[] = {MS1_CMD_RT};
        uint8_t d[MS1_RT_LEN];
        return ms1_cmd(c_rt, sizeof c_rt, d, sizeof d) && ms1_decode_rt(d, sizeof d, &ms1_cfg, rt);
    }
    uint8_t req[MS2_FRAME_MAX];
    size_t n = ms2_req_read(req, MS2_TABLE_RT, 0, MS2_RT_LEN);
    const uint8_t *d;
    size_t l;
    return ms2_transact(req, n, &d, &l) && ms2_decode_rt(d, l, rt);
}

static int abs10(int v) {
    return (v < 0 ? -v : v) % 10;
}

static void print_rt(const ecu_rt_t *r) {
    printf("rpm %5u  boost %4d.%d psi  adv %3d.%d  clt %4d.%d C  iat %4d.%d C  afr %2d.%d\n", r->rpm,
           r->boost_psi_x10 / 10, abs10(r->boost_psi_x10), r->adv_x10 / 10, abs10(r->adv_x10),
           r->clt_c_x10 / 10, abs10(r->clt_c_x10), r->iat_c_x10 / 10, abs10(r->iat_c_x10), r->afr_x10 / 10,
           abs10(r->afr_x10));
}

static void send_ack(uint8_t c, uint8_t id, uint8_t cell, uint8_t st, uint8_t val) {
    const uint8_t pl[LINK_ACK_LEN] = {c, id, cell, st, val};
    gb_send(LINK_T_ACK, pl, LINK_ACK_LEN);
}

static bool read_page(const tune_table_t *t) {
    const uint8_t c[] = {MS1_CMD_PAGE, t->page_cmd, MS1_CMD_READ};
    page_cached = NO_PAGE;
    if (!ms1_cmd(c, sizeof c, page_buf, MS1_PAGE_LEN)) {
        return false;
    }
    page_cached = t->page_cmd;
    return true;
}

static void do_read(uint8_t id) {
    const tune_table_t *t = tune_get(id);
    if (!t) {
        send_ack(LINK_C_READ, id, 0, TUNE_E_TABLE, 0);
        return;
    }
    if (!read_page(t)) {
        send_ack(LINK_C_READ, id, 0, TUNE_E_ECU, 0);
        return;
    }
    uint8_t len = tune_blob_len(t);
    uint8_t pl[2 + LINK_TABLE_CHUNK];
    for (uint8_t off = 0; off < len; off = (uint8_t)(off + LINK_TABLE_CHUNK)) {
        uint8_t n = (uint8_t)(len - off);
        if (n > LINK_TABLE_CHUNK) {
            n = LINK_TABLE_CHUNK;
        }
        pl[0] = id;
        pl[1] = off;
        for (uint8_t i = 0; i < n; i++) {
            pl[2 + i] = page_buf[tune_blob_offset(t, (uint8_t)(off + i))];
        }
        gb_send(LINK_T_TABLE, pl, (uint8_t)(2 + n));
    }
    send_ack(LINK_C_READ, id, 0, TUNE_OK, 0);
}

static void do_write(uint8_t id, uint8_t cell, uint8_t val) {
    const tune_table_t *t = tune_get(id);
    if (!t) {
        send_ack(LINK_C_WRITE, id, cell, TUNE_E_TABLE, 0);
        return;
    }
    if (page_cached != t->page_cmd && !read_page(t)) {
        send_ack(LINK_C_WRITE, id, cell, TUNE_E_ECU, 0);
        return;
    }
    uint8_t st = tune_check_write(id, cell, 0, 0);
    if (st == TUNE_E_TABLE || st == TUNE_E_CELL) {
        send_ack(LINK_C_WRITE, id, cell, st, 0);
        return;
    }
    uint8_t off = tune_cell_offset(t, cell);
    uint8_t cur = page_buf[off];
    st = tune_check_write(id, cell, cur, val);
    if (st != TUNE_OK) {
        send_ack(LINK_C_WRITE, id, cell, st, cur);
        return;
    }
    const uint8_t w[] = {MS1_CMD_PAGE, t->page_cmd, 'W', off, val};
    uart_write_blocking(ECU_UART, w, sizeof w);
    if (!read_page(t)) {
        send_ack(LINK_C_WRITE, id, cell, TUNE_E_ECU, cur);
        return;
    }
    printf("write: table %u cell %u %u -> %u\n", id, cell, cur, page_buf[off]);
    send_ack(LINK_C_WRITE, id, cell, page_buf[off] == val ? TUNE_OK : TUNE_E_VERIFY, page_buf[off]);
}

static void do_burn(uint8_t id) {
    const tune_table_t *t = tune_get(id);
    if (!t) {
        send_ack(LINK_C_BURN, id, 0, TUNE_E_TABLE, 0);
        return;
    }
    const uint8_t b[] = {MS1_CMD_PAGE, t->page_cmd, 'B'};
    uart_write_blocking(ECU_UART, b, sizeof b);
    sleep_ms(BURN_WAIT_MS);
    page_cached = NO_PAGE;
    printf("burn: table %u\n", id);
    send_ack(LINK_C_BURN, id, 0, TUNE_OK, 0);
}

static void do_ecu(uint8_t type) {
    if ((type != LINK_ECU_MS1 && type != LINK_ECU_MS2) || type == ecu_type) {
        return;
    }
    cfg_save(type);
    ecu_type = type;
    printf("ecu type saved\n");
    ecu_open();
    last_try = now_ms() - RETRY_MS;
}

static void handle_cmd(void) {
    uint8_t type = cmd_type;
    uint8_t len = cmd_len;
    cmd_pending = false;
    if (type == LINK_C_ECU && len == 1) {
        do_ecu(cmd[0]);
        return;
    }
    if (type == LINK_C_TIME && len == LINK_TIME_SET_LEN) {
        do_time(cmd);
        return;
    }
    if (!ecu_ok || ecu_type != LINK_ECU_MS1) {
        send_ack(type, len ? cmd[0] : 0, len > 1 ? cmd[1] : 0, TUNE_E_ECU, 0);
        return;
    }
    if (type == LINK_C_READ && len == 1) {
        do_read(cmd[0]);
    } else if (type == LINK_C_WRITE && len == 3) {
        do_write(cmd[0], cmd[1], cmd[2]);
    } else if (type == LINK_C_BURN && len == 1) {
        do_burn(cmd[0]);
    }
}

static void gb_init(void) {
    gpio_init(GB_SCK);
    gpio_set_dir(GB_SCK, GPIO_OUT);
    gpio_put(GB_SCK, 1);
    gpio_init(GB_OUT);
    gpio_set_dir(GB_OUT, GPIO_OUT);
    gpio_put(GB_OUT, 1);
    gpio_init(GB_IN);
    gpio_set_dir(GB_IN, GPIO_IN);
    gpio_pull_up(GB_IN);
    link_parser_reset(&up);
}

static void rtc_init(void) {
    i2c_init(RTC_I2C, RTC_HZ);
    gpio_set_function(RTC_SDA, GPIO_FUNC_I2C);
    gpio_set_function(RTC_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(RTC_SDA);
    gpio_pull_up(RTC_SCL);
}

int main(void) {
    stdio_init_all();
    gb_init();
    rtc_init();
    ecu_type = cfg_load();
    if (ecu_type == LINK_ECU_NONE) {
        printf("ecu: not set, set it on the gbc\n");
    }
    ecu_open();
    uint8_t clock_s = 0;

    while (true) {
        uint32_t now = now_ms();
        ecu_rt_t rt;

        if (ecu_type != LINK_ECU_NONE && !ecu_ok && now - last_try >= RETRY_MS) {
            last_try = now;
            ecu_ok = ecu_type == LINK_ECU_MS1 ? ms1_connect() : ms2_connect();
            errors = 0;
        }

        uint32_t poll_ms = ecu_type == LINK_ECU_MS1 ? MS1_POLL_MS : MS2_POLL_MS;
        if (ecu_ok && now - last_poll >= poll_ms) {
            last_poll = now;
            if (poll_rt(&rt)) {
                errors = 0;
                send_rt(&rt);
                if (now - last_print >= PRINT_MS) {
                    last_print = now;
                    print_rt(&rt);
                }
            } else if (++errors >= MAX_ERRORS) {
                ecu_ok = false;
            }
        }

        if (now - last_status >= STATUS_MS) {
            last_status = now;
            rtc_poll();
            if (++clock_s >= CLOCK_PRINT_S) {
                clock_s = 0;
                print_clock();
            }
            send_status();
            send_time();
        }

        if (cmd_pending) {
            handle_cmd();
        }
    }
}
