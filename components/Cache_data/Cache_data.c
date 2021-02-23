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
#include "Smartconfig.h"
#include "ServerTimer.h"
#include "Mqtt.h"

#include "Cache_data.h"

#define TAG "Cache_Data"

uint32_t flash_used_num = 0; //数据缓存的截至地址
// uint32_t data_save_num = 0;  //缓存的数据组数
uint32_t last_save_num = 0;  //最后一条数据的起始地址
uint32_t start_read_num = 0; //读取数据的开始地址
bool Exhausted_flag = 0;     //整个flash 已全部使用标志
SemaphoreHandle_t Cache_muxtex = NULL;

static Net_Err Http_post_fun(void);
//写flash 测试
void Write_Flash_Test_task(void *pvParameters);

void Data_Post_Task(void *pvParameters)
{
    Net_Err ret;
    while (1)
    {
        // ESP_LOGW("memroy check", " INTERNAL RAM left %dKB，free Heap:%d",
        //          heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024,
        //          esp_get_free_heap_size());
        Create_NET_Json();

        while ((ret = Http_post_fun()) != NET_OK)
        {
            if (ret != NET_DIS)
            {
                Start_Active();
                break;
            }
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }

        ulTaskNotifyTake(pdTRUE, -1);
    }
}

void DataSave(uint8_t *sava_buff, uint16_t Buff_len)
{
    // xSemaphoreTake(Cache_muxtex, -1);
    Mqtt_Msg Mqtt_Data = {.buff = {0},
                          .buff_len = 0};

    uint16_t Buff_len_c;
    Buff_len_c = strlen((const char *)sava_buff);
    if (Buff_len == 0 || Buff_len != Buff_len_c)
    {
        ESP_LOGE(TAG, "save err Buff_len:%d,Buff_len_c:%d", Buff_len, Buff_len_c);
        return;
    }

    memcpy(Mqtt_Data.buff, sava_buff, Buff_len);
    Mqtt_Data.buff_len = Buff_len;
    xQueueOverwrite(Send_Mqtt_Queue, (void *)&Mqtt_Data);

    flash_used_num = E2P_ReadLenByte(FLASH_USED_NUM_ADD, 4);
    start_read_num = E2P_ReadLenByte(START_READ_NUM_ADD, 4);
    Exhausted_flag = E2P_ReadOneByte(EXHAUSTED_FLAG_ADD);
    // ESP_LOGI(TAG, "flash_used_num=%d,start_read_num=%d,Exhausted_flag=%d", flash_used_num, start_read_num, Exhausted_flag);
    // data_save_num = E2P_ReadLenByte(DATA_SAVE_NUM_ADD, 4);

    //(1)读写地址完整
    if (flash_used_num > start_read_num)
    {
        if (flash_used_num + Buff_len >= SPI_FLASH_SIZE) //如果写入新数据后大须总容量，从头开始写
        {
            flash_used_num = 0;
            W25QXX_Write(sava_buff, flash_used_num, Buff_len);
            flash_used_num += Buff_len;
            E2P_WriteLenByte(FLASH_USED_NUM_ADD, flash_used_num, 4);
            //如果从头开始写后，结束地址大于下次的读取地址，说明整个储存利用已经最大，则设置读取地址为结束地址的下一个扇区，读取数据时分两次读
            if (flash_used_num > start_read_num)
            {
                // start_read_num = flash_used_num;
                start_read_num = flash_used_num + 4096 - (flash_used_num % 4096);
                if (start_read_num >= SPI_FLASH_SIZE)
                {
                    start_read_num = 0;
                }

                E2P_WriteLenByte(START_READ_NUM_ADD, start_read_num, 4);
                if (Exhausted_flag == 0)
                {
                    Exhausted_flag = 1;
                    E2P_WriteOneByte(EXHAUSTED_FLAG_ADD, Exhausted_flag);
                }
            }
        }
        else
        {
            W25QXX_Write(sava_buff, flash_used_num, Buff_len);
            flash_used_num += Buff_len;
            E2P_WriteLenByte(FLASH_USED_NUM_ADD, flash_used_num, 4);
        }
    }
    //(2)读写地址跨区
    else if (flash_used_num < start_read_num)
    {
        if (flash_used_num + Buff_len >= SPI_FLASH_SIZE) //如果写入新数据后大须总容量，从头开始写, 此时储存区已经全部用尽
        {
            flash_used_num = 0;
            W25QXX_Write(sava_buff, flash_used_num, Buff_len);
            flash_used_num += Buff_len;
            start_read_num = flash_used_num + 4096 - (flash_used_num % 4096);
            E2P_WriteLenByte(START_READ_NUM_ADD, start_read_num, 4);
            E2P_WriteLenByte(FLASH_USED_NUM_ADD, flash_used_num, 4);
            if (Exhausted_flag == 0)
            {
                Exhausted_flag = 1;
                E2P_WriteOneByte(EXHAUSTED_FLAG_ADD, Exhausted_flag);
            }
        }
        else
        {
            if (Exhausted_flag == 1 || flash_used_num + Buff_len >= start_read_num) //用尽
            {
                W25QXX_Write(sava_buff, flash_used_num, Buff_len);
                flash_used_num += Buff_len;
                start_read_num = flash_used_num + 4096 - (flash_used_num % 4096);
                if (start_read_num >= SPI_FLASH_SIZE)
                {
                    start_read_num = 0;
                }
                E2P_WriteLenByte(START_READ_NUM_ADD, start_read_num, 4);
                E2P_WriteLenByte(FLASH_USED_NUM_ADD, flash_used_num, 4);
                if (Exhausted_flag == 0)
                {
                    Exhausted_flag = 1;
                    E2P_WriteOneByte(EXHAUSTED_FLAG_ADD, Exhausted_flag);
                }
            }
            else
            {
                W25QXX_Write(sava_buff, flash_used_num, Buff_len);
                flash_used_num += Buff_len;
                E2P_WriteLenByte(FLASH_USED_NUM_ADD, flash_used_num, 4);
            }
        }
    }
    else
    {
        if (flash_used_num + Buff_len >= SPI_FLASH_SIZE)
            flash_used_num = 0;

        W25QXX_Write(sava_buff, flash_used_num, Buff_len);
        flash_used_num += Buff_len;

        if (Exhausted_flag == 1) //用尽
        {
            start_read_num = flash_used_num + 4096 - (flash_used_num % 4096);
            if (start_read_num >= SPI_FLASH_SIZE)
            {
                start_read_num = 0;
            }
            E2P_WriteLenByte(START_READ_NUM_ADD, start_read_num, 4);
        }
        E2P_WriteLenByte(FLASH_USED_NUM_ADD, flash_used_num, 4);
    }
}

