#ifndef __MQ_TT
#define __MQ_TT

#include "mqtt_client.h"

void initialise_mqtt(void);
void Start_W_Mqtt(void);
void Stop_W_Mqtt(void);
void Send_Mqtt_Buff(char *buff);
// extern esp_mqtt_client_handle_t client;
extern char topic_p[100];

QueueHandle_t Send_Mqtt_Queue;
#define MQTT_BUFF_LEN 521
typedef struct
{
    char buff[MQTT_BUFF_LEN]; //
    uint16_t buff_len;        //
} Mqtt_Msg;

#endif