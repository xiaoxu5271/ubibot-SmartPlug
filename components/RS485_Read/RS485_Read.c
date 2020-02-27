#include <stdio.h>
#include "string.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "Uart0.h"
#include "Http.h"
#include "Cache_data.h"
#include "ServerTimer.h"

#include "RS485_Read.h"

#define TAG "458"

#define UART2_TXD (GPIO_NUM_17)
#define UART2_RXD (GPIO_NUM_16)
#define UART2_RTS (UART_PIN_NO_CHANGE)
#define UART2_CTS (UART_PIN_NO_CHANGE)

#define RS485RD 21

#define BUF_SIZE 128

SemaphoreHandle_t RS485_Mutex = NULL;
float ext_tem = 0.0, ext_hum = 0.0;
bool RS485_status = false;

char wind_modbus_send_data[] = {0x20, 0x04, 0x00, 0x06, 0x00, 0x01, 0xd7, 0x7a};    //发送查询风速指令
char tem_hum_modbus_send_data[] = {0xC1, 0x03, 0x00, 0x00, 0x00, 0x02, 0xD5, 0x0B}; //发送 查询外接空气温湿度探头
char Sth_modbus_send_data[] = {0xC2, 0x03, 0x00, 0x00, 0x00, 0x02, 0xD5, 0x0B};     //发送 查询土壤探头

/*******************************************************************************
// CRC16_Modbus Check
*******************************************************************************/
static uint16_t CRC16_ModBus(uint8_t *buf, uint8_t buf_len)
{
    uint8_t i, j;
    uint8_t c_val;
    uint16_t crc16_val = 0xffff;
    uint16_t crc16_poly = 0xa001; //0x8005

    for (i = 0; i < buf_len; i++)
    {
        crc16_val = *buf ^ crc16_val;

        for (j = 0; j < 8; j++)
        {
            c_val = crc16_val & 0x01;

            crc16_val = crc16_val >> 1;

            if (c_val)
            {
                crc16_val = crc16_val ^ crc16_poly;
            }
        }
        buf++;
    }
    // printf("crc16_val=%x\n", ((crc16_val & 0x00ff) << 8) | ((crc16_val & 0xff00) >> 8));
    return ((crc16_val & 0x00ff) << 8) | ((crc16_val & 0xff00) >> 8);
}

int8_t RS485_Read(char *Send_485_Buff)
{
    uint8_t data_u2[BUF_SIZE];
    // gpio_set_level(RS485RD, 1); //RS485输出模式
    sw_uart2(uart2_485);
    uart_write_bytes(UART_NUM_2, Send_485_Buff, 8);
    // vTaskDelay(100 / portTICK_PERIOD_MS);
    // gpio_set_level(RS485RD, 0); //RS485输入模式
    int len2 = uart_read_bytes(UART_NUM_2, data_u2, BUF_SIZE, 100 / portTICK_RATE_MS);
    xSemaphoreGive(xMutex_uart2_sw);

    if (len2 != 0)
    {
        // printf("UART2 len2: %d  recv:", len2);
        // for (int i = 0; i < len2; i++)
        // {
        //     printf("%x ", data_u2[i]);
        // }
        // printf("\r\n");

        if ((data_u2[7] * 256 + data_u2[8]) == CRC16_ModBus(data_u2, (len2 - 2)))
        {
            if ((data_u2[0] == 0xC1) && (data_u2[1] == 0x03))
            {
                ext_tem = ((data_u2[3] << 8) + data_u2[4]) * 0.1;
                ext_hum = ((data_u2[5] << 8) + data_u2[6]) * 0.1;
                ESP_LOGI(TAG, "ext_tem=%f   ext_hum=%f\n", ext_tem, ext_hum);
                RS485_status = true;
                return 0;
            }
            else
            {
                RS485_status = false;
                ESP_LOGE(TAG, "485 add or cmd error\n");
                return 1;
            }
        }
        else
        {
            RS485_status = false;
            ESP_LOGE(TAG, "485 CRC error\n");
            return 1;
        }
    }
    else
    {
        RS485_status = false;
        ESP_LOGE(TAG, "RS485 NO ARK !!! \n");
        return 1;
    }
}

