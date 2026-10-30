#pragma once
#include <stdbool.h>
#include "app_types.h"
void logger_task(void *arg);
void rfid_task(void *arg);          /* arg = (void *)GATE_ENTRY or (void *)GATE_EXIT */
void access_ctrl_task(void *arg);
void door_task(void *arg);
void room_task(void *arg);          /* arg = (void *)0 for Room 1, (void *)1 for Room 2 */
void schedule_task(void *arg);
void serial_menu_task(void *arg);
void monitor_task(void *arg);
bool clock_set(int y, int mo, int d, int h, int mi, int s);   /* sets RTC and system time */
const char *clock_source(void);                               /* "RTC", "build time" or "none" */       /* health, after-hours alerts, emergency alarm */
