#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_wifi.h"
// #include "esp_wpa2.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_smartconfig.h"
/*  user include */
#include "esp_log.h"
#include "Led.h"
// #include "tcp_bsp.h"
// #include "w5500_driver.h"
#include "Json_parse.h"
#include "Bluetooth.h"
#include "EC20.h"
#include "My_Mqtt.h"
#include "Http.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "Smartconfig.h"
#define TAG "NET_CFG"

// TaskHandle_t my_tcp_connect_Handle;
// EventGroupHandle_t tcp_event_group;

uint8_t start_AP = 0;
uint16_t Net_ErrCode = 0;
bool scan_flag = false;
char AP_SSID[15] = {0};

esp_netif_t *STA_netif_t;
esp_netif_t *AP_netif_t;

static void tcp_server_task(void *pvParameters);

void timer_wifi_cb(void *arg);
esp_timer_handle_t timer_wifi_handle = NULL; //定时器句柄
esp_timer_create_args_t timer_wifi_arg = {
    .callback = &timer_wifi_cb,
    .arg = NULL,
    .name = "Wifi_Timer"};

void timer_wifi_cb(void *arg)
{
    start_user_wifi();
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        if (scan_flag == false)
        {
            esp_wifi_connect();
        }
    }
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        esp_timer_start_once(timer_wifi_handle, 15000 * 1000);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (net_mode == NET_WIFI)
        {
            Net_sta_flag = false;
            xEventGroupClearBits(Net_sta_group, CONNECTED_BIT);
            Start_Active();
        }

        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        if (event->reason >= 200)
        {
            Net_ErrCode = event->reason;
        }
        ESP_LOGI(TAG, "Wi-Fi disconnected,reason:%d", event->reason);
        if (scan_flag == false)
        {
            esp_wifi_connect();
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        esp_timer_stop(timer_wifi_handle);
        xEventGroupSetBits(Net_sta_group, CONNECTED_BIT);
        Start_Active();
        // ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        // ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    }
    else if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }

    else
    {
        ESP_LOGI(TAG, "event_base=%s,event_id=%d", event_base, event_id);
    }
}

void init_wifi(void) //
{
    start_AP = 0;
    char temp[6] = {0};
    // tcpip_adapter_init();
    // Net_sta_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_timer_create(&timer_wifi_arg, &timer_wifi_handle));

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    STA_netif_t = esp_netif_create_default_wifi_sta();
    AP_netif_t = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    // ESP_ERROR_CHECK(esp_wifi_get_config(ESP_IF_WIFI_STA, &s_staconf));
    // wifi_config_t s_staconf;
    // memset(&s_staconf.sta, 0, sizeof(s_staconf));
    // strcpy((char *)s_staconf.sta.ssid, wifi_data.wifi_ssid);
    // strcpy((char *)s_staconf.sta.password, wifi_data.wifi_pwd);

    // ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &s_staconf));
    if (net_mode == NET_WIFI)
    {
        xEventGroupSetBits(Net_sta_group, WIFI_S_BIT);
        start_user_wifi();
    }

    strncpy(temp, SerialNum, 5);
    snprintf(AP_SSID, 15, "Ubibot-%s", temp);
    ESP_LOGI(TAG, "AP_SSID:%s", AP_SSID);
    //创建TCP监听端口 ，常开
    xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 7, NULL);
}

void stop_user_wifi(void)
{
    if ((xEventGroupGetBits(Net_sta_group) & WIFI_S_BIT) == WIFI_S_BIT)
    {
        xEventGroupClearBits(Net_sta_group, WIFI_S_BIT);
        esp_err_t err = esp_wifi_stop();
        if (err == ESP_ERR_WIFI_NOT_INIT)
        {
            return;
        }
        ESP_ERROR_CHECK(err);
        ESP_LOGI(TAG, "turn off WIFI! \n");
    }
    else
    {
        ESP_LOGI(TAG, "WIFI not start! \n");
    }
}

