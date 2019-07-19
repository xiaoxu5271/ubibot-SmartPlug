
#ifndef _DS18B20_H_
#define _DS18B20_H_

#include "freertos/FreeRTOS.h"

void ds18b20_get_temp(float *temp_value1);
void start_ds18b20(void);

#endif
