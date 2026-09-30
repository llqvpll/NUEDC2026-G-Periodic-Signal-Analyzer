/* ****************************
 * Project description:
 *
 * ADC initialization head file
 *
 * Author: 创新基地 -> 2019 Mao
 *
 * Creation Date: 2021/10/06 - 1
 * ****************************/

#ifndef USER_ADC_H
#define USER_ADC_H

/* ***************************** Include & Define Part     	*****************************/
#include "User_header.h"

#define ADCDataLength 1024

/* ***************************** Function Declaration Part  *****************************/

void User_ADC_Init( uint8_t mode );

void User_ADC_GPIO_Init( uint8_t ch_num );	

void ADC_Mode_Independent_Init( void );
void ADC_DualMode_RegSimult_Init( void );

void ADC_DMA_Init( uint8_t mode );
void ADC_DMA_NVIC_Init( void );
void ADC_TIM3_Init( float fs );

void Get_DCVol( float vol[2] );
void Get_ACVol( float vol1_data[ADCDataLength] , float vol2_data[ADCDataLength] );
void FFT_Handle( void );
float Set_SamplingFre( float fs );

extern float ADCfre;
extern float ADCfre1;
extern float ADC1VOL;
extern float ADC2VOL;
extern float pha;

#endif
