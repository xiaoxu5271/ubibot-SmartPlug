#ifndef __MQ_TT
#define __MQ_TT

#include "mqtt_client.h"

void initialise_mqtt(void);
uint8_t Send_Mqtt(char *data_buff, uint16_t data_len);
// extern esp_mqtt_client_handle_t client;
extern char topic_p[100];

#endif