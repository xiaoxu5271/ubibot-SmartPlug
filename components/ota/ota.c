#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"

#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "Smartconfig.h"
#include "Json_parse.h"
#include "Http.h"
#include "My_Mqtt.h"
#include "E2prom.h"
#include "EC20.h"
#include "Led.h"

#include "ota.h"

//数据包长度
#define BUFFSIZE 1024

static const char *TAG = "ota";
bool OTA_FLAG = false;

static char ota_write_data[BUFFSIZE + 1] = {0};

TaskHandle_t ota_handle = NULL;

static void __attribute__((noreturn)) task_fatal_error()
{
    // 升级错误处理函数
    char Status_buff[100] = {0};
    snprintf(ERROE_CODE, sizeof(ERROE_CODE), "error_ota");
    snprintf(Status_buff, sizeof(Status_buff), "{\"command_id\":\"%s\",\"status\":\"FAIL\"}\r\n", mqtt_json_s.mqtt_command_id);
    Send_Mqtt_Buff(Status_buff);
    vTaskNotifyGiveFromISR(Binary_dp, NULL);
    // vTaskDelay(1000 / portTICK_RATE_MS);
    ESP_LOGE(TAG, "Exiting task due to fatal error...");
    // esp_restart();
    OTA_FLAG = false;
    (void)vTaskDelete(NULL);
}

static void http_cleanup(esp_http_client_handle_t client)
{
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
}

/*read buffer by byte still delim ,return read bytes counts*/
static int read_until(char *buffer, char delim, int len)
{
    int i = 0;
    while (buffer[i] != delim && i < len)
    {
        ++i;
    }
    return i + 1;
}
/* resolve a packet from http socket
 * return true if packet including \r\n\r\n that means http packet header finished,start to receive packet body
 * otherwise return false
 * */
static bool read_past_http_header(char text[], int total_len, uint32_t *content_len, uint32_t *binary_file_length, esp_ota_handle_t update_handle)
{
    // printf("\r\n%s\r\n", text);
    /* 获取镜像大小*/
    char sub[20];
    if (mid((char *)text, "Content-Length: ", "\r\n", sub) == NULL)
    {
        ESP_LOGE(TAG, "URL or SERVER ERROR!!!");
        return false;
    }

    *content_len = atol(sub);
    // ESP_LOGI(TAG, "Content-Length:%d\n", *content_len);

    /* i means current position */
    int i = 0, i_read_len = 0;
    while (text[i] != 0 && i < total_len)
    {
        i_read_len = read_until(&text[i], '\n', total_len);
        // if we resolve \r\n line,we think packet header is finished
        if (i_read_len == 2)
        {

            int i_write_len = total_len - (i + 2);
            memset(ota_write_data, 0, sizeof(ota_write_data));
            /*copy first http packet body to write buffer*/
            memcpy(ota_write_data, &(text[i + 2]), i_write_len);

            esp_err_t err = esp_ota_write(update_handle, (const void *)ota_write_data, i_write_len);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Error: esp_ota_write failed! err=0x%x", err);
                return false;
            }
            else
            {
                ESP_LOGI(TAG, "esp_ota_write header OK");
                *binary_file_length += i_write_len;
            }
            return true;
        }
        i += i_read_len;
    }
    return false;
}

