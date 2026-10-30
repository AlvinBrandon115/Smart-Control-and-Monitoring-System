/* OWNER: Member 1. One task instance per gate: polls its MFRC522 and sends scans to access_ctrl via q_scan. */
#include <string.h>
#include "app_config.h"
#include "app_types.h"
#include "logger.h"
#include "rfid_mfrc522.h"
#include "tasks.h"
#include "state.h"

void rfid_task(void *arg)
{
    gate_t gate = (gate_t)(intptr_t)arg;
    bool ok = mfrc522_init(gate);
    log_event(LOG_SYSTEM, "RFID %s reader %s", gate == GATE_ENTRY ? "entry" : "exit", ok ? "ready" : "NOT FOUND");
    char last[15] = ""; time_t last_ts = 0;
    for (;;) {
        if (ok) heartbeat_beat(gate == GATE_ENTRY ? T_ENTRY : T_EXIT);   /* a missing reader stays OFFLINE */
        scan_msg_t s = { .gate = gate };
        if (ok && mfrc522_read_uid(gate, s.uid)) {
            s.ts = time(NULL);
            if (strcmp(s.uid, last) != 0 || s.ts - last_ts >= SCAN_DEBOUNCE_S) {   /* ignore the same card re-read */
                xQueueSend(q_scan, &s, pdMS_TO_TICKS(100));
                strcpy(last, s.uid); last_ts = s.ts;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}
