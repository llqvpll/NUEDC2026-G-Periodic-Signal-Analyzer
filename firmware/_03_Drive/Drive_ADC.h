#ifndef __DRIVE_ADC_H
#define __DRIVE_ADC_H

/* 头文件包含 ----------------------------------------------------------------*/
#include "User_header.h"

/* 全局函数声明 --------------------------------------------------------------*/
void ADC1_Init(void);
void ADC2_Init(void);
void ADC3_Init(void);

float ADC1_GetVoltage(void);
float ADC2_GetVoltage(void);
float ADC3_GetVoltage(void);

#endif
/*******************************(C) COPYRIGHT 2016 Wind（谢玉伸）*********************************/
