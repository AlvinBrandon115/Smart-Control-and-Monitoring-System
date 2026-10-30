/* OWNER: Member 3. Serial menu. Read-only commands are open; changes need admin login. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "app_config.h"
#include "state.h"
#include "logger.h"
#include "tasks.h"
#include "auth.h"
#include "driver/uart.h"

/* Reads typed characters straight from UART0 (the console). Echoes them, handles backspace,
 * and returns true when Enter (CR or LF) completes a line. More reliable than fgets() on the console. */
static bool read_line(char *out, size_t n)
{
    static char buf[96];
    static size_t len = 0;
    uint8_t c;
    while (uart_read_bytes(UART_NUM_0, &c, 1, pdMS_TO_TICKS(50)) == 1) {
        if (c == '\r' || c == '\n') {
            uart_write_bytes(UART_NUM_0, "\r\n", 2);
            if (len == 0) continue;                       /* ignore empty lines and the LF of a CR+LF pair */
            buf[len] = 0; snprintf(out, n, "%s", buf); len = 0;
            return true;
        }
        if ((c == 0x08 || c == 0x7F) && len > 0) { len--; uart_write_bytes(UART_NUM_0, "\b \b", 3); continue; }
        if (c >= 32 && c < 127 && len < sizeof buf - 1) { buf[len++] = (char)c; uart_write_bytes(UART_NUM_0, (const char *)&c, 1); }
    }
    return false;
}

static void help(bool admin)
{
    puts("Commands: help, login <user> <password>, logout, status, health, time, inside, rooms, log [n]");
    if (admin) puts("Admin: schedule show | schedule set HH:MM HH:MM | override on|off | emergency on|off | time set YYYY-MM-DD HH:MM:SS | passwd <new> | user list | user add <UID> <name> | user del <UID>");
}

void serial_menu_task(void *arg)
{
    char line[96], who[24] = ""; bool admin = false;
    if (!uart_is_driver_installed(UART_NUM_0)) uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);
    puts("CoCIS Smart Control. Type 'help'.");
    for (;;) {
        if (!read_line(line, sizeof line)) continue;
        char a[16] = "", b[24] = "", c[24] = "";
        sscanf(line, "%15s %23s %23s", a, b, c);
        for (char *p = a; *p; p++) *p = (char)tolower((unsigned char)*p);   /* commands are not case-sensitive */
        if (!strcmp(a, "help")) help(admin);
        else if (!strcmp(a, "login")) {
            int wait = 0;
            admin = auth_login(b, c, &wait);
            if (admin) { snprintf(who, sizeof who, "%s", b); puts("Admin OK"); log_event(LOG_CONFIG, "login ok,%s", b); }
            else if (wait > 0) printf("Too many attempts. Try again in %d s\n", wait);
            else { puts("Wrong user or password"); log_event(LOG_CONFIG, "login FAILED,%s", b); }
        }
        else if (admin && !strcmp(a, "passwd")) {
            if (strlen(b) < 6) puts("Password must be at least 6 characters");
            else if (auth_set_password(who, b)) { puts("Password changed"); log_event(LOG_CONFIG, "password changed,%s", who); }
            else puts("Could not save the password");
        }
        else if (admin && !strcmp(a, "user") && !strcmp(b, "list")) state_print_users();
        else if (admin && !strcmp(a, "user") && !strcmp(b, "add")) {
            char uid[15], nm[24];
            if (sscanf(line, "user add %14s %23s", uid, nm) == 2 && state_user_add(uid, nm, false)) { log_event(LOG_CONFIG, "user added,%s,%s", uid, nm); puts("User added (saved to SD shortly)"); }
            else puts("Usage: user add <UID> <name>  (UID must be new)");
        }
        else if (admin && !strcmp(a, "user") && !strcmp(b, "del")) {
            if (state_user_remove(c)) { log_event(LOG_CONFIG, "user removed,%s", c); puts("User removed"); } else puts("No such user");
        }
        else if (!strcmp(a, "logout")) { admin = false; who[0] = 0; puts("Logged out"); }
        else if (!strcmp(a, "status")) { int o, cl; schedule_get(&o, &cl); printf("Inside: %d | Hours %02d:%02d-%02d:%02d | Override: %s | Emergency: %s\n", state_count_inside(), o / 60, o % 60, cl / 60, cl % 60, override_get() ? "ON" : "off", emergency_get() ? "ACTIVE" : "off"); }
        else if (!strcmp(a, "health")) { for (int t = 0; t < T_COUNT; t++) printf("  %-13s %s\n", terminal_name((terminal_t)t), terminal_online((terminal_t)t) ? "ONLINE" : "OFFLINE"); }
        else if (!strcmp(a, "inside")) state_print_inside();
        else if (!strcmp(a, "rooms")) printf("Room1: %s, Room2: %s (lights follow occupancy)\n", room_state_get(0) ? "occupied" : "empty", room_state_get(1) ? "occupied" : "empty");
        else if (!strcmp(a, "time") && !b[0]) { time_t n = time(NULL); struct tm t; localtime_r(&n, &t); printf("Time: %04d-%02d-%02d %02d:%02d:%02d (source: %s)\n", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, clock_source()); }
        else if (admin && !strcmp(a, "time") && !strcmp(b, "set")) {
            int y, mo, d, h, mi, s;
            if (sscanf(line, "time set %d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) == 6 && clock_set(y, mo, d, h, mi, s)) { log_event(LOG_CONFIG, "time set %04d-%02d-%02d %02d:%02d:%02d", y, mo, d, h, mi, s); puts("Time set"); }
            else puts("Usage: time set YYYY-MM-DD HH:MM:SS");
        }
        else if (!strcmp(a, "log")) log_print_recent(b[0] ? atoi(b) : 10);
        else if (admin && !strcmp(a, "schedule") && !strcmp(b, "show")) { int o, cl; schedule_get(&o, &cl); printf("%02d:%02d-%02d:%02d\n", o / 60, o % 60, cl / 60, cl % 60); }
        else if (admin && !strcmp(a, "schedule") && !strcmp(b, "set")) {
            int h1, m1, h2, m2; char *sp = strstr(line, c); (void)sp;
            if (sscanf(line, "schedule set %d:%d %d:%d", &h1, &m1, &h2, &m2) == 4) { schedule_set(h1 * 60 + m1, h2 * 60 + m2); log_event(LOG_CONFIG, "schedule set %02d:%02d-%02d:%02d", h1, m1, h2, m2); puts("Saved"); }
            else puts("Usage: schedule set HH:MM HH:MM");
        }
        else if (admin && !strcmp(a, "emergency")) {
            bool on = !strcmp(b, "on");
            emergency_set(on);
            door_msg_t dm = { GATE_EXIT, on ? DOOR_CMD_HOLD_OPEN : DOOR_CMD_CLOSE };
            xQueueSend(q_door, &dm, 0);
            log_event(LOG_CONFIG, "EMERGENCY %s", on ? "ACTIVATED: exit door held open" : "CLEARED");
            puts(on ? "EMERGENCY ACTIVE: exit door open, alarm sounding" : "Emergency cleared");
        }
        else if (admin && !strcmp(a, "override")) { override_set(!strcmp(b, "on")); log_event(LOG_CONFIG, "override %s", b); puts("Done"); }
        else if (!admin && (!strcmp(a, "passwd") || !strcmp(a, "user") || !strcmp(a, "schedule") || !strcmp(a, "override") || !strcmp(a, "emergency") || (!strcmp(a, "time") && b[0])))
            puts("Admin login required: login <user> <password>");
        else if (a[0]) printf("Unknown command '%s'. Type help.\n", a);
    }
}
