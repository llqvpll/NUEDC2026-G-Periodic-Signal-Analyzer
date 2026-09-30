/* ****************************
 * Project description:
 *
 * ADC initialization file
 *
 * Author: 创新基地 -> 2019 Mao
 *
 * Creation Date: 2021/10/06 - 1
 * ****************************/
 
 /* ***************************** Include & Define Part     	*****************************/
#include "User_ADC.h"
 
#define BaseVol 3.299f

uint32_t ADCData[ADCDataLength];
float ADCfre = 0.0f;
float ADCfre1 = 0.0f;
float ADC1VOL = 0.0f;
float ADC2VOL = 0.0f;
float pha = 0.0f;

static float ADC_SamplingFre = 5000.0f;
static float fft_inputbuf1[ADCDataLength * 2];
static float fft_outputbuf1[ADCDataLength];
static float fft_inputbuf2[ADCDataLength * 2];
static float fft_outputbuf2[ADCDataLength];
static arm_cfft_radix4_instance_f32 scfft;

/* ***************************** Initialization Part        *****************************
 * 初始化函数区
 * */

// 初始化ADC 参数：模式序号
// 注：所有模式均由TIM3触发采样，即都被配置为单次采样
// mode -> 0 ：独立模式单通道采样
// mode -> 1 ：双通道规则同步采样
void User_ADC_Init( uint8_t mode )
{
	if( mode == 0 )									// 独立模式单通道采样
	{
		User_ADC_GPIO_Init(1);	
		ADC_Mode_Independent_Init();
		ADC_DMA_Init( 0 );
		ADC_DMA_NVIC_Init();		
		ADC_TIM3_Init( 5000 );	// 初始化采样率为5kHz
	}
	else if( mode == 1 )						// 双通道规则同步采样
	{
		User_ADC_GPIO_Init(2);
		ADC_DualMode_RegSimult_Init();
		ADC_DMA_Init( 1 );
		ADC_DMA_NVIC_Init();
		ADC_TIM3_Init( 5000 );	// 初始化采样率为5kHz
	}
}
 
// 采样GPIO口初始化 参数：采样通道数量
void User_ADC_GPIO_Init( uint8_t ch_num )	
{
	GPIO_InitTypeDef GPIO_InitStruct;
	// Configure ADC -> GPIO
	RCC_AHB1PeriphClockCmd( RCC_AHB1Periph_GPIOA , ENABLE );
	if( ch_num == 1 )
		GPIO_InitStruct.GPIO_Pin = GPIO_Pin_1;
	else
		GPIO_InitStruct.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AN;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStruct.GPIO_Speed = GPIO_High_Speed;
	
	GPIO_Init( GPIOA , &GPIO_InitStruct );
}

// 独立模式单通道单次转换初始化(ADC1)
void ADC_Mode_Independent_Init()
{
	// 创建 ADC初始化结构体 以及 ADC共享初始化结构体
	ADC_InitTypeDef ADC_InitStruct;
	ADC_CommonInitTypeDef ADC_ComInitStruct;
	// 使能ADC1时钟
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_ADC1 , ENABLE );	
	// 配置所有ADC工作模式
	ADC_ComInitStruct.ADC_Mode = ADC_Mode_Independent;													// 工作模式 -> 独立模式
	ADC_ComInitStruct.ADC_Prescaler = ADC_Prescaler_Div4;												// 分频系数 -> 四分频
	ADC_ComInitStruct.ADC_DMAAccessMode = ADC_DMAAccessMode_1;									// DMA传输模式 -> 模式1
	ADC_ComInitStruct.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_20Cycles;			// 采样间隔 -> 20个时钟周期
	ADC_CommonInit( &ADC_ComInitStruct );
	// 配置ADC1工作方式
	ADC_InitStruct.ADC_Resolution            = ADC_Resolution_12b;            	// 采样分辨率 -> 12位  
	ADC_InitStruct.ADC_ScanConvMode          = DISABLE;													// 扫描转换 -> 失能
	ADC_InitStruct.ADC_ContinuousConvMode    = DISABLE;													// 连续转换 -> 失能
	ADC_InitStruct.ADC_ExternalTrigConvEdge  = ADC_ExternalTrigConvEdge_Rising;	// 外部触发转换边沿 -> 上升沿
	ADC_InitStruct.ADC_ExternalTrigConv      = ADC_ExternalTrigConv_T3_TRGO;		// 外部触发转换 -> TIM3触发
	ADC_InitStruct.ADC_DataAlign             = ADC_DataAlign_Right;							// 数据对齐方式 -> 右对齐
	ADC_InitStruct.ADC_NbrOfConversion       = 1;    														// 转换数量 -> 1个
	ADC_Init( ADC1 , &ADC_InitStruct );
	// 配置ADC1规则通道组
	ADC_RegularChannelConfig( ADC1 , ADC_Channel_1 , 1 , ADC_SampleTime_15Cycles );
	// 允许在最后一次ADC转换后发出DMA请求
	ADC_DMARequestAfterLastTransferCmd( ADC1 , ENABLE );
	// 允许ADC1使用DMA
	ADC_DMACmd( ADC1 , ENABLE );
	// 使能ADC1
	ADC_Cmd( ADC1 , ENABLE );
}

