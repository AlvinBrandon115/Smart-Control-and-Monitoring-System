/* OWNER: Member 1. Minimal MFRC522 driver (ISO14443A): wake card, anti-collision, select, halt.
 * Two readers share one SPI bus (separate CS + RST). Each reader is used by ONE task only,
 * and the ESP-IDF SPI master driver serialises the individual bus transactions. */
#include <stdio.h>
#include <string.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "app_config.h"
#include "rfid_mfrc522.h"

enum { CommandReg = 0x01, ComIrqReg = 0x04, DivIrqReg = 0x05, ErrorReg = 0x06, FIFODataReg = 0x09,
       FIFOLevelReg = 0x0A, ControlReg = 0x0C, BitFramingReg = 0x0D, CollReg = 0x0E, ModeReg = 0x11,
       TxControlReg = 0x14, TxASKReg = 0x15, CRCResultRegH = 0x21, CRCResultRegL = 0x22,
       TModeReg = 0x2A, TPrescalerReg = 0x2B, TReloadRegH = 0x2C, TReloadRegL = 0x2D, VersionReg = 0x37 };

#define DBG(stage) printf("MFRC522 %s: card answered but %s failed\n", g == 0 ? "entry" : "exit", stage)
static spi_device_handle_t s_dev[2];
static const int s_cs[2]  = { PIN_ENTRY_CS, PIN_EXIT_CS };
static const int s_rst[2] = { PIN_ENTRY_RST, PIN_EXIT_RST };

static void wr(int g, uint8_t reg, uint8_t v)
{
    uint8_t tx[2] = { (uint8_t)((reg << 1) & 0x7E), v };
    spi_transaction_t t = { .length = 16, .tx_buffer = tx };
    spi_device_polling_transmit(s_dev[g], &t);
}
static uint8_t rd(int g, uint8_t reg)
{
    uint8_t tx[2] = { (uint8_t)(((reg << 1) & 0x7E) | 0x80), 0 }, rx[2] = { 0, 0 };
    spi_transaction_t t = { .length = 16, .tx_buffer = tx, .rx_buffer = rx };
    spi_device_polling_transmit(s_dev[g], &t);
    return rx[1];
}
/* Reads n bytes from the FIFO in ONE transaction (address byte repeated, last byte 0), as the MFRC522 datasheet describes. */
static void rd_fifo(int g, uint8_t *out, uint8_t n)
{
    uint8_t tx[65], rx[65] = { 0 };
    if (n > 64) n = 64;
    for (uint8_t i = 0; i < n; i++) tx[i] = (uint8_t)(((FIFODataReg << 1) & 0x7E) | 0x80);
    tx[n] = 0;
    spi_transaction_t t = { .length = (size_t)(n + 1) * 8, .tx_buffer = tx, .rx_buffer = rx };
    spi_device_polling_transmit(s_dev[g], &t);
    memcpy(out, rx + 1, n);
}
static void setbits(int g, uint8_t reg, uint8_t m) { wr(g, reg, rd(g, reg) | m); }
static void clrbits(int g, uint8_t reg, uint8_t m) { wr(g, reg, rd(g, reg) & (uint8_t)~m); }

bool mfrc522_bus_init(void)
{
    spi_bus_config_t bus = { .mosi_io_num = PIN_RFID_MOSI, .miso_io_num = PIN_RFID_MISO, .sclk_io_num = PIN_RFID_SCK,
                             .quadwp_io_num = -1, .quadhd_io_num = -1, .max_transfer_sz = 64 };
    return spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_DISABLED) == ESP_OK;
}

bool mfrc522_init(gate_t gate)
{
    int g = (int)gate;
    spi_device_interface_config_t dc = { .clock_speed_hz = 4000000, .mode = 0, .spics_io_num = s_cs[g], .queue_size = 1 };
    if (spi_bus_add_device(SPI2_HOST, &dc, &s_dev[g]) != ESP_OK) return false;
    gpio_config_t io = { .pin_bit_mask = 1ULL << s_rst[g], .mode = GPIO_MODE_OUTPUT };
    gpio_config(&io);
    gpio_set_level(s_rst[g], 0); vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(s_rst[g], 1); vTaskDelay(pdMS_TO_TICKS(50));
    wr(g, CommandReg, 0x0F); vTaskDelay(pdMS_TO_TICKS(50));          /* soft reset */
    wr(g, TModeReg, 0x80); wr(g, TPrescalerReg, 0xA9);               /* timeout about 25 ms */
    wr(g, TReloadRegH, 0x03); wr(g, TReloadRegL, 0xE8);
    wr(g, TxASKReg, 0x40); wr(g, ModeReg, 0x3D);
    setbits(g, TxControlReg, 0x03);                                  /* antenna on */
    uint8_t ver = rd(g, VersionReg);
    printf("MFRC522 %s: version 0x%02X\n", g == GATE_ENTRY ? "entry" : "exit", ver);
    return ver != 0x00 && ver != 0xFF;
}

