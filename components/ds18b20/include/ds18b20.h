
#ifndef _DS18B20_H_
#define _DS18B20_H_

#include "freertos/FreeRTOS.h"

int8_t ds18b20_get_temp(void);
void start_ds18b20(void);

extern float DS18B20_TEM;

#endif
