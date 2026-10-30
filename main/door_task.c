/* OWNER: Member 1. Moves the door servos and sounds the buzzer; makes no decisions.
 * Servos use LEDC PWM at 50 Hz: a 0.5-2.4 ms pulse sets the angle (closed = 0 deg, open = about 90 deg). */
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "app_config.h"
#include "tasks.h"

#define SERVO_RES_BITS   LEDC_TIMER_14_BIT
#define SERVO_PERIOD_US  20000
static const int s_servo_pin[2] = { PIN_DOOR_ENTRY, PIN_DOOR_EXIT };

static void servo_init(void)
{
    ledc_timer_config_t t = { .speed_mode = LEDC_LOW_SPEED_MODE, .duty_resolution = SERVO_RES_BITS,
                              .timer_num = LEDC_TIMER_0, .freq_hz = 50, .clk_cfg = LEDC_AUTO_CLK };
    ledc_timer_config(&t);
    for (int g = 0; g < 2; g++) {
        ledc_channel_config_t c = { .gpio_num = s_servo_pin[g], .speed_mode = LEDC_LOW_SPEED_MODE, .channel = (ledc_channel_t)g,
                                    .intr_type = LEDC_INTR_DISABLE, .timer_sel = LEDC_TIMER_0, .duty = 0, .hpoint = 0 };
        ledc_channel_config(&c);
    }
}

static void servo_pulse(gate_t g, int pulse_us)
{
    uint32_t duty = (uint32_t)(((uint64_t)pulse_us * ((1u << SERVO_RES_BITS) - 1)) / SERVO_PERIOD_US);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)g, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)g);
}

static void door_set(gate_t g, bool open) { servo_pulse(g, open ? SERVO_OPEN_US : SERVO_CLOSED_US); }
static void beep(int ms) { gpio_set_level(PIN_BUZZER, 1); vTaskDelay(pdMS_TO_TICKS(ms)); gpio_set_level(PIN_BUZZER, 0); }

void door_task(void *arg)
{
    gpio_config_t io = { .pin_bit_mask = 1ULL << PIN_BUZZER, .mode = GPIO_MODE_OUTPUT };
    gpio_config(&io);
    servo_init();
    door_set(GATE_ENTRY, false); door_set(GATE_EXIT, false);          /* start with both doors closed */
    door_msg_t m;
    for (;;) {
        xQueueReceive(q_door, &m, portMAX_DELAY);
        switch (m.cmd) {
        case DOOR_CMD_OPEN:      door_set(m.gate, true); vTaskDelay(pdMS_TO_TICKS(DOOR_OPEN_MS)); door_set(m.gate, false); break;
        case DOOR_CMD_DENY:      beep(300); break;
        case DOOR_CMD_HOLD_OPEN: door_set(m.gate, true); break;          /* emergency: stays open until CLOSE */
        case DOOR_CMD_CLOSE:     door_set(m.gate, false); break;
        case DOOR_CMD_ALARM:     for (int i = 0; i < 3; i++) { beep(150); vTaskDelay(pdMS_TO_TICKS(150)); } break;
        }
    }
}
