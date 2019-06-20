#ifndef __S_C
#define __S_C

#include "freertos/event_groups.h"

#define WIFI_CONNET     0X01
#define WIFI_DISCONNET  0X02

void smartconfig_example_task(void *parm);
void initialise_wifi(char *wifi_ssid, char *wifi_password);
void init_wifi(void);
void reconnect_wifi_usr(void);
static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;

EventGroupHandle_t wifi_event_group;

uint8_t Wifi_Status;
uint8_t Wifi_ErrCode;            //7  密码错误
                                //8  未找到指定wifi



#endif