static bool transceive(int g, const uint8_t *tx, uint8_t txlen, uint8_t txbits, uint8_t *rx, uint8_t *rxlen)
{
    wr(g, CommandReg, 0x00);
    wr(g, ComIrqReg, 0x7F);
    wr(g, FIFOLevelReg, 0x80);
    for (uint8_t i = 0; i < txlen; i++) wr(g, FIFODataReg, tx[i]);
    wr(g, BitFramingReg, txbits & 0x07);
    wr(g, CommandReg, 0x0C);
    setbits(g, BitFramingReg, 0x80);
    bool done = false;
    for (int i = 0; i < 1000; i++) {                                 /* about 50 ms max */
        uint8_t irq = rd(g, ComIrqReg);
        if (irq & 0x30) { done = true; break; }
        if (irq & 0x01) break;                                       /* timer: no answer */
        esp_rom_delay_us(50);
    }
    clrbits(g, BitFramingReg, 0x80);
    if (!done || (rd(g, ErrorReg) & 0x13)) return false;
    uint8_t n = rd(g, FIFOLevelReg);
    if (n > *rxlen) n = *rxlen;
    if (n) rd_fifo(g, rx, n);
    *rxlen = n;
    return true;
}

static bool crc_a(int g, const uint8_t *d, uint8_t len, uint8_t out[2])
{
    wr(g, CommandReg, 0x00); wr(g, DivIrqReg, 0x04); wr(g, FIFOLevelReg, 0x80);
    for (uint8_t i = 0; i < len; i++) wr(g, FIFODataReg, d[i]);
    wr(g, CommandReg, 0x03);
    for (int i = 0; i < 250; i++) {
        if (rd(g, DivIrqReg) & 0x04) { wr(g, CommandReg, 0x00); out[0] = rd(g, CRCResultRegL); out[1] = rd(g, CRCResultRegH); return true; }
        esp_rom_delay_us(20);
    }
    return false;
}

bool mfrc522_read_uid(gate_t gate, char *uid_hex)
{
    int g = (int)gate;
    uint8_t uid[10], ulen = 0, b[9], n;
    clrbits(g, CollReg, 0x80);
    uint8_t cmd = 0x26; n = sizeof b;                                   /* REQA first (what simple card models answer)... */
    bool awake = transceive(g, &cmd, 1, 7, b, &n) && n == 2;
    if (!awake) { cmd = 0x52; n = sizeof b; awake = transceive(g, &cmd, 1, 7, b, &n) && n == 2; }   /* ...then WUPA (halted cards) */
    if (!awake) return false;                                           /* no card in the field */

    for (int level = 0; level < 2; level++) {
        uint8_t sel = level == 0 ? 0x93 : 0x95, ac[2] = { sel, 0x20 }, id[5];
        n = sizeof id;
        if (!transceive(g, ac, 2, 0, id, &n) || n != 5) { DBG("anti-collision"); return false; }
        if ((id[0] ^ id[1] ^ id[2] ^ id[3]) != id[4]) { DBG("UID check byte"); printf("  bytes received (%d): %02X %02X %02X %02X %02X\n", n, id[0], id[1], id[2], id[3], id[4]); return false; }
        uint8_t sc[9] = { sel, 0x70, id[0], id[1], id[2], id[3], id[4] }, sak[3];
        if (!crc_a(g, sc, 7, &sc[7])) { DBG("CRC"); return false; }
        n = sizeof sak;
        if (!transceive(g, sc, 9, 0, sak, &n) || n != 3) { DBG("select"); return false; }
        if (id[0] == 0x88) { memcpy(uid + ulen, &id[1], 3); ulen += 3; }   /* cascade tag: more UID follows */
        else { memcpy(uid + ulen, id, 4); ulen += 4; break; }
    }
    if (ulen != 4 && ulen != 7) { DBG("UID length"); return false; }

    uint8_t hlta[4] = { 0x50, 0x00 }, dummy[2]; n = sizeof dummy;       /* halt so the card is read once */
    if (crc_a(g, hlta, 2, &hlta[2])) transceive(g, hlta, 4, 0, dummy, &n);

    for (uint8_t i = 0; i < ulen; i++) sprintf(uid_hex + 2 * i, "%02X", uid[i]);
    return true;
}
