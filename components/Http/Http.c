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
#include "My_Mqtt.h"

TaskHandle_t Binary_Heart_Send = NULL;
TaskHandle_t Binary_dp = NULL;
TaskHandle_t Binary_485_t = NULL;
TaskHandle_t Binary_485_th = NULL;
TaskHandle_t Binary_485_sth = NULL;
TaskHandle_t Binary_485_ws = NULL;
TaskHandle_t Binary_485_lt = NULL;
TaskHandle_t Binary_485_co2 = NULL;
TaskHandle_t Binary_485_IS = NULL;
TaskHandle_t Binary_ext = NULL;
TaskHandle_t Binary_energy = NULL;
TaskHandle_t Binary_ele_quan = NULL;
TaskHandle_t Active_Task_Handle = NULL;
TaskHandle_t Sw_on_Task_Handle = NULL;

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
    static uint64_t min_num = 0;
    min_num++;

    static uint64_t ble_num = 0;
    //超时关闭蓝牙广播
    if (Cnof_net_flag == true && BLE_CON_FLAG == false)
    {
        ble_num++;
        if (ble_num >= 60 * 15)
        {
            ble_app_stop();
            ble_num = 0;
        }
    }
    else
    {
        ble_num = 0;
    }

    //心跳
    if (min_num % 60 == 0)
    {
        vTaskNotifyGiveFromISR(Binary_Heart_Send, NULL);
    }

    if (fn_dp)
        if (min_num % fn_dp == 0)
        {
            vTaskNotifyGiveFromISR(Binary_dp, NULL);
        }
    if (fn_485_t)
        if (min_num % fn_485_t == 0)
        {
            vTaskNotifyGiveFromISR(Binary_485_t, NULL);
        }
    if (fn_485_th)
        if (min_num % fn_485_th == 0)
        {
            vTaskNotifyGiveFromISR(Binary_485_th, NULL);
        }
    if (fn_485_sth)
        if (min_num % fn_485_sth == 0)
        {
            vTaskNotifyGiveFromISR(Binary_485_sth, NULL);
        }

    if (fn_485_ws)
        if (min_num % fn_485_ws == 0)
        {
            vTaskNotifyGiveFromISR(Binary_485_ws, NULL);
        }
    if (fn_485_lt)
        if (min_num % fn_485_lt == 0)
        {
            vTaskNotifyGiveFromISR(Binary_485_lt, NULL);
        }
    if (fn_485_co2)
        if (min_num % fn_485_co2 == 0)
        {
            vTaskNotifyGiveFromISR(Binary_485_co2, NULL);
        }

    if (fn_sw_e)
        if (min_num % fn_sw_e == 0)
        {
            vTaskNotifyGiveFromISR(Binary_energy, NULL);
        }
    if (fn_sw_pc)
        if (min_num % fn_sw_pc == 0)
        {
            vTaskNotifyGiveFromISR(Binary_ele_quan, NULL);
        }
    if (fn_ext_t)
        if (min_num % fn_ext_t == 0)
        {
            vTaskNotifyGiveFromISR(Binary_ext, NULL);
        }

    if (fn_sw_on)
        if (min_num % fn_sw_on == 0)
        {
            vTaskNotifyGiveFromISR(Sw_on_Task_Handle, NULL);
        }

    if (fn_485_is)
        if (min_num % fn_485_is == 0)
        {
            vTaskNotifyGiveFromISR(Binary_485_IS, NULL);
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

    int err = getaddrinfo((const char *)WEB_SERVER, (const char *)WEB_PORT, &hints, &res); //step1：DNS域名解析

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

    if (connect(s, res->ai_addr, res->ai_addrlen) != 0) //step3：连接IP
    {
        ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
        close(s);
        freeaddrinfo(res);
        // vTaskDelay(4000 / portTICK_PERIOD_MS);
        return -1;
    }

    freeaddrinfo(res);

    // ESP_LOGD(TAG, "http_send_buff send_buff: %s\n", (char *)send_buff);
    if (write(s, (char *)send_buff, send_size) < 0) //step4：发送http包
    {
        ESP_LOGE(TAG, "... socket send failed");
        close(s);
        // vTaskDelay(4000 / portTICK_PERIOD_MS);
        return -1;
    }
    // ESP_LOGI(TAG, "... socket send success");
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
    // ESP_LOGI(TAG, "... set socket receiving timeout success");

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
#define POST_URL_LEN 512
#define CMD_LEN 1024
int32_t http_post_init(uint32_t Content_Length)
{
    char *build_po_url = (char *)malloc(POST_URL_LEN);
    char *cmd_buf = (char *)malloc(CMD_LEN);
    bzero(build_po_url, POST_URL_LEN);
    bzero(cmd_buf, CMD_LEN);
    int32_t ret;

    if (net_mode == NET_WIFI)
    {
        if (post_status == POST_NOCOMMAND) //无commID
        {
            snprintf(build_po_url, POST_URL_LEN, "POST /update.json?api_key=%s&metadata=true&execute=true&firmware=%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nContent-Length:%d\r\n\r\n",
                     ApiKey,
                     FIRMWARE,
                     WEB_SERVER,
                     Content_Length);
        }
        else
        {
            post_status = POST_NOCOMMAND;
            snprintf(build_po_url, POST_URL_LEN, "POST /update.json?api_key=%s&metadata=true&firmware=%s&command_id=%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nContent-Length:%d\r\n\r\n",
                     ApiKey,
                     FIRMWARE,
                     mqtt_json_s.mqtt_command_id,
                     WEB_SERVER,
                     Content_Length);
        }
        ESP_LOGI(TAG, "%d,%s", __LINE__, build_po_url);

        const struct addrinfo hints = {
            .ai_family = AF_INET,
            .ai_socktype = SOCK_STREAM,
        };
        struct addrinfo *res;
        // struct in_addr *addr;
        int32_t s = 0;
        int err = getaddrinfo((const char *)WEB_SERVER, (const char *)WEB_PORT, &hints, &res); //step1：DNS域名解析

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
            snprintf(build_po_url, POST_URL_LEN, "http://%s/update.json?api_key=%s&metadata=true&execute=true&firmware=%s",
                     WEB_SERVER,
                     ApiKey,
                     FIRMWARE);
        }
        else
        {
            post_status = POST_NOCOMMAND;
            snprintf(build_po_url, POST_URL_LEN, "http://%s/update.json?api_key=%s&metadata=true&firmware=%s&command_id=%s",
                     WEB_SERVER,
                     ApiKey,
                     FIRMWARE,
                     mqtt_json_s.mqtt_command_id);
        }

        if (model_id == SIM7600)
        {
            //conect URL
            snprintf(cmd_buf, CMD_LEN, "AT+HTTPPARA=\"URL\",\"%s\"\r\n", build_po_url);
            if (AT_Cmd_Send(NULL, 0, 0, cmd_buf, "OK", 1000, 1) == false)
            {
                ESP_LOGE(TAG, "EC20_Post %d", __LINE__);
                ret = -1;
                goto end;
            }

            //send data to post ,INPUT LEN
            bzero(cmd_buf, CMD_LEN);
            snprintf(cmd_buf, CMD_LEN, "AT+HTTPDATA=%d,5000\r\n", Content_Length);
            if (AT_Cmd_Send(NULL, 0, 0, cmd_buf, "DOWNLOAD", 2000, 1) == false)
            {
                ESP_LOGE(TAG, "EC20_Post %d", __LINE__);
                ret = -1;
                goto end;
            }
            ret = 1;
        }
        else
        {
            if (EC20_Net_Check() == 0)
            {
                ESP_LOGE(TAG, "EC20_Post %d", __LINE__);
                ret = -1;
                goto end;
            }

            snprintf(cmd_buf, CMD_LEN, "AT+QHTTPURL=%d,60\r\n", (strlen(build_po_url)));
            if (AT_Cmd_Send(NULL, 0, 0, cmd_buf, "CONNECT", 2000, 1) == false)
            {
                ESP_LOGE(TAG, "EC20_Post %d", __LINE__);
                ret = -1;
                goto end;
            }

            bzero(cmd_buf, CMD_LEN);
            snprintf(cmd_buf, CMD_LEN, "%s\r\n", build_po_url);
            if (AT_Cmd_Send(NULL, 0, 0, cmd_buf, "OK", 60000, 1) == false)
            {
                ESP_LOGE(TAG, "EC20_Post %d", __LINE__);
                ret = -1;
                goto end;
            }

            bzero(cmd_buf, CMD_LEN);
            snprintf(cmd_buf, CMD_LEN, "AT+QHTTPPOST=%d,%d,%d\r\n", Content_Length, 60, 60);
            if (AT_Cmd_Send(NULL, 0, 0, cmd_buf, "CONNECT", 6000, 1) == false)
            {
                ESP_LOGE(TAG, "EC20_Post %d", __LINE__);
                ret = -1;
                goto end;
            }
            ret = 1;
        }
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
    Net_sta_flag = ret;
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
Net_Err Send_herat(void)
{
    char *build_heart_url;
    char *recv_buf;
    Net_Err ret = NET_OK;
    xEventGroupWaitBits(Net_sta_group, ACTIVED_BIT, false, true, -1); //等待激活

    xSemaphoreTake(xMutex_Http_Send, -1);
    build_heart_url = (char *)malloc(POST_URL_LEN);
    recv_buf = (char *)malloc(HTTP_RECV_BUFF_LEN);
    if (net_mode == NET_WIFI)
    {
        snprintf(build_heart_url, POST_URL_LEN, "GET /heartbeat?api_key=%s HTTP/1.1\r\nHost: %s\r\n\r\n",
                 ApiKey,
                 WEB_SERVER);

        if ((http_send_buff(build_heart_url, 256, recv_buf, HTTP_RECV_BUFF_LEN)) > 0)
        {
            if (parse_objects_heart(recv_buf))
            {
                //successed
                ret = NET_OK;
                Net_sta_flag = true;
            }
            else
            {
                ESP_LOGE(TAG, "hart recv:%s", recv_buf);
                ret = NET_400;
                Net_sta_flag = false;
            }
        }
        else
        {
            ret = NET_DIS;
            Net_sta_flag = false;
            ESP_LOGE(TAG, "hart recv 0!\r\n");
        }
    }
    else
    {
        snprintf(build_heart_url, POST_URL_LEN, "http://%s/heartbeat?api_key=%s", WEB_SERVER, ApiKey);
        if (EC20_Active(build_heart_url, recv_buf) == 0)
        {
            ret = NET_DIS;
            Net_sta_flag = false;
            ESP_LOGE(TAG, "hart recv 0!\r\n");
        }
        else
        {
            // ESP_LOGI(TAG, "active recv:%s", recv_buf);
            if (parse_objects_heart(recv_buf))
            {
                ret = NET_OK;
                Net_sta_flag = true;
            }
            else
            {
                ret = NET_400;
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
    Net_Err ret;
    while (1)
    {
        ESP_LOGW("heart_memroy check", " INTERNAL RAM left %dKB，free Heap:%d",
                 heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024,
                 esp_get_free_heap_size());

        while ((ret = Send_herat()) != NET_OK)
        {
            if (ret != NET_DIS) //非网络错误，需重新激活
            {
                Start_Active();
                break;
            }
            vTaskDelay(10000 / portTICK_PERIOD_MS);
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
    // ESP_LOGI(TAG, "%d", __LINE__);
    xSemaphoreTake(xMutex_Http_Send, -1);
    // ESP_LOGI(TAG, "%d", __LINE__);
    if (net_mode == NET_WIFI)
    {
        snprintf(build_http, POST_URL_LEN, "GET /products/%s/devices/%s/activate HTTP/1.1\r\nHost: %s\r\n\r\n", ProductId, SerialNum, WEB_SERVER);
        ESP_LOGI(TAG, "%s", build_http);
        if (wifi_http_send(build_http, 256, recv_buf, HTTP_RECV_BUFF_LEN) < 0)
        {
            Net_sta_flag = false;
            ret = 301;
        }
        else
        {
            if (parse_objects_http_active(recv_buf))
            {
                Net_sta_flag = true;
                ret = 1;
            }
            else
            {
                ESP_LOGE(TAG, "active recv:%s", recv_buf);
                Net_sta_flag = false;
                ret = 302;
            }
        }
    }
    else
    {
        snprintf(build_http, POST_URL_LEN, "http://%s/products/%s/devices/%s/activate", WEB_SERVER, ProductId, SerialNum);
        if (EC20_Active(build_http, recv_buf) == 0)
        {
            ESP_LOGE(TAG, "active ERR");
            Net_sta_flag = false;
            ret = 301;
        }
        else
        {
            // ESP_LOGI(TAG, "active recv:%s", recv_buf);
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
    xSemaphoreGive(xMutex_Http_Send);
    free(build_http);
    free(recv_buf);
    return ret;
}

void Active_Task(void *arg)
{
    xEventGroupSetBits(Net_sta_group, ACTIVE_S_BIT);
    uint8_t Retry_num = 0;
    while (1)
    {
        xEventGroupClearBits(Net_sta_group, ACTIVED_BIT);
        while ((Net_ErrCode = http_activate()) != 1) //激活
        {
            if (Net_ErrCode == 302)
            {
                Retry_num++;
                if (Retry_num > 60)
                {
                    xEventGroupClearBits(Net_sta_group, ACTIVE_S_BIT);
                    vTaskDelete(NULL);
                }

                vTaskDelay(10000 / portTICK_PERIOD_MS);
            }
            else
            {
                vTaskDelay(2000 / portTICK_PERIOD_MS);
            }
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
    else
    {
        // ESP_LOGI(TAG, "%d", __LINE__);
    }
}

void initialise_http(void)
{
    xTaskCreate(send_heart_task, "send_heart_task", 4096, NULL, 5, &Binary_Heart_Send);
    esp_err_t err = esp_timer_create(&timer_heart_arg, &timer_heart_handle);
    err = esp_timer_start_periodic(timer_heart_handle, 1000000); //创建定时器，单位us，定时1s
    if (err != ESP_OK)
    {
        ESP_LOGI(TAG, "timer heart create err code:%d\n", err);
    }
}