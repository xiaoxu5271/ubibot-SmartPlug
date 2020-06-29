

#ifndef _EC20_H_
#define _EC20_H_

#include "freertos/FreeRTOS.h"

SemaphoreHandle_t EC20_muxtex;

enum recv_mode
{
    EC_NORMAL = 0,
    EC_OTA,
    EC_INPORT
};

enum ec_err_code
{
    NO_ARK = 401,
    CPIN_ERR,
    QICSGP_ERR,
    CGATT_ERR,
    QIACT_ERR,
    NO_NET
};

extern TaskHandle_t EC20_Task_Handle;
extern TaskHandle_t Uart1_Task_Handle;

void EC20_Init(void);

#endif
