#include <SoftwareSerial.h>

extern "C" {
#include "ecu.h"
#include "link.h"
#include "ms1.h"
#include "tune.h"
}

#define ECU_RX 10
#define ECU_TX 11
#define GB_SCK 2
#define GB_OUT 3
#define GB_IN 4

#define GB_HALF_US 15
#define GB_GAP_US 150
#define POLL_MS 40
#define PRINT_MS 500
#define STATUS_MS 1000
#define RETRY_MS 1000
#define ECU_TIMEOUT_MS 400
#define MAX_ERRORS 10
#define BURN_WAIT_MS 200
#define NO_PAGE 0xff

static SoftwareSerial ecu(ECU_RX, ECU_TX);
static ms1_cfg_t cfg;
static bool ecu_ok;
static uint8_t errors;
static uint32_t last_poll;
static uint32_t last_status;
static uint32_t last_try;
static uint32_t last_print;
static link_parser_t up;
static uint8_t cmd_type;
static uint8_t cmd_len;
static uint8_t cmd[LINK_ACK_LEN];
static bool cmd_pending;
static uint8_t page_buf[MS1_PAGE_LEN];
static uint8_t page_cached = NO_PAGE;

static uint8_t gb_xfer(uint8_t out) {
    uint8_t in = 0;
    for (uint8_t i = 0; i < 8; i++) {
        digitalWrite(GB_SCK, LOW);
        digitalWrite(GB_OUT, (out & 0x80) ? HIGH : LOW);
        out <<= 1;
        delayMicroseconds(GB_HALF_US);
        digitalWrite(GB_SCK, HIGH);
        in = (uint8_t)(in << 1) | (digitalRead(GB_IN) ? 1 : 0);
        delayMicroseconds(GB_HALF_US);
    }
    delayMicroseconds(GB_GAP_US);
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
    link_status_t st;
    uint8_t pl[LINK_STATUS_LEN];
    st.state = ecu_ok ? LINK_ST_OK : LINK_ST_WAIT;
    st.ecu = LINK_ECU_MS1;
    st.hz = LINK_HZ_DEFAULT;
    gb_send(LINK_T_STATUS, pl, link_put_status(pl, &st));
}

static void send_rt(const ecu_rt_t *r, uint8_t flags) {
    link_rt_t l;
    uint8_t pl[LINK_RT_LEN];
    l.rpm = r->rpm;
    l.adv_x10 = r->adv_x10;
    l.map_x10 = r->map_x10;
    l.iat_c_x10 = r->iat_c_x10;
    l.clt_c_x10 = r->clt_c_x10;
    l.tps_x10 = r->tps_x10;
    l.batt_x10 = r->batt_x10;
    l.afr_x10 = r->afr_x10;
    l.boost_psi_x10 = r->boost_psi_x10;
    l.engine = r->engine;
    l.flags = flags;
    gb_send(LINK_T_RT, pl, link_put_rt(pl, &l));
}

static bool ecu_cmd(const uint8_t *cmd, uint8_t clen, uint8_t *out, uint8_t n) {
    while (ecu.available()) {
        ecu.read();
    }
    ecu.write(cmd, clen);
    uint32_t t0 = millis();
    uint8_t got = 0;
    while (got < n && millis() - t0 < ECU_TIMEOUT_MS) {
        if (ecu.available()) {
            out[got++] = (uint8_t)ecu.read();
        }
    }
    if (got != n) {
        Serial.print(F("error: timeout, got "));
        Serial.print(got);
        Serial.print(F(" of "));
        Serial.println(n);
        return false;
    }
    return true;
}

static bool ecu_connect(void) {
    static const uint8_t c_sig[] = {MS1_CMD_SIG};
    static const uint8_t c_page[] = {MS1_CMD_PAGE, MS1_CONFIG_PAGE, MS1_CMD_READ};
    uint8_t *buf = page_buf;
    page_cached = NO_PAGE;

    if (!ecu_cmd(c_sig, sizeof c_sig, buf, MS1_SIG_LEN)) {
        return false;
    }
    if (!ms1_check_sig(buf, MS1_SIG_LEN)) {
        Serial.println(F("error: unexpected signature"));
        return false;
    }
    Serial.print(F("signature: "));
    Serial.write(buf, MS1_SIG_LEN);
    Serial.println();

    if (!ecu_cmd(c_page, sizeof c_page, buf, MS1_PAGE_LEN) || !ms1_parse_config(buf, MS1_PAGE_LEN, &cfg)) {
        return false;
    }
    page_cached = MS1_CONFIG_PAGE;
    Serial.print(F("config: "));
    Serial.print(cfg.cyl);
    Serial.print(cfg.two_stroke ? F(" cyl, two-stroke, map ") : F(" cyl, four-stroke, map "));
    Serial.print(ms1_map_kpa(cfg.map));
    Serial.println(F(" kPa"));
    return true;
}

