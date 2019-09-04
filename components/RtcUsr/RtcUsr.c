#include <stdio.h>
#include "string.h"
#include "stdlib.h"

#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "RtcUsr.h"
#include "driver/i2c.h"
#include "E2prom.h"

static const char *TAG = "RtcUsr";

void Rtc_Set(int year, int mon, int day, int hour, int min, int sec)
{
    struct tm tm;
    tm.tm_year = year - 1900;
    tm.tm_mon = mon - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = min;
    tm.tm_sec = sec;
    time_t t = mktime(&tm);
    ESP_LOGI(TAG, "Setting time: %s", asctime(&tm));
    struct timeval now = {.tv_sec = t};
    settimeofday(&now, NULL);
}

void Rtc_Read(int *year, int *month, int *day, int *hour, int *min, int *sec)
{
    time_t timep;
    struct tm *p;
    time(&timep);
    p = gmtime(&timep);
    *year = (1900 + p->tm_year);
    *month = (1 + p->tm_mon);
    *day = p->tm_mday;
    *hour = p->tm_hour;
    *min = p->tm_min;
    *sec = p->tm_sec;
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

/*******************************************************************************
  * @file       PCF8563 Driver  
  * @author 
  * @version
  * @date 
  * @brief
  ******************************************************************************
  * @attention
  *
  *
*******************************************************************************/

/*******************************************************************************
//PCF_Format_BCD to PCF_Format_BIN
*******************************************************************************/
static uint8_t Bin_To_Bcd(uint8_t bin_val)
{
    return 16 * (bin_val / 10) + bin_val % 10;
}

/*******************************************************************************
//PCF_Format_BIN to PCF_Format_BCD
*******************************************************************************/
static uint8_t Bcd_To_Bin(uint8_t bcd_val)
{
    return 10 * (bcd_val / 16) + bcd_val % 16;
}

///*******************************************************************************
////PCF8563 update time
//*******************************************************************************/
//void Update_UTCtime(char *time)
//{
//  uint16_t year;
//  char *InpString;
//  uint8_t write_time[7];
//  uint8_t mon,day,hour,min,sec;
//
//  InpString = strtok(time, "-");
//  year = (uint16_t)strtoul(InpString, 0, 10)-2000;  //year
//
//  InpString = strtok(NULL, "-");
//  mon=(uint8_t)strtoul(InpString, 0, 10);  //mon
//
//  InpString = strtok(NULL, "T");
//  day=(uint8_t)strtoul(InpString, 0, 10);  //day
//
//  InpString = strtok(NULL, ":");
//  hour=(uint8_t)strtoul(InpString, 0, 10);  //hour
//
//  InpString = strtok(NULL, ":");
//  min=(uint8_t)strtoul(InpString, 0, 10);  //min
//
//  InpString = strtok(NULL, "Z");
//  sec=(uint8_t)strtoul(InpString, 0, 10);  //sec
//
//
//  write_time[0]=Bin_To_Bcd(sec);  //second
//
//  write_time[1]=Bin_To_Bcd(min);  //minute
//
//  write_time[2]=Bin_To_Bcd(hour);  //hour
//
//  write_time[3]=Bin_To_Bcd(day);  //date
//
//  write_time[5]=Bin_To_Bcd(mon);  //month
//
//  write_time[6]=Bin_To_Bcd(year);  //year
//
//  MulTry_I2C_WR_mulReg(PCF8563_ADDR,VL_seconds,write_time,sizeof(write_time));  //Write Timer Register
//}

/*******************************************************************************
//PCF8563 read UTC time
*******************************************************************************/
void Read_UTCtime(char *buffer, uint8_t buf_size)
{
    struct tm ts;
    uint8_t read_time[7];

    MulTry_I2C_RD_mulReg(PCF8563_ADDR, VL_seconds, read_time, sizeof(read_time)); //Read Timer Register

    read_time[0] &= 0x7f;
    ts.tm_sec = Bcd_To_Bin(read_time[0]); //second value

    read_time[1] &= 0x7f;
    ts.tm_min = Bcd_To_Bin(read_time[1]); //minite value

    read_time[2] &= 0x3f;
    ts.tm_hour = Bcd_To_Bin(read_time[2]); //hour value

    read_time[3] &= 0x3f;
    ts.tm_mday = Bcd_To_Bin(read_time[3]); //day value

    read_time[5] &= 0x1f;
    ts.tm_mon = Bcd_To_Bin(read_time[5]) - 1; //month value

    ts.tm_year = Bcd_To_Bin(read_time[6]) + 100; //year value

    ts.tm_isdst = 0; //do not use Daylight Saving Time

    strftime(buffer, buf_size, "%Y-%m-%dT%H:%M:%SZ", &ts); //UTC time
}

/*******************************************************************************
//PCF8563 read Unix time
*******************************************************************************/
unsigned long Read_UnixTime(void)
{
    struct tm ts;
    uint8_t read_time[7];

    MulTry_I2C_RD_mulReg(PCF8563_ADDR, VL_seconds, read_time, sizeof(read_time)); //Read Timer Register

    read_time[0] &= 0x7f;
    ts.tm_sec = Bcd_To_Bin(read_time[0]); //second

    read_time[1] &= 0x7f;
    ts.tm_min = Bcd_To_Bin(read_time[1]); //minite

    read_time[2] &= 0x3f;
    ts.tm_hour = Bcd_To_Bin(read_time[2]); //hour

    read_time[3] &= 0x3f;
    ts.tm_mday = Bcd_To_Bin(read_time[3]); //date

    read_time[5] &= 0x1f;
    ts.tm_mon = Bcd_To_Bin(read_time[5]) - 1; //month

    ts.tm_year = Bcd_To_Bin(read_time[6]) + 100; //2000-1900 year

    ts.tm_isdst = 0; //do not use Daylight Saving Time

    return mktime(&ts); //unix time
}

/*******************************************************************************
//PCF8563 init
*******************************************************************************/
void Timer_IC_Init(void)
{
    uint8_t status_val[2] = {0x00, 0x00};

    uint8_t alarm_setval[4] = {0x80, 0x80, 0x80, 0x80}; //MIN/HOUR/DAY/WEEDDAY ALARM DISABLED

    uint8_t timer_set[3] = {0x00, 0x00, 0x00}; //CLKOUT INHIBITED/TIMER DISABLED/TIMER VALUE

    MulTry_I2C_WR_mulReg(PCF8563_ADDR, Control_status_1, status_val, sizeof(status_val)); //Write Timer Register

    MulTry_I2C_WR_mulReg(PCF8563_ADDR, Minute_alarm, alarm_setval, sizeof(alarm_setval)); //Write Timer Register

    MulTry_I2C_WR_mulReg(PCF8563_ADDR, CLKOUT_control, timer_set, sizeof(timer_set)); //Write Timer Register
}

/*******************************************************************************
//PCF8563 Reset Time
*******************************************************************************/
void Timer_IC_Reset_Time(void)
{
    uint8_t time_val[7] = {0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x18};

    MulTry_I2C_WR_mulReg(PCF8563_ADDR, VL_seconds, time_val, sizeof(time_val)); //Write Timer Register
}

/*******************************************************************************
//Return Unix time
*******************************************************************************/
unsigned long Return_UnixTime(uint8_t *t_buf)
{
    struct tm ts;

    ts.tm_sec = t_buf[0]; //second

    ts.tm_min = t_buf[1]; //minite

    ts.tm_hour = t_buf[2]; //hour

    ts.tm_mday = t_buf[3]; //date

    ts.tm_mon = t_buf[5] - 1; //month

    ts.tm_year = t_buf[6] + 100; //2000-1900 year

    ts.tm_isdst = 0; //do not use Daylight Saving Time

    return mktime(&ts); //unix time
}

/*******************************************************************************
//PCF8563 update time
*******************************************************************************/
void Check_Update_UTCtime(char *time)
{
    char *InpString;
    uint8_t i;
    uint8_t reg_buf[7] = {0};
    uint8_t time_buf[7] = {0};
    unsigned long net_time = 0;
    unsigned long sys_time = 0;

    InpString = strtok(time, "-");
    time_buf[6] = (uint8_t)((uint16_t)strtoul(InpString, 0, 10) - 2000); //year

    InpString = strtok(NULL, "-");
    time_buf[5] = (uint8_t)strtoul(InpString, 0, 10); //month

    InpString = strtok(NULL, "T");
    time_buf[3] = (uint8_t)strtoul(InpString, 0, 10); //day

    InpString = strtok(NULL, ":");
    time_buf[2] = (uint8_t)strtoul(InpString, 0, 10); //hour

    InpString = strtok(NULL, ":");
    time_buf[1] = (uint8_t)strtoul(InpString, 0, 10); //min

    InpString = strtok(NULL, "Z");
    time_buf[0] = (uint8_t)strtoul(InpString, 0, 10); //sec

    net_time = Return_UnixTime(time_buf);

    sys_time = Read_UnixTime();

    if ((sys_time > (net_time + UPDATE_TIME_SIZE)) || (sys_time < (net_time - UPDATE_TIME_SIZE)))
    {
        reg_buf[0] = Bin_To_Bcd(time_buf[0]) & 0x7f; //second

        reg_buf[1] = Bin_To_Bcd(time_buf[1]) & 0x7f; //minute

        reg_buf[2] = Bin_To_Bcd(time_buf[2]) & 0x3f; //hour

        reg_buf[3] = Bin_To_Bcd(time_buf[3]) & 0x3f; //date

        reg_buf[5] = Bin_To_Bcd(time_buf[5]) & 0x1f; //month

        reg_buf[6] = Bin_To_Bcd(time_buf[6]); //year

        for (i = 0; i < RETRY_TIME_OUT; i++)
        {
            MulTry_I2C_WR_mulReg(PCF8563_ADDR, VL_seconds, reg_buf, sizeof(reg_buf)); //Write Timer Register

            sys_time = Read_UnixTime();

            if ((sys_time < (net_time + UPDATE_TIME_SIZE)) && (sys_time > (net_time - UPDATE_TIME_SIZE)))
            {
                break;
            }
            else
            {
                vTaskDelay(20 / portTICK_RATE_MS); //delay about 15ms
            }
        }
    }
}

/*******************************************************************************
  write a byte to slave register
*******************************************************************************/
static short IIC_WR_Reg(uint8_t sla_addr, uint8_t reg_addr, uint8_t val)
{
    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create(); //新建操作I2C句柄
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 2 * sla_addr, ACK_CHECK_EN); //启动I2C
    i2c_master_write_byte(cmd, reg_addr, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, val, ACK_CHECK_EN);

    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    // vTaskDelay(20 / portTICK_RATE_MS);

    return ret;
}