void Start_Cache(void)
{
    Cache_muxtex = xSemaphoreCreateMutex();
    xTaskCreate(Data_Post_Task, "Data_Post_Task", 4096, NULL, 10, &Binary_dp);
    // xTaskCreate(Write_Flash_Test_task, "Write_Flash_Test_task", 4096, NULL, 10, NULL);
}

/*******************************************************************************
//  HTTP POST 
*******************************************************************************/
#define ONE_POST_BUFF_LEN 1536
#define STATUS_BUFF_LEN 1024

static Net_Err Http_post_fun(void)
{
    xEventGroupWaitBits(Net_sta_group, ACTIVED_BIT, false, true, -1);

    xSemaphoreTake(xMutex_Http_Send, -1);
    const char *post_header = "{\"feeds\":["; //
    char *status_buff = NULL;                 //],"status":"mac=x","ssid_base64":"x"}
    uint8_t *one_post_buff = NULL;            //一条数据的buff,
    uint16_t one_data_len;                    //读取一条数据占用flash的大小，不一定是这数据的大小
    uint32_t start_read_num_oen;              //单条数据读取的开始地址
    uint32_t end_read_num;                    //本次数据同步 截至地址
    uint32_t end_read_num_one;                //读取一条数据 截至地址
    uint32_t cache_data_len;                  //flash内正确待发送的数据总大小，以增加 ','占用
    uint32_t post_data_len;                   //Content_Length，通过http发送的总数据大小
    int32_t socket_num;                       //http socket
    bool send_status = false;                 //http 发送状态标志 ，false:发送未完成
    char *recv_buff = NULL;                   //post 返回
    bool data_err_flag = false;               // 数据中存在错误，会导致 post 失败
    Net_Err ret;

    status_buff = (char *)malloc(STATUS_BUFF_LEN);
    one_post_buff = (uint8_t *)malloc(ONE_POST_BUFF_LEN);
    recv_buff = (char *)malloc(HTTP_RECV_BUFF_LEN);

    memset(status_buff, 0, STATUS_BUFF_LEN);

    Create_Status_Json(status_buff, STATUS_BUFF_LEN, true); //
    // ESP_LOGI(TAG, "strlen:%d,\nbuff:%s", strlen(status_buff), status_buff);
    start_read_num = E2P_ReadLenByte(START_READ_NUM_ADD, 4);
    start_read_num_oen = start_read_num;
    // ESP_LOGI(TAG, "start_read_num_oen=%d", start_read_num_oen);

    xSemaphoreTake(Cache_muxtex, -1);
    cache_data_len = Read_Post_Len(start_read_num, E2P_ReadLenByte(FLASH_USED_NUM_ADD, 4), &end_read_num, MAX_READ_NUM);
    xSemaphoreGive(Cache_muxtex);

    if (cache_data_len == 0)
    {
        ret = NET_OK;
        // ESP_LOGE(TAG, "ERR LINE%d", __LINE__);
        goto end;
    }

    post_data_len = strlen(post_header) + strlen(status_buff) + cache_data_len;
    // ESP_LOGI(TAG, "post_data_len=%d,cache_data_len=%d", post_data_len, cache_data_len);

    socket_num = http_post_init(post_data_len);
    if (socket_num < 0)
    {
        ret = NET_DIS;
        ESP_LOGE(TAG, "ERR LINE%d", __LINE__);
        goto end;
    }
    // ESP_LOGI(TAG, "ERR LINE%d", __LINE__);

    // if (write(socket_num, post_header, strlen((const char *)post_header)) < 0) //step4：发送http Header
    if (http_send_post(socket_num, (char *)post_header, false) != 1)
    {
        ret = NET_DIS;
        ESP_LOGE(TAG, "ERR LINE%d", __LINE__);
        goto end;
    }
    // ESP_LOGI(TAG, "ERR LINE%d", __LINE__);

    //如果跨区，则分两次读
    if (start_read_num > end_read_num)
    {
        end_read_num_one = SPI_FLASH_SIZE;
    }
    else
    {
        end_read_num_one = end_read_num;
    }

    while (send_status == false)
    {
        memset(one_post_buff, 0, ONE_POST_BUFF_LEN);
        xSemaphoreTake(Cache_muxtex, -1);
        one_data_len = W25QXX_Read_Data(one_post_buff, start_read_num_oen, ONE_POST_BUFF_LEN);
        xSemaphoreGive(Cache_muxtex);

        if (one_data_len > 0)
        {
            start_read_num_oen = start_read_num_oen + one_data_len;
            if (start_read_num_oen >= end_read_num_one)
            {
                //跨区 后半段读取完成
                if (start_read_num > end_read_num && end_read_num_one == SPI_FLASH_SIZE)
                {
                    start_read_num_oen = 0;
                    end_read_num_one = end_read_num;
                    one_post_buff[strlen((const char *)one_post_buff)] = ',';
                    // ESP_LOGI(TAG, "first half post/read over");
                }
                else
                {
                    //数据读完
                    send_status = true;
                    // ESP_LOGI(TAG, "All data post/read over");
                }
            }
            else
            {
                one_post_buff[strlen((const char *)one_post_buff)] = ',';
            }

            // ESP_LOGI(TAG, "post_buff:\n%s\nstrlen=%d,data_len=%d", one_post_buff, strlen((const char *)one_post_buff), one_data_len);
            // if (write(socket_num, one_post_buff, strlen((const char *)one_post_buff)) < 0) //post_buff
            if (http_send_post(socket_num, (char *)one_post_buff, false) != 1)
            {
                ret = NET_DIS;
                ESP_LOGE(TAG, "ERR LINE%d", __LINE__);
                goto end;
            }
            // ESP_LOGI(TAG, "ERR LINE%d", __LINE__);
        }
        else //当前读取的缓存中没有正确数组
        {
            start_read_num_oen += ONE_POST_BUFF_LEN; //跳过无数据的地址，继续读取

            if (start_read_num_oen >= end_read_num_one)
            {
                //跨区 后半段读取完成
                if (start_read_num > end_read_num && end_read_num_one == SPI_FLASH_SIZE)
                {
                    start_read_num_oen = 0;
                    end_read_num_one = end_read_num;
                    // ESP_LOGI(TAG, "first half post/read over");
                }
                else
                {
                    //数据读完
                    send_status = true;
                    // ESP_LOGI(TAG, "All data post/read over");
                }
            }
        }
    }
    // ESP_LOGI(TAG, "ERR LINE%d", __LINE__);

    if (http_send_post(socket_num, status_buff, true) != 1)
    {
        //这里出错很有可能是数据构建出问题，
        data_err_flag = true;
        ret = NET_DIS;
        ESP_LOGE(TAG, "ERR LINE%d", __LINE__);
        goto end;
    }
    // ESP_LOGI(TAG, "ERR LINE%d", __LINE__);

    memset(recv_buff, 0, HTTP_RECV_BUFF_LEN);
    if (http_post_read(socket_num, recv_buff, HTTP_RECV_BUFF_LEN) == false)
    {
        ESP_LOGE(TAG, "ERR LINE%d", __LINE__);
        ret = NET_DIS;
        Net_sta_flag = false;
        goto end;
    }
    // ESP_LOGI(TAG, "ERR LINE%d", __LINE__);
    // printf("解析返回数据！\n");
    ESP_LOGI(TAG, "mes recv %d,\n:%s", strlen(recv_buff), recv_buff);
    if (parse_objects_http_respond(recv_buff))
    {
        ret = NET_OK;
        Net_sta_flag = true;
        ESP_LOGI(TAG, "post success");
    }
    else
    {
        ret = NET_400;
        Net_sta_flag = false;
        goto end;
    }

end:
    if (ret == NET_OK)
    {
        xSemaphoreTake(Cache_muxtex, -1);
        E2P_WriteLenByte(START_READ_NUM_ADD, start_read_num_oen, 4);
        if (Exhausted_flag == 1)
        {
            Exhausted_flag = 0;
            E2P_WriteOneByte(EXHAUSTED_FLAG_ADD, Exhausted_flag);
        }
        xSemaphoreGive(Cache_muxtex);
    }
    else
    {
        if (data_err_flag == true)
        {
            xSemaphoreTake(Cache_muxtex, -1);
            //读flash中的一条数据长度，跳过
            Read_Post_Len(start_read_num, E2P_ReadLenByte(FLASH_USED_NUM_ADD, 4), &end_read_num, 1);
            E2P_WriteLenByte(START_READ_NUM_ADD, end_read_num, 4);
            if (Exhausted_flag == 1)
            {
                Exhausted_flag = 0;
                E2P_WriteOneByte(EXHAUSTED_FLAG_ADD, Exhausted_flag);
            }
            xSemaphoreGive(Cache_muxtex);
            data_err_flag = false;
        }
    }

    free(status_buff);
    free(recv_buff);
    free(one_post_buff);

    xSemaphoreGive(xMutex_Http_Send);
    return ret;
}

