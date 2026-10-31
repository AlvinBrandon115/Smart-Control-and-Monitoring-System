/* OWNER: Member 2 (shared state + synchronisation). Every access takes s_mutex. */
#include <stdio.h>
#include <string.h>
#include "state.h"
#include "logger.h"

static SemaphoreHandle_t s_mutex;
static user_t s_users[MAX_USERS];
static int s_user_count;
static int s_open_min = 7 * 60, s_close_min = 20 * 60; /* default 07:00-20:00 */
static bool s_override;
static bool s_room[2];
static bool s_emergency;
static char s_last[17] = "No access yet";
static uint8_t s_dirty;
static TickType_t s_beat[T_COUNT];

void state_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    /* TODO: load users.csv from the SD card. Demo users below for testing. */
    strcpy(s_users[0].uid, "A1B2C3D4"); strcpy(s_users[0].name, "Admin"); s_users[0].admin = true;
    strcpy(s_users[1].uid, "11223344"); strcpy(s_users[1].name, "Student1");
    s_user_count = 2;
}

static user_t *find_locked(const char *uid)
{
    for (int i = 0; i < s_user_count; i++)
        if (strcmp(s_users[i].uid, uid) == 0) return &s_users[i];
    return NULL;
}

bool user_lookup(const char *uid, user_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    user_t *u = find_locked(uid);
    if (u && out) *out = *u;
    xSemaphoreGive(s_mutex);
    return u != NULL;
}

bool state_enter(const char *uid, time_t ts)
{
    bool ok = false;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    user_t *u = find_locked(uid);
    if (u && !u->inside) { u->inside = true; u->since = ts; ok = true; s_dirty |= DIRTY_USERS; }
    xSemaphoreGive(s_mutex);
    return ok;
}

bool state_leave(const char *uid)
{
    bool ok = false;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    user_t *u = find_locked(uid);
    if (u && u->inside) { u->inside = false; ok = true; s_dirty |= DIRTY_USERS; }
    xSemaphoreGive(s_mutex);
    return ok;
}

int state_count_inside(void)
{
    int n = 0;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < s_user_count; i++) if (s_users[i].inside) n++;
    xSemaphoreGive(s_mutex);
    return n;
}

void state_print_inside(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < s_user_count; i++)
        if (s_users[i].inside) { char ts[20]; time_str(s_users[i].since, ts); printf("  %s (%s) since %s\n", s_users[i].name, s_users[i].uid, ts); }
    xSemaphoreGive(s_mutex);
}

void schedule_set(int o, int c) { xSemaphoreTake(s_mutex, portMAX_DELAY); s_open_min = o; s_close_min = c; s_dirty |= DIRTY_CONFIG; xSemaphoreGive(s_mutex); }
void schedule_get(int *o, int *c) { xSemaphoreTake(s_mutex, portMAX_DELAY); *o = s_open_min; *c = s_close_min; xSemaphoreGive(s_mutex); }

bool schedule_allows(time_t now)
{
    struct tm t; localtime_r(&now, &t);
    int m = t.tm_hour * 60 + t.tm_min, o, c;
    schedule_get(&o, &c);
    return (o <= c) ? (m >= o && m < c) : (m >= o || m < c); /* handles overnight windows */
}

void override_set(bool on) { xSemaphoreTake(s_mutex, portMAX_DELAY); s_override = on; xSemaphoreGive(s_mutex); }
bool override_get(void) { xSemaphoreTake(s_mutex, portMAX_DELAY); bool v = s_override; xSemaphoreGive(s_mutex); return v; }
void room_state_set(uint8_t r, bool v) { xSemaphoreTake(s_mutex, portMAX_DELAY); s_room[r] = v; xSemaphoreGive(s_mutex); }
bool room_state_get(uint8_t r) { xSemaphoreTake(s_mutex, portMAX_DELAY); bool v = s_room[r]; xSemaphoreGive(s_mutex); return v; }

void heartbeat_beat(terminal_t t) { xSemaphoreTake(s_mutex, portMAX_DELAY); s_beat[t] = xTaskGetTickCount(); xSemaphoreGive(s_mutex); }
bool terminal_online(terminal_t t)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool on = s_beat[t] != 0 && (xTaskGetTickCount() - s_beat[t]) < pdMS_TO_TICKS(5000);
    xSemaphoreGive(s_mutex);
    return on;
}
const char *terminal_name(terminal_t t)
{
    static const char *n[T_COUNT] = { "ENTRY reader", "EXIT reader", "ROOM 1", "ROOM 2", "LOGGER/SD", "CLOCK (RTC)" };
    return n[t];
}
void emergency_set(bool on) { xSemaphoreTake(s_mutex, portMAX_DELAY); s_emergency = on; xSemaphoreGive(s_mutex); }
bool emergency_get(void) { xSemaphoreTake(s_mutex, portMAX_DELAY); bool v = s_emergency; xSemaphoreGive(s_mutex); return v; }

uint8_t state_take_dirty(void) { xSemaphoreTake(s_mutex, portMAX_DELAY); uint8_t d = s_dirty; s_dirty = 0; xSemaphoreGive(s_mutex); return d; }
void state_mark_dirty(uint8_t f) { xSemaphoreTake(s_mutex, portMAX_DELAY); s_dirty |= f; xSemaphoreGive(s_mutex); }

int state_users_snapshot(user_t *out, int max)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    int n = s_user_count < max ? s_user_count : max;
    memcpy(out, s_users, n * sizeof(user_t));
    xSemaphoreGive(s_mutex);
    return n;
}
void state_users_replace(const user_t *in, int n)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (n > MAX_USERS) n = MAX_USERS;
    memcpy(s_users, in, n * sizeof(user_t)); s_user_count = n;
    xSemaphoreGive(s_mutex);
}
bool state_user_add(const char *uid, const char *name, bool admin)
{
    bool ok = false;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_user_count < MAX_USERS && !find_locked(uid)) {
        user_t *u = &s_users[s_user_count++];
        memset(u, 0, sizeof *u);
        snprintf(u->uid, sizeof u->uid, "%s", uid); snprintf(u->name, sizeof u->name, "%s", name); u->admin = admin;
        for (char *p = u->name; *p; p++) if (*p == ',') *p = ' ';   /* keep the CSV valid */
        s_dirty |= DIRTY_USERS; ok = true;
    }
    xSemaphoreGive(s_mutex);
    return ok;
}
bool state_user_remove(const char *uid)
{
    bool ok = false;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < s_user_count; i++)
        if (strcmp(s_users[i].uid, uid) == 0) { s_users[i] = s_users[--s_user_count]; s_dirty |= DIRTY_USERS; ok = true; break; }
    xSemaphoreGive(s_mutex);
    return ok;
}
void state_print_users(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < s_user_count; i++) printf("  %-14s %-23s %s%s\n", s_users[i].uid, s_users[i].name, s_users[i].admin ? "admin " : "user  ", s_users[i].inside ? "(inside)" : "");
    xSemaphoreGive(s_mutex);
}

void state_set_last(const char *t)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    int i = 0;
    for (; i < 16 && t[i]; i++) s_last[i] = t[i];
    s_last[i] = 0;
    xSemaphoreGive(s_mutex);
}
void state_get_last(char out[17])
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    int i = 0;
    for (; s_last[i]; i++) out[i] = s_last[i];
    out[i] = 0;
    xSemaphoreGive(s_mutex);
}
