#include <stdio.h>
#include <string.h>
#include <cJSON.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
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

uint32_t flash_used_num; //数据缓存的截至地址

void DataSaveTask(void *pvParameters)
{
    char *OutBuffer;
    uint8_t len = 0;
    cJSON *pJsonRoot;
    while (1)
    {
        //消息队列，等待filed传入,同类数据发送列队，使用结构体发送，可以一次构建多个field,节省空间
        //发送成功后，判断当前使用flash是否大一个扇区，大于的话再擦除

        pJsonRoot = cJSON_CreateObject();
        cJSON_AddStringToObject(pJsonRoot, "created_at", "2020-02-20T08:52:08Z");
        cJSON_AddNumberToObject(pJsonRoot, "field1", 1.001);
        cJSON_AddNumberToObject(pJsonRoot, "field2", 1.002);
        OutBuffer = cJSON_PrintUnformatted(pJsonRoot); //cJSON_Print(Root)
        cJSON_Delete(pJsonRoot);                       //delete cjson root
        len = strlen(OutBuffer);
        printf("len:%d,%s", len, OutBuffer);
        // memcpy(SaveBuffer, OutBuffer, strlen(OutBuffer));
        free(OutBuffer);

        // W25QXX_Write(SaveBuffer, 0, sizeof(SaveBuffer));
    }
}

void DataSave(uint8_t *sava_buff, uint16_t Buff_len)
{
    flash_used_num = AT24CXX_ReadLenByte(save_data_add, 4);
    if (flash_used_num + Buff_len >= SPI_FLASH_SIZE) //读写到头，从头开始写
    {
        flash_used_num = 0;
    }
    W25QXX_Write(sava_buff, flash_used_num, Buff_len);
    flash_used_num += Buff_len;
    AT24CXX_WriteLenByte(save_data_add, flash_used_num, 4);
}