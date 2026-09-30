/*
*********************************************************************************************************
                                               _03_Drive
    File       : Drive_AD9240.c
    platform   : STM32F407ZG
    function   : AD9240 14-bit parallel ADC — TIM8 dual-DMA capture
    =========================================================================

    Hardware wiring (AD9240 module silkscreen labels)
    -------------------------------------------------
    模块左侧排针 (10-pin)              模块右侧排针 (10-pin)
     ~~~~~~~~~~~~~~~~                     ~~~~~~~~~~~~~~~~
     Pin   模块丝印  STM32   方向          Pin   模块丝印  STM32   方向
     1     +5V      5V      电源           1     +5V      5V      电源
     2     GND      GND     地             2     GND      GND     地
     3     CLK      PC6     TIM8_CH1→      3     D14(MSB) PF9     模块→STM
     4     D13      PF8     模块→STM       4     D12      PF5     模块→STM
     5     D11      PF4     模块→STM       5     D10      PF3     模块→STM
     6     D9       PF2     模块→STM       6     D8       PF1     模块→STM
     7     D7       PE6     模块→STM       7     D6       PE5     模块→STM
     8     D5       PE4     模块→STM       8     D4       PE3     模块→STM
     9     D3       PE2     模块→STM       9     D2       PE1     模块→STM
     10    D1(LSB)  PE0     模块→STM       10    OR       PF10    模块→STM (可选)

    模块丝印 D1..D14 对应 14-bit ADC 的 bit0..bit13
    低7位(PE0-PE6): 左排D1/D3/D5/D7 + 右排D2/D4/D6
    高7位(PF1-PF5,PF8-PF9): 左排D9/D11/D13 + 右排D8/D10/D12/D14
    (无PF0/PF6/PF7，跳过)

    AD9240 power
      AVDD = 5.0V,  DRVDD = 3.3V (check module: may need level shifters)
      VREF = 2.5V   (internal or external reference)
      VIN+ = signal, VIN- = GND  (single-ended, 0–2.5V range)

    Timing
    ------
    TIM8 on APB2 (168 MHz).
    For Fs Hz:  PSC = 168e6 / (Fs * 2) - 1,  ARR = 1.
      CH1 PWM1 mode, Pulse=1 => 50% duty clock on PC6.
      CH2 Compare at CCR2=1, triggers DMA2_Stream3 one tick after update.

    DMA
    ---
    DMA2_Stream1 Ch7 (TIM8_UP)  : GPIOE->IDR -> ad9240_buf_lo[2048]
    DMA2_Stream3 Ch7 (TIM8_CH2) : GPIOF->IDR -> ad9240_buf_hi[2048]

    Data recombination
    ------------------
    lo  = PE0-PE6 -> bits[6:0]  (module D1..D7)
    hi  = PF1-PF5 + PF8-PF9 -> bits[13:7] (module D8..D14)
    PF1-5 = bits[5:1], PF8-9 = bits[9:8] of GPIOF->IDR
    hi_packed = ((hi >> 1) & 0x1F) | ((hi >> 3) & 0x60)
    adc_val = (lo & 0x007F) | (hi_packed << 7)
    Voltage: V = adc_val / 16383.0f * 2.5f

    Key: open app_measure, press [7] to toggle AD9240 mode.
*********************************************************************************************************
*/

#include "Drive_AD9240.h"

/* ---- Data buffers (in BSS, zero-initialised) ---- */
volatile uint16_t ad9240_buf_lo[AD9240_BUF_SIZE];
volatile uint16_t ad9240_buf_hi[AD9240_BUF_SIZE];

/* ---- DMA stream definitions ---- */
#define AD9240_DMA_LO       DMA2_Stream1
#define AD9240_DMA_HI       DMA2_Stream3
#define AD9240_DMA_CH       DMA_Channel_7
#define AD9240_DMA_LO_FLAG  DMA_FLAG_TCIF1
#define AD9240_DMA_HI_FLAG  DMA_FLAG_TCIF3

