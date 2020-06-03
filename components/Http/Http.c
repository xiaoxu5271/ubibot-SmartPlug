#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
// #include "nvs_flash.h"

// #include "nvs.h"
#include "Json_parse.h"
#include "E2prom.h"
#include "Bluetooth.h"
#include "Led.h"
#include "Smartconfig.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "EC20.h"

// #include "w5500_driver.h"
#include "my_base64.h"
#include "Http.h"
#include "Mqtt.h"

TaskHandle_t Binary_Heart_Send = NULL;
TaskHandle_t Binary_dp = NULL;
TaskHandle_t Binary_485_t = NULL;
TaskHandle_t Binary_485_th = NULL;
TaskHandle_t Binary_485_sth = NULL;
TaskHandle_t Binary_485_ws = NULL;
TaskHandle_t Binary_485_lt = NULL;
TaskHandle_t Binary_485_co2 = NULL;
TaskHandle_t Binary_ext = NULL;
TaskHandle_t Binary_energy = NULL;
TaskHandle_t Binary_ele_quan = NULL;

TaskHandle_t Active_Task_Handle = NULL;
// extern uint8_t data_read[34];

static char *TAG = "HTTP";
uint8_t post_status = POST_NOCOMMAND;

esp_timer_handle_t http_timer_suspend_p = NULL;

void timer_heart_cb(void *arg);
esp_timer_handle_t timer_heart_handle = NULL; //定时器句柄
esp_timer_create_args_t timer_heart_arg = {
    .callback = &timer_heart_cb,
    .arg = NULL,
    .name = "Heart_Timer"};

//1min 定时，用来触发各组数据采集/发送
void timer_heart_cb(void *arg)
{
    vTaskNotifyGiveFromISR(Binary_Heart_Send, NULL);
    static uint32_t min_num = 0;
    min_num++;
    if (fn_dp)
        if (min_num * 60 % fn_dp == 0)
        {
            vTaskNotifyGiveFromISR(Binary_dp, NULL);
        }
    if (fn_485_t)
        if (min_num * 60 % fn_485_t == 0)
        {
            vTaskNotifyGiveFromISR(Binary_485_t, NULL);
        }
    if (fn_485_th)
        if (min_num * 60 % fn_485_th == 0)
        {
            vTaskNotifyGiveFromISR(Binary_485_th, NULL);
        }
    if (fn_485_sth)
        if (min_num * 60 % fn_485_sth == 0)
        {
            vTaskNotifyGiveFromISR(Binary_485_sth, NULL);
        }

    if (fn_485_ws)
        if (min_num * 60 % fn_485_ws == 0)
        {
            vTaskNotifyGiveFromISR(Binary_485_ws, NULL);
        }
    if (fn_485_lt)
        if (min_num * 60 % fn_485_lt == 0)
        {
            vTaskNotifyGiveFromISR(Binary_485_lt, NULL);
        }
    if (fn_485_co2)
        if (min_num * 60 % fn_485_co2 == 0)
        {
            vTaskNotifyGiveFromISR(Binary_485_co2, NULL);
        }

    if (fn_sw_e)
        if (min_num * 60 % fn_sw_e == 0)
        {
            vTaskNotifyGiveFromISR(Binary_energy, NULL);
        }
    if (fn_sw_pc)
        if (min_num * 60 % fn_sw_pc == 0)
        {
            vTaskNotifyGiveFromISR(Binary_ele_quan, NULL);
        }
    if (fn_ext)
        if (min_num * 60 % fn_ext == 0)
        {
            vTaskNotifyGiveFromISR(Binary_ext, NULL);
        }
}

