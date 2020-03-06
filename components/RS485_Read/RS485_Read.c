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

#define BUF_SIZE 32

SemaphoreHandle_t RS485_Mutex = NULL;
float ext_tem = 0.0, ext_hum = 0.0;
bool RS485_status = false;

char Rs485_wind_cmd[] = {0x20, 0x04, 0x00, 0x06, 0x00, 0x01, 0xd7, 0x7a}; //查询风速指令
char Rs485_th_cmd[] = {0xC1, 0x03, 0x00, 0x00, 0x00, 0x02, 0xD5, 0x0B};   //查询外接空气温湿度探头
char Rs485_sth_cmd[] = {0xC2, 0x03, 0x00, 0x00, 0x00, 0x02, 0xD5, 0x0B};  //查询土壤探头
char Rs485_t_cmd[] = {0xC2, 0x03, 0x00, 0x00, 0x00, 0x01, 0x95, 0x39};    //查询温度探头
char Rs485_ws_cmd[] = {0x20, 0x04, 0x00, 0x06, 0x00, 0x01, 0xD7, 0x7A};   //风速

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

int RS485_Read(char *Send_485_Buff, uint8_t *Recv_485_buff)
{
    sw_uart2(uart2_485);
    uart_write_bytes(UART_NUM_2, Send_485_Buff, 8);
    int len = uart_read_bytes(UART_NUM_2, Recv_485_buff, BUF_SIZE, 100 / portTICK_RATE_MS);
    xSemaphoreGive(xMutex_uart2_sw);

    return len;
}

//读取485 空气探头
void read_485_th_task(void *pvParameters)
{
    char *OutBuffer;
    uint8_t *SaveBuffer;
    uint16_t len = 0;
    cJSON *pJsonRoot;
    uint8_t *recv_data;
    int racv_len = 0;
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, -1);
        xSemaphoreTake(RS485_Mutex, -1);
        recv_data = (uint8_t *)malloc(BUF_SIZE);
        racv_len = RS485_Read(Rs485_th_cmd, recv_data);
        if (racv_len > 0)
        {
            esp_log_buffer_hex(TAG, recv_data, racv_len);

            if ((recv_data[7] * 256 + recv_data[8]) == CRC16_ModBus(recv_data, (racv_len - 2)))
            {
                if ((recv_data[0] == Rs485_th_cmd[0]) && (recv_data[1] == Rs485_th_cmd[1]))
                {
                    ext_tem = ((recv_data[3] << 8) + recv_data[4]) * 0.1;
                    ext_hum = ((recv_data[5] << 8) + recv_data[6]) * 0.1;
                    ESP_LOGI(TAG, "ext_tem=%f   ext_hum=%f\n", ext_tem, ext_hum);
                    RS485_status = true;
                    // 读取成功
                    pJsonRoot = cJSON_CreateObject();
                    cJSON_AddStringToObject(pJsonRoot, "created_at", (const char *)Server_Timer_SEND());
                    cJSON_AddItemToObject(pJsonRoot, "field8", cJSON_CreateNumber(ext_tem));
                    cJSON_AddItemToObject(pJsonRoot, "field9", cJSON_CreateNumber(ext_hum));
                    OutBuffer = cJSON_PrintUnformatted(pJsonRoot); //cJSON_Print(Root)
                    cJSON_Delete(pJsonRoot);                       //delete cjson root
                    len = strlen(OutBuffer);
                    printf("len:%d\n%s\n", len, OutBuffer);
                    SaveBuffer = (uint8_t *)malloc(len);
                    memcpy(SaveBuffer, OutBuffer, len);
                    xSemaphoreTake(Cache_muxtex, -1);
                    DataSave(SaveBuffer, len);
                    xSemaphoreGive(Cache_muxtex);
                    free(OutBuffer);
                    free(SaveBuffer);
                }
                else
                {
                    RS485_status = false;
                    ESP_LOGE(TAG, "485 add or cmd error\n");
                    // return 1;
                }
            }
            else
            {
                RS485_status = false;
                ESP_LOGE(TAG, "485 CRC error\n");
                // return 1;
            }
        }
        else
        {
            ESP_LOGE(TAG, "RS485 NO ARK !!! \n");
        }
        free(recv_data);
        xSemaphoreGive(RS485_Mutex);
    }
}