void start_user_wifi(void)
{
    if ((xEventGroupGetBits(Net_sta_group) & WIFI_S_BIT) == WIFI_S_BIT)
    {
        esp_err_t err = esp_wifi_stop();
        if (err == ESP_ERR_WIFI_NOT_INIT)
        {
            return;
        }
        ESP_ERROR_CHECK(err);
    }
    xEventGroupSetBits(Net_sta_group, WIFI_S_BIT);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    wifi_config_t s_staconf;
    memset(&s_staconf.sta, 0, sizeof(s_staconf));
    strcpy((char *)s_staconf.sta.ssid, wifi_data.wifi_ssid);
    strcpy((char *)s_staconf.sta.password, wifi_data.wifi_pwd);
    s_staconf.sta.scan_method = 1;
    s_staconf.sta.sort_method = 0;

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &s_staconf));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "turn on WIFI! \n");
}

//网络状态转换任务
void Net_Switch(void)
{
    xEventGroupClearBits(Net_sta_group, ACTIVED_BIT);
    xEventGroupClearBits(Net_sta_group, CONNECTED_BIT);
    ESP_LOGI("Net_Switch", "net_mode=%d", net_mode);
    switch (net_mode)
    {
    case NET_WIFI:
        start_user_wifi();
        Start_W_Mqtt();
        // EC20_Stop();
        break;

    case NET_4G:
        stop_user_wifi();
        Stop_W_Mqtt();
        EC20_Start();
        // if (eTaskGetState(EC20_Task_Handle) == eSuspended)
        // {
        //     vTaskResume(EC20_Task_Handle);
        // }

        break;

    default:
        break;
    }
}

#define DEFAULT_SCAN_LIST_SIZE 5
void Scan_Wifi(void)
{
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));
    scan_flag = true;
    start_user_wifi();

    if (esp_wifi_scan_start(NULL, true) != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_scan_start FAIL");
        scan_flag = false;
        return;
    }
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);
    for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++)
    {
        // ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
        // ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
        // ESP_LOGI(TAG, "Channel \t\t%d\n", ap_info[i].primary);
        //串口响应
        printf("{\"SSID\":\"%s\",\"rssi\":%d}\n\r", ap_info[i].ssid, ap_info[i].rssi);
    }

    scan_flag = false;
    if (net_mode == NET_WIFI)
    {
        start_user_wifi();
    }
    else
    {
        stop_user_wifi();
    }
}

bool Check_Wifi(uint8_t *ssid, int8_t *rssi)
{
    wifi_scan_time_t scanTime = {

        .passive = 5000};

    wifi_scan_config_t scanConf = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = 1,
        .scan_time = scanTime};

    bool ret = true;
    uint16_t number = 1;
    wifi_ap_record_t ap_info[1];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));
    scan_flag = true;
    start_user_wifi();

    if (esp_wifi_scan_start(NULL, true) != ESP_OK)
    {
        ret = false;
        ESP_LOGE(TAG, "esp_wifi_scan_start FAIL");
        scan_flag = false;
        return ret;
    }
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);

    memcpy(ssid, ap_info[0].ssid, sizeof(ap_info[0].ssid));
    *rssi = ap_info[0].rssi;

    scan_flag = false;
    if (net_mode == NET_WIFI)
    {
        start_user_wifi();
    }
    else
    {
        stop_user_wifi();
    }
    return ret;
}

