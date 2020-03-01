#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"

#include "nvs_flash.h"
// #include "tcpip_adapter.h"
#include "esp_smartconfig.h"
/*  user include */
#include "Smartconfig.h"
#include "esp_log.h"
#include "Led.h"
#include "tcp_bsp.h"
// #include "w5500_driver.h"
#include "Json_parse.h"
#include "Bluetooth.h"

#define TAG "USER WIFI"

TaskHandle_t my_tcp_connect_Handle;
EventGroupHandle_t wifi_event_group;
EventGroupHandle_t tcp_event_group;

// uint8_t wifi_connect_sta = connect_N;
// uint8_t wifi_work_sta = turn_on;
uint8_t start_AP = 0;
uint8_t bl_flag = 0; //蓝牙配网模式
uint16_t Wifi_ErrCode = 0;

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        Led_Status = LED_STA_WIFIERR;
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        esp_wifi_connect();
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        Wifi_ErrCode = event->reason;
        ESP_LOGI(TAG, "Wi-Fi disconnected,reason:%d, trying to reconnect...", event->reason);
        if (Wifi_ErrCode >= 1 && Wifi_ErrCode <= 24) //适配APP，
        {
            Wifi_ErrCode += 300;
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void init_wifi(void) //
{
    start_AP = 0;
    // tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE)); //实验，测试解决wifi中断问题
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // ESP_ERROR_CHECK(esp_wifi_get_config(ESP_IF_WIFI_STA, &s_staconf));
    wifi_config_t s_staconf;
    memset(&s_staconf.sta, 0, sizeof(s_staconf));
    strcpy((char *)s_staconf.sta.ssid, wifi_data.wifi_ssid);
    strcpy((char *)s_staconf.sta.password, wifi_data.wifi_pwd);

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &s_staconf));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void stop_user_wifi(void)
{
    esp_err_t err = esp_wifi_stop();
    if (err == ESP_ERR_WIFI_NOT_INIT)
    {
        return;
    }
    ESP_ERROR_CHECK(err);
    printf("turn off WIFI! \n");
}

void start_user_wifi(void)
{
    esp_err_t err = esp_wifi_stop();
    if (err == ESP_ERR_WIFI_NOT_INIT)
    {
        return;
    }
    ESP_ERROR_CHECK(err);
    wifi_config_t s_staconf;
    memset(&s_staconf.sta, 0, sizeof(s_staconf));
    strcpy((char *)s_staconf.sta.ssid, wifi_data.wifi_ssid);
    strcpy((char *)s_staconf.sta.password, wifi_data.wifi_pwd);

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &s_staconf));
    ESP_ERROR_CHECK(esp_wifi_start());

    printf("turn on WIFI! \n");
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
//     wifi_event_group = xEventGroupCreate();
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

//测试用

// #define GOT_IPV4_BIT BIT(0)
// #define GOT_IPV6_BIT BIT(1)

// #define CONNECTED_BITS (GOT_IPV4_BIT)

// static EventGroupHandle_t s_connect_event_group;
// static esp_ip4_addr_t s_ip_addr;
// static const char *s_connection_name;
// static esp_netif_t *s_example_esp_netif = NULL;

// static const char *TAG = "wifi_connect";

// /* set up connection, Wi-Fi or Ethernet */
// static void start(void);

// /* tear down connection, release resources */
// static void stop(void);

// static void on_got_ip(void *arg, esp_event_base_t event_base,
//                       int32_t event_id, void *event_data)
// {
//     ESP_LOGI(TAG, "Got IP event!");
//     ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
//     memcpy(&s_ip_addr, &event->ip_info.ip, sizeof(s_ip_addr));
//     xEventGroupSetBits(s_connect_event_group, GOT_IPV4_BIT);
//     xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
// }
// static void on_wifi_disconnect(void *arg, esp_event_base_t event_base,
//                                int32_t event_id, void *event_data)
// {
//     xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
//     wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
//     Wifi_ErrCode = event->reason;
//     ESP_LOGI(TAG, "Wi-Fi disconnected,reason:%d, trying to reconnect...", event->reason);
//     if (Wifi_ErrCode >= 1 && Wifi_ErrCode <= 24) //适配APP，
//     {
//         Wifi_ErrCode += 300;
//     }
//     esp_err_t err = esp_wifi_connect();
//     // printf("on_wifi_disconnect ,%d", err);
//     if (err == ESP_ERR_WIFI_NOT_STARTED)
//     {
//         return;
//     }
//     ESP_ERROR_CHECK(err);
// }

// esp_err_t wifi_connect(void)
// {
//     if (s_connect_event_group != NULL)
//     {
//         return ESP_ERR_INVALID_STATE;
//     }
//     s_connect_event_group = xEventGroupCreate();
//     start();
//     // ESP_ERROR_CHECK(esp_register_shutdown_handler(&stop));
//     // ESP_LOGI(TAG, "Waiting for IP");
//     // xEventGroupWaitBits(s_connect_event_group, CONNECTED_BITS, true, true, portMAX_DELAY);
//     // ESP_LOGI(TAG, "Connected to %s", wifi_data.wifi_ssid);
//     // ESP_LOGI(TAG, "IPv4 address: " IPSTR, IP2STR(&s_ip_addr));

//     return ESP_OK;
// }

// esp_err_t wifi_disconnect(void)
// {
//     if (s_connect_event_group == NULL)
//     {
//         return ESP_ERR_INVALID_STATE;
//     }
//     vEventGroupDelete(s_connect_event_group);
//     s_connect_event_group = NULL;
//     stop();
//     ESP_LOGI(TAG, "Disconnected from %s", wifi_data.wifi_pwd);
//     s_connection_name = NULL;
//     return ESP_OK;
// }

// void re_connect_wifi(void)
// {
//     wifi_disconnect();
//     wifi_connect();
// }

// static void start(void)
// {
//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK(esp_wifi_init(&cfg));

//     esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_WIFI_STA();

//     esp_netif_t *netif = esp_netif_new(&netif_config);

//     assert(netif);

//     esp_netif_attach_wifi_station(netif);
//     esp_wifi_set_default_wifi_sta_handlers();

//     s_example_esp_netif = netif;

//     ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect, NULL));
//     ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL));

//     // ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
//     wifi_config_t wifi_config;
//     memcpy((char *)wifi_config.sta.ssid, wifi_data.wifi_ssid, 32);
//     memcpy((char *)wifi_config.sta.password, wifi_data.wifi_pwd, 64);
//     ESP_LOGI(TAG, "Connecting to %s...", wifi_config.sta.ssid);
//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//     ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
//     ESP_ERROR_CHECK(esp_wifi_start());
//     esp_err_t ret = (esp_wifi_connect());
//     ESP_LOGI(TAG, "esp_wifi_connect:%s", esp_err_to_name(ret));
//     // s_connection_name = (const char *)wifi_config.sta.ssid;
// }

// static void stop(void)
// {
//     ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect));
//     ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip));

//     esp_err_t err = esp_wifi_stop();
//     if (err == ESP_ERR_WIFI_NOT_INIT)
//     {
//         return;
//     }
//     ESP_ERROR_CHECK(err);
//     ESP_ERROR_CHECK(esp_wifi_deinit());
//     ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(s_example_esp_netif));
//     esp_netif_destroy(s_example_esp_netif);
//     s_example_esp_netif = NULL;
// }

// void init_wifi(void) //
// {
//     ESP_ERROR_CHECK(esp_netif_init());
//     ESP_ERROR_CHECK(esp_event_loop_create_default());
//     wifi_event_group = xEventGroupCreate();
//     wifi_connect();
// }