int32_t wifi_http_send(char *send_buff, uint16_t send_size, char *recv_buff, uint16_t recv_size)
{
    // printf("wifi http send start!\n");
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    struct in_addr *addr;
    int32_t s = 0, r = 0;

    int err = getaddrinfo((const char *)WEB_SERVER, "80", &hints, &res); //step1：DNS域名解析

    if (err != 0 || res == NULL)
    {
        ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
        // vTaskDelay(1000 / portTICK_PERIOD_MS);
        return -1;
    }

    /* Code to print the resolved IP.
		Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
    addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
    ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

    s = socket(res->ai_family, res->ai_socktype, 0); //step2：新建套接字
    if (s < 0)
    {
        ESP_LOGE(TAG, "... Failed to allocate socket. err:%d", s);
        close(s);
        freeaddrinfo(res);
        // vTaskDelay(4000 / portTICK_PERIOD_MS);
        return -1;
    }
    ESP_LOGI(TAG, "... allocated socket");

    if (connect(s, res->ai_addr, res->ai_addrlen) != 0) //step3：连接IP
    {
        ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
        close(s);
        freeaddrinfo(res);
        // vTaskDelay(4000 / portTICK_PERIOD_MS);
        return -1;
    }

    ESP_LOGI(TAG, "... connected");
    freeaddrinfo(res);

    ESP_LOGD(TAG, "http_send_buff send_buff: %s\n", (char *)send_buff);
    if (write(s, (char *)send_buff, send_size) < 0) //step4：发送http包
    {
        ESP_LOGE(TAG, "... socket send failed");
        close(s);
        // vTaskDelay(4000 / portTICK_PERIOD_MS);
        return -1;
    }
    ESP_LOGI(TAG, "... socket send success");
    struct timeval receiving_timeout;
    receiving_timeout.tv_sec = 5;
    receiving_timeout.tv_usec = 0;
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, //step5：设置接收超时
                   sizeof(receiving_timeout)) < 0)
    {
        ESP_LOGE(TAG, "... failed to set socket receiving timeout");
        close(s);
        // vTaskDelay(1000 / portTICK_PERIOD_MS);
        return -1;
    }
    ESP_LOGI(TAG, "... set socket receiving timeout success");

    /* Read HTTP response */

    bzero((uint16_t *)recv_buff, recv_size);
    r = read(s, (uint16_t *)recv_buff, recv_size);
    ESP_LOGD(TAG, "r=%d,activate recv_buf=%s\r\n", r, (char *)recv_buff);
    close(s);
    // printf("http send end!\n");
    return r;
}

//http post 初始化
//return socke
int32_t http_post_init(uint32_t Content_Length)
{
    char *build_po_url = (char *)malloc(512);
    char *cmd_buf = (char *)malloc(30);
    bzero(build_po_url, 512);
    bzero(cmd_buf, 30);
    int32_t ret;

    if (net_mode == NET_WIFI)
    {
        if (post_status == POST_NOCOMMAND) //无commID
        {
            sprintf(build_po_url, "POST http://%s/update.json?api_key=%s&metadata=true&firmware=%s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json;charset=UTF-8\r\nConnection: close\r\nContent-Length:%d\r\n\r\n",
                    WEB_SERVER,
                    ApiKey,
                    FIRMWARE,
                    WEB_SERVER,
                    Content_Length);
        }
        else
        {
            post_status = POST_NOCOMMAND;
            sprintf(build_po_url, "POST http://%s/update.json?api_key=%s&metadata=true&firmware=%s&command_id=%s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json;charset=UTF-8\r\nConnection: close\r\nContent-Length:%d\r\n\r\n",
                    WEB_SERVER,
                    ApiKey,
                    FIRMWARE,
                    mqtt_json_s.mqtt_command_id,
                    WEB_SERVER,
                    Content_Length);
        }

        const struct addrinfo hints = {
            .ai_family = AF_INET,
            .ai_socktype = SOCK_STREAM,
        };
        struct addrinfo *res;
        // struct in_addr *addr;
        int32_t s = 0;

        int err = getaddrinfo("api.ubibot.cn", "80", &hints, &res); //step1：DNS域名解析

        if (err != 0 || res == NULL)
        {
            ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
            // vTaskDelay(1000 / portTICK_PERIOD_MS);
            ret = -1;
            goto end;
        }

        // addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
        // ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

        s = socket(res->ai_family, res->ai_socktype, 0); //step2：新建套接字
        if (s < 0)
        {
            ESP_LOGE(TAG, "... Failed to allocate socket. err:%d", s);
            close(s);
            freeaddrinfo(res);
            // vTaskDelay(4000 / portTICK_PERIOD_MS);
            ret = -1;
            goto end;
        }
        // ESP_LOGI(TAG, "... allocated socket");

        if (connect(s, res->ai_addr, res->ai_addrlen) != 0) //step3：连接IP
        {
            ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
            close(s);
            freeaddrinfo(res);
            // vTaskDelay(4000 / portTICK_PERIOD_MS);
            ret = -1;
            goto end;
        }

        // ESP_LOGI(TAG, "... connected");
        freeaddrinfo(res);

        if (write(s, build_po_url, strlen(build_po_url)) < 0) //step4：发送http Header
        {
            ESP_LOGE(TAG, "... socket send failed");
            close(s);
            // vTaskDelay(4000 / portTICK_PERIOD_MS);
            ret = -1;
            goto end;
        }
        ret = s;
    }
    else
    {
        // ESP_LOGI(TAG, "EC20 POST INIT");
        if (post_status == POST_NOCOMMAND) //无commID
        {
            sprintf(build_po_url, "http://%s/update.json?api_key=%s&metadata=true&firmware=%s\r\n",
                    WEB_SERVER,
                    ApiKey,
                    FIRMWARE);
        }
        else
        {
            post_status = POST_NOCOMMAND;
            sprintf(build_po_url, "http://%s/update.json?api_key=%s&metadata=true&firmware=%s&command_id=%s\r\n",
                    WEB_SERVER,
                    ApiKey,
                    FIRMWARE,
                    mqtt_json_s.mqtt_command_id);
        }

        if (EC20_Net_Check() == 0)
        {
            ESP_LOGE(TAG, "EC20_Post %d", __LINE__);
            ret = -1;
            goto end;
        }

        sprintf(cmd_buf, "AT+QHTTPURL=%d,60\r\n", (strlen(build_po_url) - 2));
        if (AT_Cmd_Send(cmd_buf, "CONNECT", 1000, 1) == NULL)
        {
            ESP_LOGE(TAG, "EC20_Post %d", __LINE__);
            ret = -1;
            goto end;
        }

        if (AT_Cmd_Send(build_po_url, "OK", 60000, 1) == NULL)
        {
            ESP_LOGE(TAG, "EC20_Post %d", __LINE__);
            ret = -1;
            goto end;
        }

        bzero(cmd_buf, 30);
        sprintf(cmd_buf, "AT+QHTTPPOST=%d,%d,%d\r\n", Content_Length, 60, 60);
        if (AT_Cmd_Send(cmd_buf, "CONNECT", 6000, 1) == NULL)
        {
            ESP_LOGE(TAG, "EC20_Post %d", __LINE__);
            ret = -1;
            goto end;
        }
        ret = 1;
    }

end:
    free(build_po_url);
    free(cmd_buf);
    return ret;
}

