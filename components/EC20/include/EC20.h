

#ifndef _EC20_H_
#define _EC20_H_

#include "freertos/FreeRTOS.h"

// SemaphoreHandle_t EC20_muxtex;

extern bool EC20_NET_STA;

void EC20_Start(void);
char *AT_Cmd_Send(char *cmd_buf, char *check_buff, uint32_t time_out, uint8_t try_num);
uint8_t EC20_Net_Check(void);
uint8_t EC20_Init(void);
uint8_t EC20_Http_CFG(void);
uint8_t EC20_Active(char *active_url, char *recv_buf);
uint8_t EC20_Send_Post_Data(char *post_buf, bool end_flag);
uint8_t EC20_Read_Post_Data(char *recv_buff, uint16_t buff_size);
uint8_t EC20_Post_Data(void);
uint8_t EC20_MQTT(void);
uint8_t EC20_MQTT_PUB(void);

#endif