/* ---- Timer definitions ---- */
#define AD9240_TIM          TIM8
#define AD9240_TIM_CLK      168000000UL
#define AD9240_TIM_PERIOD   1           /* ARR = 1, toggle every 2 ticks */
#define AD9240_TIM_CCR1     1           /* CH1 PWM pulse: 50% duty       */
#define AD9240_TIM_CCR2     1           /* CH2 compare: triggers 1 tick after update */

/* ---- GPIO port/pin masks ---- */
#define AD9240_GPIO_LO_PORT    GPIOE
#define AD9240_GPIO_LO_PINS    (GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | \
                                GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 | \
                                GPIO_Pin_6)

#define AD9240_GPIO_HI_PORT    GPIOF
#define AD9240_GPIO_HI_PINS    (GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | \
                                GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_8 | \
                                GPIO_Pin_9)                    /* PF10=OTR handled separately */

#define AD9240_GPIO_CLK_PORT   GPIOC
#define AD9240_GPIO_CLK_PIN    GPIO_Pin_6
#define AD9240_GPIO_CLK_AF     GPIO_AF_TIM8

/* ---- Internal state ---- */
static uint32_t g_sample_rate = AD9240_DEFAULT_FS;

/* ===================================================================== *
 *  AD9240_Init — one-time hardware initialisation
 * ===================================================================== */
