#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "Switch.h"
#include "Http.h"
#include "Json_parse.h"
#include "Led.h"
#include "Bluetooth.h"

static const char *TAG = "switch";

void Switch_Init(void)
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << GPIO_RLY);
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    gpio_set_level(GPIO_RLY, mqtt_json_s.mqtt_switch_status);
}

void Key_Switch_Relay(void)
{
    mqtt_json_s.mqtt_switch_status = !mqtt_json_s.mqtt_switch_status;
    gpio_set_level(GPIO_RLY, mqtt_json_s.mqtt_switch_status);
    need_send = 1;
}

void Mqtt_Switch_Relay(uint8_t set_value)
{
    if (set_value >= 1 && set_value < 100)
    {
        mqtt_json_s.mqtt_switch_status = 1;
    }
    else if (set_value == 0)
    {
        mqtt_json_s.mqtt_switch_status = 0;
    }
    gpio_set_level(GPIO_RLY, mqtt_json_s.mqtt_switch_status);
    need_send = 1;
}
