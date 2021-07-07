#ifndef __H_P
#define __H_P

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#define FIRMWARE "SP1-V0.1.80"

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

#define HTTP_RECV_BUFF_LEN 2048

typedef enum
{
    NET_OK = 0,
    NET_DIS,
    NET_410,
    NET_400,
    NET_402,
} Net_Err;

SemaphoreHandle_t xMutex_Http_Send;

//需要发送的二值信号量
extern TaskHandle_t Binary_Heart_Send;
extern TaskHandle_t Binary_dp;
extern TaskHandle_t Binary_485_t;
extern TaskHandle_t Binary_485_th;
extern TaskHandle_t Binary_485_sth;
extern TaskHandle_t Binary_485_ws;
extern TaskHandle_t Binary_485_lt;
extern TaskHandle_t Binary_485_co2;
extern TaskHandle_t Binary_485_IS;
extern TaskHandle_t Binary_ext;
extern TaskHandle_t Binary_energy;
extern TaskHandle_t Binary_ele_quan;
extern TaskHandle_t Active_Task_Handle;
extern TaskHandle_t Sw_on_Task_Handle;
esp_timer_handle_t http_timer_suspend_p;

extern uint8_t post_status;
extern uint8_t Last_Led_Status;

void initialise_http(void);
void http_send_mes(void);
void Start_Active(void);
int32_t http_post_init(uint32_t Content_Length);
int8_t http_send_post(int32_t s, char *post_buf, bool end_flag);
bool http_post_read(int32_t s, char *recv_buff, uint16_t buff_size);

#endif