//发送 http post 数据
int8_t http_send_post(int32_t s, char *post_buf, bool end_flag)
{
    int8_t ret = 1;
    if (net_mode == NET_WIFI)
    {
        if (write(s, post_buf, strlen((const char *)post_buf)) < 0) //step4：发送http Header
        {
            ESP_LOGE(TAG, "... socket send failed");
            close(s);
            ret = -1;
        }
    }
    else
    {
        if (EC20_Send_Post_Data(post_buf, end_flag) != 1)
        {
            ESP_LOGE(TAG, "EC20 send failed");
            ret = -1;
        }
    }
    return ret;
}

//读取http post 返回
bool http_post_read(int32_t s, char *recv_buff, uint16_t buff_size)
{
    bool ret;
    if (net_mode == NET_WIFI)
    {
        struct timeval receiving_timeout;
        receiving_timeout.tv_sec = 15;
        receiving_timeout.tv_usec = 0;
        if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, //设置接收超时
                       sizeof(receiving_timeout)) < 0)
        {
            ESP_LOGE(TAG, "... failed to set socket receiving timeout");
            close(s);
            // vTaskDelay(1000 / portTICK_PERIOD_MS);
            ret = false;
            goto end;
        }
        // ESP_LOGI(TAG, "... set socket receiving timeout success");

        // bzero((uint16_t *)recv_buff, buff_size);
        if (read(s, (uint16_t *)recv_buff, buff_size) > 0)
        {
            ret = true;
            // ESP_LOGI(TAG, "r=%d,activate recv_buf=%s\r\n", ret, (char *)recv_buff);
        }
        else
        {
            ret = false;
        }
        close(s);
    }
    else
    {
        if (EC20_Read_Post_Data(recv_buff, buff_size) == 1)
        {
            ret = true;
        }

        else
        {
            ret = false;
        }
    }

end:
    return ret;
}

int32_t http_send_buff(char *send_buff, uint16_t send_size, char *recv_buff, uint16_t recv_size)
{
    int32_t ret;

    ESP_LOGI(TAG, "wifi send!!!\n");
    ret = wifi_http_send(send_buff, send_size, recv_buff, recv_size);

    return ret;
}

