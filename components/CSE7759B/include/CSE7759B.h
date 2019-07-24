
#include "freertos/FreeRTOS.h"
#ifndef _7759_H_
#define _7759_H_

extern int8_t CSE7759B_Read(void);

void STOP_7759B_READ(void);
void START_7759B_READ(void);
void CSE7759B_Init(void);

#endif
