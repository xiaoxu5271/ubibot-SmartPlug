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
#include "Mqtt.h"
#include "Http.h"

#include "Smartconfig.h"
#define TAG "NET_CFG"

// TaskHandle_t my_tcp_connect_Handle;
// EventGroupHandle_t tcp_event_group;

uint8_t start_AP = 0;
uint16_t Net_ErrCode = 0;
bool scan_flag = false;

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
    else
    {
        ESP_LOGI(TAG, "event_base=%s,event_id=%d", event_base, event_id);
    }
}

void init_wifi(void) //
{
    start_AP = 0;
    // tcpip_adapter_init();
    // Net_sta_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_timer_create(&timer_wifi_arg, &timer_wifi_handle));

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
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

// /*
// * WIFI作为AP的初始化
// * @param[in]   void  		       :无
// * @retval      void                :无
// * @note        修改日志
// *               Ver0.0.1:
//                     hx-zsj, 2018/08/06, 初始化版本\n
// */

// //TaskHandle_t my_tcp_connect_Handle = NULL;
// void wifi_init_softap(void)
// {
//     tcp_event_group = xEventGroupCreate();
//     start_AP = 1;
//     Led_Status = LED_STA_AP;
//     ESP_ERROR_CHECK(esp_wifi_stop());
//     wifi_config_t wifi_config = {
//         .ap = {
//             .ssid = SOFT_AP_SSID, // SOFT_AP_SSID,
//             .password = SOFT_AP_PAS,
//             .ssid_len = 0,
//             .max_connection = SOFT_AP_MAX_CONNECT,
//             .authmode = WIFI_AUTH_WPA_WPA2_PSK}};
//     if (strlen(SOFT_AP_PAS) == 0)
//     {
//         wifi_config.ap.authmode = WIFI_AUTH_OPEN;
//     }
//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
//     ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
//     ESP_ERROR_CHECK(esp_wifi_start());
//     ESP_LOGI(TAG, "SoftAP set finish,SSID:%s password:%s \n",
//              wifi_config.ap.ssid, wifi_config.ap.password);

//     //my_tcp_connect_Handle = NULL;
//     xTaskCreate(&my_tcp_connect_task, "my_tcp_connect_task", 4096, NULL, 5, &my_tcp_connect_Handle);
// }

// /*
// * WIFI作为AP+STA的初始化
// * @param[in]   void  		       :无
// * @retval      void                :无
// * @note        修改日志
// *               Ver0.0.1:

// */
// void wifi_init_apsta(void)
// {
//     Net_sta_group = xEventGroupCreate();
//     tcpip_adapter_init();
//     ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK(esp_wifi_init(&cfg));
//     ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
//     wifi_config_t sta_wifi_config;
//     ESP_ERROR_CHECK(esp_wifi_get_config(ESP_IF_WIFI_STA, &sta_wifi_config));

//     wifi_config_t ap_wifi_config =
//         {
//             .ap = {
//                 .ssid = "esp32_ap",
//                 .password = "",
//                 //     .authmode = WIFI_AUTH_WPA_WPA2_PSK,
//                 .max_connection = 1,
//             },
//         };

//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
//     ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_wifi_config));
//     ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_wifi_config));
//     ESP_ERROR_CHECK(esp_wifi_start());
// }
