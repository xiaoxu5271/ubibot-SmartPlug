#ifndef __H_P
#define __H_P

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#define FIRMWARE "SP1-V0.1.5"

#define POST_NORMAL 0X00
#define POST_HEIGHT_ADD 0X01

#define POST_HEIGHT_SUB 0X02
#define POST_ANGLE_ADD 0X03
#define POST_ANGLE_SUB 0X04
#define POST_ALLDOWN 0X05
#define POST_ALLUP 0X06
#define POST_TARGET 0X07    //HTTP直接上传目标值，用于手动切自动的状态上传
#define POST_NOCOMMAND 0X08 //HTTP只上传平台，无commnd id

#define HTTP_STA_SERIAL_NUMBER 0x00
#define HTTP_KEY_GET 0x01
#define HTTP_KEY_NOT_GET 0x02

#define NOHUMAN 0x00
#define HAVEHUMAN 0x01

#define WEB_PORT 80

//需要发送的二值信号量
extern TaskHandle_t Binary_Heart_Send;
extern TaskHandle_t Binary_dp;
extern TaskHandle_t Binary_485_th;
extern TaskHandle_t Binary_485_sth;
extern TaskHandle_t Binary_ext;
extern TaskHandle_t Binary_energy;
extern TaskHandle_t Binary_ele_quan;
extern TaskHandle_t Binary_mqtt;

void initialise_http(void);

void http_send_mes(void);
int32_t http_activate(void);
int32_t http_post_init(uint32_t Content_Length);
int8_t http_post_read(int32_t s, char *recv_buff, uint16_t buff_size);

char SerialNum[16];
char ProductId[32];
char BleName[64];
char ApiKey[33];
char ChannelId[16];
char USER_ID[48];
char WEB_SERVER[20];

extern uint8_t post_status;
uint8_t human_status;
TaskHandle_t httpHandle;
esp_timer_handle_t http_timer_suspend_p;
extern uint8_t Last_Led_Status;

#endif