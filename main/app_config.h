#pragma once
/* Pin map for the classic ESP32 DevKit (matches the Wokwi diagram.json). */
#define PIN_RFID_SCK   18
#define PIN_RFID_MISO  19
#define PIN_RFID_MOSI  23
#define PIN_ENTRY_CS    5
#define PIN_ENTRY_RST  16
#define PIN_EXIT_CS    17
#define PIN_EXIT_RST    4
#define PIN_SD_SCK     25
#define PIN_SD_MISO    26
#define PIN_SD_MOSI    27
#define PIN_SD_CS      32
#define PIN_I2C_SDA    21
#define PIN_I2C_SCL    22
#define PIN_DOOR_ENTRY 13
#define PIN_DOOR_EXIT  33
#define PIN_ROOM1_PIR  34
#define PIN_ROOM1_LIGHT 14
#define PIN_ROOM2_PIR  35
#define PIN_ROOM2_LIGHT 2
#define PIN_BUZZER     15

#define DOOR_OPEN_MS        3000
#define SERVO_CLOSED_US      544   /* pulse width for 0 degrees (door closed) */
#define SERVO_OPEN_US       1472   /* about 90 degrees (door open): tune in Wokwi */
#define ROOM_EMPTY_TIMEOUT_S  30   /* lights-off delay (innovation) */
#define SCAN_DEBOUNCE_S        2
#define MAX_USERS             32