//写flash 测试
void Write_Flash_err_Test(void)
{
    char test_buff[20] = {0};
    sprintf(test_buff, "12345566543}"); //写入错误数据
    xSemaphoreTake(Cache_muxtex, -1);
    DataSave((uint8_t *)test_buff, strlen(test_buff));
    xSemaphoreGive(Cache_muxtex);
}

void Write_Flash_Test_task(void *pvParameters)
{
    char test_buff[100] = {0};
    while (1)
    {
        // sprintf(test_buff, "{\"created_at\":\"%s\",\"field1\":1,\"field2\":250,\"field3\":2,\"field4\":3}", Server_Timer_SEND());
        // printf("test_buff len %d, \n%s \n", strlen(test_buff), test_buff);
        sprintf(test_buff, "{12345566543}"); //写入错误数据
        xSemaphoreTake(Cache_muxtex, -1);
        DataSave((uint8_t *)test_buff, strlen(test_buff));
        xSemaphoreGive(Cache_muxtex);
        // vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

void Erase_Flash_data_test(void)
{
    ESP_LOGI("TEST", "\nstart erase flash\n");
    W25QXX_Erase_Sector(0);
    E2P_WriteLenByte(START_READ_NUM_ADD, 0, 4);
    E2P_WriteLenByte(DATA_SAVE_NUM_ADD, 0, 4);
    E2P_WriteLenByte(FLASH_USED_NUM_ADD, 0, 4);
    ESP_LOGI("TEST", "\nerase flash ok\n");
}

void Raad_flash_Soctor(void)
{
    uint8_t test_buff[10] = {0};
    W25QXX_Read(test_buff, 0, 10);
    for (uint8_t i = 0; i < 10; i++)
    {
        ESP_LOGI("TEST", "i:%d,num:%d,char:%c\n", i, test_buff[i], test_buff[i]);
    }

    // printf("len:%d\ntest_buff:%s\n", strlen((const char *)test_buff), test_buff);
}

// /*******************************************************************************
//                                       END
// *******************************************************************************/