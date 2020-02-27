#include <stdio.h>
#include <string.h>
#include <cJSON.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "Http.h"
#include "Json_parse.h"
#include "Led.h"
#include "Bluetooth.h"
#include "CSE7759B.h"
#include "my_spi_flash.h"
#include "E2prom.h"
#include "lwip/sockets.h"

#include "Cache_data.h"

#define TAG "Cache_Data"

uint32_t flash_used_num = 0; //数据缓存的截至地址
uint32_t data_save_num = 0;  //缓存的数据组数
uint32_t last_save_num = 0;  //最后一条数据的起始地址
uint32_t start_read_num = 0; //读取数据的开始地址
SemaphoreHandle_t Cache_muxtex = NULL;

static uint8_t Http_post_fun(void);

void Data_Post_Task(void *pvParameters)
{
    while (1)
    {
        //发送成功后，判断当前使用flash是否大一个扇区，大于的话再擦除
        ulTaskNotifyTake(pdTRUE, -1);
        if (!Http_post_fun())
        {
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            xTaskNotifyGive(Binary_dp);
        }
    }
}

void DataSave(uint8_t *sava_buff, uint16_t Buff_len)
{
    // xSemaphoreTake(Cache_muxtex, -1);
    flash_used_num = AT24CXX_ReadLenByte(flash_used_num_add, 4);
    data_save_num = AT24CXX_ReadLenByte(data_save_num_add, 4);

    if (flash_used_num + Buff_len >= SPI_FLASH_SIZE) //如果写入新数据后大须总容量，从头开始写
    {
        flash_used_num = 0;
    }
    W25QXX_Write(sava_buff, flash_used_num, Buff_len);
    data_save_num++;
    flash_used_num += Buff_len;
    AT24CXX_WriteLenByte(flash_used_num_add, flash_used_num, 4);
    ESP_LOGI(TAG, "flash_used_num=%d", flash_used_num);
    // xSemaphoreGive(Cache_muxtex);
}

void Start_Cache(void)
{
    Cache_muxtex = xSemaphoreCreateMutex();
    xTaskCreate(Data_Post_Task, "Data_Post_Task", 8192, NULL, 10, &Binary_dp);
}

/*******************************************************************************
//  HTTP POST 
*******************************************************************************/
static uint8_t Http_post_fun(void)
{
    const char *post_header = "{\"feeds\":["; //
    uint8_t status_buff[128];                 //],"status":"mac=x","ssid_base64":"x"}
    uint8_t one_post_buff[512] = {0};         //一条数据的buff,
    uint16_t one_data_len;                    //读取一条数据占用flash的大小，不一定是这数据的大小
    uint32_t start_read_num_oen;              //单条数据读取的开始地址
    uint32_t end_read_num;                    //本次数据同步 截至地址
    uint8_t status_buff_len;                  //],"status":"mac=x","ssid_base64":"x"}
    uint32_t cache_data_len;                  //flash内正确待发送的数据总大小，以增加 ','占用
    uint32_t post_data_len;                   //Content_Length，通过http发送的总数据大小
    int32_t socket_num;                       //http socket
    bool send_status = false;                 //http 发送状态标志 ，false:发送未完成
    char recv_buff[512];

    status_buff_len = Create_NET_Json((char *)status_buff); //该函数中save 了一条最近的数据，需在read end_raad_num之前
    ESP_LOGI(TAG, "status_buff_len:%d,buff:%s", status_buff_len, status_buff);
    start_read_num_oen = AT24CXX_ReadLenByte(start_read_num_add, 4);

    xSemaphoreTake(Cache_muxtex, -1);
    end_read_num = AT24CXX_ReadLenByte(flash_used_num_add, 4); //放入 互斥锁内读取，防止数据被改写
    cache_data_len = Read_Post_Len(start_read_num, end_read_num);
    xSemaphoreGive(Cache_muxtex);

    post_data_len = strlen(post_header) + status_buff_len + cache_data_len;
    ESP_LOGI(TAG, "post_data_len=%d,cache_data_len=%d", post_data_len, cache_data_len);

    socket_num = http_post_init(post_data_len);
    if (socket_num > 0)
    {
        if (write(socket_num, post_header, strlen((const char *)post_header)) < 0) //step4：发送http Header
        {
            ESP_LOGE(TAG, "... socket send failed");
            close(socket_num);
            return 0;
        }

        while (send_status == false)
        {
            xSemaphoreTake(Cache_muxtex, -1);
            one_data_len = W25QXX_Read_Data(one_post_buff, start_read_num_oen, sizeof(one_post_buff));
            xSemaphoreGive(Cache_muxtex);
            ESP_LOGI(TAG, "post_buff:%s,strlen=%d,data_len=%d", one_post_buff, strlen((const char *)one_post_buff), one_data_len);
            if (one_data_len > 0)
            {
                start_read_num_oen += one_data_len;
                if (start_read_num_oen >= end_read_num)
                {
                    send_status = true; //数据读完
                    ESP_LOGI(TAG, "All data post/read over");
                }
                else
                {
                    // sprintf(one_post_buff, "%s,", one_post_buff); //不是最后一条数据，末尾追加 ',' 分隔
                    one_post_buff[strlen((const char *)one_post_buff)] = ',';
                }

                if (write(socket_num, one_post_buff, strlen((const char *)one_post_buff)) < 0) //step4：发送http Header
                {
                    ESP_LOGE(TAG, "... socket send failed");
                    close(socket_num);
                    return 0;
                }
            }
            else //当前读取的缓存中没有正确数组
            {
                start_read_num_oen += sizeof(one_post_buff); //跳过无数据的地址，继续读取
                if (start_read_num_oen >= end_read_num)
                {
                    send_status = true; //数据读完
                    ESP_LOGI(TAG, "All data post/read over");
                }
            }
        }
        if (http_post_read(socket_num, recv_buff, sizeof(recv_buff)) < 0)
        {
            ESP_LOGE(TAG, "POST READ ERR");
            return 0;
        }
        // printf("解析返回数据！\n");
        ESP_LOGI(TAG, "mes recv:%s", recv_buff);
        if (parse_objects_http_respond(strchr(recv_buff, '{')))
        {
            Led_Status = LED_STA_WORK;
        }
        else
        {
            Led_Status = LED_STA_ACTIVE_ERR;
        }
    }
    else
    {
        ESP_LOGE(TAG, "http_post_init ERR");
        close(socket_num);
        return 0;
    }
    AT24CXX_WriteLenByte(start_read_num_add, start_read_num_oen, 4);
    return 1;
}

void Erase_Flash_data_test(void)
{
    printf("\nstart erase flash\n");
    W25QXX_Erase_Sector(1);
    AT24CXX_WriteLenByte(start_read_num_add, 0, 4);
    AT24CXX_WriteLenByte(data_save_num_add, 0, 4);
    AT24CXX_WriteLenByte(flash_used_num_add, 0, 4);
    printf("\nerase flash ok\n");
}

// /*******************************************************************************
//                                       END
// *******************************************************************************/