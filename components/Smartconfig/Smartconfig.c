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
#include "esp_wifi_types.h"
#include "nvs_flash.h"
#include "tcpip_adapter.h"
#include "esp_smartconfig.h"
/*  user include */
#include "Smartconfig.h"
#include "Led.h"

wifi_config_t s_staconf;

enum wifi_connect_sta
{
    connect_Y = 1,
    connect_N = 2,
};
uint8_t wifi_con_sta = 0;
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    uint8_t reason = event->event_info.disconnected.reason;
    switch (event->event_id)
    {

    case SYSTEM_EVENT_STA_STOP:

        break;

    case SYSTEM_EVENT_STA_START:
        //esp_wifi_connect();
        break;

    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        Led_Status = LED_STA_INIT;
        Wifi_Status = WIFI_CONNET;
        ///ESP_LOGE("smart", "SYSTEM_EVENT_STA_GOT_IP");
        break;

    case SYSTEM_EVENT_STA_DISCONNECTED:
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        Led_Status = LED_STA_WIFIERR;
        Wifi_Status = WIFI_DISCONNET;
        Wifi_ErrCode = reason;
        //ESP_LOGE("smart", "wifi_err_reason_t=%d",reason);

        //reason=7  密码错误
        //reason=8  未找到指定wifi
        break;

    default:
        ESP_LOGI("smart", "event_id=%d", event->event_id);
        break;
    }
    return ESP_OK;
}

void initialise_wifi(char *wifi_ssid, char *wifi_password)
{
    printf("WIFI Reconnect,SSID=%s,PWD=%s\r\n", wifi_ssid, wifi_password);

    ESP_ERROR_CHECK(esp_wifi_get_config(ESP_IF_WIFI_STA, &s_staconf));
    if (s_staconf.sta.ssid[0] == '\0')
    {
        //ESP_ERROR_CHECK(esp_wifi_get_config(ESP_IF_WIFI_STA, &s_staconf));
        strcpy((char *)s_staconf.sta.ssid, wifi_ssid);
        strcpy((char *)s_staconf.sta.password, wifi_password);

        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &s_staconf));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_ERROR_CHECK(esp_wifi_connect());
    }
    else
    {
        ESP_ERROR_CHECK(esp_wifi_stop());
        memset(&s_staconf.sta, 0, sizeof(s_staconf));
        //printf("WIFI CHANGE\r\n");
        strcpy((char *)s_staconf.sta.ssid, wifi_ssid);
        strcpy((char *)s_staconf.sta.password, wifi_password);
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &s_staconf));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_ERROR_CHECK(esp_wifi_connect());
    }
    /*else if (strcmp(wifi_ssid, s_staconf.sta.ssid) == 0 && strcmp(wifi_password, s_staconf.sta.password) == 0)
    {

        if (wifi_con_sta == connect_Y)
        {
            printf("ALREADY CONNECT \r\n");
        }
        else
        {
            printf("WIFI NO CHANGE\r\n");
            ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &s_staconf));
            ESP_ERROR_CHECK(esp_wifi_start());
            esp_wifi_connect();
        }
    }
    else if (strcmp(wifi_ssid, s_staconf.sta.ssid) != 0 || strcmp(wifi_password, s_staconf.sta.password) != 0)
    {
        ESP_ERROR_CHECK(esp_wifi_stop());
        memset(&s_staconf.sta, 0, sizeof(s_staconf));
        printf("WIFI CHANGE\r\n");
        strcpy(s_staconf.sta.ssid, wifi_ssid);
        strcpy(s_staconf.sta.password, wifi_password);
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &s_staconf));
        ESP_ERROR_CHECK(esp_wifi_start());
        esp_wifi_connect();
    }*/
}

void reconnect_wifi_usr(void)
{
    printf("WIFI Reconnect\r\n");
    ESP_ERROR_CHECK(esp_wifi_get_config(ESP_IF_WIFI_STA, &s_staconf));

    ESP_ERROR_CHECK(esp_wifi_stop());
    memset(&s_staconf.sta, 0, sizeof(s_staconf));

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &s_staconf));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_connect();
}

void init_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    memset(&s_staconf.sta, 0, sizeof(s_staconf));
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_get_config(ESP_IF_WIFI_STA, &s_staconf));
    if (s_staconf.sta.ssid[0] != '\0')
    {
        printf("wifi_init_sta finished.");
        printf("connect to ap SSID:%s password:%s\r\n",
               s_staconf.sta.ssid, s_staconf.sta.password);

        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &s_staconf));

        ESP_ERROR_CHECK(esp_wifi_start());
        esp_wifi_connect();
    }
    else
    {
        printf("11Waiting for ble connect info ....\r\n");

        /*wifi_config_t wifi_config = {
        .sta = {
            .ssid = "test32",
            .password = "work12345678",
          },
        };
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        esp_wifi_connect();*/
    }
}