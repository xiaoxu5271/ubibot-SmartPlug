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
#include "freertos/queue.h"
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

static const char *TAG = "MQTT";

extern const int CONNECTED_BIT;

esp_mqtt_client_handle_t client = NULL;
TaskHandle_t Binary_mqtt = NULL;

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
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

        msg_id = esp_mqtt_client_subscribe(client, topic_s, 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        // msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
        // ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

        // msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        // ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        // msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
        // ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        // msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        // ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
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
        parse_objects_mqtt(event->data); //收到平台MQTT数据并解析
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
    char mqtt_pwd[42];
    char mqtt_usr[23];

    sprintf(topic_s, "%s%s%s%s%s%c", "/product/", ProductId, "/channel/", ChannelId, "/control", '\0');
    sprintf(topic_p, "%s%s%s%s%s%c", "/product/", ProductId, "/channel/", ChannelId, "/status", '\0');

    sprintf(mqtt_pwd, "%s%s%c", "api_key=", ApiKey, '\0');
    sprintf(mqtt_usr, "%s%s%c", "c_id=", ChannelId, '\0');

    const esp_mqtt_client_config_t mqtt_cfg = {
        .uri = "mqtt://mqtt.ubibot.cn",
        .username = mqtt_usr,
        .password = mqtt_pwd,

    };

    // xTaskCreate(mqtt_send_task, "mqtt_send_task", 4096, NULL, 7, &Binary_mqtt);
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}

uint8_t Send_Mqtt(char *data_buff, uint16_t data_len)
{
    uint8_t *status_buff = NULL; //],"status":"mac=x","ssid_base64":"x"}
    // uint16_t status_buff_len;
    char *mqtt_buff = NULL;

    if ((status_buff = (uint8_t *)malloc(350)) == NULL)
    {
        ESP_LOGE(TAG, "status_buff malloc fail! ");
        return 0;
    }
    memset(status_buff, 0, 350);
    Create_Status_Json((char *)status_buff); //

    if ((mqtt_buff = (char *)malloc(data_len + 350 + 10)) == NULL)
    {
        ESP_LOGE(TAG, "status_buff malloc fail! ");
        return 0;
    }
    memset(mqtt_buff, 0, data_len + 350 + 10);
    snprintf(mqtt_buff, data_len + 350 + 10, "{\"feeds\":[%s%s", data_buff, status_buff);
    // printf("mqtt buff:\n%s\n", mqtt_buff);
    if (client != NULL)
    {
        esp_mqtt_client_publish(client, topic_p, mqtt_buff, 0, 1, 0);
    }
    free(status_buff);
    free(mqtt_buff);
    return 1;
}
