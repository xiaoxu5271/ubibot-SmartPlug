

#ifndef _EC20_H_
#define _EC20_H_

#include "freertos/FreeRTOS.h"

extern bool EC20_NET_STA;

void EC20_Start(void);
char *AT_Cmd_Send(char *cmd_buf, char *check_buff, uint16_t time_out, uint8_t try_num);

uint8_t EC20_Init(void);
uint8_t EC20_Http_CFG(void);
uint8_t EC20_Active(void);
uint8_t EC20_Post_Data(void);
uint8_t EC20_MQTT(void);
uint8_t EC20_MQTT_PUB(void);

#endif
