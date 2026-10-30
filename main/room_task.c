/* OWNER: Member 4. One task per room: PIR in, lights out, events to access_ctrl via q_room. */
#include "driver/gpio.h"
#include "app_config.h"
#include "tasks.h"
#include "state.h"

void room_task(void *arg)
{
    uint8_t room = (uint8_t)(intptr_t)arg;
    int pir = room ? PIN_ROOM2_PIR : PIN_ROOM1_PIR, light = room ? PIN_ROOM2_LIGHT : PIN_ROOM1_LIGHT;
    gpio_config_t in = { .pin_bit_mask = 1ULL << pir, .mode = GPIO_MODE_INPUT };
    gpio_config_t out = { .pin_bit_mask = 1ULL << light, .mode = GPIO_MODE_OUTPUT };
    gpio_config(&in); gpio_config(&out);
    bool occupied = false; time_t last_motion = 0;
    for (;;) {
        heartbeat_beat(room ? T_ROOM2 : T_ROOM1);
        time_t now = time(NULL);
        if (gpio_get_level(pir)) last_motion = now;
        bool want = gpio_get_level(pir) || (occupied && now - last_motion < ROOM_EMPTY_TIMEOUT_S);
        if (want != occupied) {
            occupied = want;
            gpio_set_level(light, occupied);
            room_msg_t m = { room, occupied, now };
            xQueueSend(q_room, &m, pdMS_TO_TICKS(100));
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
