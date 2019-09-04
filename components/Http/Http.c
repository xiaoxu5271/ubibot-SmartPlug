#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "nvs.h"
#include "Json_parse.h"
#include "E2prom.h"
#include "Bluetooth.h"
#include "Led.h"
#include "Smartconfig.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "Http.h"
#include "ds18b20.h"
#include "RS485_Read.h"

SemaphoreHandle_t xMutex_Http_Send;

// extern uint8_t data_read[34];
char current_net_ip[20]; //当前内网IP，用于上传

static char *TAG = "HTTP";
uint8_t six_time_count = 4;
uint8_t post_status = POST_NOCOMMAND;
// bool need_send = 1;
bool need_reactivate = 0;

struct HTTP_STA
{
    char GET[10];
    char POST[10];
    char HEART_BEAT[64];

    char POST_URL1[64];
    char POST_URL_METADATA[16];
    char POST_URL_FIRMWARE[16];
    char POST_URL_SSID[16];
    char POST_URL_COMMAND_ID[16];
    char IP[10];

    char WEB_URL1[50];
    char WEB_URL2[20];
    char WEB_URL3[20];

    char HTTP_VERSION10[20];
    char HTTP_VERSION11[20];

    char HOST[30];
    char USER_AHENT[40];
    char CONTENT_LENGTH[30];
    char ENTER[10];

}

http =
    {
        "GET ",
        "POST ",
        "http://api.ubibot.cn/heartbeat?api_key=",

        "http://api.ubibot.cn/update.json?api_key=",
        "&metadata=true",
        "&firmware=",
        "&ssid=",
        "&command_id=",
        "&IP=",

        "http://api.ubibot.cn/products/",
        "/devices/",
        "/activate",

        " HTTP/1.0\r\n",
        " HTTP/1.1\r\n",

        "Host: api.ubibot.cn\r\n",
        "User-Agent: dalian urban ILS1\r\n",
        "Content-Length:",
        "\r\n\r\n",

};

TaskHandle_t httpHandle = NULL;

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

    int err = getaddrinfo(WEB_SERVER, "80", &hints, &res); //step1：DNS域名解析

    if (err != 0 || res == NULL)
    {
        ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
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
        vTaskDelay(4000 / portTICK_PERIOD_MS);
        return -1;
    }
    ESP_LOGI(TAG, "... allocated socket");

    if (connect(s, res->ai_addr, res->ai_addrlen) != 0) //step3：连接IP
    {
        ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
        close(s);
        freeaddrinfo(res);
        vTaskDelay(4000 / portTICK_PERIOD_MS);
        return -1;
    }

    ESP_LOGI(TAG, "... connected");
    freeaddrinfo(res);

    ESP_LOGD(TAG, "http_send_buff send_buff: %s\n", (char *)send_buff);
    if (write(s, (char *)send_buff, send_size) < 0) //step4：发送http包
    {
        ESP_LOGE(TAG, "... socket send failed");
        close(s);
        vTaskDelay(4000 / portTICK_PERIOD_MS);
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
        vTaskDelay(1000 / portTICK_PERIOD_MS);
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

int32_t http_send_buff(char *send_buff, uint16_t send_size, char *recv_buff, uint16_t recv_size)
{
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY); //等网络连接

    xSemaphoreTake(xMutex_Http_Send, portMAX_DELAY);

    int32_t ret;
    printf("wifi send!!!\n");
    ret = wifi_http_send(send_buff, send_size, recv_buff, recv_size);
    xSemaphoreGive(xMutex_Http_Send);

    return ret;
}

void http_get_task(void *pvParameters)
{
    char recv_buf[1024];
    char build_heart_url[256];

    sprintf(build_heart_url, "%s%s%s%s%s%s%s", http.GET, http.HEART_BEAT, ApiKey,
            http.HTTP_VERSION10, http.HOST, http.USER_AHENT, http.ENTER);

    while (1)
    {
        /* Wait for the callback to set the CONNECTED_BIT in the
		   event group.a
		*/
        xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                            false, true, portMAX_DELAY);

        // ESP_LOGI("heap_size", "Free Heap:%d,%d", esp_get_free_heap_size(), heap_caps_get_free_size(MALLOC_CAP_8BIT));
        //需要把数据发送到平台

        if (xSemaphoreTake(Binary_Http_Send, (fn_dp * 1000) / portTICK_PERIOD_MS) == pdTRUE)
        {
            http_send_mes();
            continue;
            // six_time_count = 0;
        }

        // if (need_reactivate == 1)
        // {
        //     need_reactivate = 0;
        //     http_activate();
        // }

        // if (fn_dp > 0)
        // {
        //     if (six_time_count++ >= fn_dp - 1)
        //     {
        //         six_time_count = 0;

        if ((http_send_buff(build_heart_url, 256, recv_buf, 1024)) > 0)
        {
            RS485_Read();
            ds18b20_get_temp();
            parse_objects_heart(strchr(recv_buf, '{'));
            http_send_mes();
        }
        else
        {
            printf("hart recv 0!\r\n");
        }
        //     }
        // }
        // vTaskDelay(1000 / portTICK_PERIOD_MS); //1s
    }
}