// 双通道ADC规则采样初始化
void ADC_DualMode_RegSimult_Init()
{
	// Creat ADC InitStruct and ComInitStruct
	ADC_InitTypeDef ADC_InitStruct;
	ADC_CommonInitTypeDef ADC_ComInitStruct;
	// Enable the ADC clock
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_ADC1 , ENABLE );
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_ADC2 , ENABLE );
	// Configure ADC Common
	ADC_ComInitStruct.ADC_Mode = ADC_DualMode_RegSimult;										// 工作模式 -> 双通道规则同步
	ADC_ComInitStruct.ADC_Prescaler = ADC_Prescaler_Div4;										// 分频系数 -> 四分频
	ADC_ComInitStruct.ADC_DMAAccessMode = ADC_DMAAccessMode_2;							// DMA传输模式 -> 模式2
	ADC_ComInitStruct.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;	// 采样间隔 -> 五个时钟周期
	ADC_CommonInit( &ADC_ComInitStruct );
	// Configure ADC
	ADC_InitStruct.ADC_Resolution            = ADC_Resolution_12b;       				// 采样分辨率 -> 12位  
	ADC_InitStruct.ADC_ScanConvMode          = ENABLE;													// 扫描转换 -> 失能
	ADC_InitStruct.ADC_ContinuousConvMode    = DISABLE;													// 连续转换 -> 失能
	ADC_InitStruct.ADC_ExternalTrigConvEdge  = ADC_ExternalTrigConvEdge_Rising; // 外部触发转换边沿 -> 上升沿
	ADC_InitStruct.ADC_ExternalTrigConv      = ADC_ExternalTrigConv_T3_TRGO;		// 外部触发转换 -> TIM3触发
	ADC_InitStruct.ADC_DataAlign             = ADC_DataAlign_Right;							// 数据对齐方式 -> 右对齐
	ADC_InitStruct.ADC_NbrOfConversion       = 1;    														// 转换数量 -> 1个
	ADC_Init( ADC1 , &ADC_InitStruct );	
	ADC_Init( ADC2 , &ADC_InitStruct );
	// Configure ADC Channel
	ADC_RegularChannelConfig( ADC1 , ADC_Channel_1 , 1 , ADC_SampleTime_15Cycles );
	ADC_RegularChannelConfig( ADC2 , ADC_Channel_2 , 1 , ADC_SampleTime_15Cycles );
	ADC_DMARequestAfterLastTransferCmd( ADC1 , ENABLE );
	ADC_DMARequestAfterLastTransferCmd( ADC2 , ENABLE );
	// Enable ADC -> DMA
	ADC_DMACmd( ADC1 , ENABLE );
	ADC_DMACmd( ADC2 , ENABLE );
	// Enable ADC
	ADC_Cmd( ADC1 , ENABLE );
	ADC_Cmd( ADC2 , ENABLE );
}