//wifi ota
void WIFI_OTA(void)
{
    char Status_buff[100] = {0};
    OTA_FLAG = true;
    esp_err_t err;
    //进度 百分比
    uint8_t percentage = 0, last_percentage = 0;
    /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
    esp_ota_handle_t update_handle = 0;
    const esp_partition_t *update_partition = NULL;

    ESP_LOGI(TAG, "Starting OTA example...");

    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    if (configured != running)
    {
        ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
                 configured->address, running->address);
        ESP_LOGW(TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
    }
    ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)",
             running->type, running->subtype, running->address);

    /* Wait for the callback to set the CONNECTED_BIT in the
       event group.
    */
    xEventGroupWaitBits(Net_sta_group, ACTIVED_BIT,
                        false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Connect to Wifi ! Start to Connect to Server....");

    // E2prom_page_Read(ota_url_add, (uint8_t *)mqtt_json_s.mqtt_ota_url, 128);
    ESP_LOGI(TAG, "OTA-URL=[%s]\r\n", mqtt_json_s.mqtt_ota_url);

    esp_http_client_config_t config = {
        .url = mqtt_json_s.mqtt_ota_url,
        // .cert_pem = (char *)server_cert_pem_start,   //https 证书
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialise HTTP connection");
        task_fatal_error();
    }
    err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        task_fatal_error();
    }
    uint32_t content_len = esp_http_client_fetch_headers(client);

    update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
             update_partition->subtype, update_partition->address);
    assert(update_partition != NULL);

    int binary_file_length = 0;
    /*deal with all receive packet*/
    bool image_header_was_checked = false;
    while (1)
    {
        int data_read = esp_http_client_read(client, ota_write_data, sizeof(ota_write_data));
        // printf("\n ota_write_data=%2x \n", (unsigned int)ota_write_data);
        if (data_read < 0)
        {
            ESP_LOGE(TAG, "Error: SSL data read error");
            http_cleanup(client);
            task_fatal_error();
        }
        else if (data_read > 0)
        {
            if (image_header_was_checked == false)
            {
                // esp_app_desc_t new_app_info;
                if (data_read > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t))
                {

                    image_header_was_checked = true;

                    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
                    if (err != ESP_OK)
                    {
                        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
                        http_cleanup(client);
                        task_fatal_error();
                    }
                    ESP_LOGI(TAG, "esp_ota_begin succeeded");
                }
                else
                {
                    ESP_LOGE(TAG, "received package is not fit len");
                    http_cleanup(client);
                    task_fatal_error();
                }
            }
            err = esp_ota_write(update_handle, (const void *)ota_write_data, data_read);
            if (err != ESP_OK)
            {
                http_cleanup(client);
                task_fatal_error();
            }
            binary_file_length += data_read;

            percentage = (int)(binary_file_length * 100 / content_len);
            if (percentage != last_percentage && percentage % 10 == 0)
            {

                snprintf(Status_buff, sizeof(Status_buff), "{\"command_id\":\"%s\",\"status\":\"upgrading\",\"progress\":%d}\r\n", mqtt_json_s.mqtt_command_id, percentage);
                Send_Mqtt_Buff(Status_buff);
                printf("%s", Status_buff);
            }
            last_percentage = percentage;
        }
        else if (data_read == 0)
        {
            ESP_LOGI(TAG, "Connection closed,all data received");
            break;
        }
    }
    ESP_LOGI(TAG, "Total Write binary data length : %d", binary_file_length);

    if (esp_ota_end(update_handle) != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_end failed!");
        http_cleanup(client);
        task_fatal_error();
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        http_cleanup(client);
        task_fatal_error();
    }
    snprintf(Status_buff, sizeof(Status_buff), "{\"command_id\":\"%s\",\"status\":\"OK\"}\r\n", mqtt_json_s.mqtt_command_id);
    Send_Mqtt_Buff(Status_buff);
    printf("%s", Status_buff);

    ESP_LOGI(TAG, "Prepare to restart system!");
    vTaskDelay(1000 / portTICK_RATE_MS);
    esp_restart();
    return;
}

