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
#include "crc8_16.h"
#include "Smartconfig.h"

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
char Rs485_is_cmd[] = {0x03, 0x03, 0x00, 0x02, 0x00, 0x02, 0x64, 0x29};   //乙烯氧气探头

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

int RS485_Read(char *Send_485_Buff, uint8_t *Recv_485_buff, uint8_t Uart_mode)
{
    sw_uart2(Uart_mode);
    uart_write_bytes(UART_NUM_2, Send_485_Buff, 8);
    int len = uart_read_bytes(UART_NUM_2, Recv_485_buff, BUF_SIZE, 200 / portTICK_RATE_MS);
    xSemaphoreGive(xMutex_uart2_sw);

    return len;
}

//读取485 空气探头
void read_485_th_task(void *pvParameters)
{
    char *OutBuffer;
    // uint8_t *SaveBuffer;
    uint16_t len = 0;
    cJSON *pJsonRoot;
    uint8_t *recv_data;
    char *filed_buff;
    char *time_buff;
    int racv_len = 0;
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, -1);
        xSemaphoreTake(RS485_Mutex, -1);
        recv_data = (uint8_t *)malloc(BUF_SIZE);

        for (uint8_t i = 0; i < 5; i++)
        {
            racv_len = RS485_Read(Rs485_th_cmd, recv_data, uart2_485);
            if (racv_len >= 9)
            {
                esp_log_buffer_hex(TAG, recv_data, racv_len);

                if ((recv_data[7] * 256 + recv_data[8]) == Get_Crc16(recv_data, 7))
                {
                    if ((recv_data[0] == Rs485_th_cmd[0]) && (recv_data[1] == Rs485_th_cmd[1]))
                    {
                        ext_tem = ((recv_data[3] << 8) + recv_data[4]);
                        if (ext_tem != 0x8000)
                        {
                            if (ext_tem <= 0x7fff) //"+"
                            {
                                ext_tem = ext_tem / 10;
                            }
                            else //"-"
                            {
                                ext_tem = 0xffff - ext_tem;
                                ext_tem = -ext_tem / 10;
                            }
                        }

                        ext_hum = ((recv_data[5] << 8) + recv_data[6]);
                        if (ext_hum != 0x8000)
                        {
                            ext_hum = ext_hum / 10;
                        }
                        ESP_LOGI(TAG, "ext_tem=%f   ext_hum=%f\n", ext_tem, ext_hum);
                        // RS485_status = true;

                        //读取成功
                        xEventGroupSetBits(Net_sta_group, RS485_CHECK_BIT);

                        if ((xEventGroupGetBits(Net_sta_group) & TIME_CAL_BIT) == TIME_CAL_BIT)
                        {
                            filed_buff = (char *)malloc(9);
                            time_buff = (char *)malloc(24);
                            Server_Timer_SEND(time_buff);
                            pJsonRoot = cJSON_CreateObject();
                            cJSON_AddStringToObject(pJsonRoot, "created_at", (const char *)time_buff);
                            snprintf(filed_buff, 9, "field%d", r1_th_t_f_num);
                            cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(Cali_filed(r1_th_t_f_num, ext_tem)));
                            snprintf(filed_buff, 9, "field%d", r1_th_h_f_num);
                            cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(Cali_filed(r1_th_h_f_num, ext_hum)));

                            OutBuffer = cJSON_PrintUnformatted(pJsonRoot); //cJSON_Print(Root)
                            if (OutBuffer != NULL)
                            {
                                len = strlen(OutBuffer);
                                ESP_LOGI(TAG, "len:%d\n%s\n", len, OutBuffer);
                                xSemaphoreTake(Cache_muxtex, -1);
                                DataSave((uint8_t *)OutBuffer, len);
                                xSemaphoreGive(Cache_muxtex);
                                cJSON_free(OutBuffer);
                            }
                            cJSON_Delete(pJsonRoot); //delete cjson root

                            free(time_buff);
                            free(filed_buff);
                        }
                        break;
                    }
                    else
                    {
                        RS485_status = false;
                        ESP_LOGE(TAG, "485 add or cmd error line:%d\n", __LINE__);
                        // return 1;
                    }
                }
                else
                {
                    RS485_status = false;
                    ESP_LOGE(TAG, "485 CRC error line:%d\n", __LINE__);
                    // return 1;
                }
            }
            else
            {
                ESP_LOGE(TAG, "RS485 NO ARK !!! line:%d\n", __LINE__);
            }
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
    // uint8_t *SaveBuffer;
    uint16_t len;
    cJSON *pJsonRoot;
    uint8_t *recv_data;
    char *filed_buff;
    char *time_buff;
    int racv_len;
    while (1)
    {
        // vTaskDelay(1000 / portTICK_PERIOD_MS);
        ulTaskNotifyTake(pdTRUE, -1);

        xSemaphoreTake(RS485_Mutex, -1);
        recv_data = (uint8_t *)malloc(BUF_SIZE);
        for (uint8_t i = 0; i < 5; i++)
        {
            racv_len = RS485_Read(Rs485_t_cmd, recv_data, uart2_485);
            if (racv_len >= 7)
            {
                esp_log_buffer_hex(TAG, recv_data, racv_len);

                if ((recv_data[5] * 256 + recv_data[6]) == Get_Crc16(recv_data, 5))
                {
                    if ((recv_data[0] == Rs485_t_cmd[0]) && (recv_data[1] == Rs485_t_cmd[1]))
                    {
                        Rs485_t_val = ((recv_data[3] << 8) + recv_data[4]);
                        if (Rs485_t_val != 0x8000)
                        {
                            if (Rs485_t_val <= 0x7fff) //"+"
                            {
                                Rs485_t_val = Rs485_t_val / 10;
                            }
                            else //"-"
                            {
                                Rs485_t_val = 0xffff - Rs485_t_val;
                                Rs485_t_val = -Rs485_t_val / 10;
                            }
                        }

                        ESP_LOGI(TAG, "Rs485_t_val=%f\n", Rs485_t_val);

                        if ((xEventGroupGetBits(Net_sta_group) & TIME_CAL_BIT) == TIME_CAL_BIT)
                        {

                            filed_buff = (char *)malloc(9);
                            time_buff = (char *)malloc(24);
                            Server_Timer_SEND(time_buff);
                            pJsonRoot = cJSON_CreateObject();
                            cJSON_AddStringToObject(pJsonRoot, "created_at", (const char *)time_buff);
                            snprintf(filed_buff, 9, "field%d", r1_t_f_num);
                            cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(Cali_filed(r1_t_f_num, Rs485_t_val)));
                            OutBuffer = cJSON_PrintUnformatted(pJsonRoot); //cJSON_Print(Root)
                            if (OutBuffer != NULL)
                            {
                                len = strlen(OutBuffer);
                                ESP_LOGI(TAG, "len:%d\n%s\n", len, OutBuffer);
                                xSemaphoreTake(Cache_muxtex, -1);
                                DataSave((uint8_t *)OutBuffer, len);
                                xSemaphoreGive(Cache_muxtex);
                                cJSON_free(OutBuffer);
                            }
                            cJSON_Delete(pJsonRoot); //delete cjson root
                            free(time_buff);
                            free(filed_buff);
                        }
                        break;
                    }
                    else
                    {
                        RS485_status = false;
                        ESP_LOGE(TAG, "485 add or cmd error line:%d\n", __LINE__);
                        // return 1;
                    }
                }
                else
                {
                    RS485_status = false;
                    ESP_LOGE(TAG, "485 CRC error line:%d\n", __LINE__);
                    // return 1;
                }
            }
            else
            {
                ESP_LOGE(TAG, "RS485 NO ARK !!! line:%d\n", __LINE__);
            }
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
    // uint8_t *SaveBuffer;
    uint16_t len;
    cJSON *pJsonRoot;
    uint8_t *recv_data;
    char *filed_buff;
    int racv_len;
    char *time_buff;
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, -1);
        xSemaphoreTake(RS485_Mutex, -1);
        recv_data = (uint8_t *)malloc(BUF_SIZE);
        for (uint8_t i = 0; i < 5; i++)
        {
            racv_len = RS485_Read(Rs485_ws_cmd, recv_data, uart2_485);
            if (racv_len >= 7)
            {
                esp_log_buffer_hex(TAG, recv_data, racv_len);

                if ((recv_data[5] * 256 + recv_data[6]) == Get_Crc16(recv_data, 5))
                {
                    if ((recv_data[0] == Rs485_ws_cmd[0]) && (recv_data[1] == Rs485_ws_cmd[1]))
                    {
                        Rs485_ws_val = ((recv_data[3] << 8) + recv_data[4]) * 0.1;
                        ESP_LOGI(TAG, "Rs485_ws_val=%f\n", Rs485_ws_val);

                        if ((xEventGroupGetBits(Net_sta_group) & TIME_CAL_BIT) == TIME_CAL_BIT)
                        {
                            filed_buff = (char *)malloc(9);
                            time_buff = (char *)malloc(24);
                            Server_Timer_SEND(time_buff);
                            pJsonRoot = cJSON_CreateObject();
                            cJSON_AddStringToObject(pJsonRoot, "created_at", (const char *)time_buff);
                            snprintf(filed_buff, 9, "field%d", r1_ws_f_num);
                            cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(Cali_filed(r1_ws_f_num, Rs485_ws_val)));
                            OutBuffer = cJSON_PrintUnformatted(pJsonRoot); //cJSON_Print(Root)
                            if (OutBuffer != NULL)
                            {
                                len = strlen(OutBuffer);
                                ESP_LOGI(TAG, "len:%d\n%s\n", len, OutBuffer);
                                xSemaphoreTake(Cache_muxtex, -1);
                                DataSave((uint8_t *)OutBuffer, len);
                                xSemaphoreGive(Cache_muxtex);
                                cJSON_free(OutBuffer);
                            }
                            cJSON_Delete(pJsonRoot); //delete cjson root

                            free(time_buff);
                            free(filed_buff);
                        }
                        break;
                    }
                    else
                    {
                        RS485_status = false;
                        ESP_LOGE(TAG, "485 add or cmd error line:%d\n", __LINE__);
                    }
                }
                else
                {
                    RS485_status = false;
                    ESP_LOGE(TAG, "485 CRC error line:%d\n", __LINE__);
                    // return 1;
                }
            }
            else
            {
                ESP_LOGE(TAG, "RS485 NO ARK !!! line:%d\n", __LINE__);
            }
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
    // uint8_t *SaveBuffer;
    uint16_t len = 0;
    cJSON *pJsonRoot;
    uint8_t *recv_data;
    char *filed_buff;
    char *time_buff;
    int racv_len = 0;
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, -1);
        xSemaphoreTake(RS485_Mutex, -1);
        recv_data = (uint8_t *)malloc(BUF_SIZE);
        for (uint8_t i = 0; i < 5; i++)
        {
            racv_len = RS485_Read(Rs485_sth_cmd, recv_data, uart2_485);
            if (racv_len >= 9)
            {
                esp_log_buffer_hex(TAG, recv_data, racv_len);

                if ((recv_data[7] * 256 + recv_data[8]) == Get_Crc16(recv_data, 7))
                {
                    if ((recv_data[0] == Rs485_sth_cmd[0]) && (recv_data[1] == Rs485_sth_cmd[1]))
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

                        if ((xEventGroupGetBits(Net_sta_group) & TIME_CAL_BIT) == TIME_CAL_BIT)
                        {
                            filed_buff = (char *)malloc(9);
                            time_buff = (char *)malloc(24);
                            Server_Timer_SEND(time_buff);
                            pJsonRoot = cJSON_CreateObject();
                            cJSON_AddStringToObject(pJsonRoot, "created_at", (const char *)time_buff);
                            snprintf(filed_buff, 9, "field%d", r1_sth_t_f_num);
                            cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(Cali_filed(r1_sth_t_f_num, rs485_st_val)));
                            snprintf(filed_buff, 9, "field%d", r1_sth_h_f_num);
                            cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(Cali_filed(r1_sth_h_f_num, rs485_sh_val)));
                            OutBuffer = cJSON_PrintUnformatted(pJsonRoot); //cJSON_Print(Root)
                            if (OutBuffer != NULL)
                            {
                                len = strlen(OutBuffer);
                                ESP_LOGI(TAG, "len:%d\n%s\n", len, OutBuffer);
                                xSemaphoreTake(Cache_muxtex, -1);
                                DataSave((uint8_t *)OutBuffer, len);
                                xSemaphoreGive(Cache_muxtex);
                                cJSON_free(OutBuffer);
                            }
                            cJSON_Delete(pJsonRoot); //delete cjson root

                            free(time_buff);
                            free(filed_buff);
                        }
                        break;
                    }
                    else
                    {
                        RS485_status = false;
                        ESP_LOGE(TAG, "485 add or cmd error line:%d\n", __LINE__);
                        // return 1;
                    }
                }
                else
                {
                    RS485_status = false;
                    ESP_LOGE(TAG, "485 CRC error line:%d\n", __LINE__);
                    // return 1;
                }
            }
            else
            {
                ESP_LOGE(TAG, "RS485 NO ARK !!! line:%d\n", __LINE__);
            }
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
    // uint8_t *SaveBuffer;
    uint16_t len = 0;
    cJSON *pJsonRoot;
    uint8_t *recv_data;
    char *filed_buff;
    char *time_buff;
    int racv_len = 0;
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, -1);
        xSemaphoreTake(RS485_Mutex, -1);
        recv_data = (uint8_t *)malloc(BUF_SIZE);
        for (uint8_t i = 0; i < 5; i++)
        {
            racv_len = RS485_Read(Rs485_lt_cmd, recv_data, uart2_485);
            if (racv_len >= 9)
            {
                esp_log_buffer_hex(TAG, recv_data, racv_len);

                if ((recv_data[7] * 256 + recv_data[8]) == Get_Crc16(recv_data, 7))
                {
                    if ((recv_data[0] == Rs485_th_cmd[0]) && (recv_data[1] == Rs485_th_cmd[1]))
                    {
                        light_val = (recv_data[3] << 24) + (recv_data[4] << 16) + (recv_data[5] << 8) + recv_data[6];
                        ESP_LOGI(TAG, "light_val=%f\n", light_val);
                        RS485_status = true;
                        // 读取成功
                        if ((xEventGroupGetBits(Net_sta_group) & TIME_CAL_BIT) == TIME_CAL_BIT)
                        {
                            filed_buff = (char *)malloc(9);
                            time_buff = (char *)malloc(24);
                            Server_Timer_SEND(time_buff);
                            pJsonRoot = cJSON_CreateObject();
                            cJSON_AddStringToObject(pJsonRoot, "created_at", (const char *)time_buff);
                            snprintf(filed_buff, 9, "field%d", r1_light_f_num);
                            cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(Cali_filed(r1_light_f_num, light_val)));
                            OutBuffer = cJSON_PrintUnformatted(pJsonRoot); //cJSON_Print(Root)
                            if (OutBuffer != NULL)
                            {
                                len = strlen(OutBuffer);
                                ESP_LOGI(TAG, "len:%d\n%s\n", len, OutBuffer);
                                xSemaphoreTake(Cache_muxtex, -1);
                                DataSave((uint8_t *)OutBuffer, len);
                                xSemaphoreGive(Cache_muxtex);
                                cJSON_free(OutBuffer);
                            }
                            cJSON_Delete(pJsonRoot); //delete cjson root

                            free(time_buff);
                            free(filed_buff);
                        }
                        break;
                    }
                    else
                    {
                        RS485_status = false;
                        ESP_LOGE(TAG, "485 add or cmd error line:%d\n", __LINE__);
                        // return 1;
                    }
                }
                else
                {
                    RS485_status = false;
                    ESP_LOGE(TAG, "485 CRC error line:%d\n", __LINE__);
                    // return 1;
                }
            }
            else
            {
                ESP_LOGE(TAG, "RS485 NO ARK !!!  line:%d\n", __LINE__);
            }
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
    // uint8_t *SaveBuffer;
    uint16_t len = 0;
    cJSON *pJsonRoot;
    uint8_t *recv_data;
    char *filed_buff;
    char *time_buff;
    int racv_len = 0;
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, -1);
        xSemaphoreTake(RS485_Mutex, -1);
        recv_data = (uint8_t *)malloc(BUF_SIZE);
        for (uint8_t i = 0; i < 5; i++)
        {
            racv_len = RS485_Read(Rs485_co2_tcmd, recv_data, uart2_co2);
            if (racv_len >= 8)
            {
                // esp_log_buffer_hex("co2_t", recv_data, racv_len);
                if ((recv_data[6] * 256 + recv_data[7]) == Get_Crc16(recv_data, 6))
                {
                    racv_len = RS485_Read(Rs485_co2_scmd, recv_data, uart2_co2);
                    if (racv_len >= 8)
                    {
                        // esp_log_buffer_hex("co2_s", recv_data, racv_len);
                        if ((recv_data[6] * 256 + recv_data[7]) == Get_Crc16(recv_data, 6))
                        {
                            for (uint8_t j = 0; j < 10; j++)
                            {
                                racv_len = RS485_Read(Rs485_co2_gcmd, recv_data, uart2_co2);
                                if (racv_len >= 7)
                                {
                                    // esp_log_buffer_hex("co2_g", recv_data, racv_len);
                                    if ((recv_data[5] * 256 + recv_data[6]) == Get_Crc16(recv_data, 5))
                                    {
                                        racv_len = RS485_Read(Rs485_co2_rcmd, recv_data, uart2_co2);
                                        // esp_log_buffer_hex(TAG, recv_data, racv_len);
                                        if (racv_len >= 17)
                                        {
                                            // esp_log_buffer_hex("co2_r", recv_data, racv_len);
                                            if ((recv_data[15] * 256 + recv_data[16]) == Get_Crc16(recv_data, 15))
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
                                                ESP_LOGE(TAG, "co2_r CRC line:%d\n", __LINE__);
                                            }
                                        }
                                        else
                                        {
                                            ESP_LOGE(TAG, "co2_r NO RECV line:%d\n", __LINE__);
                                        }
                                    }
                                    else
                                    {
                                        ESP_LOGE(TAG, "co2_g CRC ERR line:%d\n", __LINE__);
                                    }
                                }
                                else
                                {
                                    ESP_LOGE(TAG, "co2_g NO RECV line:%d\n", __LINE__);
                                }
                            }
                            co2_val = ((int)(co2_val * 1000 + 0.5)) * 0.001; //保留3位
                            t_val = ((int)(t_val * 1000 + 0.5)) * 0.001;     //保留3位
                            h_val = ((int)(h_val * 1000 + 0.5)) * 0.001;     //保留3位

                            ESP_LOGI(TAG, "CO2:%f,T:%f,H:%f", co2_val, t_val, h_val);
                            if ((co2_val >= 100) && (co2_val <= 20000))
                            {
                                if ((xEventGroupGetBits(Net_sta_group) & TIME_CAL_BIT) == TIME_CAL_BIT)
                                {
                                    filed_buff = (char *)malloc(9);
                                    time_buff = (char *)malloc(24);
                                    Server_Timer_SEND(time_buff);
                                    pJsonRoot = cJSON_CreateObject();
                                    cJSON_AddStringToObject(pJsonRoot, "created_at", (const char *)time_buff);
                                    snprintf(filed_buff, 9, "field%d", r1_co2_f_num);
                                    cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(Cali_filed(r1_co2_f_num, co2_val)));
                                    snprintf(filed_buff, 9, "field%d", r1_co2_t_f_num);
                                    cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(Cali_filed(r1_co2_t_f_num, t_val)));
                                    snprintf(filed_buff, 9, "field%d", r1_co2_h_f_num);
                                    cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(Cali_filed(r1_co2_h_f_num, h_val)));
                                    OutBuffer = cJSON_PrintUnformatted(pJsonRoot); //cJSON_Print(Root)
                                    if (OutBuffer != NULL)
                                    {
                                        len = strlen(OutBuffer);
                                        ESP_LOGI(TAG, "len:%d\n%s\n", len, OutBuffer);
                                        xSemaphoreTake(Cache_muxtex, -1);
                                        DataSave((uint8_t *)OutBuffer, len);
                                        xSemaphoreGive(Cache_muxtex);
                                        cJSON_free(OutBuffer);
                                    }
                                    cJSON_Delete(pJsonRoot); //delete cjson root

                                    free(time_buff);
                                    free(filed_buff);
                                }
                                break;
                            }
                        }
                        else
                        {
                            ESP_LOGE(TAG, "co2_s CRC ERR line:%d\n", __LINE__);
                        }
                    }
                    else
                    {
                        ESP_LOGE(TAG, "co2_s NO RECV line:%d\n", __LINE__);
                    }
                }
                else
                {
                    ESP_LOGE(TAG, "co2_t CRC ERR line:%d\n", __LINE__);
                    // return 1;
                }
            }
            else
            {
                ESP_LOGE(TAG, "co2_t NO RECV line:%d\n", __LINE__);
            }
        }
        free(recv_data);
        xSemaphoreGive(RS485_Mutex);
        // vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

