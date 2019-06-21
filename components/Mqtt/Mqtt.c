#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"

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

static const char *TAG = "MQTT";

extern const int CONNECTED_BIT;

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    char topic[100];
    sprintf(topic, "%s%s%s%s%s%c", "/product/", ProductId, "/channel/", ChannelId, "/control", '\0');
    //printf("!!!!!!!!!!!!!!!!!topic=%s\n",topic);
    switch (event->event_id)
    {

    case MQTT_EVENT_BEFORE_CONNECT:
        break;

    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, topic, 0);
        //msg_id = esp_mqtt_client_subscribe(client, "/product/undefined/channel/225/control", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        //msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
        //ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        //msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        //ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        //ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        //msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        //ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        //printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        //printf("DATA=%.*s\r\n", event->data_len, event->data);
        //printf("strchr --> %s", strchr(event->data, "{"));
        //parse_objects_mqtt(strchr(event->data, "{"));
        parse_objects_mqtt(event->data); //收到平台MQTT数据并解析
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    }
    return ESP_OK;
}

void initialise_mqtt(void)
{
    char mqtt_pwd[41];
    char mqtt_usr[17];

    sprintf(mqtt_pwd, "%s%s%c", "api_key=", ApiKey, '\0');
    sprintf(mqtt_usr, "%s%s%c", "c_id=", ChannelId, '\0');

    const esp_mqtt_client_config_t mqtt_cfg = {
        .uri = "mqtt://api.ubibot.cn",
        .event_handle = mqtt_event_handler,
        //.username = "c_id=225",
        .username = mqtt_usr,
        //.password = "api_key=00000000000000000000000000000000"
        .password = mqtt_pwd,
        .client_id = BleName};

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);
}
