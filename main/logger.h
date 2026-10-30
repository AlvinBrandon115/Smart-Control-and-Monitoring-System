#pragma once
#include "app_types.h"
void log_event(log_type_t type, const char *fmt, ...);  /* non-blocking: queues a record for logger_task */
void log_print_recent(int n);                            /* prints last n records from RAM */