//EC20 OTA
bool EC20_OTA(void)
{
    ESP_LOGI(TAG, "EC20 OTA");
    bool ret = false;
    uint32_t buff_len;
    esp_err_t err;
    char *recv_buff = (char *)malloc(BUFFSIZE);

    //已写入镜像大小
    uint32_t binary_file_length = 0;
    //报文镜像长度
    uint32_t content_len = 0;
    //进度 百分比
    uint8_t percentage = 0;

    esp_ota_handle_t update_handle = 0;
    const esp_partition_t *update_partition = NULL;

    //获取当前boot位置
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    //获取当前系统执行的固件所在的Flash分区
    const esp_partition_t *running = esp_ota_get_running_partition();

    if (configured != running)
    {
        ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
                 configured->address, running->address);
        ESP_LOGW(TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
    }
    ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)",
             running->type, running->subtype, running->address);

    //获取当前系统下一个（紧邻当前使用的OTA_X分区）可用于烧录升级固件的Flash分区
    update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
             update_partition->subtype, update_partition->address);
    assert(update_partition != NULL);
    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_begin failed, error=%d", err);
        ret = false;
        goto end;
    }
    ESP_LOGI(TAG, "esp_ota_begin succeeded");
    xEventGroupWaitBits(Net_sta_group, ACTIVED_BIT, false, true, -1); //等网络连接
    xSemaphoreTake(xMutex_Http_Send, -1);

    if (model_id == SIM7600)
    {
        content_len = Start_SIM_OTA();
    }
    else
    {
        //获取升级文件
        if (Start_EC20_TCP_OTA() == false)
        {
            ESP_LOGE(TAG, "%d", __LINE__);
            ret = false;
            goto end;
        }

        //接收http包
        memset(recv_buff, 0, BUFFSIZE);
        buff_len = Read_OTA_File(recv_buff);
        if (buff_len > 0)
        {
            if (read_past_http_header(recv_buff, buff_len, &content_len, &binary_file_length, update_handle) != true)
            {
                ESP_LOGE(TAG, "%d", __LINE__);
                ret = false;
                goto end;
            }
        }
        else
        {
            ESP_LOGE(TAG, "%d", __LINE__);
            ret = false;
            goto end;
        }
    }
    ESP_LOGI(TAG, "content_len:%d", content_len);

    while (binary_file_length < content_len)
    {
        //写入之前清0
        memset(ota_write_data, 0, sizeof(ota_write_data));
        //接收http包
        // if (model_id == SIM7600)
        // {
        //     buff_len = Read_HTTP_OTA_File(ota_write_data);
        // }
        // else

        buff_len = Read_OTA_File(ota_write_data);

        if (buff_len <= 0)
        {
            //包异常
            ESP_LOGE(TAG, "Error: receive data error! errno=%d buff_len=%d", errno, buff_len);
            ret = false;
            goto end;
            // task_fatal_error();
        }
        else
        {
            //写flash
            err = esp_ota_write(update_handle, (const void *)ota_write_data, buff_len);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Error: esp_ota_write failed! err=0x%x", err);
                ret = false;
                goto end;
            }
            binary_file_length += buff_len;
            if (percentage != (int)(binary_file_length * 100 / content_len))
            {
                percentage = (int)(binary_file_length * 100 / content_len);
                printf("4G OTA:%d%%\n", percentage);
            }
        }
    }
    //OTA写结束
    if (esp_ota_end(update_handle) != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_end failed!");
        ret = false;
        goto end;
    }
    //升级完成更新OTA data区数据，重启时根据OTA data区数据到Flash分区加载执行目标（新）固件
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed! err=0x%x", err);
        ret = false;
        goto end;
    }
    ESP_LOGI(TAG, "Update Successed\r\n ");
    ret = true;

end:
    // End_EC_OTA(file_handle);
    free(recv_buff);
    End_EC_TCP_OTA();
    xSemaphoreGive(xMutex_Http_Send);
    return ret;
    // esp_restart();
}

void ota_task(void *pvParameter)
{
    vTaskDelay(1000 / portTICK_PERIOD_MS); //等待数据同步完成
    ESP_LOGI(TAG, "Starting OTA...");

    if (net_mode == NET_WIFI)
    {
        WIFI_OTA();
    }
    else
    {

        if (EC20_OTA())
        {
            esp_restart();
        }
        vTaskDelete(NULL);
    }
}

void ota_start(void) //建立OTA升级任务，目的是为了让此函数被调用后尽快执行完毕
{
    if (OTA_FLAG == false)
    {
        xTaskCreate(ota_task, "ota task", 5120, NULL, 10, &ota_handle);
    }
}

void ota_back(void)
{
    esp_ota_mark_app_invalid_rollback_and_reboot();
}