//构建 空气探头 数据包
void Create_485th_Json(void)
{
    char *OutBuffer;
    uint8_t *SaveBuffer;
    uint16_t len = 0;
    cJSON *pJsonRoot;
    pJsonRoot = cJSON_CreateObject();
    cJSON_AddStringToObject(pJsonRoot, "created_at", (const char *)Server_Timer_SEND());
    cJSON_AddItemToObject(pJsonRoot, "field8", cJSON_CreateNumber(ext_tem));
    cJSON_AddItemToObject(pJsonRoot, "field9", cJSON_CreateNumber(ext_hum));
    OutBuffer = cJSON_PrintUnformatted(pJsonRoot); //cJSON_Print(Root)
    cJSON_Delete(pJsonRoot);                       //delete cjson root
    len = strlen(OutBuffer);
    printf("len:%d\n%s\n", len, OutBuffer);
    // SaveBuffer = (uint8_t *)malloc(len);
    SaveBuffer = (uint8_t *)malloc(len);
    memcpy(SaveBuffer, OutBuffer, len);
    xSemaphoreTake(Cache_muxtex, -1);
    DataSave(SaveBuffer, len);
    xSemaphoreGive(Cache_muxtex);
    free(OutBuffer);
    free(SaveBuffer);
}
//读取485 空气探头
void read_485_th_task(void *pvParameters)
{
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, -1);
        xSemaphoreTake(RS485_Mutex, -1);
        while (RS485_Read(tem_hum_modbus_send_data))
        {
            vTaskDelay(2000 / portTICK_RATE_MS);
        }
        Create_485th_Json();
        xSemaphoreGive(RS485_Mutex);
    }
}

//构建 土壤探头 数据包
void Create_485sth_Json(void)
{
    char *OutBuffer;
    uint8_t *SaveBuffer;
    uint16_t len = 0;
    cJSON *pJsonRoot;
    pJsonRoot = cJSON_CreateObject();
    cJSON_AddStringToObject(pJsonRoot, "created_at", (const char *)Server_Timer_SEND());
    cJSON_AddItemToObject(pJsonRoot, "field10", cJSON_CreateNumber(ext_tem));
    cJSON_AddItemToObject(pJsonRoot, "field11", cJSON_CreateNumber(ext_hum));
    OutBuffer = cJSON_PrintUnformatted(pJsonRoot); //cJSON_Print(Root)
    cJSON_Delete(pJsonRoot);                       //delete cjson root
    len = strlen(OutBuffer);
    printf("len:%d\n%s\n", len, OutBuffer);
    // SaveBuffer = (uint8_t *)malloc(len);
    SaveBuffer = (uint8_t *)malloc(len);
    memcpy(SaveBuffer, OutBuffer, len);
    xSemaphoreTake(Cache_muxtex, -1);
    DataSave(SaveBuffer, len);
    xSemaphoreGive(Cache_muxtex);
    free(OutBuffer);
    free(SaveBuffer);
}
//读取485 土壤探头
void read_485_sth_task(void *pvParameters)
{
    ulTaskNotifyTake(pdTRUE, -1);
    xSemaphoreTake(RS485_Mutex, -1);
    while (RS485_Read(Sth_modbus_send_data))
    {
        vTaskDelay(2000 / portTICK_RATE_MS);
    }
    Create_485sth_Json();
    xSemaphoreGive(RS485_Mutex);
}

void RS485_Init(void)
{
    RS485_Mutex = xSemaphoreCreateMutex();
    xTaskCreate(read_485_th_task, "read_485_th_task", 4096, NULL, 3, &Binary_485_th);
    xTaskCreate(read_485_sth_task, "read_485_sth_task", 4096, NULL, 3, &Binary_485_sth);
}



// /*******************************************************************************
//                                       END
// *******************************************************************************/