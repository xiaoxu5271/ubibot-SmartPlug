#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"

#include "nvs_flash.h"
#include "tcpip_adapter.h"
#include "esp_smartconfig.h"
/*  user include */
#include "Smartconfig.h"
#include "esp_log.h"
#include "Led.h"
#include "tcp_bsp.h"
#include "Json_parse.h"
#include "Bluetooth.h"

TaskHandle_t my_tcp_connect_Handle;
EventGroupHandle_t wifi_event_group;
EventGroupHandle_t tcp_event_group;

wifi_config_t s_staconf;

uint8_t wifi_connect_sta = connect_N;
uint8_t wifi_work_sta = turn_on;
uint8_t start_AP = 0;
uint8_t Wifi_ErrCode = 0;

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id)
    {
    case SYSTEM_EVENT_STA_START:
        //esp_wifi_connect();
        break;

    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        Led_Status = LED_STA_WORK; //联网工作
        wifi_connect_sta = connect_Y;
        break;

    case SYSTEM_EVENT_STA_DISCONNECTED:

        ESP_LOGI(TAG, "断网");
        Wifi_ErrCode = event->event_info.disconnected.reason;
        wifi_connect_sta = connect_N;
        if (start_AP != 1) //判断是不是要进入AP模式
        {
            Led_Status = LED_STA_WIFIERR; //断网
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            esp_wifi_connect();
        }
        else
        {
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        }
        break;

    case SYSTEM_EVENT_AP_STACONNECTED: //AP模式-有STA连接成功
        //作为ap，有sta连接
        ESP_LOGI(TAG, "station:" MACSTR " join,AID=%d\n",
                 MAC2STR(event->event_info.sta_connected.mac),
                 event->event_info.sta_connected.aid);
        xEventGroupSetBits(tcp_event_group, AP_STACONNECTED_BIT);
        break;

    case SYSTEM_EVENT_AP_STADISCONNECTED: //AP模式-有STA断线
        ESP_LOGI(TAG, "station:" MACSTR "leave,AID=%d\n",
                 MAC2STR(event->event_info.sta_disconnected.mac),
                 event->event_info.sta_disconnected.aid);

        g_rxtx_need_restart = true;
        xEventGroupClearBits(tcp_event_group, AP_STACONNECTED_BIT);
        break;

    default:
        ESP_LOGI(TAG, "event->event_id:%d", event->event_id);
        break;
    }
    return ESP_OK;
}

void initialise_wifi(void) //(char *wifi_ssid, char *wifi_password)
{
    // printf("WIFI Reconnect,SSID=%s,PWD=%s\r\n", wifi_ssid, wifi_password);
    // bzero(wifi_data.wifi_ssid, sizeof(wifi_data.wifi_ssid));
    // strcpy(wifi_data.wifi_ssid, wifi_ssid);

    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_get_config(ESP_IF_WIFI_STA, &s_staconf));

    if (s_staconf.sta.ssid[0] != '\0') //判断当前系统中是否有WIFI信息,
    {
        memset(&s_staconf.sta, 0, sizeof(s_staconf)); //清空原有数据
    }
    strcpy((char *)s_staconf.sta.ssid, wifi_data.wifi_ssid);
    strcpy((char *)s_staconf.sta.password, wifi_data.wifi_pwd);

    if (start_AP == 1) //如果是从AP模式进入，则需要重新设置为STA模式
    {
        start_AP = 0;
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &s_staconf));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_connect();
}

void reconnect_wifi_usr(void)
{
    printf("WIFI Reconnect\r\n");
    ESP_ERROR_CHECK(esp_wifi_get_config(ESP_IF_WIFI_STA, &s_staconf));

    ESP_ERROR_CHECK(esp_wifi_stop());
    memset(&s_staconf.sta, 0, sizeof(s_staconf));

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &s_staconf));
    start_user_wifi();
}

void init_wifi(void) //
{
    start_AP = 0;
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    memset(&s_staconf.sta, 0, sizeof(s_staconf));
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE)); //实验，测试解决wifi中断问题
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_get_config(ESP_IF_WIFI_STA, &s_staconf));
    if (s_staconf.sta.ssid[0] != '\0')
    {
        printf("wifi_init_sta finished.");
        printf("connect to ap SSID:%s password:%s\r\n",
               s_staconf.sta.ssid, s_staconf.sta.password);

        bzero(wifi_data.wifi_ssid, sizeof(wifi_data.wifi_ssid));
        strcpy(wifi_data.wifi_ssid, (char *)s_staconf.sta.ssid);

        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &s_staconf));
        ESP_ERROR_CHECK(esp_wifi_start());
        esp_wifi_connect();
        //Led_Status = LED_STA_TOUCH;
    }
    else
    {
        // printf("Waiting for SetupWifi ....\r\n");
        // wifi_init_softap();
        // my_tcp_connect();
    }
}

/*
* WIFI作为AP的初始化
* @param[in]   void  		       :无
* @retval      void                :无
* @note        修改日志 
*               Ver0.0.1:
                    hx-zsj, 2018/08/06, 初始化版本\n 
*/

//TaskHandle_t my_tcp_connect_Handle = NULL;
void wifi_init_softap(void)
{
    tcp_event_group = xEventGroupCreate();
    start_AP = 1;
    Led_Status = LED_STA_AP;
    ESP_ERROR_CHECK(esp_wifi_stop());
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = SOFT_AP_SSID, // SOFT_AP_SSID,
            .password = SOFT_AP_PAS,
            .ssid_len = 0,
            .max_connection = SOFT_AP_MAX_CONNECT,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK}};
    if (strlen(SOFT_AP_PAS) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "SoftAP set finish,SSID:%s password:%s \n",
             wifi_config.ap.ssid, wifi_config.ap.password);

    //my_tcp_connect_Handle = NULL;
    xTaskCreate(&my_tcp_connect_task, "my_tcp_connect_task", 4096, NULL, 5, &my_tcp_connect_Handle);
}

/*
* WIFI作为AP+STA的初始化
* @param[in]   void  		       :无
* @retval      void                :无
* @note        修改日志 
*               Ver0.0.1:

*/
void wifi_init_apsta(void)
{
    wifi_event_group = xEventGroupCreate();
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t sta_wifi_config;
    ESP_ERROR_CHECK(esp_wifi_get_config(ESP_IF_WIFI_STA, &sta_wifi_config));

    wifi_config_t ap_wifi_config =
        {
            .ap = {
                .ssid = "esp32_ap",
                .password = "",
                //     .authmode = WIFI_AUTH_WPA_WPA2_PSK,
                .max_connection = 1,
            },
        };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void stop_user_wifi(void)
{
    if (wifi_work_sta == turn_on)
    {
        ESP_ERROR_CHECK(esp_wifi_stop());
        wifi_work_sta = turn_off;
        printf("turn off WIFI! \n");
    }
}

void start_user_wifi(void)
{
    if (wifi_work_sta == turn_off)
    {
        ESP_ERROR_CHECK(esp_wifi_start());
        esp_wifi_connect();

        wifi_work_sta = turn_on;
        printf("turn on WIFI! \n");
    }
}