//激活流程
int32_t http_activate(void)
{
    char build_http[256];
    char recv_buf[1024];

    sprintf(build_http, "%s%s%s%s%s%s%s", http.GET, http.WEB_URL1, ProductId, http.WEB_URL2, SerialNum, http.WEB_URL3, http.ENTER);
    //http.HTTP_VERSION10, http.HOST, http.USER_AHENT, http.ENTER);

    ESP_LOGI(TAG, "build_http=%s\n", build_http);

    if (http_send_buff(build_http, 256, recv_buf, 1024) < 0)
    {
        return -1;
    }
    else
    {
        if (parse_objects_http_active(strchr(recv_buf, '{')))
        {
            return 1;
        }
        else
        {
            return -1;
        }
    }

    // return parse_objects_http_active(strchr(recv_buf, '{'));
}

// uint8_t Last_Led_Status;

void http_send_mes(void)
{
    int ret = 0;

    if (Led_Status != LED_STA_SEND) //解决两次发送间隔过短，导致LED一直闪烁
    {
        Last_Led_Status = Led_Status;
    }
    Led_Status = LED_STA_SEND;

    char recv_buf[1024];
    char build_po_url[512];
    char build_po_url_json[1024];
    char NET_NAME[35];
    char NET_MODE[16];

    // bzero(current_net_ip, sizeof(current_net_ip)); //有线网断开，不上传有线网IP
    bzero(NET_NAME, sizeof(NET_NAME));
    strcpy(NET_NAME, wifi_data.wifi_ssid);
    bzero(NET_MODE, sizeof(NET_MODE));
    strcpy(NET_MODE, http.POST_URL_SSID);

    creat_json *pCreat_json1 = malloc(sizeof(creat_json)); //为 pCreat_json1 分配内存  动态内存分配，与free() 配合使用
    //pCreat_json1->creat_json_b=malloc(1024);
    //创建POST的json格式
    create_http_json(pCreat_json1);

    if (post_status == POST_NOCOMMAND) //无commID
    {
        sprintf(build_po_url, "%s%s%s%s%s%s%s%s%s%s%s%s%s%d%s", http.POST, http.POST_URL1, ApiKey, http.POST_URL_METADATA, http.POST_URL_FIRMWARE, FIRMWARE, current_net_ip, NET_MODE, NET_NAME,
                http.HTTP_VERSION11, http.HOST, http.USER_AHENT, http.CONTENT_LENGTH, pCreat_json1->creat_json_c, http.ENTER);
        // sprintf(build_po_url, "%s%s%s%s%s%s%s%s%s%s%s%s%d%s", http.POST, http.POST_URL1, ApiKey, http.POST_URL_METADATA, http.POST_URL_FIRMWARE, FIRMWARE, http.POST_URL_SSID, NET_NAME,
        //         http.HTTP_VERSION11, http.HOST, http.USER_AHENT, http.CONTENT_LENGTH, pCreat_json1->creat_json_c, http.ENTER);
    }
    else
    {
        post_status = POST_NOCOMMAND;
        sprintf(build_po_url, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%d%s", http.POST, http.POST_URL1, ApiKey, http.POST_URL_METADATA, http.POST_URL_FIRMWARE, FIRMWARE, current_net_ip, NET_MODE, NET_NAME, http.POST_URL_COMMAND_ID, mqtt_json_s.mqtt_command_id,
                http.HTTP_VERSION11, http.HOST, http.USER_AHENT, http.CONTENT_LENGTH, pCreat_json1->creat_json_c, http.ENTER);
        // sprintf(build_po_url, "%s%s%s%s%s%s%s%s%s%s%s%s%d%s", http.POST, http.POST_URL1, ApiKey, http.POST_URL_METADATA, http.POST_URL_SSID, NET_NAME, http.POST_URL_COMMAND_ID, mqtt_json_s.mqtt_command_id,
        //         http.HTTP_VERSION11, http.HOST, http.USER_AHENT, http.CONTENT_LENGTH, pCreat_json1->creat_json_c, http.ENTER);
    }

    sprintf(build_po_url_json, "%s%s", build_po_url, pCreat_json1->creat_json_b);

    // printf("JSON_test = : %s\n", pCreat_json1->creat_json_b);

    free(pCreat_json1);
    printf("build_po_url_json =\r\n%s\r\n build end \r\n", build_po_url_json);

    //发送并解析返回数据
    /***********調用函數發送***********/

    if (http_send_buff(build_po_url_json, 1024, recv_buf, 1024) > 0)
    {
        // printf("解析返回数据！\n");
        parse_objects_http_respond(strchr(recv_buf, '{'));
    }
    else
    {
        printf("send return : %d \n", ret);
    }
}

void initialise_http(void)
{
    xMutex_Http_Send = xSemaphoreCreateMutex(); //创建HTTP发送互斥信号
    Binary_Http_Send = xSemaphoreCreateBinary();

    while (http_activate() < 0) //激活
    {
        ESP_LOGE(TAG, "activate fail\n");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    xTaskCreate(&http_get_task, "http_get_task", 8192, NULL, 6, &httpHandle);
}