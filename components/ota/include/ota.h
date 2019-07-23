#ifndef _OTA_H_
#define _OTA_H_

#include "freertos/FreeRTOS.h"

uint8_t need_update;
uint8_t update_fail_num;
extern void ota_start(void);

#endif