// ADC采样传输DMA初始化 参数：1>工作模式
void ADC_DMA_Init( uint8_t mode )	
{
	DMA_InitTypeDef DMA_InitStruct;
	
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);
	
	DMA_DeInit( DMA2_Stream0 );																						// 初始化数据流
	
	if( mode == 0 )
		DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&ADC1 -> DR;			// 源地址 -> ADC1的规则数据寄存器
	else
		DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&ADC -> CDR;			// 源地址 -> ADC通用规则数据寄存器
	
	DMA_InitStruct.DMA_Channel = DMA_Channel_0;														// DMA通道
	DMA_InitStruct.DMA_Memory0BaseAddr = (uint32_t)&ADCData;							// 目标地址
	DMA_InitStruct.DMA_DIR = DMA_DIR_PeripheralToMemory;									// 传输方向(外设->内存)
	DMA_InitStruct.DMA_BufferSize = ADCDataLength;												// 数据大小
	DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;					// 源地址是否递增
	DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;									// 目标地址是否递增
	DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;	// 源数据大小(字节)
	DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;					// 目标数据大小(字节)
	DMA_InitStruct.DMA_Mode = DMA_Mode_Circular;													// 工作模式
	DMA_InitStruct.DMA_Priority = DMA_Priority_High;
	DMA_InitStruct.DMA_FIFOMode = DMA_FIFOMode_Disable;         
	DMA_InitStruct.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
	DMA_InitStruct.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	DMA_InitStruct.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
	DMA_Init( DMA2_Stream0 , &DMA_InitStruct );
	
	// 清除DMA2中断标志
	DMA_ClearFlag( DMA2_Stream0 , DMA_FLAG_TCIF0 );  
	// 使能传输完成时触发中断
	DMA_ITConfig( DMA2_Stream0 , DMA_IT_TC , ENABLE );  
	
	// 使能DMA2
	DMA_Cmd(DMA2_Stream0, ENABLE); 
}

// 采样传输DMA的中断初始化
void ADC_DMA_NVIC_Init()
{
	NVIC_InitTypeDef NVIC_InitStruct;
	
	// Configure DMA -> NVIC
	NVIC_InitStruct.NVIC_IRQChannel = DMA2_Stream0_IRQn;
	NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;
	
	NVIC_PriorityGroupConfig( NVIC_PriorityGroup_2 );
	NVIC_Init( &NVIC_InitStruct );
}

// ADC采样定时器TIM3初始化 参数：初始化采样率
void ADC_TIM3_Init( float fs )
{
	TIM_TimeBaseInitTypeDef TIM_BaseInitStruct;
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	
	TIM_BaseInitStruct.TIM_Prescaler = (1000 - 1);
	TIM_BaseInitStruct.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_BaseInitStruct.TIM_Period = (21 - 1);
	TIM_BaseInitStruct.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_BaseInitStruct.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit( TIM3 , &TIM_BaseInitStruct );
	TIM_SelectOutputTrigger( TIM3 , TIM_TRGOSource_Update );
	TIM_UpdateDisableConfig( TIM3 , DISABLE ); 
	//TIM_DMACmd( TIM3 , TIM_DMA_Update , ENABLE );
	TIM_Cmd( TIM3 , DISABLE );
	ADC_SamplingFre = Set_SamplingFre( fs );
}

/* ***************************** Custom Function Part       *****************************
 * 自定义函数区
 */

// 获取电压平均值(用于直流直接采集) 参数：两个ADC转换的电压的平均值(大小为2的数组)(输入指针)
void Get_DCVol( float vol[2] )
{
	float temp_vol1 , temp_vol2;
	uint32_t count;
	
	ADCData[ADCDataLength - 1] = 0;
	TIM_Cmd( TIM3 , ENABLE );
	while( ADCData[ADCDataLength - 1] == 0 );
	TIM_Cmd( TIM3 , DISABLE );
	
	vol[0] = vol[1] = 0;
	for( count = 0 ; count < ADCDataLength ; count ++ )
	{
		temp_vol1 = (float)( ADCData[count] & 0x0000ffff ) / 0xfff * BaseVol;
		temp_vol2 = (float)( ADCData[count] >> 16 ) / 0xfff * BaseVol;
		vol[0] = vol[0] + temp_vol1;
		vol[1] = vol[1] + temp_vol2; 
	}
	vol[0] = vol[0] / ADCDataLength;
	vol[1] = vol[1] / ADCDataLength;
}

// 将所有AD数据转换为电压(用于采集交流电压) 参数：1>ADC1转换的电压数据；2>ADC2转换的电压数据(输入两指针)
void Get_ACVol( float vol1_data[ADCDataLength] , float vol2_data[ADCDataLength] )
{
	uint32_t count;	//计数器
	
	// 最后一位清零
	ADCData[ADCDataLength - 1] = 0;
	// 使能TIM3，开始采样
	TIM_Cmd( TIM3 , ENABLE );
	// 最后一位有值则采样结束
	while( ADCData[ADCDataLength - 1] == 0 );
	// 关闭定时器( 后改为了使用DMA中断关闭 )
	//TIM_Cmd( TIM3 , DISABLE );
	
	// 将ADC数据转换为电压返回
	for( count = 0 ; count < ADCDataLength ; count ++ )
	{
		// 低16位为ADC1的数据
		vol1_data[count] = (float)( ADCData[count] & 0x0000ffff ) / 0xfff * BaseVol;
		// 高16位为ADC2的数据
		vol2_data[count] = (float)( ADCData[count] >> 16 ) / 0xfff * BaseVol;
	}
}

