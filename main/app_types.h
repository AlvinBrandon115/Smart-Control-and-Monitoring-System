#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

typedef enum { GATE_ENTRY = 0, GATE_EXIT = 1 } gate_t;
typedef enum { DOOR_CMD_OPEN, DOOR_CMD_DENY, DOOR_CMD_HOLD_OPEN, DOOR_CMD_CLOSE, DOOR_CMD_ALARM } door_cmd_t;
typedef enum { LOG_ACCESS, LOG_ROOM, LOG_CONFIG, LOG_SYSTEM } log_type_t;

typedef struct { gate_t gate; char uid[15]; time_t ts; } scan_msg_t;   /* q_scan */
typedef struct { gate_t gate; door_cmd_t cmd; } door_msg_t;            /* q_door */
typedef struct { uint8_t room; bool occupied; time_t ts; } room_msg_t; /* q_room */
typedef struct { log_type_t type; time_t ts; char text[96]; } log_msg_t; /* q_log */

extern QueueHandle_t q_scan, q_door, q_room, q_log;
