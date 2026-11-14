#include "linkrx.h"

#include <gb/gb.h>

#include "app.h"
#include "link.h"
#include "tdata.h"

#define RX_SIZE 64u
#define RX_MASK (RX_SIZE - 1u)
#define SC_ARM (SIOF_XFER_START | SIOF_CLOCK_EXT)

static volatile uint8_t rx_buf[RX_SIZE];
static volatile uint8_t rx_head;
static uint8_t rx_tail;
static link_parser_t parser;
static uint8_t tx_buf[LINK_FRAME_MAX];
static volatile uint8_t tx_pos;
static volatile uint8_t tx_len;

static void sio_isr(void) {
    rx_buf[rx_head & RX_MASK] = SB_REG;
    rx_head++;
    if (tx_pos < tx_len) {
        SB_REG = tx_buf[tx_pos];
        tx_pos++;
    } else {
        SB_REG = 0;
    }
    SC_REG = SC_ARM;
}

void linkrx_init(void) {
    link_parser_reset(&parser);
    CRITICAL {
        add_SIO(sio_isr);
    }
    set_interrupts(VBL_IFLAG | SIO_IFLAG);
    SB_REG = 0;
    SC_REG = SC_ARM;
}

static void on_rt(void) {
    link_rt_t l;
    link_get_rt(parser.buf, &l);
    app.rt.rpm = l.rpm;
    app.rt.adv_x10 = l.adv_x10;
    app.rt.map_x10 = l.map_x10;
    app.rt.iat_c_x10 = l.iat_c_x10;
    app.rt.clt_c_x10 = l.clt_c_x10;
    app.rt.tps_x10 = l.tps_x10;
    app.rt.batt_x10 = l.batt_x10;
    app.rt.afr_x10 = l.afr_x10;
    app.rt.boost_psi_x10 = l.boost_psi_x10;
    app_rt_received();
}

static void on_status(void) {
    link_status_t st;
    link_get_status(parser.buf, &st);
    app.ecu = st.ecu;
    app.ecu_state = st.state;
    if (app.set.ecu && st.ecu != (uint8_t)app.set.ecu) {
        uint8_t t = (uint8_t)app.set.ecu;
        linkrx_send(LINK_C_ECU, &t, 1);
    }
}

static void on_time(void) {
    app.clk_state = parser.buf[0];
    link_get_time(parser.buf + 1, &app.clk);
}

static void on_log(void) {
    link_log_t l;
    link_get_log(parser.buf, &l);
    app.log_state = l.state;
    app.logging = l.state == LINK_LOG_REC;
    app.log_num = l.num;
    app.log_s = l.secs;
    app.log_bytes = l.bytes;
    app.log_hz = l.hz;
}

static void on_frame(void) {
    if (parser.type == LINK_T_RT && parser.len == LINK_RT_LEN) {
        on_rt();
    } else if (parser.type == LINK_T_STATUS && parser.len == LINK_STATUS_LEN) {
        on_status();
    } else if (parser.type == LINK_T_LOG && parser.len == LINK_LOG_LEN) {
        on_log();
    } else if (parser.type == LINK_T_TIME && parser.len == LINK_TIME_LEN) {
        on_time();
    } else if (parser.type == LINK_T_TABLE) {
        td_on_table(parser.buf, parser.len);
    } else if (parser.type == LINK_T_ACK && parser.len == LINK_ACK_LEN) {
        td_on_ack(parser.buf);
    } else {
        return;
    }
    app.src = SRC_LINK;
    app.link_age = 0;
}

void linkrx_poll(void) {
    uint8_t b;
    while (rx_tail != rx_head) {
        b = rx_buf[rx_tail & RX_MASK];
        rx_tail++;
        if (link_feed(&parser, b)) {
            on_frame();
        }
    }
}

uint8_t linkrx_send(uint8_t type, const uint8_t *payload, uint8_t len) {
    uint8_t n;
    if (tx_pos < tx_len || len > LINK_MAX_PAYLOAD) {
        return 0;
    }
    CRITICAL {
        n = link_encode(tx_buf, type, payload, len);
        tx_pos = 0;
        tx_len = n;
    }
    return 1;
}