/*******************************************************************************
  write a byte to slave register whit multiple try
*******************************************************************************/
void MulTry_IIC_WR_Reg(uint8_t sla_addr, uint8_t reg_addr, uint8_t val)
{
    uint8_t n_try;

    for (n_try = 0; n_try < RETRY_TIME_OUT; n_try++)
    {
        if (IIC_WR_Reg(sla_addr, reg_addr, val) == ESP_OK)
        {
            break;
        }
        vTaskDelay(10 / portTICK_RATE_MS); //delay about 6ms
    }
}

/*******************************************************************************
  Read a byte from slave register
*******************************************************************************/
static short IIC_RD_Reg(uint8_t sla_addr, uint8_t reg_addr, uint8_t *val)
{
    int ret;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create(); //新建操作I2C句柄
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 2 * sla_addr, ACK_CHECK_EN); //
    i2c_master_write_byte(cmd, reg_addr, ACK_CHECK_EN);

    i2c_master_start(cmd); //IIC start

    i2c_master_write_byte(cmd, 2 * sla_addr + 1, ACK_CHECK_EN); //

    i2c_master_read_byte(cmd, val, NACK_VAL); //只读1 byte 不需要应答

    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}

/*******************************************************************************
//Read a byte from slave register whit multiple try
*******************************************************************************/
void MulTry_IIC_RD_Reg(uint8_t sla_addr, uint8_t reg_addr, uint8_t *val)
{
    uint8_t n_try;

    for (n_try = 0; n_try < RETRY_TIME_OUT; n_try++)
    {
        if (IIC_RD_Reg(sla_addr, reg_addr, val) == ESP_OK)
        {
            break;
        }
        vTaskDelay(3 / portTICK_RATE_MS);
        //delay about 3ms
    }
}

