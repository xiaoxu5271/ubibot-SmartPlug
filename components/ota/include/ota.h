#ifndef _OTA_H_
#define _OTA_H_

#include "freertos/FreeRTOS.h"

uint8_t need_update;
uint8_t update_fail_num;
void ota_start(void);
void ota_back(void);

#endif
