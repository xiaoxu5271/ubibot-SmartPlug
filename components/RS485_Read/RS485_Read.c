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
#include "Json_parse.h"

#include "RS485_Read.h"

#define TAG "458"

#define UART2_TXD (GPIO_NUM_17)
#define UART2_RXD (GPIO_NUM_16)
#define UART2_RTS (UART_PIN_NO_CHANGE)
#define UART2_CTS (UART_PIN_NO_CHANGE)

#define BUF_SIZE 35

SemaphoreHandle_t RS485_Mutex = NULL;
float ext_tem = 0.0, ext_hum = 0.0;
bool RS485_status = false;

char Rs485_th_cmd[] = {0xC1, 0x03, 0x00, 0x00, 0x00, 0x02, 0xD5, 0x0B};   //查询外接空气温湿度探头
char Rs485_t_cmd[] = {0xC2, 0x03, 0x00, 0x00, 0x00, 0x01, 0x95, 0x39};    //查询温度探头
char Rs485_ws_cmd[] = {0x20, 0x04, 0x00, 0x06, 0x00, 0x01, 0xD7, 0x7A};   //风速
char Rs485_sth_cmd[] = {0xFE, 0x03, 0x00, 0x00, 0x00, 0x02, 0xD0, 0x04};  //查询土壤探头
char Rs485_lt_cmd[] = {0x0A, 0x04, 0x00, 0x00, 0x00, 0x02, 0x70, 0xB0};   //光照
char Rs485_co2_tcmd[] = {0x61, 0x06, 0x00, 0x36, 0x00, 0x00, 0x60, 0x64}; //CO2
char Rs485_co2_scmd[] = {0x61, 0x06, 0x00, 0x25, 0x00, 0x02, 0x10, 0x60}; //
char Rs485_co2_gcmd[] = {0x61, 0x03, 0x00, 0x27, 0x00, 0x01, 0x3D, 0xA1}; //
char Rs485_co2_rcmd[] = {0x61, 0x03, 0x00, 0x28, 0x00, 0x06, 0x4C, 0x60}; //

