#ifndef __SERVER_TIME
#define __SERVER_TIME
#include <stdio.h>
#include "esp_err.h"
void Server_Timer_SEND(char *time_buff);
esp_err_t Server_Timer_GET(char *Server_timer_data);
#endif