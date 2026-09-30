/*
*********************************************************************************************************
                                               _04_OS
    File			 : Drive_DAC.c
    By  			 :
    platform   : STM32F407ZG
	Data   		 : 2018/7/16
    function 	 : DAC锟斤拷锟矫筹拷锟斤拷
*********************************************************************************************************
*/
#include "Drive_DAC.h"


/* 私锟叫宏定锟斤拷 ----------------------------------------------------------------*/
#define DAC_ADDRESS 0x40007414
/* 私锟叫ｏ拷锟斤拷态锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷 ------------------------------------------------------*/
const uint16_t aSine12bit[4] = {2772,3796,1323,299};
/* 全锟街憋拷锟斤拷锟斤拷锟斤拷 --------------------------------------------------------------*/
ddsStruct ddsStructData;
/* 全锟街猴拷锟斤拷锟斤拷写 --------------------------------------------------------------*/

/**
    @brief  锟斤拷锟斤拷sin锟斤拷
    @param  锟斤拷
    @retval 锟斤拷
*/
static double ddsSinWave(uint32_t t, uint32_t T,double cycle)
{
	t %= T;
	return sin(2 * 3.14159265358979 * t / T);
}

static double ddsTriangleWave(uint32_t t,uint32_t T,double cycle)
{
	t %= T;
	if(t < (T/4) )
		return 4.0f * t /T ;
	else if(t < (T*3/4))
		return 2.0f - 4.0f * t /T;
	else
		return 4.0f * t/T - 4.0f;
}

static double ddsSawtoothWave(uint32_t t,uint32_t T,double cycle)
{
	t %= T;
	if(t <(T /2))
		return 2.0 * t/T;
	else
		return 2.0f * t /T - 2.0f;
}

static double ddsSquareWave(uint32_t t,uint32_t T,double cycle)
{
	t %= T;
	if(t < (T*(cycle/100.0f)) )
		return 1.0f;
	else
		return -1.0f;
}

/**
    @brief  dds锟结构锟斤拷锟绞硷拷锟?
    @param  锟斤拷
    @retval 锟斤拷
*/
static void ddsDataInit(void)
{
	uint16_t i;
	for(i=0 ; i<1000 ; i++)
		ddsStructData.data[i] = 0;
	ddsStructData.wave = SINWAVE;
	ddsStructData.hz = 1000;
	ddsStructData.vpp = 2.0;
	ddsStructData.dutycycle = 50;
	ddsStructData.length = 512;  /* 默锟较筹拷锟饺ｏ拷确锟斤拷锟阶达拷DMA锟斤拷锟斤拷锟斤拷效 */
	ddsStructData.createWaveData = ddsSinWave;
}

/**
    @brief   dacDMA 锟斤拷始锟斤拷
    @param  锟斤拷
    @retval 锟斤拷
*/
void dacInit(void)
{
	static uint8_t firstInitFlag = False;
	GPIO_InitTypeDef GPIO_InitStruct;
	DMA_InitTypeDef  DMA_InitStruct;
	DAC_InitTypeDef  DAC_InitStruct;
	
	/* 锟阶次碉拷锟斤拷时锟饺筹拷始锟斤拷 ddsStructData锟斤拷确锟斤拷 length 锟斤拷锟街讹拷锟斤拷效 */
	if(firstInitFlag == False)
	{
		ddsDataInit();
		firstInitFlag = True;
	}
	
	//使锟斤拷锟斤拷应时锟斤拷
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA|RCC_AHB1Periph_DMA1,ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC,ENABLE);
	//锟斤拷锟斤拷IO锟斤拷
	GPIO_InitStruct.GPIO_Mode   =  GPIO_Mode_AN;
	GPIO_InitStruct.GPIO_PuPd   =  GPIO_PuPd_DOWN;
	GPIO_InitStruct.GPIO_Pin    =  GPIO_Pin_4|GPIO_Pin_5;
	GPIO_Init(GPIOA,&GPIO_InitStruct);
	//锟斤拷锟斤拷DAC1
	DAC_InitStruct.DAC_LFSRUnmask_TriangleAmplitude   = DAC_LFSRUnmask_Bit0;
	DAC_InitStruct.DAC_OutputBuffer   = DAC_OutputBuffer_Disable;
	DAC_InitStruct.DAC_Trigger     =  DAC_Trigger_T6_TRGO;
	DAC_InitStruct.DAC_WaveGeneration    = DAC_WaveGeneration_None;
	DAC_Init(DAC_Channel_1,&DAC_InitStruct);
	
	DMA_DeInit(DMA1_Stream5);
	DMA_InitStruct.DMA_Mode                  = DMA_Mode_Circular;   //循锟斤拷模式
	DMA_InitStruct.DMA_Channel               = DMA_Channel_7;
	DMA_InitStruct.DMA_BufferSize            = ddsStructData.length;
	DMA_InitStruct.DMA_DIR                   = DMA_DIR_MemoryToPeripheral;
	DMA_InitStruct.DMA_FIFOMode              = DMA_FIFOMode_Disable;   //锟斤拷止FIFO
	DMA_InitStruct.DMA_FIFOThreshold         = DMA_FIFOThreshold_1QuarterFull;
	DMA_InitStruct.DMA_PeripheralBaseAddr    = (u32)&(DAC->DHR12R1);
	DMA_InitStruct.DMA_Memory0BaseAddr       = (u32)ddsStructData.data;
	DMA_InitStruct.DMA_PeripheralBurst       = DMA_PeripheralBurst_Single;
	DMA_InitStruct.DMA_MemoryBurst           = DMA_MemoryBurst_Single;
	DMA_InitStruct.DMA_PeripheralDataSize    = DMA_PeripheralDataSize_HalfWord;
	DMA_InitStruct.DMA_MemoryDataSize        = DMA_MemoryDataSize_HalfWord;
	DMA_InitStruct.DMA_PeripheralInc         = DMA_PeripheralInc_Disable;
	DMA_InitStruct.DMA_MemoryInc             = DMA_MemoryInc_Enable;
	DMA_InitStruct.DMA_Priority              = DMA_Priority_High;
	DMA_Init(DMA1_Stream5,&DMA_InitStruct);
	DMA_Cmd(DMA1_Stream5,ENABLE);
	
	DAC_DMACmd(DAC_Channel_1,ENABLE);
	
	DAC_Cmd(DAC_Channel_1,ENABLE);
	
	DAC_SoftwareTriggerCmd(DAC_Channel_1,ENABLE);
}