/*******************************************************************************
//CO2 and temp/huimi value change
*******************************************************************************/
float unsignedchar_to_float(char *c_buf)
{
    float Concentration_val;
    unsigned long tempU32;

    //cast 4 bytes to one unsigned 32 bit integer
    tempU32 = (unsigned long)((((unsigned long)c_buf[0]) << 24) |
                              (((unsigned long)c_buf[1]) << 16) |
                              (((unsigned long)c_buf[2]) << 8) |
                              ((unsigned long)c_buf[3]));

    //cast unsigned 32 bit integer to 32 bit float
    Concentration_val = *(float *)&tempU32; //

    return Concentration_val;
}

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
    char *filed_buff;
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
                    // RS485_status = true;
                    // // 读取成功
                    filed_buff = (char *)malloc(9);
                    pJsonRoot = cJSON_CreateObject();
                    cJSON_AddStringToObject(pJsonRoot, "created_at", (const char *)Server_Timer_SEND());
                    snprintf(filed_buff, 9, "field%d", r1_th_t_f_num);
                    cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(ext_tem));
                    snprintf(filed_buff, 9, "field%d", r1_th_h_f_num);
                    cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(ext_hum));

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
                    free(filed_buff);
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
    char *filed_buff;
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

                    filed_buff = (char *)malloc(9);
                    pJsonRoot = cJSON_CreateObject();
                    cJSON_AddStringToObject(pJsonRoot, "created_at", (const char *)Server_Timer_SEND());
                    snprintf(filed_buff, 9, "field%d", r1_t_f_num);
                    cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(Rs485_t_val));
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
                    free(filed_buff);
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
    char *filed_buff;
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
                    filed_buff = (char *)malloc(9);
                    pJsonRoot = cJSON_CreateObject();
                    cJSON_AddStringToObject(pJsonRoot, "created_at", (const char *)Server_Timer_SEND());
                    snprintf(filed_buff, 9, "field%d", r1_ws_f_num);
                    cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(Rs485_ws_val));
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
                    free(filed_buff);
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
//读取485 土壤温湿度探头
void read_485_sth_task(void *pvParameters)
{
    float rs485_st_val, rs485_sh_val;
    char *OutBuffer;
    uint8_t *SaveBuffer;
    uint16_t len = 0;
    cJSON *pJsonRoot;
    uint8_t *recv_data;
    char *filed_buff;
    int racv_len = 0;
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, -1);
        xSemaphoreTake(RS485_Mutex, -1);
        recv_data = (uint8_t *)malloc(BUF_SIZE);
        racv_len = RS485_Read(Rs485_sth_cmd, recv_data);
        if (racv_len > 0)
        {
            esp_log_buffer_hex(TAG, recv_data, racv_len);

            if ((recv_data[7] * 256 + recv_data[8]) == CRC16_ModBus(recv_data, (racv_len - 2)))
            {
                if ((recv_data[0] == Rs485_th_cmd[0]) && (recv_data[1] == Rs485_th_cmd[1]))
                {
                    rs485_st_val = ((recv_data[5] << 8) + recv_data[6]);
                    if (rs485_st_val <= 0x7fff)
                    {
                        rs485_st_val = rs485_st_val / 10;
                    }
                    else
                    {
                        rs485_st_val = 0xffff - rs485_st_val;
                        rs485_st_val = -rs485_st_val / 10;
                    }
                    rs485_sh_val = ((recv_data[3] << 8) + recv_data[4]) * 0.1;

                    ESP_LOGI(TAG, "rs485_st_val=%f   rs485_sh_val=%f\n", rs485_st_val, rs485_sh_val);

                    filed_buff = (char *)malloc(9);
                    pJsonRoot = cJSON_CreateObject();
                    cJSON_AddStringToObject(pJsonRoot, "created_at", (const char *)Server_Timer_SEND());
                    snprintf(filed_buff, 9, "field%d", r1_sth_t_f_num);
                    cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(rs485_st_val));
                    snprintf(filed_buff, 9, "field%d", r1_sth_h_f_num);
                    cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(rs485_sh_val));
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
                    free(filed_buff);
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

//读取485 光照探头
void read_485_lt_task(void *pvParameters)
{
    float light_val;
    char *OutBuffer;
    uint8_t *SaveBuffer;
    uint16_t len = 0;
    cJSON *pJsonRoot;
    uint8_t *recv_data;
    char *filed_buff;
    int racv_len = 0;
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, -1);
        xSemaphoreTake(RS485_Mutex, -1);
        recv_data = (uint8_t *)malloc(BUF_SIZE);
        racv_len = RS485_Read(Rs485_lt_cmd, recv_data);
        if (racv_len > 0)
        {
            esp_log_buffer_hex(TAG, recv_data, racv_len);

            if ((recv_data[7] * 256 + recv_data[8]) == CRC16_ModBus(recv_data, (racv_len - 2)))
            {
                if ((recv_data[0] == Rs485_th_cmd[0]) && (recv_data[1] == Rs485_th_cmd[1]))
                {
                    light_val = (recv_data[3] << 24) + (recv_data[4] << 16) + (recv_data[5] << 8) + recv_data[6];
                    ESP_LOGI(TAG, "light_val=%f\n", light_val);
                    RS485_status = true;
                    // 读取成功
                    filed_buff = (char *)malloc(9);
                    pJsonRoot = cJSON_CreateObject();
                    cJSON_AddStringToObject(pJsonRoot, "created_at", (const char *)Server_Timer_SEND());
                    snprintf(filed_buff, 9, "field%d", r1_light_f_num);
                    cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(light_val));
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
                    free(filed_buff);
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

//读取485 CO2探头
void read_485_co2_task(void *pvParameters)
{
    float co2_val = 0.0, t_val = 0.0, h_val = 0.0;
    char val_buf[4] = {0};
    char *OutBuffer;
    uint8_t *SaveBuffer;
    uint16_t len = 0;
    cJSON *pJsonRoot;
    uint8_t *recv_data;
    char *filed_buff;
    int racv_len = 0;
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, -1);
        xSemaphoreTake(RS485_Mutex, -1);
        recv_data = (uint8_t *)malloc(BUF_SIZE);
        racv_len = RS485_Read(Rs485_co2_tcmd, recv_data);
        if (racv_len > 0)
        {
            // esp_log_buffer_hex(TAG, recv_data, racv_len);
            if ((recv_data[6] * 256 + recv_data[7]) == CRC16_ModBus(recv_data, (racv_len - 2)))
            {
                racv_len = RS485_Read(Rs485_co2_scmd, recv_data);
                if (racv_len > 0)
                {
                    if ((recv_data[6] * 256 + recv_data[7]) == CRC16_ModBus(recv_data, (racv_len - 2)))
                    {

                        for (uint8_t i = 0; i < 10; i++)
                        {
                            racv_len = RS485_Read(Rs485_co2_gcmd, recv_data);
                            if (racv_len > 0)
                            {
                                if ((recv_data[6] * 256 + recv_data[7]) == CRC16_ModBus(recv_data, (racv_len - 2)))
                                {

                                    racv_len = RS485_Read(Rs485_co2_rcmd, recv_data);
                                    esp_log_buffer_hex(TAG, recv_data, racv_len);
                                    if (racv_len > 0)
                                    {
                                        if ((recv_data[15] * 256 + recv_data[16]) == CRC16_ModBus(recv_data, (racv_len - 2)))
                                        {

                                            memcpy(val_buf, recv_data + 3, 4);
                                            co2_val = unsignedchar_to_float(val_buf);
                                            memcpy(val_buf, recv_data + 7, 4);
                                            t_val = unsignedchar_to_float(val_buf);
                                            memcpy(val_buf, recv_data + 11, 4);
                                            h_val = unsignedchar_to_float(val_buf);
                                            if ((co2_val >= 400) && (co2_val <= 10000))
                                                break;
                                        }
                                        else
                                        {
                                        }
                                    }
                                    else
                                    {
                                    }
                                }
                                else
                                {
                                }
                            }
                            else
                            {
                            }
                        }
                        ESP_LOGI(TAG, "CO2:%f,T:%f,H:%f", co2_val, t_val, h_val);
                        if ((co2_val >= 400) && (co2_val <= 10000))
                        {
                            filed_buff = (char *)malloc(9);
                            pJsonRoot = cJSON_CreateObject();
                            cJSON_AddStringToObject(pJsonRoot, "created_at", (const char *)Server_Timer_SEND());
                            snprintf(filed_buff, 9, "field%d", r1_co2_f_num);
                            cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(co2_val));
                            snprintf(filed_buff, 9, "field%d", r1_th_t_f_num);
                            cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(t_val));
                            snprintf(filed_buff, 9, "field%d", r1_th_h_f_num);
                            cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(h_val));
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
                            free(filed_buff);
                        }
                    }
                    else
                    {
                    }
                }
                else
                {
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
    xTaskCreate(read_485_ws_task, "read_485_ws_task", 4096, NULL, 3, &Binary_485_ws);
    xTaskCreate(read_485_sth_task, "read_485_sth_task", 4096, NULL, 3, &Binary_485_sth);
    xTaskCreate(read_485_lt_task, "read_485_lt_task", 4096, NULL, 3, &Binary_485_lt);
    xTaskCreate(read_485_co2_task, "read_485_co2_task", 4096, NULL, 3, &Binary_485_co2);
    // xTaskCreate(read_485_sth_task, "read_485_sth_task", 4096, NULL, 3, &Binary_485_sth);
}
// /*******************************************************************************
//                                       END
// *******************************************************************************/