//读取485 温度探头
void read_485_t_task(void *pvParameters)
{
    float Rs485_t_val;
    char *OutBuffer;
    uint8_t *SaveBuffer;
    uint16_t len;
    cJSON *pJsonRoot;
    uint8_t *recv_data;
    int racv_len;
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, -1);
        xSemaphoreTake(RS485_Mutex, -1);
        recv_data = (uint8_t *)malloc(BUF_SIZE);
        racv_len = RS485_Read(Rs485_t_cmd, recv_data);
        if (racv_len > 0)
        {
            esp_log_buffer_hex(TAG, recv_data, racv_len);

            if ((recv_data[5] * 256 + recv_data[6]) == CRC16_ModBus(recv_data, (racv_len - 2)))
            {
                if ((recv_data[0] == Rs485_t_cmd[0]) && (recv_data[1] == Rs485_t_cmd[1]))
                {
                    Rs485_t_val = ((recv_data[3] << 8) + recv_data[4]) * 0.1;
                    ESP_LOGI(TAG, "Rs485_t_val=%f\n", Rs485_t_val);
                    // RS485_status = true;
                    // 读取成功
                    pJsonRoot = cJSON_CreateObject();
                    cJSON_AddStringToObject(pJsonRoot, "created_at", (const char *)Server_Timer_SEND());
                    cJSON_AddItemToObject(pJsonRoot, "field10", cJSON_CreateNumber(Rs485_t_val));
                    OutBuffer = cJSON_PrintUnformatted(pJsonRoot); //cJSON_Print(Root)
                    cJSON_Delete(pJsonRoot);                       //delete cjson root
                    len = strlen(OutBuffer);
                    printf("len:%d\n%s\n", len, OutBuffer);
                    SaveBuffer = (uint8_t *)malloc(len);
                    memcpy(SaveBuffer, OutBuffer, len);
                    xSemaphoreTake(Cache_muxtex, -1);
                    DataSave(SaveBuffer, len);
                    xSemaphoreGive(Cache_muxtex);
                    free(OutBuffer);
                    free(SaveBuffer);
                }
                else
                {
                    RS485_status = false;
                    ESP_LOGE(TAG, "485 add or cmd error\n");
                    // return 1;
                }
            }
            else
            {
                RS485_status = false;
                ESP_LOGE(TAG, "485 CRC error\n");
                // return 1;
            }
        }
        else
        {
            ESP_LOGE(TAG, "RS485 NO ARK !!! \n");
        }
        free(recv_data);
        xSemaphoreGive(RS485_Mutex);
    }
}

//读取485 风速探头
void read_485_ws_task(void *pvParameters)
{
    float Rs485_ws_val;
    char *OutBuffer;
    uint8_t *SaveBuffer;
    uint16_t len;
    cJSON *pJsonRoot;
    uint8_t *recv_data;
    int racv_len;
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, -1);
        xSemaphoreTake(RS485_Mutex, -1);
        recv_data = (uint8_t *)malloc(BUF_SIZE);
        racv_len = RS485_Read(Rs485_ws_cmd, recv_data);
        if (racv_len > 0)
        {
            esp_log_buffer_hex(TAG, recv_data, racv_len);

            if ((recv_data[5] * 256 + recv_data[6]) == CRC16_ModBus(recv_data, (racv_len - 2)))
            {
                if ((recv_data[0] == Rs485_ws_cmd[0]) && (recv_data[1] == Rs485_ws_cmd[1]))
                {
                    Rs485_ws_val = ((recv_data[3] << 8) + recv_data[4]) * 0.1;
                    ESP_LOGI(TAG, "Rs485_ws_val=%f\n", Rs485_ws_val);
                    // RS485_status = true;
                    // 读取成功
                    pJsonRoot = cJSON_CreateObject();
                    cJSON_AddStringToObject(pJsonRoot, "created_at", (const char *)Server_Timer_SEND());
                    cJSON_AddItemToObject(pJsonRoot, "field10", cJSON_CreateNumber(Rs485_ws_val));
                    OutBuffer = cJSON_PrintUnformatted(pJsonRoot); //cJSON_Print(Root)
                    cJSON_Delete(pJsonRoot);                       //delete cjson root
                    len = strlen(OutBuffer);
                    printf("len:%d\n%s\n", len, OutBuffer);
                    SaveBuffer = (uint8_t *)malloc(len);
                    memcpy(SaveBuffer, OutBuffer, len);
                    xSemaphoreTake(Cache_muxtex, -1);
                    DataSave(SaveBuffer, len);
                    xSemaphoreGive(Cache_muxtex);
                    free(OutBuffer);
                    free(SaveBuffer);
                }
                else
                {
                    RS485_status = false;
                    ESP_LOGE(TAG, "485 add or cmd error\n");
                    // return 1;
                }
            }
            else
            {
                RS485_status = false;
                ESP_LOGE(TAG, "485 CRC error\n");
                // return 1;
            }
        }
        else
        {
            ESP_LOGE(TAG, "RS485 NO ARK !!! \n");
        }
        free(recv_data);
        xSemaphoreGive(RS485_Mutex);
    }
}

void RS485_Init(void)
{
    RS485_Mutex = xSemaphoreCreateMutex();
    xTaskCreate(read_485_th_task, "read_485_th_task", 4096, NULL, 3, &Binary_485_th);
    xTaskCreate(read_485_t_task, "read_485_t_task", 4096, NULL, 3, &Binary_485_t);
    // xTaskCreate(read_485_sth_task, "read_485_sth_task", 4096, NULL, 3, &Binary_485_sth);
}
// /*******************************************************************************
//                                       END
// *******************************************************************************/