/**
    @brief   锟截憋拷dac锟斤拷锟?
    @param  锟斤拷
    @retval 锟斤拷
*/
void dacClose(void)
{
	DAC_Cmd(DAC_Channel_1,DISABLE);
	TIM_Cmd(TIM6,DISABLE);
}


/**
    @brief   锟截憋拷dac锟斤拷锟?
    @param  锟斤拷
    @retval 锟斤拷
*/
void dacOpen(void)
{
	DAC_Cmd(DAC_Channel_1,ENABLE);
	TIM_Cmd(TIM6,ENABLE);
}


/**
    @brief  锟侥憋拷dds锟斤拷锟?
    @param  vpp  锟斤拷锟斤拷锟窖癸拷锟斤拷值
    @param  fre  锟斤拷锟狡碉拷锟?
    @param  dutyCycle  占锟秸憋拷
    @param  wave  锟斤拷锟斤拷锟斤拷锟?
    @retval 锟斤拷
*/
void setDDS(double vpp,uint32_t fre,double dutyCycle,waveEnum wave)
{
	uint16_t i;
	float temp;
	static uint16_t preLength = 0;
	TIM_Cmd(TIM6,DISABLE);
	for(i=0 ; i<1000 ; i++)
		ddsStructData.data[i] = 0;
	if(fre < 500)
		ddsStructData.length = 512;
	else if(fre < 5000)
		ddsStructData.length = 128;
	else if(fre < 50000)
		ddsStructData.length = 64;
	else if(fre < 100000)
		ddsStructData.length  = 32;
	else if(fre < 200000)
		ddsStructData.length  = 32;
	else if(fre >= 200000)
		ddsStructData.length  = 32;
	else
		ddsStructData.length  = 512;
	if(preLength != ddsStructData.length)
	{
		preLength = ddsStructData.length;
		dacInit();
	}
	if(wave == SINWAVE)
		ddsStructData.createWaveData = ddsSinWave;
	else if(wave == TRIANGLEWAE)
		ddsStructData.createWaveData = ddsTriangleWave;
	else if(wave == SAWTOOTHWAVE)
		ddsStructData.createWaveData = ddsSawtoothWave;
	else if(wave == SQUAREWAVE)
		ddsStructData.createWaveData = ddsSquareWave;
	ddsStructData.wave = wave;
	ddsStructData.vpp = vpp;
	ddsStructData.hz = fre;            
	ddsStructData.dutycycle = dutyCycle;
	for(i=0 ; i<ddsStructData.length ; i++)
	{
		temp = ((ddsStructData.createWaveData(i,ddsStructData.length,ddsStructData.dutycycle) + 1.0f)/2.0f);
		ddsStructData.data[i] =  (uint16_t)(temp * (ddsStructData.vpp/3.3f) * 4095.0f);
	}
	timer6Init(ddsStructData.hz * ddsStructData.length);
	TIM_Cmd(TIM6,ENABLE);
}
/**
    @brief  锟斤拷锟节达拷锟斤拷DAC锟斤拷锟?
    @param  锟斤拷
    @retval 锟斤拷
*/
void timer6Init(uint32_t hz)
{
	uint32_t period;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruct;
	
	if(hz == 0) hz = 1;
	period = 84000000 / hz - 1;
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6,ENABLE);
	
	TIM_TimeBaseInitStruct.TIM_Period = period;
	TIM_TimeBaseInitStruct.TIM_Prescaler = 0;
	TIM_TimeBaseInitStruct.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInitStruct.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM6,&TIM_TimeBaseInitStruct);
	
	TIM_SelectOutputTrigger(TIM6,TIM_TRGOSource_Update);
	
	TIM_Cmd(TIM6,ENABLE);
}


