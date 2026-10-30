/* OWNER: Member 1 (decision logic). Receives scans and room events, never touches hardware or the SD card. */
#include <string.h>
#include "state.h"
#include "logger.h"

static void send_door(gate_t g, door_cmd_t c) { door_msg_t m = { g, c }; xQueueSend(q_door, &m, 0); }

static void handle_scan(const scan_msg_t *s)
{
    const char *gate = (s->gate == GATE_ENTRY) ? "ENTRY" : "EXIT";
    user_t u;
    bool known = user_lookup(s->uid, &u);
    if (emergency_get()) {                      /* emergency override: anyone may leave, nobody may enter */
        if (s->gate == GATE_EXIT) {
            if (known) state_leave(s->uid);
            log_event(LOG_ACCESS, "%s,%s,GRANT,emergency exit", gate, s->uid);
            send_door(s->gate, DOOR_CMD_OPEN);
        } else {
            log_event(LOG_ACCESS, "%s,%s,DENY,emergency active", gate, s->uid);
            send_door(s->gate, DOOR_CMD_DENY);
        }
        return;
    }
    if (!known) { log_event(LOG_ACCESS, "%s,%s,DENY,unknown tag", gate, s->uid); send_door(s->gate, DOOR_CMD_DENY); return; }

    bool allowed = schedule_allows(s->ts) || override_get();
    if (!allowed) { log_event(LOG_ACCESS, "%s,%s,DENY,outside schedule", gate, s->uid); send_door(s->gate, DOOR_CMD_DENY); return; }

    bool ok = (s->gate == GATE_ENTRY) ? state_enter(s->uid, s->ts) : state_leave(s->uid);
    if (!ok) {
        log_event(LOG_ACCESS, "%s,%s,DENY,%s", gate, s->uid, (s->gate == GATE_ENTRY) ? "already inside" : "not inside");
        send_door(s->gate, DOOR_CMD_DENY);
        return;
    }
    log_event(LOG_ACCESS, "%s,%s,GRANT,%s", gate, s->uid, u.name);
    send_door(s->gate, DOOR_CMD_OPEN);
}

void access_ctrl_task(void *arg)
{
    scan_msg_t s; room_msg_t r;
    for (;;) {
        if (xQueueReceive(q_scan, &s, pdMS_TO_TICKS(50)) == pdTRUE) handle_scan(&s);
        while (xQueueReceive(q_room, &r, 0) == pdTRUE) {
            room_state_set(r.room, r.occupied);
            log_event(LOG_ROOM, "ROOM%d,%s", r.room + 1, r.occupied ? "OCCUPIED,lights ON" : "EMPTY,lights OFF");
        }
    }
}