// FFT测量ADC1/ADC2的峰峰值、频率与相位差
void FFT_Handle( void )
{
	uint32_t count;
	uint32_t BaseIndexADC1 = 0;
	uint32_t BaseIndexADC2 = 0;
	float BaseValueADC1 = 0.0f;
	float BaseValueADC2 = 0.0f;
	float pha1 = 0.0f;
	float pha2 = 0.0f;

	ADCData[ADCDataLength - 1] = 0;
	TIM_Cmd( TIM3 , ENABLE );
	while( ADCData[ADCDataLength - 1] == 0 );

	for( count = 0 ; count < ADCDataLength ; count ++ )
	{
		fft_inputbuf1[2 * count] = (float)( ADCData[count] & 0x0000ffff ) / 0xfff * BaseVol;
		fft_inputbuf1[2 * count + 1] = 0.0f;
		fft_inputbuf2[2 * count] = (float)( ADCData[count] >> 16 ) / 0xfff * BaseVol;
		fft_inputbuf2[2 * count + 1] = 0.0f;
	}

	arm_cfft_radix4_init_f32( &scfft , ADCDataLength , 0 , 1 );
	arm_cfft_radix4_f32( &scfft , fft_inputbuf1 );
	arm_cmplx_mag_f32( fft_inputbuf1 , fft_outputbuf1 , ADCDataLength );
	fft_outputbuf1[0] = 0.0f;
	arm_max_f32( fft_outputbuf1 , ADCDataLength / 2 , &BaseValueADC1 , &BaseIndexADC1 );
	pha1 = atan2( fft_inputbuf1[2 * BaseIndexADC1 + 1] , fft_inputbuf1[2 * BaseIndexADC1] ) * 180.0f / PI;

	arm_cfft_radix4_init_f32( &scfft , ADCDataLength , 0 , 1 );
	arm_cfft_radix4_f32( &scfft , fft_inputbuf2 );
	arm_cmplx_mag_f32( fft_inputbuf2 , fft_outputbuf2 , ADCDataLength );
	fft_outputbuf2[0] = 0.0f;
	arm_max_f32( fft_outputbuf2 , ADCDataLength / 2 , &BaseValueADC2 , &BaseIndexADC2 );
	pha2 = atan2( fft_inputbuf2[2 * BaseIndexADC2 + 1] , fft_inputbuf2[2 * BaseIndexADC2] ) * 180.0f / PI;

	ADCfre = BaseIndexADC1 * ADC_SamplingFre / ADCDataLength;
	ADCfre1 = BaseIndexADC2 * ADC_SamplingFre / ADCDataLength;
	ADC1VOL = 4.0f * BaseValueADC1 / ADCDataLength;
	ADC2VOL = 4.0f * BaseValueADC2 / ADCDataLength;
	pha = pha1 - pha2;

	while( pha > 180.0f || pha < -180.0f )
	{
		if( pha > 180.0f )
			pha -= 360.0f;
		else if( pha < -180.0f )
			pha += 360.0f;
		else
			break;
	}
}

// 设置TIM3采样率 参数：1>预设采样率 返回：实际采样率
float Set_SamplingFre( float fs )
{
	uint32_t psc = 1;
	uint32_t arr = 1;
	TIM_Cmd(TIM3 , DISABLE);						// 关闭定时器
	do																	// 循环查找满足条件的PSC和ARR值
	{
		arr = 84000000 / (fs * psc);
		psc ++;
	}
	while( arr > 65535 );
	psc --;
	fs = (float)84000000 / (psc * arr); // 由PSC和ARR算出实际采样率
	TIM3 -> ARR = arr - 1;							// 改变定时器重装载值改变采样率
	TIM3 -> PSC = psc - 1;							// 改变定时器分频系数改变采样率
	return fs;
}
 
/* ***************************** IRQHandler Part    	     	*****************************
 * 中断执行函数区
 */

// DMA2_Stream0中断执行函数 用于关闭定时器
void DMA2_Stream0_IRQHandler()
{
	DMA_ClearFlag( DMA2_Stream0 , DMA_FLAG_TCIF0 );
	
	TIM_Cmd( TIM3 , DISABLE );
}
 
 
/* ***************************** 						END 	   	     	*****************************/






