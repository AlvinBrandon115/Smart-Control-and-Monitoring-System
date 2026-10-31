/* OWNER: Member 4 (innovation features): terminal health, after-hours alert, emergency alarm.
 * It never touches hardware itself: it asks door_task to sound the buzzer through q_door. */
#include "app_config.h"
#include "state.h"
#include "logger.h"
#include "tasks.h"

void emergency_apply(bool on, const char *source)
{
    emergency_set(on);
    door_msg_t m = { GATE_EXIT, on ? DOOR_CMD_HOLD_OPEN : DOOR_CMD_CLOSE };
    xQueueSend(q_door, &m, 0);
    log_event(LOG_CONFIG, "EMERGENCY %s,by %s", on ? "ACTIVATED (exit door held open)" : "CLEARED", source);
}

static void sound_alarm(void) { door_msg_t m = { GATE_EXIT, DOOR_CMD_ALARM }; xQueueSend(q_door, &m, 0); }

void monitor_task(void *arg)
{
    bool was_online[T_COUNT] = { false };
    TickType_t last_alarm = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(2000));

        for (int t = 0; t < T_COUNT; t++) {                       /* 1. terminal heartbeat: log every change */
            bool on = terminal_online((terminal_t)t);
            if (on != was_online[t]) { log_event(LOG_SYSTEM, "HEALTH,%s,%s", terminal_name((terminal_t)t), on ? "ONLINE" : "OFFLINE"); was_online[t] = on; }
        }

        TickType_t now = xTaskGetTickCount();
        bool emergency = emergency_get();
        int inside = state_count_inside();
        bool after_hours = inside > 0 && !schedule_allows(time(NULL)) && !override_get() && !emergency;

        if (emergency && now - last_alarm >= pdMS_TO_TICKS(6000)) {   /* 2. emergency alarm until cleared */
            sound_alarm(); last_alarm = now;
        } else if (after_hours && now - last_alarm >= pdMS_TO_TICKS(30000)) {   /* 3. after-hours alert */
            log_event(LOG_SYSTEM, "ALERT,%d person(s) still inside after hours", inside);
            sound_alarm(); last_alarm = now;
        }
    }
}
