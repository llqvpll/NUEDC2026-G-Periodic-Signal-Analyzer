/* 防止重调用 ----------------------------------------------------------------*/
#ifndef __DRIVE_DAC_H
#define __DRIVE_DAC_H
/* 头文件包含 ----------------------------------------------------------------*/
#include "User_header.h"

/* 全局宏定义 ----------------------------------------------------------------*/
#ifndef True
#define True 1
#endif

#ifndef False
#define False 0
#endif
/* 结构体声明 ----------------------------------------------------------------*/
typedef enum
{
	SINWAVE,
	TRIANGLEWAE,
	SAWTOOTHWAVE,
	SQUAREWAVE

} waveEnum;

typedef struct
{
	uint16_t data[1000];
	uint16_t length;
	float vpp;
	uint32_t  hz;
	waveEnum wave;
	uint16_t dutycycle;
	double (*createWaveData)(uint32_t t,uint32_t T,double cycle);
} ddsStruct;


/* 全局变量声明 --------------------------------------------------------------*/
extern ddsStruct ddsStructData;
/* 全局函数声明 --------------------------------------------------------------*/
void timer6Init(uint32_t hz);
void DAC1_Init(void);
void DAC2_Init(void);
void DAC1_Out(float vol);
void DAC2_Out(float vol);
void dacClose(void);
void dacOpen(void);
void dacInit(void);
void setDDS(double vpp,uint32_t fre,double dutyCycle,waveEnum wave);

#endif


