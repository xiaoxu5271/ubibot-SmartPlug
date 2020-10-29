#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
// #include "nvs_flash.h"
#include "esp_event.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "Json_parse.h"
#include "Smartconfig.h"
#include "Bluetooth.h"
#include "Led.h"
#include "Http.h"
#include "EC20.h"

#include "Mqtt.h"

static const char *TAG = "MQTT";
QueueHandle_t Send_Mqtt_Queue;
void Send_Mqtt_Task(void *arg);

esp_mqtt_client_handle_t client = NULL;
bool MQTT_W_STA = false;
bool MQTT_E_STA = false;
// TaskHandle_t Binary_mqtt = NULL;

char topic_s[100] = {0};
char topic_p[100] = {0};

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        MQTT_W_STA = true;
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, topic_s, 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        break;
    case MQTT_EVENT_DISCONNECTED:
        MQTT_W_STA = false;
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        // printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        // printf("DATA=%.*s\r\n", event->data_len, event->data);
        parse_objects_mqtt(event->data, true); //收到平台MQTT数据并解析
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

void initialise_mqtt(void)
{
    xTaskCreate(Send_Mqtt_Task, "Send Mqtt", 4096, NULL, 10, NULL);
}

void Start_W_Mqtt(void)
{
    // if ((xEventGroupGetBits(Net_sta_group) & MQTT_W_S_BIT) != MQTT_W_S_BIT)
    if ((xEventGroupGetBits(Net_sta_group) & (MQTT_W_S_BIT | MQTT_INITED_BIT)) == MQTT_INITED_BIT) //设备激活过，但MQTT未启动
    {
        esp_mqtt_client_start(client);
        xEventGroupSetBits(Net_sta_group, MQTT_W_S_BIT);
    }
}
void Stop_W_Mqtt(void)
{
    if ((xEventGroupGetBits(Net_sta_group) & MQTT_W_S_BIT) == MQTT_W_S_BIT)
    {
        esp_mqtt_client_stop(client);
        MQTT_W_STA = false;
        xEventGroupClearBits(Net_sta_group, MQTT_W_S_BIT);
    }
}

#define MQTT_STATUS_BUFF_LEN 150
void Send_Mqtt_Task(void *arg)
{
    Mqtt_Msg Mqtt_Send;

    xEventGroupWaitBits(Net_sta_group, ACTIVED_BIT, false, true, -1); //等待激活
    char mqtt_pwd[42];
    char mqtt_usr[23];
    char mqtt_uri[64];

    sprintf(topic_s, "%s%s%s%s%s%c", "/product/", ProductId, "/channel/", ChannelId, "/control", '\0');
    sprintf(topic_p, "%s%s%s%s%s%c", "/product/", ProductId, "/channel/", ChannelId, "/status", '\0');

    sprintf(mqtt_pwd, "%s%s%c", "api_key=", ApiKey, '\0');
    sprintf(mqtt_usr, "%s%s%c", "c_id=", ChannelId, '\0');
    sprintf(mqtt_uri, "mqtt://%s", MQTT_SERVER);
    const esp_mqtt_client_config_t mqtt_cfg = {
        .uri = mqtt_uri,
        .username = mqtt_usr,
        .password = mqtt_pwd,
        .port = (uint32_t)strtoul(MQTT_PORT, 0, 10),
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    xEventGroupSetBits(Net_sta_group, MQTT_INITED_BIT);

    if (net_mode == NET_WIFI)
    {
        Start_W_Mqtt();
    }

    while (1)
    {
        xQueueReceive(Send_Mqtt_Queue, &Mqtt_Send, -1);
        if (MQTT_W_STA == true)
        {
            uint8_t *status_buff = (uint8_t *)malloc(MQTT_STATUS_BUFF_LEN);
            char *mqtt_buff = (char *)malloc(Mqtt_Send.buff_len + MQTT_STATUS_BUFF_LEN + 10);
            memset(status_buff, 0, MQTT_STATUS_BUFF_LEN);
            memset(mqtt_buff, 0, Mqtt_Send.buff_len + MQTT_STATUS_BUFF_LEN + 10);
            Create_Status_Json((char *)status_buff, false); //
            snprintf(mqtt_buff, Mqtt_Send.buff_len + MQTT_STATUS_BUFF_LEN + 10, "{\"feeds\":[%s%s\r\n", Mqtt_Send.buff, status_buff);
            esp_mqtt_client_publish(client, topic_p, mqtt_buff, 0, 1, 0);
            // ESP_LOGI(TAG, "MQTT:%s", mqtt_buff);

            free(status_buff);
            free(mqtt_buff);
            // return 1;
        }
        else if (MQTT_E_STA == true)
        {
            xSemaphoreTake(xMutex_Http_Send, -1);
            uint8_t *status_buff = (uint8_t *)malloc(MQTT_STATUS_BUFF_LEN);
            char *mqtt_buff = (char *)malloc(Mqtt_Send.buff_len + MQTT_STATUS_BUFF_LEN + 10);
            memset(status_buff, 0, MQTT_STATUS_BUFF_LEN);
            memset(mqtt_buff, 0, Mqtt_Send.buff_len + MQTT_STATUS_BUFF_LEN + 10);
            Create_Status_Json((char *)status_buff, false); //
            snprintf(mqtt_buff, Mqtt_Send.buff_len + MQTT_STATUS_BUFF_LEN + 10, "{\"feeds\":[%s%s\r\n", Mqtt_Send.buff, status_buff);
            EC20_MQTT_PUB(mqtt_buff);
            xSemaphoreGive(xMutex_Http_Send);

            free(status_buff);
            free(mqtt_buff);
        }
    }
}
