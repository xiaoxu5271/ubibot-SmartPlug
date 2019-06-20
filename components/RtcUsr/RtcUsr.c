#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "RtcUsr.h"

static const char *TAG = "RtcUsr";


void Rtc_Set(int year,int mon,int day,int hour,int min,int sec)
{
    struct tm tm;
    tm.tm_year = year - 1900;
    tm.tm_mon = mon-1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = min;
    tm.tm_sec = sec;
    time_t t = mktime(&tm);
    ESP_LOGI(TAG, "Setting time: %s", asctime(&tm));
    struct timeval now = { .tv_sec = t };
    settimeofday(&now, NULL);
}

void Rtc_Read(int* year,int* month,int* day,int* hour,int* min,int* sec)
{    
    time_t timep;
    struct tm *p;
    time (&timep);
    p=gmtime(&timep);
    *year=(1900+p->tm_year);
    *month=(1+p->tm_mon);
    *day=p->tm_mday;
    *hour=p->tm_hour;
    *min=p->tm_min;
    *sec=p->tm_sec;
    //ESP_LOGI(TAG, "Read:%d-%d-%d %d:%d:%d No.%d",(1900+p->tm_year),(1+p->tm_mon),p->tm_mday,p->tm_hour,p->tm_min,p->tm_sec,(1+p->tm_yday));
    
    
    /*printf("%d\n",p->tm_sec); //获取当前秒
    printf("%d\n",p->tm_min); //获取当前分
    printf("%d\n",p->tm_hour);//获取当前时,这里获取西方的时间,刚好相差八个小时
    printf("%d\n",p->tm_mday);//获取当前月份日数,范围是1-31
    printf("%d\n",1+p->tm_mon);//获取当前月份,范围是0-11,所以要加1
    printf("%d\n",1900+p->tm_year);//获取当前年份,从1900开始，所以要加1900
    printf("%d\n",p->tm_yday); //从今年1月1日算起至今的天数，范围为0-365
    */

}