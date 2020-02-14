#ifndef __MQ_TT
#define __MQ_TT

#include "mqtt_client.h"

void initialise_mqtt(void);
extern esp_mqtt_client_handle_t client;
extern char topic_p[100];

#endif