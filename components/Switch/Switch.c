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
#include "CSE7759B.h"

// static const char *TAG = "switch";

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

//切换继电器
void Switch_Relay(int8_t set_value)
{
    if (set_value == -1)
    {
        mqtt_json_s.mqtt_switch_status = !mqtt_json_s.mqtt_switch_status;
    }

    else if (set_value >= 1 && set_value < 100)
    {
        if (mqtt_json_s.mqtt_switch_status != 1)
        {
            mqtt_json_s.mqtt_switch_status = 1;
        }
    }
    else if (set_value == 0)
    {
        if (mqtt_json_s.mqtt_switch_status != 0)
        {
            mqtt_json_s.mqtt_switch_status = 0;
        }
    }
    gpio_set_level(GPIO_RLY, mqtt_json_s.mqtt_switch_status);
    if (Binary_energy != NULL)
    {
        xTaskNotifyGive(Binary_energy);
    }
}
