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

void Switch_interrupt_callBack(void *arg);
static xQueueHandle gpio_evt_queue = NULL; //定义一个队列返回变量

void Switch_interrupt_callBack(void *arg)
{
    uint32_t io_num;
    while (1)
    {
        //不断读取gpio队列，读取完后将删除队列
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) //队列阻塞等待
        {
            vTaskDelay(5 / portTICK_RATE_MS);
            ESP_LOGW(TAG, "key_interrupt,gpio[%d]=%d\n", io_num, gpio_get_level(io_num));
            if (gpio_get_level(io_num) == 0) //按下
            {
                printf("key down!\n");
                mqtt_json_s.mqtt_switch_status = !mqtt_json_s.mqtt_switch_status;
                gpio_set_level(GPIO_RLY, mqtt_json_s.mqtt_switch_status);
                http_send_mes();
            }
            else //抬起
            {
            }
        }
    }
}

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

void Switch_Relay(void)
{
    mqtt_json_s.mqtt_switch_status = !mqtt_json_s.mqtt_switch_status;
    gpio_set_level(GPIO_RLY, mqtt_json_s.mqtt_switch_status);
    http_send_mes();
}
