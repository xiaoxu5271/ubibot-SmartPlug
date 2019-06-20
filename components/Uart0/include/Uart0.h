/*
串口0接收程序

创建日期：2018年112月08日
作者：孙浩

Wallkey_Init(void);
初始化函数，主要为UART初始化和GPIO初始化
本例使用UART1，9600，8N1
UART1_TXD = (GPIO_NUM_5)
UART1_RXD = (GPIO_NUM_4)

void Wallkey_App(uint8_t* Key_Id,int8_t Switch);
WallKey应用函数，给入Key_Id(蓝牙发送过来的)和方向左0右1
在task中使用

*/

#ifndef _USERUART0_H_
#define _USERUART0_H_



extern void Uart0_Init(void);
extern void Uart0_read(void);


#endif

