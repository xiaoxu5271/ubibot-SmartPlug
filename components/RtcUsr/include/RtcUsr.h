/*
实时时钟程序
创建日期：2018年11月3日
作者：孙浩

void Rtc_Set(int year,int mon,int day,int hour,int min,int sec);
设置实时时钟，给定年月日时分秒设置当前系统RTC时间

void Rtc_Read(int* year,int* month,int* day,int* hour,int* min,int* sec);
读取当前时钟，给定年月日时分秒指针参数，获取当前实时时间


*/
#ifndef _RTCUSR_H_
#define _RTCUSR_H_

#include "stdint.h"

#define RETRY_TIME_OUT 3
#define UPDATE_TIME_SIZE 60 //1min

#define PCF8563_ADDR 0x51

#define Control_status_1 0x00
#define Control_status_2 0x01

#define VL_seconds 0x02
#define Minutes 0x03
#define Hours 0x04
#define Days 0x05
#define Weekdays 0x06
#define Century_months 0x07
#define Years 0x08

#define Minute_alarm 0x09
#define Hour_alarm 0x0a
#define Day_alarm 0x0b
#define Weekday_alarm 0x0c

#define CLKOUT_control 0x0d
#define Timer_control 0x0e
#define Timer_value 0x0f

/*******************************************************************************
 FUNCTION PROTOTYPES
*******************************************************************************/
extern void Rtc_Set(int year, int mon, int day, int hour, int min, int sec);
extern void Rtc_Read(int *year, int *month, int *day, int *hour, int *min, int *sec);
extern void Timer_IC_Init(void); //PCF8563 init

extern void Timer_IC_Reset_Time(void); //PCF8563 Reset Time

extern void Read_UTCtime(char *buffer, uint8_t buf_size); //PCF8563 read UTC time

//extern void Update_UTCtime(char *time);  //PCF8563 update time

extern unsigned long Read_UnixTime(void); //PCF8563 read Unix time

extern void Check_Update_UTCtime(char *time);

extern void MulTry_IIC_WR_Reg(uint8_t sla_addr, uint8_t reg_addr, uint8_t val); //write a byte to slave register whit multiple try

extern void MulTry_IIC_RD_Reg(uint8_t sla_addr, uint8_t reg_addr, uint8_t *val); //Read a byte from slave register whit multiple try

extern void MulTry_I2C_WR_mulReg(uint8_t sla_addr, uint8_t reg_addr, uint8_t *buf, uint8_t len); //write multi byte to slave register whit multiple try

extern void MulTry_I2C_RD_mulReg(uint8_t sla_addr, uint8_t reg_addr, uint8_t *buf, uint8_t len); //read multiple byte from slave register whit multiple time

#endif