static void print_x10(const __FlashStringHelper *name, int16_t v) {
    Serial.print(name);
    if (v < 0) {
        Serial.print('-');
        v = (int16_t)-v;
    }
    Serial.print(v / 10);
    Serial.print('.');
    Serial.print(v % 10);
}

static void print_rt(const ecu_rt_t *r) {
    Serial.print(F("rpm "));
    Serial.print(r->rpm);
    print_x10(F("  boost "), r->boost_psi_x10);
    print_x10(F("  adv "), r->adv_x10);
    print_x10(F("  clt "), r->clt_c_x10);
    print_x10(F("  iat "), r->iat_c_x10);
    print_x10(F("  map "), r->map_x10);
    print_x10(F("  tps "), r->tps_x10);
    print_x10(F("  batt "), r->batt_x10);
    print_x10(F("  afr "), r->afr_x10);
    Serial.println();
}

static bool read_page(uint8_t page) {
    const uint8_t c[] = {MS1_CMD_PAGE, page, MS1_CMD_READ};
    page_cached = NO_PAGE;
    if (!ecu_cmd(c, sizeof c, page_buf, MS1_PAGE_LEN)) {
        return false;
    }
    page_cached = page;
    return true;
}

static void send_ack(uint8_t c, uint8_t id, uint8_t cell, uint8_t st, int16_t val) {
    uint8_t pl[LINK_ACK_LEN] = {c, id, cell, st};
    link_put_s16(pl + 4, val);
    gb_send(LINK_T_ACK, pl, LINK_ACK_LEN);
}

static int16_t elem(const tune_table_t *t, uint16_t i) {
    uint8_t page;
    uint16_t off;
    uint8_t type;
    tune_loc(t, i, &page, &off, &type);
    return tune_decode(type, &page_buf[off]);
}

static void do_read(uint8_t id) {
    const tune_table_t *t = tune_get(TUNE_ECU_MS1, id);
    if (!t) {
        send_ack(LINK_C_READ, id, 0, TUNE_E_TABLE, 0);
        return;
    }
    if (!read_page(t->page)) {
        send_ack(LINK_C_READ, id, 0, TUNE_E_ECU, 0);
        return;
    }
    uint16_t len = tune_blob_len(t);
    uint8_t pl[LINK_TABLE_HDR + LINK_TABLE_VALS * 2u];
    for (uint16_t off = 0; off < len; off = (uint16_t)(off + LINK_TABLE_VALS)) {
        uint16_t n = (uint16_t)min((int)LINK_TABLE_VALS, (int)(len - off));
        pl[0] = id;
        pl[1] = (uint8_t)off;
        pl[2] = (uint8_t)(off >> 8);
        for (uint16_t i = 0; i < n; i++) {
            link_put_s16(pl + LINK_TABLE_HDR + i * 2u, elem(t, (uint16_t)(off + i)));
        }
        gb_send(LINK_T_TABLE, pl, (uint8_t)(LINK_TABLE_HDR + n * 2u));
    }
    send_ack(LINK_C_READ, id, 0, TUNE_OK, 0);
}

