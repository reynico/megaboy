#include "hardware/spi.h"
#include "pico/stdlib.h"

#include "ff.h"
#include "diskio.h"

#define SD_SPI spi0
#define SD_SCK 2
#define SD_MOSI 3
#define SD_MISO 4
#define SD_CS 5
#define SD_INIT_HZ 400000
#define SD_RUN_HZ 8000000
#define SECTOR 512u

#define CMD0 0
#define CMD8 8
#define CMD16 16
#define CMD17 17
#define CMD24 24
#define CMD55 55
#define CMD58 58
#define ACMD41 41

#define R1_IDLE 0x01
#define R1_ILLEGAL 0x04
#define TOKEN_DATA 0xfe
#define DATA_ACCEPTED 0x05

#define INIT_TIMEOUT_MS 1000
#define READ_TIMEOUT_MS 200
#define BUSY_TIMEOUT_MS 500

static volatile DSTATUS status = STA_NOINIT;
static bool block_addr;

static uint8_t xfer(uint8_t b) {
    uint8_t r;
    spi_write_read_blocking(SD_SPI, &b, &r, 1);
    return r;
}

static bool wait_ready(uint32_t ms) {
    absolute_time_t end = make_timeout_time_ms(ms);
    do {
        if (xfer(0xff) == 0xff) {
            return true;
        }
    } while (!time_reached(end));
    return false;
}

static void deselect(void) {
    gpio_put(SD_CS, 1);
    xfer(0xff);
}

static bool select_card(void) {
    gpio_put(SD_CS, 0);
    xfer(0xff);
    if (wait_ready(BUSY_TIMEOUT_MS)) {
        return true;
    }
    deselect();
    return false;
}

static uint8_t cmd(uint8_t c, uint32_t arg) {
    uint8_t r;
    if (c & 0x80) {
        c &= 0x7f;
        r = cmd(CMD55, 0);
        if (r > 1) {
            return r;
        }
    }
    deselect();
    if (!select_card()) {
        return 0xff;
    }
    xfer(0x40 | c);
    xfer((uint8_t)(arg >> 24));
    xfer((uint8_t)(arg >> 16));
    xfer((uint8_t)(arg >> 8));
    xfer((uint8_t)arg);
    xfer(c == CMD0 ? 0x95 : (c == CMD8 ? 0x87 : 0x01));
    for (int i = 0; i < 10; i++) {
        r = xfer(0xff);
        if (!(r & 0x80)) {
            return r;
        }
    }
    return 0xff;
}

#define ACMD(c) (0x80 | (c))

static bool read_block(uint8_t *buf) {
    absolute_time_t end = make_timeout_time_ms(READ_TIMEOUT_MS);
    uint8_t t;
    do {
        t = xfer(0xff);
    } while (t == 0xff && !time_reached(end));
    if (t != TOKEN_DATA) {
        return false;
    }
    spi_read_blocking(SD_SPI, 0xff, buf, SECTOR);
    xfer(0xff);
    xfer(0xff);
    return true;
}

static bool write_block(const uint8_t *buf) {
    if (!wait_ready(BUSY_TIMEOUT_MS)) {
        return false;
    }
    xfer(TOKEN_DATA);
    spi_write_blocking(SD_SPI, buf, SECTOR);
    xfer(0xff);
    xfer(0xff);
    return (xfer(0xff) & 0x1f) == DATA_ACCEPTED;
}

DSTATUS disk_initialize(BYTE pdrv) {
    uint8_t ocr[4];
    absolute_time_t end;
    if (pdrv) {
        return STA_NOINIT;
    }
    spi_init(SD_SPI, SD_INIT_HZ);
    gpio_set_function(SD_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SD_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(SD_MISO, GPIO_FUNC_SPI);
    gpio_pull_up(SD_MISO);
    gpio_init(SD_CS);
    gpio_set_dir(SD_CS, GPIO_OUT);
    gpio_put(SD_CS, 1);
    for (int i = 0; i < 10; i++) {
        xfer(0xff);
    }

    status = STA_NOINIT;
    block_addr = false;
    end = make_timeout_time_ms(INIT_TIMEOUT_MS);
    if (cmd(CMD0, 0) == R1_IDLE) {
        if (cmd(CMD8, 0x1aa) == R1_IDLE) {
            for (int i = 0; i < 4; i++) {
                ocr[i] = xfer(0xff);
            }
            if (ocr[2] == 0x01 && ocr[3] == 0xaa) {
                while (cmd(ACMD(ACMD41), 1u << 30) && !time_reached(end)) {
                }
                if (!time_reached(end) && cmd(CMD58, 0) == 0) {
                    for (int i = 0; i < 4; i++) {
                        ocr[i] = xfer(0xff);
                    }
                    block_addr = ocr[0] & 0x40;
                    status = 0;
                }
            }
        } else {
            while (cmd(ACMD(ACMD41), 0) && !time_reached(end)) {
            }
            if (!time_reached(end) && cmd(CMD16, SECTOR) == 0) {
                status = 0;
            }
        }
    }
    deselect();
    if (status == 0) {
        spi_set_baudrate(SD_SPI, SD_RUN_HZ);
    }
    return status;
}

DSTATUS disk_status(BYTE pdrv) {
    return pdrv ? STA_NOINIT : status;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv || !count) {
        return RES_PARERR;
    }
    if (status & STA_NOINIT) {
        return RES_NOTRDY;
    }
    for (UINT i = 0; i < count; i++) {
        uint32_t addr = block_addr ? sector + i : (sector + i) * SECTOR;
        bool ok = cmd(CMD17, addr) == 0 && read_block(buff + i * SECTOR);
        deselect();
        if (!ok) {
            return RES_ERROR;
        }
    }
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv || !count) {
        return RES_PARERR;
    }
    if (status & STA_NOINIT) {
        return RES_NOTRDY;
    }
    for (UINT i = 0; i < count; i++) {
        uint32_t addr = block_addr ? sector + i : (sector + i) * SECTOR;
        bool ok = cmd(CMD24, addr) == 0 && write_block(buff + i * SECTOR);
        deselect();
        if (!ok) {
            return RES_ERROR;
        }
    }
    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE c, void *buff) {
    (void)buff;
    if (pdrv) {
        return RES_PARERR;
    }
    if (status & STA_NOINIT) {
        return RES_NOTRDY;
    }
    if (c == CTRL_SYNC) {
        bool ok = select_card();
        deselect();
        return ok ? RES_OK : RES_ERROR;
    }
    return RES_PARERR;
}
