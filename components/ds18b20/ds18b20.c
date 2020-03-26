#include <stdio.h>
#include <math.h>
#include "string.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "esp_timer.h"
#include "esp_log.h"
#include "Http.h"
#include "ServerTimer.h"
#include "Cache_data.h"
#include "Json_parse.h"

#include "ds18b20.h"

#define TAG "DS18B20"
#define ds18b20_gpio GPIO_NUM_4

//采集数据的次数
#define Cla_Num 9

#define DATA_IO_ON() gpio_set_level(ds18b20_gpio, 1)
#define DATA_IO_OFF() gpio_set_level(ds18b20_gpio, 0)

#define ds18b20_io_in() gpio_set_direction(ds18b20_gpio, GPIO_MODE_INPUT)
#define ds18b20_io_out() gpio_set_direction(ds18b20_gpio, GPIO_MODE_OUTPUT)

float DS18B20_TEM = 0.0;
bool DS18b20_status = false;

/*******************************************************************************
//中间值滤波
*******************************************************************************/
float GetMedianNum(float *bArray, int iFilterLen)
{
    int i, j; // 循环变量
    float bTemp;
    // 用冒泡法对数组进行排序
    for (j = 0; j < iFilterLen - 1; j++)
    {
        for (i = 0; i < iFilterLen - j - 1; i++)
        {
            if (bArray[i] > bArray[i + 1])
            {
                // 互换
                bTemp = bArray[i];
                bArray[i] = bArray[i + 1];
                bArray[i + 1] = bTemp;
            }
        }
    }

    // 计算中值
    if ((iFilterLen & 1) > 0)
    {
        // 数组有奇数个元素，返回中间一个元素
        bTemp = bArray[(iFilterLen - 1) / 2];
    }
    else
    {
        // 数组有偶数个元素，返回中间两个元素平均值
        bTemp = (bArray[iFilterLen / 2] + bArray[iFilterLen / 2 - 1]) / 2;
    }

    return bTemp;
}

/*******************************************************************************
//ds18b20 reset,return:0 reset success,return:-1,failured
*******************************************************************************/
static short ds18b20_reset(void)
{
    uint8_t retry = 0;
    ds18b20_io_out();
    DATA_IO_ON();
    ets_delay_us(6); //delay about 6 us
    DATA_IO_OFF();
    ets_delay_us(750); //delay about 750 us
    DATA_IO_ON();
    ets_delay_us(15); //delay about 15 us
    ds18b20_io_in();

    while (gpio_get_level(ds18b20_gpio)) //waite ds18b20 respon
    {
        retry++;
        if (retry > 200)
        {
            return -1;
        }
        ets_delay_us(3); //delay about 3 us
    }

    ets_delay_us(480); //delay about 480 us
    ds18b20_io_out();  //data pin out mode
    DATA_IO_ON();

    return 0;
}

/*******************************************************************************
//ds18b20 read a bit,return 0/1 
*******************************************************************************/
static uint8_t ds18b20_read_bit(void)
{
    uint8_t data;
    ds18b20_io_out();
    DATA_IO_ON();
    ets_delay_us(2); //delay about 1.5 us
    DATA_IO_OFF();
    ets_delay_us(3); //delay about 3 us
    DATA_IO_ON();
    ds18b20_io_in();
    ets_delay_us(15); //delay about 15 us

    if (gpio_get_level(ds18b20_gpio))
    {
        data = 1;
    }
    else
    {
        data = 0;
    }
    ets_delay_us(60); //delay about 60 us
    return data;
}

/*******************************************************************************
//ds18b20 read a byte
*******************************************************************************/
static uint8_t ds18b20_read_byte(void)
{
    uint8_t i, j, data = 0;
    for (i = 0; i < 8; i++)
    {
        j = ds18b20_read_bit();
        data = (j << 7) | (data >> 1);
    }
    return data;
}

/*******************************************************************************
//ds18b20 write a byte
*******************************************************************************/
static void ds18b20_write_byte(uint8_t data)
{
    uint8_t i, data_bit;
    ds18b20_io_out(); //data pin out mode
    for (i = 0; i < 8; i++)
    {
        data_bit = data & 0x01;
        if (data_bit)
        {
            DATA_IO_OFF();
            ets_delay_us(3); //delay about 3 us
            DATA_IO_ON();
            ets_delay_us(60); //delay about 60 us
        }
        else
        {
            DATA_IO_OFF();
            ets_delay_us(60); //delay about 60 us
            DATA_IO_ON();
            ets_delay_us(3); //delay about 3 us
        }
        data = data >> 1;
    }
}

