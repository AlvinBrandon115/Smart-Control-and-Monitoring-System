#pragma once
#include <stdbool.h>
#include "app_types.h"
#define SD_FILE(name) "/sdcard/" name        /* use 8.3 file names */

bool storage_init(void);                     /* mount SD, load users + schedule (call after state_init) */
bool storage_ready(void);
void storage_lock(void);                     /* serialise file access between tasks */
void storage_unlock(void);
bool storage_commit(const char *tmp_path, const char *final_path);   /* replace a file with a fully written temp file */
bool storage_append(const char *file_name, const char *line);
bool storage_save_users(void);
bool storage_save_config(void);
void storage_persist_task(void *arg);        /* saves users/schedule shortly after they change */