/*******************************************************************************
// write multi byte to slave register
*******************************************************************************/
static short I2C_WR_mulReg(uint8_t sla_addr, uint8_t reg_addr, uint8_t *buf, uint8_t len)
{
    int ret;
    int i;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 2 * sla_addr, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg_addr, ACK_CHECK_EN);
    for (i = 0; i < len; i++)
    {
        i2c_master_write_byte(cmd, *buf, ACK_CHECK_EN);
        buf++;
    }
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

/*******************************************************************************
  write multi byte to slave register whit multiple try
*******************************************************************************/
void MulTry_I2C_WR_mulReg(uint8_t sla_addr, uint8_t reg_addr, uint8_t *buf, uint8_t len)
{
    uint8_t n_try;

    for (n_try = 0; n_try < RETRY_TIME_OUT; n_try++)
    {
        if (I2C_WR_mulReg(sla_addr, reg_addr, buf, len) == ESP_OK)
        {
            break;
        }
        vTaskDelay(10 / portTICK_RATE_MS);
        //delay about 6ms
    }
}

/*******************************************************************************
// read multiple byte from slave register
*******************************************************************************/
static short I2C_RD_mulReg(uint8_t sla_addr, uint8_t reg_addr, uint8_t *buf, uint8_t len)
{
    int ret;
    int i;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 2 * sla_addr, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg_addr, ACK_CHECK_EN);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 2 * sla_addr + 1, ACK_CHECK_EN);

    for (i = 0; i < len; i++)
    {
        if (i != len - 1)
        {
            i2c_master_read_byte(cmd, buf, ACK_VAL);
            buf++;
        }
        else
        {
            i2c_master_read_byte(cmd, buf, NACK_VAL);
        }
    }
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

/*******************************************************************************
  read multiple byte from slave register whit multiple try
*******************************************************************************/
void MulTry_I2C_RD_mulReg(uint8_t sla_addr, uint8_t reg_addr, uint8_t *buf, uint8_t len)
{
    uint8_t n_try;

    for (n_try = 0; n_try < RETRY_TIME_OUT; n_try++)
    {
        if (I2C_RD_mulReg(sla_addr, reg_addr, buf, len) == ESP_OK)
        {
            break;
        }
        vTaskDelay(5 / portTICK_RATE_MS);
    }
}

/*******************************************************************************
                                      END         
*******************************************************************************/
