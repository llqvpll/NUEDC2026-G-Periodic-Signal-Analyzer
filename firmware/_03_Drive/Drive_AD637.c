/*
*********************************************************************************************************
                                               _04_OS
    File       : Drive_AD637.c
    platform   : STM32F407ZG
    function   : AD637 RMS-to-DC converter driver (ADC1 CH0, PA0)
*********************************************************************************************************
*/
#include "Drive_AD637.h"

#define AD637_SAMPLE_COUNT  16
#define AD637_TRIM_COUNT    2    /* 鎺掑簭鍚庨?栧熬鍚勫幓鎺?2涓?鏋佸?? */
#define AD637_REF_VOLTAGE   3.3f
#define AD637_ADC_MAX       4095.0f

/*
*********************************************************************************************************
*   鍑芥暟鍚? : AD637_Init
*   璇?  鏄? : 鍒濆?嬪寲 ADC1 閫氶亾0 (PA0), 鍗曟?¤浆鎹㈡ā寮?, 涓嶄娇鐢? DMA
*   鍙?  鏁? : 鏃?
*   杩斿洖鍊? : 鏃?
*********************************************************************************************************
*/
void AD637_Init(void)
{
    GPIO_InitTypeDef        GPIO_InitStructure;
    ADC_CommonInitTypeDef   ADC_CommonInitStructure;
    ADC_InitTypeDef         ADC_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    /* PA0 妯℃嫙杈撳叆 */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    RCC_APB2PeriphResetCmd(RCC_APB2Periph_ADC1, ENABLE);
    RCC_APB2PeriphResetCmd(RCC_APB2Periph_ADC1, DISABLE);

    ADC_CommonInitStructure.ADC_Mode              = ADC_Mode_Independent;
    ADC_CommonInitStructure.ADC_TwoSamplingDelay  = ADC_TwoSamplingDelay_5Cycles;
    ADC_CommonInitStructure.ADC_DMAAccessMode     = ADC_DMAAccessMode_Disabled;
    ADC_CommonInitStructure.ADC_Prescaler         = ADC_Prescaler_Div4;
    ADC_CommonInit(&ADC_CommonInitStructure);

    ADC_InitStructure.ADC_Resolution             = ADC_Resolution_12b;
    ADC_InitStructure.ADC_ScanConvMode           = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode     = DISABLE;   /* 鍗曟?¤浆鎹? */
    ADC_InitStructure.ADC_ExternalTrigConvEdge   = ADC_ExternalTrigConvEdge_None;
    ADC_InitStructure.ADC_ExternalTrigConv       = ADC_ExternalTrigConv_T1_CC1;
    ADC_InitStructure.ADC_DataAlign              = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfConversion        = 1;
    ADC_Init(ADC1, &ADC_InitStructure);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_56Cycles);

    ADC_Cmd(ADC1, ENABLE);
}

/*
*********************************************************************************************************
*   鍑芥暟鍚? : AD637_ReadVoltage
*   璇?  鏄? : 杩炵画閲囨牱16娆?, 鎺掑簭鍚庡幓鎺夐?栧熬鍚?2涓?鏋佸??, 鍙栦腑闂?12涓?骞冲潎
*            鍐嶇粡绾挎?ф牎鍑? (SCALE * x + OFFSET) 鍚庤繑鍥炵數鍘嬪??
*   鍙?  鏁? : 鏃?
*   杩斿洖鍊? : 鏍″噯鍚庣殑鐢靛帇鍊? (V)
*********************************************************************************************************
*/
float AD637_ReadVoltage(void)
{
    u16 samples[AD637_SAMPLE_COUNT];
    u16 i, j, temp;
    u32 sum = 0;
    float voltage;

    /* 1. 杩炵画閲囨牱 16 娆? */
    for (i = 0; i < AD637_SAMPLE_COUNT; i++)
    {
        ADC_SoftwareStartConv(ADC1);
        while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
        samples[i] = ADC_GetConversionValue(ADC1);
    }

    /* 2. 鍐掓场鎺掑簭 (鍗囧簭) */
    for (i = 0; i < AD637_SAMPLE_COUNT - 1; i++)
    {
        for (j = 0; j < AD637_SAMPLE_COUNT - 1 - i; j++)
        {
            if (samples[j] > samples[j + 1])
            {
                temp         = samples[j];
                samples[j]   = samples[j + 1];
                samples[j+1] = temp;
            }
        }
    }

    /* 3. 鍘绘帀棣栧熬鍚? TRIM_COUNT 涓?鏋佸??, 鍙栦腑闂撮儴鍒嗘眰骞冲潎 */
    for (i = AD637_TRIM_COUNT; i < AD637_SAMPLE_COUNT - AD637_TRIM_COUNT; i++)
    {
        sum += samples[i];
    }

    voltage = (sum / (float)(AD637_SAMPLE_COUNT - 2 * AD637_TRIM_COUNT))
              * AD637_REF_VOLTAGE / AD637_ADC_MAX;

    /* 4. 绾挎?ф牎鍑? */
    voltage = AD637_SCALE * voltage + AD637_OFFSET;

    return voltage;
}
