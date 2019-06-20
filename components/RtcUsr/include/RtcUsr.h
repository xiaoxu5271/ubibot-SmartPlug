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

extern void Rtc_Set(int year,int mon,int day,int hour,int min,int sec);
extern void Rtc_Read(int* year,int* month,int* day,int* hour,int* min,int* sec);


#endif

