/* OWNER: Member 4 (innovation features). Two push buttons (to GND, internal pull-ups):
 * the emergency button toggles the emergency exit override (logged), the page button cycles the LCD pages. */
#include "driver/gpio.h"
#include "app_config.h"
#include "state.h"
#include "tasks.h"

void button_task(void *arg)
{
    gpio_config_t io = { .pin_bit_mask = (1ULL << PIN_BTN_EMERGENCY) | (1ULL << PIN_BTN_PAGE), .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE };
    gpio_config(&io);
    int last_e = 1, last_p = 1;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(30));                       /* polling interval doubles as debounce */
        int e = gpio_get_level(PIN_BTN_EMERGENCY), p = gpio_get_level(PIN_BTN_PAGE);
        if (last_e && !e) emergency_apply(!emergency_get(), "emergency button");
        if (last_p && !p) display_next_page();
        last_e = e; last_p = p;
    }
}
