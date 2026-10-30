/* OWNER: Member 3. Time source: DS1307 RTC over I2C feeds the system clock used by the schedule and logs.
 * The RTC keeps local time (East Africa Time) and TZ stays UTC, so localtime() equals the RTC reading. */
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "driver/i2c.h"
#include "app_config.h"
#include "state.h"
#include "logger.h"
#include "tasks.h"

#define RTC_ADDR  0x68
#define I2C_PORT  I2C_NUM_0

static const char *s_source = "none";

static uint8_t bcd2bin(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 0x0F)); }
static uint8_t bin2bcd(int v)     { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

static bool rtc_read(struct tm *t)
{
    uint8_t reg = 0, b[7];
    if (i2c_master_write_read_device(I2C_PORT, RTC_ADDR, &reg, 1, b, 7, pdMS_TO_TICKS(100)) != ESP_OK) return false;
    if (b[0] & 0x80) return false;                     /* clock-halt bit set: RTC not running */
    memset(t, 0, sizeof *t);
    t->tm_sec = bcd2bin(b[0] & 0x7F); t->tm_min = bcd2bin(b[1]); t->tm_hour = bcd2bin(b[2] & 0x3F);
    t->tm_mday = bcd2bin(b[4]); t->tm_mon = bcd2bin(b[5]) - 1; t->tm_year = bcd2bin(b[6]) + 100;
    return t->tm_year >= 125;                          /* reject years before 2025 as invalid */
}

static bool rtc_write(const struct tm *t)
{
    uint8_t b[8] = { 0, bin2bcd(t->tm_sec) & 0x7F, bin2bcd(t->tm_min), bin2bcd(t->tm_hour),
                     (uint8_t)(t->tm_wday + 1), bin2bcd(t->tm_mday), bin2bcd(t->tm_mon + 1), bin2bcd(t->tm_year % 100) };
    return i2c_master_write_to_device(I2C_PORT, RTC_ADDR, b, sizeof b, pdMS_TO_TICKS(100)) == ESP_OK;
}

static void set_system_time(const struct tm *t)
{
    struct tm c = *t; c.tm_isdst = 0;
    struct timeval tv = { .tv_sec = mktime(&c), .tv_usec = 0 };
    settimeofday(&tv, NULL);
}

bool clock_set(int y, int mo, int d, int h, int mi, int s)
{
    if (y < 2025 || mo < 1 || mo > 12 || d < 1 || d > 31 || h < 0 || h > 23 || mi < 0 || mi > 59 || s < 0 || s > 59) return false;
    struct tm t = { .tm_year = y - 1900, .tm_mon = mo - 1, .tm_mday = d, .tm_hour = h, .tm_min = mi, .tm_sec = s };
    time_t tt = mktime(&t); localtime_r(&tt, &t);          /* fills tm_wday */
    if (!rtc_write(&t)) return false;
    set_system_time(&t); s_source = "RTC";
    return true;
}

const char *clock_source(void) { return s_source; }

static void use_build_time(void)                           /* fallback so the system still starts with a sensible time */
{
    static const char *mon = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char m[4] = { 0 }; int d, y, h, mi, s;
    sscanf(__DATE__, "%3s %d %d", m, &d, &y); sscanf(__TIME__, "%d:%d:%d", &h, &mi, &s);
    int mo = (int)((strstr(mon, m) - mon) / 3) + 1;
    if (clock_set(y, mo, d, h, mi, s)) { s_source = "build time (RTC set)"; return; }
    struct tm t = { .tm_year = y - 1900, .tm_mon = mo - 1, .tm_mday = d, .tm_hour = h, .tm_min = mi, .tm_sec = s };
    set_system_time(&t); s_source = "build time";
}

void schedule_task(void *arg)
{
    i2c_config_t cfg = { .mode = I2C_MODE_MASTER, .sda_io_num = PIN_I2C_SDA, .scl_io_num = PIN_I2C_SCL,
                         .sda_pullup_en = true, .scl_pullup_en = true, .master.clk_speed = 100000 };
    bool i2c_ok = i2c_param_config(I2C_PORT, &cfg) == ESP_OK && i2c_driver_install(I2C_PORT, cfg.mode, 0, 0, 0) == ESP_OK;
    struct tm t;
    if (i2c_ok && rtc_read(&t)) { set_system_time(&t); s_source = "RTC"; log_event(LOG_SYSTEM, "Clock set from RTC"); }
    else { use_build_time(); log_event(LOG_SYSTEM, "RTC not set or not found: using %s", s_source); }

    for (;;) {                                              /* every 2 s: re-read the RTC, correct drift, show it is alive */
        if (i2c_ok && rtc_read(&t)) {
            heartbeat_beat(T_CLOCK);
            time_t n = time(NULL);
            struct tm c = t; c.tm_isdst = 0;
            long diff = (long)(mktime(&c) - n);
            if (diff > 2 || diff < -2) set_system_time(&t);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
