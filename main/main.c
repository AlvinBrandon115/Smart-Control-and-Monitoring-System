#include <stdio.h>
#include "app_types.h"
#include "state.h"
#include "tasks.h"
#include "storage.h"
#include "auth.h"
#include "rfid_mfrc522.h"

QueueHandle_t q_scan, q_door, q_room, q_log;

void app_main(void)
{
    setvbuf(stdin, NULL, _IONBF, 0);
    q_scan = xQueueCreate(8, sizeof(scan_msg_t));
    q_door = xQueueCreate(8, sizeof(door_msg_t));
    q_room = xQueueCreate(8, sizeof(room_msg_t));
    q_log  = xQueueCreate(32, sizeof(log_msg_t));
    state_init();
    i2c_bus_init();
    storage_init();
    auth_init();
    mfrc522_bus_init();

    xTaskCreate(logger_task,      "logger",  4096, NULL, 5, NULL);
    xTaskCreate(access_ctrl_task, "access",  4096, NULL, 6, NULL);
    xTaskCreate(door_task,        "door",    3072, NULL, 5, NULL);
    xTaskCreate(rfid_task,        "rfid_in", 3072, (void *)GATE_ENTRY, 5, NULL);
    xTaskCreate(rfid_task,        "rfid_out",3072, (void *)GATE_EXIT,  5, NULL);
    xTaskCreate(room_task,        "room1",   3072, (void *)0, 4, NULL);
    xTaskCreate(room_task,        "room2",   3072, (void *)1, 4, NULL);
    xTaskCreate(schedule_task,    "sched",   3072, NULL, 3, NULL);
    xTaskCreate(button_task,      "buttons", 3072, NULL, 4, NULL);
    xTaskCreate(display_task,     "display", 4096, NULL, 2, NULL);
    xTaskCreate(storage_persist_task, "persist", 4096, NULL, 3, NULL);
    xTaskCreate(monitor_task,     "monitor", 3072, NULL, 3, NULL);
    xTaskCreate(serial_menu_task, "menu",    4096, NULL, 2, NULL);
}
