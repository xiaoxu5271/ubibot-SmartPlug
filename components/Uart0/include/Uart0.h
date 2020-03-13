

#ifndef _USERUART0_H_
#define _USERUART0_H_

#include "freertos/FreeRTOS.h"

#define UART2_SW 26

#define uart2_485 0
#define uart2_cse 1
#define uart2_co2 2

extern SemaphoreHandle_t xMutex_uart2_sw;

void Uart_Init(void);
void Uart0_read(void);
void sw_uart2(uint8_t uart2_mode);

#endif
