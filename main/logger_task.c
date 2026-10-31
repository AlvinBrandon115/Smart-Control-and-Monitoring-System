/* OWNER: Member 2. Collects log records from q_log: keeps a RAM ring for the serial menu and appends to the SD log files via storage.c. */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include "storage.h"
#include "app_config.h"
#include "logger.h"
#include "tasks.h"
#include "state.h"

#define RING_N 20
static const char *TYPE_NAME[] = { "ACCESS", "ROOM", "CONFIG", "SYSTEM" };
static log_msg_t s_ring[RING_N];
static int s_ring_head, s_ring_count;
static SemaphoreHandle_t s_ring_mutex;

void time_str(time_t t, char out[20])
{
    struct tm tm; localtime_r(&t, &tm);
    strftime(out, 20, "%Y-%m-%d %H:%M:%S", &tm);
}

void log_event(log_type_t type, const char *fmt, ...)
{
    log_msg_t m = { .type = type, .ts = time(NULL) };
    va_list ap; va_start(ap, fmt);
    vsnprintf(m.text, sizeof m.text, fmt, ap);
    va_end(ap);
    xQueueSend(q_log, &m, 0);
}

void log_print_recent(int n)
{
    if (n > s_ring_count) n = s_ring_count;
    xSemaphoreTake(s_ring_mutex, portMAX_DELAY);
    for (int i = n; i > 0; i--) {
        log_msg_t *m = &s_ring[(s_ring_head - i + RING_N) % RING_N];
        char ts[20]; time_str(m->ts, ts);
        printf("  %s,%s,%s\n", ts, TYPE_NAME[m->type], m->text);
    }
    xSemaphoreGive(s_ring_mutex);
}

void logger_task(void *arg)
{
    s_ring_mutex = xSemaphoreCreateMutex();
    log_event(LOG_SYSTEM, storage_ready() ? "SD card ready" : "SD card NOT available (RAM log only)");
    log_msg_t m;
    for (;;) {
        if (storage_ready()) heartbeat_beat(T_LOGGER);   /* no SD card = OFFLINE */
        if (xQueueReceive(q_log, &m, pdMS_TO_TICKS(1000)) != pdTRUE) continue;
        xSemaphoreTake(s_ring_mutex, portMAX_DELAY);
        s_ring[s_ring_head] = m; s_ring_head = (s_ring_head + 1) % RING_N;
        if (s_ring_count < RING_N) s_ring_count++;
        xSemaphoreGive(s_ring_mutex);
        char ts[20], out[160]; time_str(m.ts, ts);
        snprintf(out, sizeof out, "%s,%s,%s\n", ts, TYPE_NAME[m.type], m.text);
        storage_append(m.type == LOG_ACCESS ? "access.csv" : m.type == LOG_ROOM ? "rooms.csv" : "system.log", out);
    }
}
