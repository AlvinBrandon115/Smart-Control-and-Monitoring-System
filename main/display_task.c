/* OWNER: Member 3. 16x2 character LCD on the shared I2C bus (PCF8574 backpack, 4-bit mode).
 * Page 0: time, people inside, rooms, last access result. Page 1: schedule, emergency, override. Page 2: health. */
#include <stdio.h>
#include "driver/i2c.h"
#include "esp_rom_sys.h"
#include "app_config.h"
#include "state.h"
#include "tasks.h"

#define LCD_BL 0x08
#define LCD_EN 0x04
#define LCD_RS 0x01

static volatile int s_page;
void display_next_page(void) { s_page = (s_page + 1) % 3; }

static bool pcf(uint8_t v) { return i2c_master_write_to_device(I2C_NUM_0, LCD_I2C_ADDR, &v, 1, pdMS_TO_TICKS(50)) == ESP_OK; }
static bool nib(uint8_t n, uint8_t rs)
{
    uint8_t b = (uint8_t)((n << 4) | LCD_BL | rs);
    bool ok = pcf(b | LCD_EN);
    esp_rom_delay_us(2);
    ok &= pcf(b);
    esp_rom_delay_us(60);
    return ok;
}
static void cmd(uint8_t c) { nib(c >> 4, 0); nib(c & 0x0F, 0); }

static bool lcd_init(void)
{
    vTaskDelay(pdMS_TO_TICKS(50));
    if (!nib(0x3, 0)) return false;                           /* no LCD answering at this address */
    vTaskDelay(pdMS_TO_TICKS(5)); nib(0x3, 0); vTaskDelay(pdMS_TO_TICKS(2)); nib(0x3, 0); nib(0x2, 0);
    cmd(0x28); cmd(0x0C); cmd(0x06); cmd(0x01);
    vTaskDelay(pdMS_TO_TICKS(5));
    return true;
}

static void lcd_row(int row, const char *text)                /* always writes exactly 16 characters */
{
    cmd((uint8_t)(0x80 | (row ? 0x40 : 0x00)));
    bool end = false;
    for (int i = 0; i < 16; i++) {
        if (!end && !text[i]) end = true;
        nib(((end ? ' ' : text[i]) >> 4) & 0x0F, LCD_RS);
        nib((end ? ' ' : text[i]) & 0x0F, LCD_RS);
    }
}

void display_task(void *arg)
{
    if (!lcd_init()) { printf("LCD not found at I2C address 0x%02X: display disabled\n", LCD_I2C_ADDR); vTaskDelete(NULL); return; }
    char r1[48], r2[48], last[17];
    for (;;) {
        time_t now = time(NULL); struct tm t; localtime_r(&now, &t);
        int o, c; schedule_get(&o, &c);
        switch (s_page) {
        case 0:
            state_get_last(last);
            snprintf(r1, sizeof r1, "%02d:%02d In:%d R:%c%c", t.tm_hour, t.tm_min, state_count_inside(), room_state_get(0) ? '*' : '-', room_state_get(1) ? '*' : '-');
            snprintf(r2, sizeof r2, "%s", last);
            break;
        case 1:
            snprintf(r1, sizeof r1, "Hrs %02d:%02d-%02d:%02d", o / 60, o % 60, c / 60, c % 60);
            snprintf(r2, sizeof r2, "Emg:%s Ovr:%s", emergency_get() ? "ON" : "off", override_get() ? "ON" : "off");
            break;
        default: {
            int up = 0;
            for (int i = 0; i < T_COUNT; i++) if (terminal_online((terminal_t)i)) up++;
            snprintf(r1, sizeof r1, "Health: %d/%d up", up, T_COUNT);
            snprintf(r2, sizeof r2, "Clock:%s", clock_source());
            break;
        }
        }
        lcd_row(0, r1); lcd_row(1, r2);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