//发送心跳包
bool Send_herat(void)
{
    char *build_heart_url;
    char *recv_buf;
    bool ret = false;
    xEventGroupWaitBits(Net_sta_group, ACTIVED_BIT, false, true, -1); //等网络连接

    xSemaphoreTake(xMutex_Http_Send, -1);
    build_heart_url = (char *)malloc(256);
    recv_buf = (char *)malloc(HTTP_RECV_BUFF_LEN);
    if (net_mode == NET_WIFI)
    {
        sprintf(build_heart_url, "GET http://%s/heartbeat?api_key=%s HTTP/1.0\r\nHost: %sUser-Agent: dalian urban ILS1\r\n\r\n",
                WEB_SERVER,
                ApiKey,
                WEB_SERVER);

        if ((http_send_buff(build_heart_url, 256, recv_buf, HTTP_RECV_BUFF_LEN)) > 0)
        {
            ESP_LOGI(TAG, "hart recv:%s", recv_buf);
            if (parse_objects_heart(recv_buf))
            {
                //successed
                ret = true;
                Net_sta_flag = true;
            }
            else
            {
                ret = false;
                Net_sta_flag = false;
            }
        }
        else
        {
            ret = false;
            Net_sta_flag = false;
            ESP_LOGE(TAG, "hart recv 0!\r\n");
        }
    }
    else
    {
        sprintf(build_heart_url, "http://%s/heartbeat?api_key=%s\r\n", WEB_SERVER, ApiKey);
        if (EC20_Active(build_heart_url, recv_buf) == 0)
        {
            ret = false;
            Net_sta_flag = false;
            ESP_LOGE(TAG, "hart recv 0!\r\n");
        }
        else
        {
            // ESP_LOGI(TAG, "active recv:%s", recv_buf);
            if (parse_objects_heart(recv_buf))
            {
                ret = true;
                Net_sta_flag = true;
            }
            else
            {
                ret = false;
                Net_sta_flag = false;
            }
        }
    }

    xSemaphoreGive(xMutex_Http_Send);
    free(recv_buf);
    free(build_heart_url);
    return ret;
}

void send_heart_task(void *arg)
{
    while (1)
    {
        ESP_LOGW("heart_memroy check", " INTERNAL RAM left %dKB，free Heap:%d",
                 heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024,
                 esp_get_free_heap_size());

        while (Send_herat() == false)
        {
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }

        ulTaskNotifyTake(pdTRUE, -1);
    }
}

//激活流程
uint16_t http_activate(void)
{
    char *build_http = (char *)malloc(256);
    char *recv_buf = (char *)malloc(HTTP_RECV_BUFF_LEN);
    uint16_t ret;

    xEventGroupWaitBits(Net_sta_group, CONNECTED_BIT, false, true, -1); //等网络连接
    xSemaphoreTake(xMutex_Http_Send, -1);
    if (net_mode == NET_WIFI)
    {
        sprintf(build_http, "GET http://%s/products/%s/devices/%s/activate\r\n\r\n", WEB_SERVER, ProductId, SerialNum);
        if (wifi_http_send(build_http, 256, recv_buf, HTTP_RECV_BUFF_LEN) < 0)
        {
            Net_sta_flag = false;
            ret = 301;
        }
        else
        {
            ESP_LOGI(TAG, "active recv:%s", recv_buf);
            if (parse_objects_http_active(recv_buf))
            {
                Net_sta_flag = true;
                ret = 1;
            }
            else
            {
                Net_sta_flag = false;
                ret = 302;
            }
        }
    }
    else
    {
        sprintf(build_http, "http://%s/products/%s/devices/%s/activate\r\n", WEB_SERVER, ProductId, SerialNum);
        if (EC20_Active(build_http, recv_buf) == 0)
        {
            ESP_LOGE(TAG, "active ERR");
            Net_sta_flag = false;
            ret = 101;
        }
        else
        {
            ESP_LOGI(TAG, "active recv:%s", recv_buf);
            if (parse_objects_http_active(recv_buf))
            {
                Net_sta_flag = true;
                ret = 1;
            }
            else
            {
                Net_sta_flag = false;
                ret = 102;
            }
        }
    }
    xSemaphoreGive(xMutex_Http_Send);
    free(build_http);
    free(recv_buf);
    return ret;
}

void Active_Task(void *arg)
{
    xEventGroupSetBits(Net_sta_group, ACTIVE_S_BIT);
    while (1)
    {
        xEventGroupClearBits(Net_sta_group, ACTIVED_BIT);
        while ((Net_ErrCode = http_activate()) != 1) //激活
        {
            ESP_LOGE(TAG, "activate fail\n");
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
        xEventGroupSetBits(Net_sta_group, ACTIVED_BIT);
        break;
    }
    xEventGroupClearBits(Net_sta_group, ACTIVE_S_BIT);
    vTaskDelete(NULL);
}

void Start_Active(void)
{
    if ((xEventGroupGetBits(Net_sta_group) & ACTIVE_S_BIT) != ACTIVE_S_BIT)
    {
        xTaskCreate(Active_Task, "Active_Task", 3072, NULL, 4, &Active_Task_Handle);
    }
}

void initialise_http(void)
{
    xTaskCreate(send_heart_task, "send_heart_task", 4096, NULL, 5, &Binary_Heart_Send);
    esp_err_t err = esp_timer_create(&timer_heart_arg, &timer_heart_handle);
    err = esp_timer_start_periodic(timer_heart_handle, 60 * 1000000); //创建定时器，单位us，定时60s
    if (err != ESP_OK)
    {
        ESP_LOGI(TAG, "timer heart create err code:%d\n", err);
    }
}