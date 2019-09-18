

#ifndef _SOFT_UART_H_
#define _SOFT_UART_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

xQueueHandle Soft_uart_evt_queue;

void soft_uart_init(void);
void en_uart_recv(void);
void dis_uart_recv(void);

#endif
