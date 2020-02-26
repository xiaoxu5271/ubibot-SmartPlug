#include <stdio.h>
#include <string.h>
#include <cJSON.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "Cache_data.h"
#include "Http.h"
#include "Json_parse.h"
#include "Led.h"
#include "Bluetooth.h"
#include "CSE7759B.h"
#include "my_spi_flash.h"
#include "E2prom.h"

#define TAG "Cache_Data"

uint32_t flash_used_num = 0; //数据缓存的截至地址
uint32_t data_save_num = 0;  //缓存的数据组数
uint32_t last_save_num = 0;  //最后一条数据的起始地址
uint32_t start_read_num = 0; //读取数据的开始地址
SemaphoreHandle_t Cache_muxtex = NULL;

void Data_Read_Task(void *pvParameters)
{
    char *OutBuffer;
    uint8_t len = 0;
    cJSON *pJsonRoot;
    while (1)
    {
        //发送成功后，判断当前使用flash是否大一个扇区，大于的话再擦除
        ulTaskNotifyTake(pdTRUE, -1);
        ESP_LOGI(TAG, "start send data！");
    }
}

void DataSave(uint8_t *sava_buff, uint16_t Buff_len)
{
    xSemaphoreTake(Cache_muxtex, -1);
    flash_used_num = AT24CXX_ReadLenByte(flash_used_num_add, 4);
    data_save_num = AT24CXX_ReadLenByte(data_save_num_add, 4);
    ESP_LOGI(TAG, "flash_used_num=%d", flash_used_num);
    if (flash_used_num + Buff_len >= SPI_FLASH_SIZE) //如果写入新数据后大须总容量，从头开始写
    {
        flash_used_num = 0;
    }
    W25QXX_Write(sava_buff, flash_used_num, Buff_len);
    data_save_num++;
    flash_used_num += Buff_len;
    AT24CXX_WriteLenByte(flash_used_num_add, flash_used_num, 4);
    xSemaphoreGive(Cache_muxtex);
}

void Start_Cache(void)
{
    Cache_muxtex = xSemaphoreCreateMutex();
    xTaskCreate(Data_Read_Task, "Data_Read_Task", 8192, NULL, 5, &Binary_dp);
}