/* OWNER: Member 3. Admin authentication. Passwords are never stored in clear text:
 * the SD card holds salt + SHA-256(salt + password). Three wrong attempts lock logins for 30 s.
 * Used only from serial_menu_task (single task), and auth_init() runs before the tasks start. */
#include <stdio.h>
#include <string.h>
#include "mbedtls/sha256.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "storage.h"
#include "logger.h"
#include "auth.h"

#define MAX_ADMINS 4
typedef struct { char name[16]; char salt[9]; char hash[65]; } admin_t;
static admin_t s_admins[MAX_ADMINS];
static int s_count, s_fails;
static TickType_t s_locked_until;

static void hash_pw(const char *salt, const char *pass, char out[65])
{
    char buf[80]; unsigned char h[32];
    snprintf(buf, sizeof buf, "%s%s", salt, pass);
    mbedtls_sha256((const unsigned char *)buf, strlen(buf), h, 0);
    for (int i = 0; i < 32; i++) sprintf(out + 2 * i, "%02x", h[i]);
}

static void make_admin(admin_t *a, const char *user, const char *pass)
{
    snprintf(a->name, sizeof a->name, "%s", user);
    snprintf(a->salt, sizeof a->salt, "%08x", (unsigned)esp_random());
    hash_pw(a->salt, pass, a->hash);
}

static bool save_admins(void)
{
    if (!storage_ready()) return false;
    storage_lock();
    FILE *f = fopen(SD_FILE("admin.tmp"), "w");
    bool ok = f != NULL;
    if (f) {
        for (int i = 0; i < s_count; i++) fprintf(f, "%s,%s,%s\n", s_admins[i].name, s_admins[i].salt, s_admins[i].hash);
        ok = (fclose(f) == 0) && storage_commit(SD_FILE("admin.tmp"), SD_FILE("admin.csv"));
    }
    storage_unlock();
    return ok;
}

void auth_init(void)
{
    s_count = 0;
    if (storage_ready()) {
        storage_lock();
        FILE *f = fopen(SD_FILE("admin.csv"), "r");
        char line[128];
        while (f && s_count < MAX_ADMINS && fgets(line, sizeof line, f)) {
            admin_t a = { 0 };
            if (sscanf(line, "%15[^,],%8[^,],%64s", a.name, a.salt, a.hash) == 3) s_admins[s_count++] = a;
        }
        if (f) fclose(f);
        storage_unlock();
    }
    if (s_count == 0) {                          /* first start: default account, change the password with 'passwd' */
        make_admin(&s_admins[0], "admin", "admin123"); s_count = 1;
        save_admins();
        printf("Default admin created (user: admin, password: admin123). Change it with 'passwd'.\n");
    }
}

bool auth_login(const char *user, const char *pass, int *wait_s)
{
    TickType_t now = xTaskGetTickCount();
    *wait_s = 0;
    if (now < s_locked_until) { *wait_s = (int)((s_locked_until - now) / configTICK_RATE_HZ) + 1; return false; }
    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_admins[i].name, user) != 0) continue;
        char h[65]; hash_pw(s_admins[i].salt, pass, h);
        if (strcmp(h, s_admins[i].hash) == 0) { s_fails = 0; return true; }
    }
    if (++s_fails >= 3) { s_locked_until = now + pdMS_TO_TICKS(30000); s_fails = 0; log_event(LOG_SYSTEM, "LOGIN LOCKOUT,3 failed attempts"); }
    return false;
}

bool auth_set_password(const char *user, const char *new_pass)
{
    for (int i = 0; i < s_count; i++)
        if (strcmp(s_admins[i].name, user) == 0) { make_admin(&s_admins[i], user, new_pass); return save_admins(); }
    return false;
}
