#ifndef __SERVER_TIME
#define __SERVER_TIME
#include <stdio.h>
#include "esp_err.h"
char * Server_Timer_SEND(void);
esp_err_t Server_Timer_GET(char *Server_timer_data);
#endif