void AD9240_Init(uint32_t sample_rate_hz)
{
    GPIO_InitTypeDef  gpio;
    TIM_TimeBaseInitTypeDef tim;
    TIM_OCInitTypeDef  oc;
    DMA_InitTypeDef    dma;

    g_sample_rate = sample_rate_hz;

    /* ============  GPIO  ============ */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE | RCC_AHB1Periph_GPIOF | RCC_AHB1Periph_GPIOC, ENABLE);

    /* PE0-PE6: AD9240 D0-D6 (input, no pull) */
    gpio.GPIO_Pin   = AD9240_GPIO_LO_PINS;
    gpio.GPIO_Mode  = GPIO_Mode_IN;
    gpio.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    gpio.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(AD9240_GPIO_LO_PORT, &gpio);

    /* PF1-PF5,PF8-PF9: AD9240 D7-D13 (input, no pull) */
    gpio.GPIO_Pin   = AD9240_GPIO_HI_PINS;
    GPIO_Init(AD9240_GPIO_HI_PORT, &gpio);

    /* PF10: AD9240 OTR overflow flag (input, optional) */
    gpio.GPIO_Pin = GPIO_Pin_10;
    GPIO_Init(AD9240_GPIO_HI_PORT, &gpio);

    /* PC6: TIM8_CH1 -> AD9240 CLK (AF output) */
    gpio.GPIO_Pin   = AD9240_GPIO_CLK_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_AF;
    gpio.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    gpio.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(AD9240_GPIO_CLK_PORT, &gpio);
    GPIO_PinAFConfig(AD9240_GPIO_CLK_PORT, GPIO_PinSource6, AD9240_GPIO_CLK_AF);

    /* ============  TIM8  ============ */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8, ENABLE);

    TIM_DeInit(AD9240_TIM);

    uint16_t psc = (uint16_t)(AD9240_TIM_CLK / (sample_rate_hz * (AD9240_TIM_PERIOD + 1)) - 1);

    tim.TIM_Prescaler         = psc;
    tim.TIM_Period            = AD9240_TIM_PERIOD;
    tim.TIM_ClockDivision     = TIM_CKD_DIV1;
    tim.TIM_CounterMode       = TIM_CounterMode_Up;
    tim.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(AD9240_TIM, &tim);

    /* CH1: PWM output on PC6 (ADC clock) */
    TIM_OCStructInit(&oc);
    oc.TIM_OCMode      = TIM_OCMode_PWM1;
    oc.TIM_OutputState = TIM_OutputState_Enable;
    oc.TIM_Pulse       = AD9240_TIM_CCR1;
    oc.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC1Init(AD9240_TIM, &oc);
    TIM_OC1PreloadConfig(AD9240_TIM, TIM_OCPreload_Enable);

    /* CH2: output compare (no GPIO output, just DMA trigger) */
    oc.TIM_OCMode      = TIM_OCMode_Toggle;    /* toggle to generate DMA event */
    oc.TIM_OutputState = TIM_OutputState_Disable;
    oc.TIM_Pulse       = AD9240_TIM_CCR2;
    TIM_OC2Init(AD9240_TIM, &oc);
    TIM_OC2PreloadConfig(AD9240_TIM, TIM_OCPreload_Enable);

    /* TIM8 master: update event as TRGO (for debug, not needed for DMA) */
    TIM_SelectOutputTrigger(AD9240_TIM, TIM_TRGOSource_Update);

    /* Enable update DMA request (for DMA2_Stream1) */
    TIM_DMACmd(AD9240_TIM, TIM_DMA_Update, ENABLE);

    /* Enable CC2 DMA request (for DMA2_Stream3) */
    TIM_DMACmd(AD9240_TIM, TIM_DMA_CC2, ENABLE);

    /* ============  DMA2  ============ */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);

    /* DMA stream for lower 7 bits (PE0-PE6) — triggered by TIM8_UP */
    DMA_DeInit(AD9240_DMA_LO);
    while (DMA_GetCmdStatus(AD9240_DMA_LO) != DISABLE);

    DMA_StructInit(&dma);
    dma.DMA_Channel            = AD9240_DMA_CH;
    dma.DMA_PeripheralBaseAddr = (uint32_t)&AD9240_GPIO_LO_PORT->IDR;
    dma.DMA_Memory0BaseAddr    = (uint32_t)ad9240_buf_lo;
    dma.DMA_DIR                = DMA_DIR_PeripheralToMemory;
    dma.DMA_BufferSize         = AD9240_BUF_SIZE;
    dma.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    dma.DMA_MemoryDataSize     = DMA_MemoryDataSize_HalfWord;
    dma.DMA_Mode               = DMA_Mode_Normal;
    dma.DMA_Priority           = DMA_Priority_High;
    dma.DMA_FIFOMode           = DMA_FIFOMode_Disable;
    DMA_Init(AD9240_DMA_LO, &dma);

    /* DMA stream for upper bits (PF1-PF5,PF8-PF9) — triggered by TIM8_CC2 */
    DMA_DeInit(AD9240_DMA_HI);
    while (DMA_GetCmdStatus(AD9240_DMA_HI) != DISABLE);

    DMA_StructInit(&dma);
    dma.DMA_Channel            = AD9240_DMA_CH;
    dma.DMA_PeripheralBaseAddr = (uint32_t)&AD9240_GPIO_HI_PORT->IDR;
    dma.DMA_Memory0BaseAddr    = (uint32_t)ad9240_buf_hi;
    dma.DMA_DIR                = DMA_DIR_PeripheralToMemory;
    dma.DMA_BufferSize         = AD9240_BUF_SIZE;
    dma.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    dma.DMA_MemoryDataSize     = DMA_MemoryDataSize_HalfWord;
    dma.DMA_Mode               = DMA_Mode_Normal;
    dma.DMA_Priority           = DMA_Priority_High;
    dma.DMA_FIFOMode           = DMA_FIFOMode_Disable;
    DMA_Init(AD9240_DMA_HI, &dma);

    /* Disable timer until AD9240_Start is called */
    TIM_Cmd(AD9240_TIM, DISABLE);
}

/* ===================================================================== *
 *  AD9240_Start — one-shot acquisition (blocking with timeout)
 * ===================================================================== */
