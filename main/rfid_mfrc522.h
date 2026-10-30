#pragma once
#include <stdbool.h>
#include "app_types.h"

bool mfrc522_bus_init(void);                         /* call ONCE, before starting the RFID tasks */
bool mfrc522_init(gate_t gate);                      /* per reader: adds the SPI device, resets and configures the chip */
bool mfrc522_read_uid(gate_t gate, char *uid_hex);   /* true if a card was read; uid_hex needs 15 bytes */