//读取485 乙烯氧气
void read_485_IS_task(void *pvParameters)
{
    float IS_c2h4, IS_o2;
    char *OutBuffer;
    // uint8_t *SaveBuffer;
    uint16_t len = 0;
    cJSON *pJsonRoot;
    uint8_t *recv_data;
    char *filed_buff;
    char *time_buff;
    int racv_len = 0;
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, -1);
        xSemaphoreTake(RS485_Mutex, -1);
        recv_data = (uint8_t *)malloc(BUF_SIZE);
        for (uint8_t i = 0; i < 5; i++)
        {
            racv_len = RS485_Read(Rs485_is_cmd, recv_data, uart2_485);
            if (racv_len >= 9)
            {
                esp_log_buffer_hex(TAG, recv_data, racv_len);

                if ((recv_data[7] * 256 + recv_data[8]) == Get_Crc16(recv_data, 7))
                {
                    if ((recv_data[0] == Rs485_is_cmd[0]) && (recv_data[1] == Rs485_is_cmd[1]))
                    {
                        IS_c2h4 = ((recv_data[3] * 256 + recv_data[4]) * 0.1);
                        IS_o2 = ((recv_data[5] * 256 + recv_data[6]) * 0.1);
                        ESP_LOGI(TAG, "IS_c2h4=%f,IS_o2=%f\n", IS_c2h4, IS_o2);
                        RS485_status = true;
                        // 读取成功
                        if ((xEventGroupGetBits(Net_sta_group) & TIME_CAL_BIT) == TIME_CAL_BIT)
                        {
                            filed_buff = (char *)malloc(9);
                            time_buff = (char *)malloc(24);
                            Server_Timer_SEND(time_buff);
                            pJsonRoot = cJSON_CreateObject();
                            cJSON_AddStringToObject(pJsonRoot, "created_at", (const char *)time_buff);
                            snprintf(filed_buff, 9, "field%d", r1_is_c2h4_f_num);
                            cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(Cali_filed(r1_is_c2h4_f_num, IS_c2h4)));
                            snprintf(filed_buff, 9, "field%d", r1_is_o2_f_num);
                            cJSON_AddItemToObject(pJsonRoot, filed_buff, cJSON_CreateNumber(Cali_filed(r1_is_o2_f_num, IS_o2)));

                            OutBuffer = cJSON_PrintUnformatted(pJsonRoot); //cJSON_Print(Root)
                            if (OutBuffer != NULL)
                            {
                                len = strlen(OutBuffer);
                                ESP_LOGI(TAG, "len:%d\n%s\n", len, OutBuffer);
                                xSemaphoreTake(Cache_muxtex, -1);
                                DataSave((uint8_t *)OutBuffer, len);
                                xSemaphoreGive(Cache_muxtex);
                                cJSON_free(OutBuffer);
                            }
                            cJSON_Delete(pJsonRoot); //delete cjson root

                            free(time_buff);
                            free(filed_buff);
                        }
                        break;
                    }
                    else
                    {
                        RS485_status = false;
                        ESP_LOGE(TAG, "485 add or cmd error line:%d\n", __LINE__);
                        // return 1;
                    }
                }
                else
                {
                    RS485_status = false;
                    ESP_LOGE(TAG, "485 CRC error line:%d\n", __LINE__);
                    // return 1;
                }
            }
            else
            {
                ESP_LOGE(TAG, "RS485 NO ARK !!!  line:%d\n", __LINE__);
            }
        }
        free(recv_data);
        xSemaphoreGive(RS485_Mutex);
    }
}

void RS485_Init(void)
{
    RS485_Mutex = xSemaphoreCreateMutex();
    xTaskCreate(read_485_th_task, "485_th", 3072, NULL, 3, &Binary_485_th);
    xTaskCreate(read_485_t_task, "485_t", 3072, NULL, 3, &Binary_485_t);
    xTaskCreate(read_485_ws_task, "485_ws", 3072, NULL, 3, &Binary_485_ws);
    xTaskCreate(read_485_sth_task, "485_sth", 3072, NULL, 3, &Binary_485_sth);
    xTaskCreate(read_485_lt_task, "485_lt", 3072, NULL, 3, &Binary_485_lt);
    xTaskCreate(read_485_co2_task, "485_co2", 4096, NULL, 3, &Binary_485_co2);
    xTaskCreate(read_485_IS_task, "485_IS", 4096, NULL, 3, &Binary_485_IS);
}
// /*******************************************************************************
//                                       END
// *******************************************************************************/