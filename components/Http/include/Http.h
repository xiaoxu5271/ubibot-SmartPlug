#ifndef __H_P
#define __H_P

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "cJSON.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "Smartconfig.h"
#include "Json_parse.h"

#define FIRMWARE  "SP1-V0.0.0"



void initialise_http(void);

void http_send_mes();
int http_activate(void);




#endif