/** ----------------------------------------------------------------------------
    @FunctionName  : DAC1_Init()
    @Description   : None
    @Data          : 2016/7/11
    @Explain       : None
    ------------------------------------------------------------------------------*/
void DAC1_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	DAC_InitTypeDef DAC_InitType;
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);//锟斤拷使锟斤拷 PA 时锟斤拷
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC, ENABLE);//锟斤拷使锟斤拷 DAC 时锟斤拷
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;//模锟斤拷锟斤拷锟斤拷
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;//锟斤拷锟斤拷
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);//锟劫筹拷始锟斤拷 GPIO
	
	DAC_InitType.DAC_Trigger=DAC_Trigger_None; //锟斤拷使锟矫达拷锟斤拷锟斤拷锟斤拷 TEN1=0
	DAC_InitType.DAC_WaveGeneration=DAC_WaveGeneration_None;//锟斤拷使锟矫诧拷锟轿凤拷锟斤拷
	DAC_InitType.DAC_LFSRUnmask_TriangleAmplitude=DAC_LFSRUnmask_Bit0; //锟斤拷锟轿★拷锟斤拷值锟斤拷锟斤拷
	DAC_InitType.DAC_OutputBuffer=DAC_OutputBuffer_Disable ;//锟斤拷锟斤拷锟斤拷锟截憋拷
	DAC_Init(DAC_Channel_1,&DAC_InitType); //锟桔筹拷始锟斤拷 DAC 通锟斤拷 1
	
	DAC_Cmd(DAC_Channel_1, ENABLE); //锟斤拷使锟斤拷 DAC 通锟斤拷 1
	
	DAC_SetChannel1Data(DAC_Align_12b_R, 0);  //锟斤拷12 位锟揭讹拷锟斤拷锟斤拷锟捷革拷式
}
/** ----------------------------------------------------------------------------
    @FunctionName  : DAC2_Init()
    @Description   : None
    @Data          : 2026/6/6
    @Explain       : PA5 DAC2 output
    ------------------------------------------------------------------------------*/
void DAC2_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	DAC_InitTypeDef DAC_InitType;
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC, ENABLE);
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	DAC_InitType.DAC_Trigger = DAC_Trigger_None;
	DAC_InitType.DAC_WaveGeneration = DAC_WaveGeneration_None;
	DAC_InitType.DAC_LFSRUnmask_TriangleAmplitude = DAC_LFSRUnmask_Bit0;
	DAC_InitType.DAC_OutputBuffer = DAC_OutputBuffer_Disable;
	DAC_Init(DAC_Channel_2, &DAC_InitType);
	
	DAC_Cmd(DAC_Channel_2, ENABLE);
	DAC_SetChannel2Data(DAC_Align_12b_R, 0);
}


void DAC1_Out(float vol)
{
	u16 dac_val;

	if(vol < 0.0f)
	{
		vol = 0.0f;
	}
	else if(vol > 3.3f)
	{
		vol = 3.3f;
	}

	dac_val = (u16)((vol * 4095.0f / 3.3f) + 0.5f);
	DAC_SetChannel1Data(DAC_Align_12b_R, dac_val);
}

void DAC2_Out(float vol)
{
	u16 dac_val;
	if(vol < 0.0f)
	{
		vol = 0.0f;
	}
	else if(vol > 3.3f)
	{
		vol = 3.3f;
	}
	dac_val = (u16)((vol * 4095.0f / 3.3f) + 0.5f);
	DAC_SetChannel2Data(DAC_Align_12b_R, dac_val);
}



/*******************************(C) COPYRIGHT 2016 Wind锟斤拷谢锟斤拷锟届）*********************************/





