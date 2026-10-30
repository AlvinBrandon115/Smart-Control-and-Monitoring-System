/* OWNER: Member 2. SD card: mounts the card, loads and saves users + schedule, appends logs.
 * One mutex serialises all file access. Saves write a temp file first, then replace the old one,
 * so a failure while writing cannot corrupt the existing data. No SD access happens while a state mutex is held. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "app_config.h"
#include "state.h"
#include "logger.h"
#include "storage.h"
#include "tasks.h"

static SemaphoreHandle_t s_lock;
static bool s_ready;

void storage_lock(void)   { if (s_lock) xSemaphoreTake(s_lock, portMAX_DELAY); }
void storage_unlock(void) { if (s_lock) xSemaphoreGive(s_lock); }
bool storage_ready(void)  { return s_ready; }

bool storage_commit(const char *tmp, const char *fin) { unlink(fin); return rename(tmp, fin) == 0; }

static bool sd_mount(void)
{
    esp_vfs_fat_sdmmc_mount_config_t mc = { .format_if_mount_failed = true, .max_files = 4, .allocation_unit_size = 16 * 1024 };
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI3_HOST;
    spi_bus_config_t bus = { .mosi_io_num = PIN_SD_MOSI, .miso_io_num = PIN_SD_MISO, .sclk_io_num = PIN_SD_SCK,
                             .quadwp_io_num = -1, .quadhd_io_num = -1, .max_transfer_sz = 4000 };
    if (spi_bus_initialize(host.slot, &bus, SDSPI_DEFAULT_DMA) != ESP_OK) return false;
    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs = PIN_SD_CS; slot.host_id = host.slot;
    sdmmc_card_t *card;
    return esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot, &mc, &card) == ESP_OK;
}

bool storage_save_users(void)
{
    if (!s_ready) return false;
    user_t u[MAX_USERS];
    int n = state_users_snapshot(u, MAX_USERS);
    storage_lock();
    FILE *f = fopen(SD_FILE("users.tmp"), "w");
    bool ok = f != NULL;
    if (f) {
        for (int i = 0; i < n; i++) fprintf(f, "%s,%s,%d,%d,%lld\n", u[i].uid, u[i].name, u[i].admin, u[i].inside, (long long)u[i].since);
        ok = (fclose(f) == 0) && storage_commit(SD_FILE("users.tmp"), SD_FILE("users.csv"));
    }
    storage_unlock();
    return ok;
}

static bool load_users(void)
{
    storage_lock();
    FILE *f = fopen(SD_FILE("users.csv"), "r");
    if (!f) { storage_unlock(); return false; }
    user_t u[MAX_USERS]; int n = 0; char line[96];
    while (n < MAX_USERS && fgets(line, sizeof line, f)) {
        user_t t = { 0 }; int adm, ins; long long since;
        if (sscanf(line, "%14[^,],%23[^,],%d,%d,%lld", t.uid, t.name, &adm, &ins, &since) == 5) { t.admin = adm; t.inside = ins; t.since = (time_t)since; u[n++] = t; }
    }
    fclose(f);
    storage_unlock();
    state_users_replace(u, n);
    return true;
}

bool storage_save_config(void)
{
    if (!s_ready) return false;
    int o, c; schedule_get(&o, &c);
    storage_lock();
    FILE *f = fopen(SD_FILE("config.tmp"), "w");
    bool ok = f != NULL;
    if (f) { fprintf(f, "open=%d\nclose=%d\n", o, c); ok = (fclose(f) == 0) && storage_commit(SD_FILE("config.tmp"), SD_FILE("config.cfg")); }
    storage_unlock();
    return ok;
}

static bool load_config(void)
{
    storage_lock();
    FILE *f = fopen(SD_FILE("config.cfg"), "r");
    if (!f) { storage_unlock(); return false; }
    int o = -1, c = -1; char line[48];
    while (fgets(line, sizeof line, f)) { sscanf(line, "open=%d", &o); sscanf(line, "close=%d", &c); }
    fclose(f);
    storage_unlock();
    if (o < 0 || c < 0 || o > 1439 || c > 1439) return false;
    schedule_set(o, c);
    state_take_dirty();                               /* loading is not a change */
    return true;
}

bool storage_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_ready = sd_mount();
    if (!s_ready) { printf("SD card NOT available: users and schedule will not be saved\n"); return false; }
    if (!load_users())  { storage_save_users();  printf("users.csv created with default users\n"); }
    if (!load_config()) { storage_save_config(); printf("config.cfg created with default schedule\n"); }
    state_take_dirty();
    return true;
}

bool storage_append(const char *name, const char *line)
{
    if (!s_ready) return false;
    char path[40]; snprintf(path, sizeof path, "/sdcard/%s", name);
    storage_lock();
    FILE *f = fopen(path, "a");
    bool ok = f != NULL;
    if (f) { fputs(line, f); ok = fclose(f) == 0; }
    storage_unlock();
    return ok;
}

void storage_persist_task(void *arg)
{
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        if (!s_ready) continue;
        uint8_t d = state_take_dirty();
        if ((d & DIRTY_USERS) && !storage_save_users())   { state_mark_dirty(DIRTY_USERS);  log_event(LOG_SYSTEM, "SAVE FAILED,users.csv"); }
        if ((d & DIRTY_CONFIG) && !storage_save_config()) { state_mark_dirty(DIRTY_CONFIG); log_event(LOG_SYSTEM, "SAVE FAILED,config.cfg"); }
    }
}
