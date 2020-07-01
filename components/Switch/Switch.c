#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "Cache_data.h"
#include "ServerTimer.h"
#include "Switch.h"
#include "Http.h"
#include "Json_parse.h"
#include "Led.h"
#include "Bluetooth.h"
#include "CSE7759B.h"
#include "E2prom.h"
#include "Smartconfig.h"

#include "Json_parse.h"

#define TAG "SWITCH"

// static const char *TAG = "switch";
uint64_t SW_last_time = 0, SW_on_time = 0;

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
    Create_Switch_Json(); //构建开关状态
    if (Binary_energy != NULL)
    {
        xTaskNotifyGive(Binary_energy);
    }
    if (Binary_dp != NULL)
    {
        xTaskNotifyGive(Binary_dp);
    }
    if (de_sw_s == 2)
    {
        E2P_WriteOneByte(LAST_SWITCH_ADD, mqtt_json_s.mqtt_switch_status); //写入开关状态
    }

    if (mqtt_json_s.mqtt_switch_status == 1)
    {
        SW_last_time = (uint64_t)esp_timer_get_time();
    }
    else
    {
        //累加单次开启时长
        SW_on_time += (uint64_t)esp_timer_get_time() - SW_last_time;
    }
}

//读取，构建累积开启时长
void Sw_on_quan_Task(void *pvParameters)
{
    char *filed_buff;
    char *OutBuffer;
    char *time_buff;
    // uint8_t *SaveBuffer;
    uint16_t len = 0;
    cJSON *pJsonRoot;

    while (1)
    {
        ulTaskNotifyTake(pdTRUE, -1);

        //仍处于开启状态
        if (mqtt_json_s.mqtt_switch_status == 1)
        {
            SW_on_time += (uint64_t)esp_timer_get_time() - SW_last_time;
            SW_last_time = (uint64_t)esp_timer_get_time();
        }
        ESP_LOGI(TAG, "SW_on_time:%lld", SW_on_time);

        if ((xEventGroupGetBits(Net_sta_group) & TIME_CAL_BIT) == TIME_CAL_BIT)
        {
            filed_buff = (char *)malloc(9);
            time_buff = (char *)malloc(24);
            Server_Timer_SEND(time_buff);
            pJsonRoot = cJSON_CreateObject();
            cJSON_AddStringToObject(pJsonRoot, "created_at", (const char *)time_buff);
            snprintf(filed_buff, 9, "field%d", sw_on_f_num);
            cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber((uint64_t)((SW_on_time + 500000) / 1000000))); //四舍五入
            OutBuffer = cJSON_PrintUnformatted(pJsonRoot);                                                                 //cJSON_Print(Root)
            cJSON_Delete(pJsonRoot);                                                                                       //delete cjson root
            len = strlen(OutBuffer);
            ESP_LOGI(TAG, "len:%d\n%s\n", len, OutBuffer);

            xSemaphoreTake(Cache_muxtex, -1);
            DataSave((uint8_t *)OutBuffer, len);
            xSemaphoreGive(Cache_muxtex);
            free(OutBuffer);
            free(filed_buff);
            free(time_buff);
            //清空统计
            SW_on_time = 0;
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
    xTaskCreate(Sw_on_quan_Task, "sw on quan", 4096, NULL, 5, &Sw_on_Task_Handle);
}