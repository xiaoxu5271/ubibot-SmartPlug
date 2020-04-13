#ifndef __S_C
#define __S_C

#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "Bluetooth.h"
#include "esp_wifi.h"

void init_wifi(void);
void start_user_wifi(void);
void stop_user_wifi(void);
void Net_Switch(void);

extern uint8_t wifi_connect_sta; //wifi连接状态
// extern uint8_t wifi_work_sta;    //wifi开启状态
extern uint8_t start_AP;
extern uint8_t Net_ErrCode; //
extern bool WIFI_STA;
extern uint8_t bl_flag; //蓝牙配网模式

#define CONNECTED_BIT (1 << 0)
#define ACTIVED_BIT (1 << 1)
#define MQTT_W_BIT (1 << 2)
#define MQTT_E_BIT (1 << 3)

// static const int CONNECTED_BIT = BIT0;
static const int AP_STACONNECTED_BIT = BIT0;
extern TaskHandle_t my_tcp_connect_Handle;
extern EventGroupHandle_t Net_sta_group;
extern EventGroupHandle_t tcp_event_group;

#define WIFISTATUS_CONNET 0X01
#define WIFISTATUS_DISCONNET 0X00

//server
//AP热点模式的配置信息
#define SOFT_AP_SSID "CloudForce-SP1" //账号
#define SOFT_AP_PAS ""                //密码，可以为空
#define SOFT_AP_MAX_CONNECT 2         //最多的连接点

#endif