int AD9240_Start(uint32_t sample_rate_hz)
{
    uint16_t psc;
    uint32_t timeout;

    if (sample_rate_hz < 10000 || sample_rate_hz > 10000000)
        return -1;

    psc = (uint16_t)(AD9240_TIM_CLK / (sample_rate_hz * (AD9240_TIM_PERIOD + 1)) - 1);
    if (psc < 1) psc = 1;
    if (psc > 65535) psc = 65535;

    g_sample_rate = (uint32_t)(AD9240_TIM_CLK / ((uint32_t)(psc + 1) * (AD9240_TIM_PERIOD + 1)));

    /* Update timer prescaler for the requested rate */
    TIM_PrescalerConfig(AD9240_TIM, psc, TIM_PSCReloadMode_Immediate);

    /* Clear DMA flags */
    DMA_ClearFlag(AD9240_DMA_LO, DMA_FLAG_TCIF1 | DMA_FLAG_HTIF1 | DMA_FLAG_TEIF1 |
                                  DMA_FLAG_DMEIF1 | DMA_FLAG_FEIF1);
    DMA_ClearFlag(AD9240_DMA_HI, DMA_FLAG_TCIF3 | DMA_FLAG_HTIF3 | DMA_FLAG_TEIF3 |
                                  DMA_FLAG_DMEIF3 | DMA_FLAG_FEIF3);

    /* Set transfer sizes */
    DMA_SetCurrDataCounter(AD9240_DMA_LO, AD9240_BUF_SIZE);
    DMA_SetCurrDataCounter(AD9240_DMA_HI, AD9240_BUF_SIZE);

    /* Enable DMA streams */
    DMA_Cmd(AD9240_DMA_LO, ENABLE);
    DMA_Cmd(AD9240_DMA_HI, ENABLE);

    /* Reset and start timer — ADC clock + DMA triggers begin */
    TIM_SetCounter(AD9240_TIM, 0);
    TIM_Cmd(AD9240_TIM, ENABLE);

    /* Wait for BOTH DMAs to complete */
    timeout = 0;
    while (DMA_GetFlagStatus(AD9240_DMA_LO, AD9240_DMA_LO_FLAG) == RESET ||
           DMA_GetFlagStatus(AD9240_DMA_HI, AD9240_DMA_HI_FLAG) == RESET)
    {
        if (++timeout > 5000000)
        {
            /* Timeout — shut everything down */
            TIM_Cmd(AD9240_TIM, DISABLE);
            DMA_Cmd(AD9240_DMA_LO, DISABLE);
            DMA_Cmd(AD9240_DMA_HI, DISABLE);
            return -2;
        }
    }

    /* Clear flags and stop hardware */
    DMA_ClearFlag(AD9240_DMA_LO, AD9240_DMA_LO_FLAG);
    DMA_ClearFlag(AD9240_DMA_HI, AD9240_DMA_HI_FLAG);

    TIM_Cmd(AD9240_TIM, DISABLE);
    DMA_Cmd(AD9240_DMA_LO, DISABLE);
    DMA_Cmd(AD9240_DMA_HI, DISABLE);

    return 0;
}

/* ===================================================================== *
 *  AD9240_Stop — halt timer + DMA
 * ===================================================================== */
void AD9240_Stop(void)
{
    TIM_Cmd(AD9240_TIM, DISABLE);
    DMA_Cmd(AD9240_DMA_LO, DISABLE);
    DMA_Cmd(AD9240_DMA_HI, DISABLE);
}

/* ===================================================================== *
 *  AD9240_Combine — merge lo/hi buffers into contiguous 14-bit values
 *
 *  Input:  ad9240_buf_lo[count] = PE0-PE6 raw GPIO reads  (bits 6:0)
 *          ad9240_buf_hi[count] = PF1-5 + PF8-9 GPIO reads
 *
 *  Output: dst[count] = 14-bit ADC value (0..16383)
 *
 *  PF1-5 = GPIOF->IDR[5:1], mapped to D8-D12 (bits 4:0 via >>1)
 *  PF8-9 = GPIOF->IDR[9:8], mapped to D13-D14 (bits 6:5 via >>3)
 *  hi_packed = ((hi >> 1) & 0x1F) | ((hi >> 3) & 0x60)
 * ===================================================================== */
void AD9240_Combine(uint16_t *dst, uint16_t count)
{
    uint16_t i;
    for (i = 0; i < count; i++)
    {
        uint16_t lo = ad9240_buf_lo[i] & 0x007F;                      /* PE0-PE6 -> D0-D6     */
        uint16_t hi = ad9240_buf_hi[i];                                /* GPIOF->IDR raw read  */
        uint16_t hi_packed = ((hi >> 1) & 0x001F)   /* PF1-5 -> D8-D12 */
                           | ((hi >> 3) & 0x0060);  /* PF8-9 -> D13-D14 */
        dst[i] = lo | (hi_packed << 7);
    }
}

/* ===================================================================== *
 *  AD9240_GetSampleRate
 * ===================================================================== */
uint32_t AD9240_GetSampleRate(void)
{
    return g_sample_rate;
}
