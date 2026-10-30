idf_component_register(
    SRCS "main.c" "state.c" "logger_task.c" "rfid_task.c" "rfid_mfrc522.c" "access_ctrl.c"
         "door_task.c" "room_task.c" "schedule_task.c" "serial_menu_task.c" "monitor_task.c" "storage.c" "auth.c"
    INCLUDE_DIRS "."
    REQUIRES driver mbedtls esp_timer fatfs sdmmc vfs)
