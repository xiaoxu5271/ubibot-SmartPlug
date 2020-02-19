/*
LED驱动程序
创建日期：2018年10月29日
作者：孙浩

*/
#ifndef _LED_H_
#define _LED_H_

#include "freertos/FreeRTOS.h"
TaskHandle_t Led_Task_Handle;

extern void Led_Init(void);

uint8_t Led_Status;

enum led_sta
{
    LED_STA_INIT = 0x00, //初始化
    LED_STA_AP,          //配网
    LED_STA_WIFIERR,     //网络错误
    LED_STA_NOSER,       //无序列号
    LED_STA_WORK,        //正常工作
    LED_STA_SEND,        //发送数据
    LED_STA_ACTIVE_ERR,  //激活失败
    LED_STA_HEARD_ERR,   //硬件错误
    LED_STA_OTA          //OTA ING
};

void Led_R_On(void);
void Led_G_On(void);
void Led_B_On(void);
void Led_Y_On(void);
void Led_C_On(void);
void Led_Off(void);
void Led_B_Off(void);

void Turn_ON_LED(void);
void Turn_Off_LED(void);

#endif