static void do_write(uint8_t id, uint8_t cell, int16_t val, bool verify) {
    const tune_table_t *t = tune_get(TUNE_ECU_MS1, id);
    if (!t) {
        send_ack(LINK_C_WRITE, id, cell, TUNE_E_TABLE, 0);
        return;
    }
    if (cell >= tune_cells(t)) {
        send_ack(LINK_C_WRITE, id, cell, TUNE_E_CELL, 0);
        return;
    }
    if (page_cached != t->page && !read_page(t->page)) {
        send_ack(LINK_C_WRITE, id, cell, TUNE_E_ECU, 0);
        return;
    }
    uint8_t page;
    uint16_t off;
    uint8_t type;
    tune_loc(t, cell, &page, &off, &type);
    int16_t cur = page_buf[off];
    uint8_t st = tune_check_write(t, cell, cur, val);
    if (st == TUNE_OK && t->norder) {
        int16_t v[4];
        for (uint8_t i = 0; i < tune_cells(t) && i < 4; i++) {
            v[i] = elem(t, i);
        }
        st = tune_check_order(t, v, cell, val);
    }
    if (st != TUNE_OK) {
        send_ack(LINK_C_WRITE, id, cell, st, cur);
        return;
    }
    const uint8_t w[] = {MS1_CMD_PAGE, page, 'W', (uint8_t)off, (uint8_t)val};
    ecu.write(w, sizeof w);
    if (!verify) {
        page_buf[off] = (uint8_t)val;
        send_ack(LINK_C_WRITE, id, cell, TUNE_OK, val);
        return;
    }
    if (!read_page(page)) {
        send_ack(LINK_C_WRITE, id, cell, TUNE_E_ECU, cur);
        return;
    }
    Serial.print(F("write: table "));
    Serial.print(id);
    Serial.print(F(" cell "));
    Serial.print(cell);
    Serial.print(F(" "));
    Serial.print(cur);
    Serial.print(F(" -> "));
    Serial.println(page_buf[off]);
    send_ack(LINK_C_WRITE, id, cell, page_buf[off] == val ? TUNE_OK : TUNE_E_VERIFY, page_buf[off]);
}

static void do_burn(uint8_t id) {
    const tune_table_t *t = tune_get(TUNE_ECU_MS1, id);
    if (!t) {
        send_ack(LINK_C_BURN, id, 0, TUNE_E_TABLE, 0);
        return;
    }
    const uint8_t b[] = {MS1_CMD_PAGE, t->page, 'B'};
    ecu.write(b, sizeof b);
    delay(BURN_WAIT_MS);
    page_cached = NO_PAGE;
    Serial.print(F("burn: table "));
    Serial.println(id);
    send_ack(LINK_C_BURN, id, 0, TUNE_OK, 0);
}

static void handle_cmd(void) {
    uint8_t type = cmd_type;
    uint8_t len = cmd_len;
    cmd_pending = false;
    if (!ecu_ok) {
        send_ack(type, len ? cmd[0] : 0, len > 1 ? cmd[1] : 0, TUNE_E_ECU, 0);
        return;
    }
    if (type == LINK_C_READ && len == 1) {
        do_read(cmd[0]);
    } else if (type == LINK_C_WRITE && len == LINK_WRITE_LEN) {
        do_write(cmd[0], cmd[1], link_get_s16(cmd + 2), cmd[4] & LINK_WF_VERIFY);
    } else if (type == LINK_C_BURN && len == 1) {
        do_burn(cmd[0]);
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(GB_SCK, OUTPUT);
    digitalWrite(GB_SCK, HIGH);
    pinMode(GB_OUT, OUTPUT);
    digitalWrite(GB_OUT, HIGH);
    pinMode(GB_IN, INPUT_PULLUP);
    ecu.begin(MS1_BAUD);
    link_parser_reset(&up);
    Serial.println(F("megaboy nano: ms1/extra"));
}

void loop() {
    uint32_t now = millis();
    ecu_rt_t rt;

    if (!ecu_ok && now - last_try >= RETRY_MS) {
        last_try = now;
        ecu_ok = ecu_connect();
        errors = 0;
    }

    if (ecu_ok && now - last_poll >= POLL_MS) {
        static const uint8_t c_rt[] = {MS1_CMD_RT};
        uint8_t d[MS1_RT_LEN];
        last_poll = now;
        if (ecu_cmd(c_rt, sizeof c_rt, d, MS1_RT_LEN) && ms1_decode_rt(d, MS1_RT_LEN, &cfg, &rt)) {
            errors = 0;
            send_rt(&rt, 0);
            if (now - last_print >= PRINT_MS) {
                last_print = now;
                print_rt(&rt);
            }
        } else {
            page_cached = NO_PAGE;
            if (++errors >= MAX_ERRORS) {
                ecu_ok = false;
            }
        }
    }

    if (now - last_status >= STATUS_MS) {
        last_status = now;
        send_status();
    }

    if (cmd_pending) {
        handle_cmd();
    }
}
