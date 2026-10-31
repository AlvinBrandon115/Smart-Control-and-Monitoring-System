/* OWNER: Member 1 (decision logic). Receives scans and room events, never touches hardware or the SD card.
 * Admin-flagged tags may pass outside the schedule (logged as an admin override). */
#include <string.h>
#include <stdio.h>
#include "state.h"
#include "logger.h"
#include "tasks.h"

static void send_door(gate_t g, door_cmd_t c) { door_msg_t m = { g, c }; xQueueSend(q_door, &m, 0); }

static void last_result(gate_t g, bool ok, const char *info)
{
    char b[48];
    snprintf(b, sizeof b, "%c %s %s", g == GATE_ENTRY ? 'E' : 'X', ok ? "OK" : "NO", info);
    state_set_last(b);
}

static void handle_scan(const scan_msg_t *s)
{
    const char *gate = (s->gate == GATE_ENTRY) ? "ENTRY" : "EXIT";
    user_t u;
    bool known = user_lookup(s->uid, &u);

    if (emergency_get()) {                      /* emergency override: anyone may leave, nobody may enter */
        if (s->gate == GATE_EXIT) {
            if (known) state_leave(s->uid);
            log_event(LOG_ACCESS, "%s,%s,GRANT,emergency exit", gate, s->uid);
            last_result(s->gate, true, "emergency");
            send_door(s->gate, DOOR_CMD_OPEN);
        } else {
            log_event(LOG_ACCESS, "%s,%s,DENY,emergency active", gate, s->uid);
            last_result(s->gate, false, "emergency");
            send_door(s->gate, DOOR_CMD_DENY);
        }
        return;
    }
    if (!known) {
        log_event(LOG_ACCESS, "%s,%s,DENY,unknown tag", gate, s->uid);
        last_result(s->gate, false, "unknown tag");
        send_door(s->gate, DOOR_CMD_DENY);
        return;
    }

    bool in_hours = schedule_allows(s->ts);
    bool allowed = in_hours || override_get() || u.admin;
    if (!allowed) {
        log_event(LOG_ACCESS, "%s,%s,DENY,outside schedule", gate, s->uid);
        last_result(s->gate, false, "closed");
        send_door(s->gate, DOOR_CMD_DENY);
        return;
    }

    bool ok = (s->gate == GATE_ENTRY) ? state_enter(s->uid, s->ts) : state_leave(s->uid);
    if (!ok) {
        const char *why = (s->gate == GATE_ENTRY) ? "already inside" : "not inside";
        log_event(LOG_ACCESS, "%s,%s,DENY,%s", gate, s->uid, why);
        last_result(s->gate, false, why);
        send_door(s->gate, DOOR_CMD_DENY);
        return;
    }
    log_event(LOG_ACCESS, "%s,%s,GRANT,%s%s", gate, s->uid, u.name, (!in_hours && u.admin) ? " (admin override)" : "");
    last_result(s->gate, true, u.name);
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