/*******************************************************************************
//ds18b20 start convert
*******************************************************************************/
static void ds18b20_start(void)
{
    if (ds18b20_reset() == 0)
    {
        ds18b20_write_byte(0xcc); //skip rom
        ds18b20_write_byte(0x44); //start convert
        DATA_IO_ON();
    }
    ets_delay_us(750 * 1000); //delay about 750ms
}

/*******************************************************************************
//ds18b20 get temperature value
*******************************************************************************/
int8_t ds18b20_get_temp(void)
{
    static short temp;
    static uint8_t data_h, data_l;
    float temp_arr[Cla_Num];
    // temp_arr = (float *)malloc(Cla_Num);

    for (uint8_t i = 0; i < Cla_Num; i++)
    {
        DS18B20_TEM = 0.0;
        ds18b20_start(); //ds18b20 start convert
        if (ds18b20_reset() == 0)
        {
            ds18b20_write_byte(0xcc);     //skip rom
            ds18b20_write_byte(0xbe);     //read data
            data_l = ds18b20_read_byte(); //LSB
            data_h = ds18b20_read_byte(); //MSB

            if (data_h > 7) //temperature value<0
            {
                data_l = ~data_l;
                data_h = ~data_h;
                temp = data_h;
                temp <<= 8;
                temp += data_l;
                DS18B20_TEM = (float)-temp * 0.0625;
                // printf("temp=%f\n", DS18B20_TEM);
            }
            else //temperature value>=0
            {
                temp = data_h;
                temp <<= 8;
                temp += data_l;
                DS18B20_TEM = (float)temp * 0.0625;
                // printf("temp1=%f\n", DS18B20_TEM);
            }
            temp_arr[i] = DS18B20_TEM;
            // ESP_LOGI(TAG, "18B20_TEM[%d] = %f", i, temp_arr[i]);
        }
        else
        {
            DS18b20_status = false;
            // free(temp_arr);
            // printf("18b20 err\n");
            return -1;
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    //进行中间滤波处理
    DS18B20_TEM = GetMedianNum(temp_arr, Cla_Num);
    ESP_LOGI(TAG, "18B20_TEM= %f", DS18B20_TEM);
    DS18b20_status = true;
    return 1;
}

void get_ds18b20_task(void *org)
{
    char *Field_e1_t = NULL;
    char *OutBuffer;
    // uint8_t *SaveBuffer;
    uint16_t len = 0;
    cJSON *pJsonRoot;

    while (1)
    {
        ulTaskNotifyTake(pdTRUE, -1);
        printf("start 18b20!");
        if (ds18b20_get_temp() > 0)
        {
            Field_e1_t = (char *)malloc(9);
            snprintf(Field_e1_t, 9, "field%d", e1_t_f_num);

            pJsonRoot = cJSON_CreateObject();
            cJSON_AddStringToObject(pJsonRoot, "created_at", (const char *)Server_Timer_SEND());
            cJSON_AddItemToObject(pJsonRoot, Field_e1_t, cJSON_CreateNumber(DS18B20_TEM));
            OutBuffer = cJSON_PrintUnformatted(pJsonRoot); //cJSON_Print(Root)
            cJSON_Delete(pJsonRoot);                       //delete cjson root
            len = strlen(OutBuffer);
            printf("len:%d\n%s\n", len, OutBuffer);
            // SaveBuffer = (uint8_t *)malloc(len);
            // memcpy(SaveBuffer, OutBuffer, len);
            xSemaphoreTake(Cache_muxtex, -1);
            DataSave((uint8_t *)OutBuffer, len);
            xSemaphoreGive(Cache_muxtex);
            free(OutBuffer);
            // free(SaveBuffer);
            free(Field_e1_t);
        }
        else
        {
            ESP_LOGE(TAG, "READ DS18B20 ERR");
        }
    }
}

void start_ds18b20(void)
{
    xTaskCreate(get_ds18b20_task, "get_ds18b20_task", 4096, NULL, 3, &Binary_ext);
}

/*******************************************************************************
                                      END         
*******************************************************************************/
