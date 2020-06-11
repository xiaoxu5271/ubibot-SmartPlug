#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "Smartconfig.h"

#include "ServerTimer.h"

static const char *TAG = "SeverTime";

time_t timer_timstamp;
time_t base_timstamp;
time_t now_timstamp;
struct tm *now_time;
esp_err_t Server_Timer_GET(char *Server_timer_data)
{
    struct tm *tmp_time = (struct tm *)malloc(sizeof(struct tm));
    strptime(Server_timer_data, "%Y-%m-%dT%H:%M:%SZ", tmp_time);
    base_timstamp = mktime(tmp_time);
    ESP_LOGI(TAG, "this is time tamp%ld\r\n", base_timstamp);

    //time_t t = mktime(tmp_time);
    ESP_LOGI(TAG, "Setting time: %s", asctime(tmp_time));
    struct timeval now = {.tv_sec = (base_timstamp + 28800)};
    settimeofday(&now, NULL);
    //校准标志
    xEventGroupSetBits(Net_sta_group, TIME_CAL_BIT);
    free(tmp_time);
    return base_timstamp;
}
void Server_Timer_SEND(char *time_buff)
{
    time(&now_timstamp);
    now_timstamp -= 28800;
    now_time = gmtime(&now_timstamp);
    strftime(time_buff, 24, "%Y-%m-%dT%H:%M:%SZ", now_time);
}
