#include <stdio.h>
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "Smartconfig.h"
#include "Http.h"
#include "Mqtt.h"
#include "Bluetooth.h"
#include "Json_parse.h"
#include "esp_log.h"

#include "Uart0.h"
#include "Wind.h"
#include "CSE7759B.h"
#include "Led.h"
#include "E2prom.h"
#include "RtcUsr.h"
#include "Switch.h"
#include "ota.h"
#include "ds18b20.h"

#include "esp_system.h"

void timer_periodic_cb(void *arg);

esp_timer_handle_t timer_periodic_handle = 0; //定时器句柄

esp_timer_create_args_t timer_periodic_arg = {
    .callback =
        &timer_periodic_cb,
    .arg = NULL,
    .name = "PeriodicTimer"};

void timer_periodic_cb(void *arg) //1ms中断一次
{
    static int64_t timer_count = 0;
    timer_count++;
    if (timer_count >= 1000) //1s
    {
        timer_count = 0;

        ESP_LOGI("", "free Heap:%d,%d", esp_get_free_heap_size(), heap_caps_get_free_size(MALLOC_CAP_8BIT));
        float temp = 0;
        ds18b20_get_temp(&temp);
        printf("temp2=%f\n", temp);
    }
}

static void Uart0_Task(void *arg)
{
    while (1)
    {
        Uart0_read();
        vTaskDelay(10 / portTICK_RATE_MS);
    }
}

/*
  EEPROM PAGE0 
    0x00 APIkey(32byte)
    0x20 chnnel_id(4byte)
    0x30 Serial_No(16byte)
    0x40 Protuct_id(32byte)
  EEPROM PAGE1+PAGE2
    0X00 bluesave  (512byte)

  */

void app_main(void)
{
    // nvs_flash_erase();
    ESP_ERROR_CHECK(nvs_flash_init());

    Led_Init();
    Wind_Init();
    CSE7759B_Init();
    E2prom_Init();
    Uart0_Init();
    Switch_Init();

    xTaskCreate(Uart0_Task, "Uart0_Task", 4096, NULL, 10, NULL);

    /*step1 判断是否有序列号和product id****/
    E2prom_Read(0x30, (uint8_t *)SerialNum, 16);
    printf("SerialNum=%s\n", SerialNum);
    printf("SerialNum_size=%d\n", strlen(SerialNum));

    E2prom_Read(0x40, (uint8_t *)ProductId, 32);
    printf("ProductId=%s\n", ProductId);

    if ((SerialNum[0] == 0xff) && (SerialNum[1] == 0xff)) //新的eeprom，先清零
    {
        printf("new eeprom\n");
        char zero_data[256];
        bzero(zero_data, sizeof(zero_data));
        E2prom_Write(0x00, (uint8_t *)zero_data, 256);
        E2prom_BluWrite(0x00, (uint8_t *)zero_data, 256); //清空蓝牙

        E2prom_Read(0x30, (uint8_t *)SerialNum, 16);
        printf("SerialNum=%s\n", SerialNum);

        E2prom_Read(0x40, (uint8_t *)ProductId, 32);
        printf("ProductId=%s\n", ProductId);
    }

    if ((strlen(SerialNum) == 0) || (strlen(ProductId) == 0)) //未获取到序列号或productid，未烧写序列号
    {

        printf("no SerialNum or product id!\n");
        while (1)
        {
            //故障灯闪烁
            Led_Status = LED_STA_NOSER;
            vTaskDelay(500 / portTICK_RATE_MS);
        }
    }

    ble_app_start();
    init_wifi();

    /*step2 判断是否有蓝牙配置信息****/
    if (read_bluetooth() == 0) //未获取到蓝牙配置信息
    {
        printf("no Ble message!waiting for ble message\n");
        Ble_mes_status = BLEERR;
        while (1)
        {
            //故障灯闪烁
            Led_Status = LED_STA_TOUCH;
            vTaskDelay(500 / portTICK_RATE_MS);
            //待蓝牙配置正常后，退出
            if (Ble_mes_status == BLEOK)
            {
                break;
            }
        }
    }

    /*step3 判断是否有API-KEY和channel-id****/
    E2prom_Read(0x00, (uint8_t *)ApiKey, 32);
    printf("readApiKey=%s\n", ApiKey);
    E2prom_Read(0x20, (uint8_t *)ChannelId, 16);
    printf("readChannelId=%s\n", ChannelId);
    if ((strlen(SerialNum) == 0) || (strlen(ChannelId) == 0)) //未获取到API-KEY，和channelid进行激活流程
    {
        printf("no ApiKey or channelId!\n");

        while (http_activate() == 0) //激活失败
        {
            vTaskDelay(10000 / portTICK_RATE_MS);
        }

        //激活成功
        E2prom_Read(0x00, (uint8_t *)ApiKey, 32);
        printf("ApiKey=%s\n", ApiKey);
        E2prom_Read(0x20, (uint8_t *)ChannelId, 16);
        printf("ChannelId=%s\n", ChannelId);
    }

    /*******************************timer 1s init**********************************************/
    esp_err_t err = esp_timer_create(&timer_periodic_arg, &timer_periodic_handle);
    err = esp_timer_start_periodic(timer_periodic_handle, 1000); //创建定时器，单位us，定时1ms
    if (err != ESP_OK)
    {
        printf("timer periodic create err code:%d\n", err);
    }

    initialise_http();
    initialise_mqtt();

    //ota_start();
}
