/*
风速传感器读取程序

创建日期：2018年10月29日
作者：孙浩

Wind_Init(void);
初始化函数，主要为UART初始化和GPIO初始化
本例使用UART2，9600，8N1
UART2_TXD = (GPIO_NUM_17)
UART2_RXD = (GPIO_NUM_16)
RS485RD   =  21

float Wind_Read(void);
风速读取函数，返回风速数值，单位M/S，float类型，小数点后1位

*/
#ifndef _WIND_H_
#define _WIND_H_

extern void Wind_Init(void);
extern float Wind_Read(void);

#endif