void start_softap(void)
{
    if ((xEventGroupGetBits(Net_sta_group) & WIFI_S_BIT) == WIFI_S_BIT)
    {
        esp_err_t err = esp_wifi_stop();
        if (err == ESP_ERR_WIFI_NOT_INIT)
        {
            return;
        }
        ESP_ERROR_CHECK(err);
    }
    xEventGroupSetBits(Net_sta_group, WIFI_S_BIT);

    // esp_netif_create_default_wifi_ap();

    wifi_config_t wifi_config = {
        .ap = {
            // .ssid = AP_SSID,
            // .ssid_len = 5,
            // .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            // .password = "12345678",
            .max_connection = 5,
            .authmode = WIFI_AUTH_OPEN},
    };

    memcpy(wifi_config.ap.ssid, AP_SSID, sizeof(AP_SSID));
    // wifi_config.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));

    const esp_netif_ip_info_t ip_info = {
        .ip = {.addr = ESP_IP4TOADDR(192, 168, 1, 1)},
        .gw = {.addr = ESP_IP4TOADDR(192, 168, 1, 1)},
        .netmask = {.addr = ESP_IP4TOADDR(255, 255, 255, 0)},
    };

    //注意，ap使用DHCPS ,即 DHCP服务器
    ESP_ERROR_CHECK(esp_netif_dhcps_stop(AP_netif_t));
    esp_netif_set_ip_info(AP_netif_t, &ip_info);
    ESP_ERROR_CHECK(esp_netif_dhcps_start(AP_netif_t));

    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. ");
}

int Tcp_Send(int sock, char *Send_Buff)
{
    uint16_t Send_Buff_len = strlen(Send_Buff);
    int to_write = Send_Buff_len;
    int written = 0;
    // ESP_LOGI(TAG, "Send_Buff:%s,Len:%d,sock:%d", Send_Buff, Send_Buff_len, sock);
    while (to_write > 0)
    {
        written = send(sock, Send_Buff + (Send_Buff_len - to_write), to_write, 0);
        if (written < 0)
        {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
            return written;
        }
        to_write -= written;
    }

    return written;
}

//TCP 收发数据
static void do_retransmit(const int sock)
{
    int len;
    esp_err_t ret;
    char rx_buffer[1024];

    do
    {
        len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0)
        {
            ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
        }
        else if (len == 0)
        {
            ESP_LOGW(TAG, "Connection closed");
        }
        else
        {
            rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
            ESP_LOGI(TAG, "Received %d,strlen:%d, bytes: %s,sock:%d", len, strlen(rx_buffer), rx_buffer, sock);
            // send(sock, rx_buffer, len, 0);
            ret = ParseTcpUartCmd(rx_buffer, 1, sock);
            ESP_LOGI(TAG, "TCP ret:%d", ret);
        }
    } while (len > 0);
}

static void tcp_server_task(void *pvParameters)
{
    char addr_str[128];

    int ip_protocol = 0;
    struct sockaddr_in6 dest_addr;

    struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
    dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr_ip4->sin_family = AF_INET;
    dest_addr_ip4->sin_port = htons(TCP_PORT);
    ip_protocol = IPPROTO_IP;

    int listen_sock = socket(AF_INET, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0)
    {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0)
    {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        ESP_LOGE(TAG, "IPPROTO: %d", AF_INET);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", TCP_PORT);

    err = listen(listen_sock, 1);
    if (err != 0)
    {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    while (1)
    {

        ESP_LOGI(TAG, "Socket listening");

        struct sockaddr_in6 source_addr; // Large enough for both IPv4 or IPv6
        uint addr_len = sizeof(source_addr);
        ESP_LOGI(TAG, "%d", __LINE__);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        ESP_LOGI(TAG, "%d", __LINE__);
        if (sock < 0)
        {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        // Convert ip address to string
        if (source_addr.sin6_family == PF_INET)
        {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
            ESP_LOGI(TAG, "%d", __LINE__);
        }
        else if (source_addr.sin6_family == PF_INET6)
        {
            inet6_ntoa_r(source_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
            ESP_LOGI(TAG, "%d", __LINE__);
        }
        ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

        do_retransmit(sock);

        shutdown(sock, 0);
        close(sock);
    }

CLEAN_UP:
    esp_restart();
    close(listen_sock);
    vTaskDelete(NULL);
}