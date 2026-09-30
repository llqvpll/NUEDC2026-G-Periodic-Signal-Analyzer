/*
*********************************************************************************************************
                                               _01_App
    File       : app_fft.c
    platform   : STM32F407ZG
    function   : FFT spectrum analysis (G-tuned: 2048-pt, configurable Fs)
*********************************************************************************************************
*/

#pragma clang diagnostic ignored "-Wunknown-pragmas"

#include "User_header.h"
#include "TFT_LCD.h"
#include "arm_math.h"
#include "app_fft.h"
#include "Drive_AD9240.h"

/* AD9240 external ADC voltage reference (V).  Default 2.5V.
 * Set via FFT_SetAD9240VRef() before acquisition. */
static float g_ad9240_vref = 2.5f;/* ============================ Configuration ============================ */
/* FFT_SIZE, FFT_NUM_BINS, FFT_DEFAULT_FS come from app_fft.h (2048-point) */

#define FFT_MAX_DB         0.0f
#define FFT_MIN_DB         (-60.0f)

#define FFT_ADC_CHANNEL    ADC_Channel_1          /* PA1 */
#define FFT_ADC_GPIO_PIN   GPIO_Pin_1
#define FFT_ADC_GPIO_PORT  GPIOA

#define FFT_TIM            TIM3                   /* APB1 timer, 84 MHz */
#define FFT_TIM_CLK        84000000UL
#define FFT_TIM_PERIOD     1                      /* ARR = 1 => update every (PSC+1)*2 ticks */

#define FFT_DMA_STREAM     DMA2_Stream0
#define FFT_DMA_CHANNEL    DMA_Channel_0
#define FFT_DMA_FLAG_TC    DMA_FLAG_TCIF0

/* ============================ Static buffers (in BSS, not stack) ====== */

static float   FFT_Input[FFT_SIZE];
static float   FFT_Output[FFT_SIZE];
float          Spectrum[FFT_NUM_BINS];      /* linear magnitude, bins 0..N/2-1 */
static float   Spectrum_dB[FFT_NUM_BINS];   /* normalised dB spectrum          */
static volatile u16 FFT_DMA_Buf[FFT_SIZE];
static arm_rfft_fast_instance_f32 FFT_Instance;
static u8      fft_instance_ready = 0;

/* ============================ Graph geometry (menu-5 display) ========= */

#define FFT_GRAPH_X_MIN    (400 + 16)
#define FFT_GRAPH_X_MAX    (800 - 16)
#define FFT_GRAPH_Y_MIN    (64 + 8 + 32)
#define FFT_GRAPH_Y_MAX    (480 - 32 - 16)
#define FFT_GRAPH_WIDTH    (FFT_GRAPH_X_MAX - FFT_GRAPH_X_MIN)
#define FFT_GRAPH_HEIGHT   (FFT_GRAPH_Y_MAX - FFT_GRAPH_Y_MIN)

/* ============================ Forward declarations ==================== */

static void FFT_ADC_Init(void);
static void FFT_ADC_Prepare(void);
static void FFT_Timer_Init(uint16_t psc);
static void FFT_DMA_Init(void);
static void FFT_Draw_Axis(float nyquist_hz);
static void FFT_Compute(float sample_rate, FFT_Result_t *result);
static void FFT_Display(float nyquist_hz);

/* ===================================================================== *
 *  FFT_SingleShot  ��  one-shot acquisition + compute (for G-measure)
 * ===================================================================== */
int FFT_SingleShot(float sample_rate, FFT_Result_t *result)
{
    uint16_t psc;
    uint32_t timeout;

    if (sample_rate < 1000.0f || sample_rate > 1500000.0f)
        return -1;                          /* out of range */

    /* PSC = TIM_CLK / Fs / (ARR+1) - 1,  ARR = 1 */
    psc = (uint16_t)(FFT_TIM_CLK / sample_rate / (FFT_TIM_PERIOD + 1) - 1);

    /* Initialise CMSIS-DSP RFFT instance only once */
    if (!fft_instance_ready)
    {
        arm_rfft_fast_init_f32(&FFT_Instance, FFT_SIZE);
        fft_instance_ready = 1;
    }

    /* Hardware setup (lightweight re-init, no full RCC reset) */
    FFT_ADC_Prepare();
    FFT_Timer_Init(psc);
    FFT_DMA_Init();

    /* Start acquisition: ADC first, then DMA, then timer trigger */
    ADC_Cmd(ADC1, ENABLE);
    delay_us(10);                    /* let ADC stabilize */
    DMA_Cmd(FFT_DMA_STREAM, ENABLE);
    TIM_Cmd(FFT_TIM, ENABLE);

    /* Wait for DMA transfer-complete (blocking with timeout) */
    timeout = 0;
    while (DMA_GetFlagStatus(FFT_DMA_STREAM, FFT_DMA_FLAG_TC) == RESET)
    {
        if (++timeout > 2000000)
        {
            /* timeout �� something went wrong */
            TIM_Cmd(FFT_TIM, DISABLE);
            DMA_Cmd(FFT_DMA_STREAM, DISABLE);
            ADC_Cmd(ADC1, DISABLE);
            return -2;
        }
    }
    DMA_ClearFlag(FFT_DMA_STREAM, FFT_DMA_FLAG_TC);

    /* Stop hardware immediately �� we only need one block */
    TIM_Cmd(FFT_TIM, DISABLE);
    DMA_Cmd(FFT_DMA_STREAM, DISABLE);
    ADC_Cmd(ADC1, DISABLE);
    ADC_DMACmd(ADC1, DISABLE);

    /* Compute FFT + extract results */
    if (result)
        FFT_Compute(sample_rate, result);

    return 0;
}

/* ===================================================================== *
 *  Start_FFT_Analysis  ��  legacy continuous display loop (menu 5)
 * ===================================================================== */
void Start_FFT_Analysis(void)
{
    float nyquist = FFT_DEFAULT_FS / 2.0f;

    if (!fft_instance_ready)
    {
        arm_rfft_fast_init_f32(&FFT_Instance, FFT_SIZE);
        fft_instance_ready = 1;
    }

    FFT_ADC_Init();
    FFT_Timer_Init((uint16_t)(FFT_TIM_CLK / FFT_DEFAULT_FS / 2 - 1));
    FFT_DMA_Init();

    LCD_Appoint_Clear(FFT_GRAPH_X_MIN, FFT_GRAPH_Y_MIN,
                      FFT_GRAPH_X_MAX, FFT_GRAPH_Y_MAX, Black);
    FFT_Draw_Axis(nyquist);

    ADC_Cmd(ADC1, ENABLE);
    TIM_Cmd(FFT_TIM, ENABLE);
    DMA_Cmd(FFT_DMA_STREAM, ENABLE);

    while (Ps2KeyValue != KeyValue_Back)
    {
        if (DMA_GetFlagStatus(FFT_DMA_STREAM, FFT_DMA_FLAG_TC) != RESET)
        {
            DMA_ClearFlag(FFT_DMA_STREAM, FFT_DMA_FLAG_TC);
            DMA_Cmd(FFT_DMA_STREAM, DISABLE);

            FFT_Compute(FFT_DEFAULT_FS, NULL);
            FFT_Display(nyquist);

            DMA_SetCurrDataCounter(FFT_DMA_STREAM, FFT_SIZE);
            DMA_Cmd(FFT_DMA_STREAM, ENABLE);
        }
        delay_ms(10);
    }

    TIM_Cmd(FFT_TIM, DISABLE);
    DMA_Cmd(FFT_DMA_STREAM, DISABLE);
    ADC_Cmd(ADC1, DISABLE);
    ADC_DMACmd(ADC1, DISABLE);

    LCD_Appoint_Clear(FFT_GRAPH_X_MIN, FFT_GRAPH_Y_MIN,
                      FFT_GRAPH_X_MAX, FFT_GRAPH_Y_MAX, Black);
}

/* ===================================================================== *
 *  Hardware init helpers
 * ===================================================================== */

static void FFT_ADC_Init(void)
{
    GPIO_InitTypeDef        GPIO_InitStructure;
    ADC_CommonInitTypeDef   ADC_CommonInitStructure;
    ADC_InitTypeDef         ADC_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    GPIO_InitStructure.GPIO_Pin  = FFT_ADC_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(FFT_ADC_GPIO_PORT, &GPIO_InitStructure);

    RCC_APB2PeriphResetCmd(RCC_APB2Periph_ADC1, ENABLE);
    RCC_APB2PeriphResetCmd(RCC_APB2Periph_ADC1, DISABLE);

    ADC_CommonInitStructure.ADC_Mode              = ADC_Mode_Independent;
    ADC_CommonInitStructure.ADC_TwoSamplingDelay  = ADC_TwoSamplingDelay_5Cycles;
    ADC_CommonInitStructure.ADC_DMAAccessMode     = ADC_DMAAccessMode_Disabled;
    ADC_CommonInitStructure.ADC_Prescaler         = ADC_Prescaler_Div4;  /* 21 MHz ADC clk */
    ADC_CommonInit(&ADC_CommonInitStructure);

    ADC_InitStructure.ADC_Resolution             = ADC_Resolution_12b;
    ADC_InitStructure.ADC_ScanConvMode           = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode     = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConvEdge   = ADC_ExternalTrigConvEdge_Rising;
    ADC_InitStructure.ADC_ExternalTrigConv       = ADC_ExternalTrigConv_T3_CC1;
    ADC_InitStructure.ADC_DataAlign              = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfConversion        = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    /* 3-cycle sample time: total 15 cycles / 21 MHz = 1.4 MSPS max */
    ADC_RegularChannelConfig(ADC1, FFT_ADC_CHANNEL, 1, ADC_SampleTime_3Cycles);

    ADC_DMARequestAfterLastTransferCmd(ADC1, ENABLE);
    ADC_DMACmd(ADC1, ENABLE);
}

/* Lightweight re-prepare for repeated one-shot acquisitions.
 * Reconfigures ADC trigger to T3_CC1 (matching TIM3 OC1 PWM output).
 * This is essential because User_ADC.c may have set it to T3_TRGO. */
static void FFT_ADC_Prepare(void)
{
    ADC_InitTypeDef adc_init;

    ADC_Cmd(ADC1, DISABLE);
    ADC_DMACmd(ADC1, DISABLE);

    /* Reconfigure ADC trigger source: MUST match TIM3 CC1 */
    ADC_StructInit(&adc_init);
    adc_init.ADC_Resolution            = ADC_Resolution_12b;
    adc_init.ADC_ScanConvMode          = DISABLE;
    adc_init.ADC_ContinuousConvMode    = DISABLE;
    adc_init.ADC_ExternalTrigConvEdge  = ADC_ExternalTrigConvEdge_Rising;
    adc_init.ADC_ExternalTrigConv      = ADC_ExternalTrigConv_T3_CC1;
    adc_init.ADC_DataAlign             = ADC_DataAlign_Right;
    adc_init.ADC_NbrOfConversion       = 1;
    ADC_Init(ADC1, &adc_init);

    /* Clear any leftover ADC flags */
    ADC_ClearFlag(ADC1, ADC_FLAG_EOC | ADC_FLAG_OVR | ADC_FLAG_AWD);

    /* Re-enable DMA in single-shot mode */
    ADC_DMARequestAfterLastTransferCmd(ADC1, ENABLE);
    ADC_DMACmd(ADC1, ENABLE);
}

static void FFT_Timer_Init(uint16_t psc)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruct;
    TIM_OCInitTypeDef       TIM_OCInitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    /* De-init first to clear any stale state from previous acquisition */
    TIM_DeInit(FFT_TIM);

    TIM_TimeBaseInitStruct.TIM_Prescaler     = psc;
    TIM_TimeBaseInitStruct.TIM_Period        = FFT_TIM_PERIOD;
    TIM_TimeBaseInitStruct.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStruct.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(FFT_TIM, &TIM_TimeBaseInitStruct);

    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse       = 1;
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC1Init(FFT_TIM, &TIM_OCInitStructure);
}

static void FFT_DMA_Init(void)
{
    DMA_InitTypeDef DMA_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);

    DMA_DeInit(FFT_DMA_STREAM);
    while (DMA_GetCmdStatus(FFT_DMA_STREAM) != DISABLE);

    DMA_InitStructure.DMA_Channel            = FFT_DMA_CHANNEL;
    DMA_InitStructure.DMA_PeripheralBaseAddr = (u32)&ADC1->DR;
    DMA_InitStructure.DMA_Memory0BaseAddr    = (u32)FFT_DMA_Buf;
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralToMemory;
    DMA_InitStructure.DMA_BufferSize         = FFT_SIZE;
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority           = DMA_Priority_High;
    DMA_InitStructure.DMA_FIFOMode           = DMA_FIFOMode_Disable;
    DMA_InitStructure.DMA_FIFOThreshold      = DMA_FIFOThreshold_HalfFull;
    DMA_InitStructure.DMA_MemoryBurst        = DMA_MemoryBurst_Single;
    DMA_InitStructure.DMA_PeripheralBurst    = DMA_PeripheralBurst_Single;
    DMA_Init(FFT_DMA_STREAM, &DMA_InitStructure);

    /* Clear ALL DMA flags for this stream (TC, HT, TE, DME, FE) */
    DMA_ClearFlag(FFT_DMA_STREAM,
                  DMA_FLAG_TCIF0 | DMA_FLAG_HTIF0 | DMA_FLAG_TEIF0 |
                  DMA_FLAG_DMEIF0 | DMA_FLAG_FEIF0);

    /* FFT uses polled DMA completion.  Make sure the shared DMA2_Stream0
     * interrupt (used by User_ADC.c) cannot clear TC before we see it. */
    DMA_ITConfig(FFT_DMA_STREAM, DMA_IT_TC, DISABLE);
    NVIC_DisableIRQ(DMA2_Stream0_IRQn);
}

/* ===================================================================== *
 *  FFT_Compute  ��  windowing, RFFT, magnitude, peak extraction
 * ===================================================================== */
static void FFT_Compute(float sample_rate, FFT_Result_t *result)
{
    uint16_t i;
    float    max_val = 0.0f;
    float    adc_max = 0.0f, adc_min = 4095.0f;
    u32      adc_sum = 0;
    double   sum_sq_raw = 0;          /* for software RMS */
    float    vref = 3.3f;
    float    adc_scale = vref / 4095.0f;

    /* 1. Hamming window + Vpp / DC from raw samples + zero-crossing count */
    uint16_t zc_count = 0;
    u16      prev_raw = FFT_DMA_Buf[0];
    /* Hysteresis for zero-crossing: require signal to swing past mid +/- margin */
    const int16_t ZC_HYST = 8;  /* ~6.5mV at 3.3V/4095 */
    uint8_t      zc_state = 0;  /* 0=below, 1=above */
    if (prev_raw >= 2048) zc_state = 1;

    for (i = 0; i < FFT_SIZE; i++)
    {
        u16 raw = FFT_DMA_Buf[i];
        float idx_f = (float)i;
        float window = 0.54f - 0.46f * cosf(2.0f * 3.14159265f * idx_f / (float)(FFT_SIZE - 1));
        FFT_Input[i] = ((float)raw - 2048.0f) * window;

        if (raw > adc_max) adc_max = raw;
        if (raw < adc_min) adc_min = raw;
        adc_sum += raw;
        sum_sq_raw += (double)raw * (double)raw;

        /* Zero-crossing with hysteresis: count rising edges only */
        if (zc_state == 0 && raw > 2048 + ZC_HYST)
        {
            zc_count++;
            zc_state = 1;
        }
        else if (zc_state == 1 && raw < 2048 - ZC_HYST)
        {
            zc_state = 0;
        }
        prev_raw = raw;
    }

    /* 2. RFFT */
    arm_rfft_fast_f32(&FFT_Instance, FFT_Input, FFT_Output, 0);

    /* 3. Magnitude spectrum (correct RFFT output layout).
     * arm_rfft_fast_f32 output for N-point real input:
     *   [0] = DC real, [1] = Nyquist real,
     *   [2..N-1] = real[1],imag[1], real[2],imag[2], ..., real[N/2-1],imag[N/2-1]
     * Store magnitudes in Spectrum[0..N/2-1] for DC .. Nyquist.
     */
    Spectrum[0] = fabsf(FFT_Output[0]);                       /* DC */
    for (i = 1; i < FFT_NUM_BINS - 1; i++)                    /* bins 1..N/2-2 */
    {
        float re = FFT_Output[2 * i];
        float im = FFT_Output[2 * i + 1];
        Spectrum[i] = sqrtf(re * re + im * im);
    }
    Spectrum[FFT_NUM_BINS - 1] = fabsf(FFT_Output[1]);        /* Nyquist */

    /* 4. Normalise to dB relative to max bin */
    for (i = 1; i < FFT_NUM_BINS; i++)
    {
        if (Spectrum[i] > max_val)
            max_val = Spectrum[i];
    }
    if (max_val < 1e-10f)
        max_val = 1e-10f;

    {
        float dc_mag = Spectrum[0];
        if (dc_mag > max_val) dc_mag = max_val;
        Spectrum_dB[0] = 20.0f * log10f(dc_mag / max_val);
        if (Spectrum_dB[0] < FFT_MIN_DB) Spectrum_dB[0] = FFT_MIN_DB;
    }

    for (i = 1; i < FFT_NUM_BINS; i++)
    {
        float mag_dB = 20.0f * log10f(Spectrum[i] / max_val);
        if (mag_dB < FFT_MIN_DB)
            mag_dB = FFT_MIN_DB;
        Spectrum_dB[i] = mag_dB;
    }

    /* 5. Extract results if requested */
    if (result)
    {
        uint16_t peak_bin = 1;
        float    peak_mag = 0.0f;

        for (i = 1; i < FFT_NUM_BINS; i++)
        {
            if (Spectrum[i] > peak_mag)
            {
                peak_mag = Spectrum[i];
                peak_bin = i;
            }
        }

        /* Parabolic interpolation for sub-bin accuracy */
        float freq;
        if (peak_bin > 1 && peak_bin < FFT_NUM_BINS - 1)
        {
            float yL = Spectrum[peak_bin - 1];
            float yC = Spectrum[peak_bin];
            float yR = Spectrum[peak_bin + 1];
            float denom = (yL - 2.0f * yC + yR);
            float delta = 0.0f;
            if (fabsf(denom) > 1e-20f)
                delta = 0.5f * (yL - yR) / denom;
            freq = (peak_bin + delta) * sample_rate / (float)FFT_SIZE;
        }
        else
        {
            freq = (float)peak_bin * sample_rate / (float)FFT_SIZE;
        }

        result->peak_freq       = freq;
        result->peak_bin        = peak_bin;
        result->peak_mag        = peak_mag / max_val;
        result->vpp             = (adc_max - adc_min) * adc_scale;
        result->vdc             = (float)adc_sum / (float)FFT_SIZE * adc_scale;

        /* Software true RMS: sqrt(E[x^2] - (E[x])^2) * scale
         * Works for ANY waveform (sine, square, triangle, sawtooth) */
        {
            double mean_raw = (double)adc_sum / (double)FFT_SIZE;
            double var_raw  = sum_sq_raw / (double)FFT_SIZE - mean_raw * mean_raw;
            if (var_raw < 0.0) var_raw = 0.0;
            result->vrms     = sqrtf((float)var_raw) * adc_scale;
        }

        result->freq_resolution = sample_rate / (float)FFT_SIZE;

        /* Zero-crossing frequency estimate (robust backup / cross-check) */
        if (zc_count > 1)
            result->zc_freq = (float)zc_count * sample_rate / (float)(FFT_SIZE - 1);
        else
            result->zc_freq = 0.0f;

        /* Cross-check FFT peak vs zero-crossing frequency.
         * ZC is robust to sampling-rate jitter; FFT bin position is not.
         * If they disagree by >10%, or FFT peak is implausibly low,
         * trust ZC as the fundamental frequency. */
        if (result->zc_freq > 100.0f)
        {
            float zc_bin      = result->zc_freq / result->freq_resolution;
            float diff_ratio  = fabsf((float)peak_bin - zc_bin) / zc_bin;

            if (diff_ratio > 0.10f || peak_bin < 5)
            {
                result->peak_freq = result->zc_freq;
                result->peak_bin  = (uint16_t)(zc_bin + 0.5f);
            }
        }
    }
}

/* ===================================================================== *
 *  Display helpers (menu-5 continuous mode)
 * ===================================================================== */

static void FFT_Draw_Axis(float nyquist_hz)
{
    char  freq_str[24];
    uint8_t i;

    LCD_SetTextColor(White);

    LCD_DrawLine(FFT_GRAPH_X_MIN, FFT_GRAPH_Y_MAX, FFT_GRAPH_WIDTH, 0);
    LCD_DrawLine(FFT_GRAPH_X_MIN, FFT_GRAPH_Y_MIN, FFT_GRAPH_HEIGHT, 1);

    OS_String_Show(FFT_GRAPH_X_MIN, FFT_GRAPH_Y_MAX + 8, 16, 1, "Freq(kHz)");
    OS_String_Show(FFT_GRAPH_X_MIN - 40, FFT_GRAPH_Y_MIN + FFT_GRAPH_HEIGHT / 2, 16, 1, "dB");

    /* X axis: 0 .. nyquist in 5 steps */
    float khz_step = (nyquist_hz / 1000.0f) / 5.0f;
    for (i = 0; i <= 5; i++)
    {
        uint16_t x_pos = FFT_GRAPH_X_MIN + (uint16_t)((float)i / 5.0f * FFT_GRAPH_WIDTH);
        LCD_DrawLine(x_pos, FFT_GRAPH_Y_MAX, 4, 1);
        sprintf(freq_str, "%d", (uint16_t)(i * khz_step));
        OS_String_Show(x_pos - 8, FFT_GRAPH_Y_MAX + 16, 16, 1, freq_str);
    }

    /* Y axis: 0 .. -60 dB */
    for (i = 0; i <= 4; i++)
    {
        uint16_t y_pos = FFT_GRAPH_Y_MAX - (uint16_t)((float)i / 4.0f * FFT_GRAPH_HEIGHT);
        LCD_DrawLine(FFT_GRAPH_X_MIN, y_pos, 4, 0);
        sprintf(freq_str, "%d", -i * 15);
        OS_String_Show(FFT_GRAPH_X_MIN - 32, y_pos - 8, 16, 1, freq_str);
    }
}

static void FFT_Display(float nyquist_hz)
{
    (void)nyquist_hz;  /* parameter reserved for future axis labeling */
    uint16_t x, x_next, bar_width;
    uint16_t color;

    for (uint16_t i = 0; i < FFT_NUM_BINS; i++)
    {
        x      = FFT_GRAPH_X_MIN + (uint32_t)i * FFT_GRAPH_WIDTH / FFT_NUM_BINS;
        x_next = FFT_GRAPH_X_MIN + (uint32_t)(i + 1) * FFT_GRAPH_WIDTH / FFT_NUM_BINS;
        bar_width = x_next - x;
        if (bar_width == 0) bar_width = 1;

        float ratio = (Spectrum_dB[i] - FFT_MIN_DB) / (FFT_MAX_DB - FFT_MIN_DB);
        uint16_t height = (uint16_t)(ratio * FFT_GRAPH_HEIGHT);
        if (height == 0) height = 1;

        if (ratio > 0.8f)
            color = Red;
        else if (ratio > 0.5f)
            color = Yellow;
        else
            color = Green;

        LCD_SetTextColor(color);
        for (uint16_t w = 0; w < bar_width; w++)
        {
            for (uint16_t h = 0; h < height; h++)
            {
                LCD_DrawPoint(x + w, FFT_GRAPH_Y_MAX - 1 - h, color);
            }
        }
    }
}

/* ===================================================================== *
 *  FFT_DrawWaveform  --  draw time-domain waveform from latest ADC data
 *  Call after FFT_SingleShot().  Graph fits in (x_min,y_min)-(x_max,y_max)
 * ===================================================================== */
void FFT_DrawWaveform(uint16_t x_min, uint16_t y_min, uint16_t x_max, uint16_t y_max)
{
    uint16_t graph_w = x_max - x_min;
    uint16_t graph_h = y_max - y_min;
    uint16_t mid_y   = y_min + graph_h / 2;
    uint16_t i;
    uint16_t prev_y  = mid_y;

    /* Clear graph area */
    LCD_Appoint_Clear(x_min, y_min, x_max, y_max, Black);

    /* Draw border */
    LCD_SetTextColor(White);
    LCD_DrawLine(x_min, y_min, graph_w, 0);   /* top    */
    LCD_DrawLine(x_min, y_max, graph_w, 0);   /* bottom */
    LCD_DrawLine(x_min, y_min, graph_h, 1);   /* left   */
    LCD_DrawLine(x_max, y_min, graph_h, 1);   /* right  */

    /* Plot waveform: map 1024 ADC samples to graph width */
    for (i = 0; i < graph_w; i++)
    {
        uint16_t idx = (uint32_t)i * FFT_SIZE / graph_w;
        u16  raw  = FFT_DMA_Buf[idx];
        int  y_off = (int)(raw - 2048) * (graph_h / 2 - 4) / 2048;
        uint16_t y_pos = (uint16_t)(mid_y - y_off);

        if (y_pos <= y_min) y_pos = y_min + 1;
        if (y_pos >= y_max) y_pos = y_max - 1;

        LCD_DrawPoint(x_min + i, y_pos, Green);

        /* Connect to previous sample with vertical line */
        if (i > 0 && y_pos != prev_y)
        {
            LCD_SetTextColor(Green);
            if (y_pos > prev_y)
                LCD_DrawLine(x_min + i, prev_y, y_pos - prev_y, 1);
            else
                LCD_DrawLine(x_min + i, y_pos, prev_y - y_pos, 1);
        }
        prev_y = y_pos;
    }
}

/* ===================================================================== *
 *  FFT_DrawSpectrum  --  draw frequency spectrum (qualitative bar chart)
 *  Call after FFT_SingleShot().  Graph fits in (x_min,y_min)-(x_max,y_max)
 * ===================================================================== */
void FFT_DrawSpectrum(uint16_t x_min, uint16_t y_min, uint16_t x_max, uint16_t y_max, float nyquist_hz)
{
    uint16_t graph_w = x_max - x_min;
    uint16_t graph_h = y_max - y_min;
    uint16_t i;
    uint16_t prev_y = y_max - 1;
    char label[32];

    /* Clear graph area */
    LCD_Appoint_Clear(x_min, y_min, x_max, y_max, Black);

    /* Draw border */
    LCD_SetTextColor(White);
    LCD_DrawLine(x_min, y_min, graph_w, 0);   /* top    */
    LCD_DrawLine(x_min, y_max, graph_w, 0);   /* bottom */
    LCD_DrawLine(x_min, y_min, graph_h, 1);   /* left   */
    LCD_DrawLine(x_max, y_min, graph_h, 1);   /* right  */

    /* Plot spectrum as connected line (skip DC bin 0) */
    for (i = 0; i < graph_w; i++)
    {
        uint16_t bin = (uint32_t)(i + 1) * (FFT_NUM_BINS - 1) / graph_w;
        if (bin >= FFT_NUM_BINS) bin = FFT_NUM_BINS - 1;

        float dB = Spectrum_dB[bin];
        if (dB < FFT_MIN_DB) dB = FFT_MIN_DB;

        float ratio = (dB - FFT_MIN_DB) / (FFT_MAX_DB - FFT_MIN_DB);
        uint16_t y_pos = y_max - 1 - (uint16_t)(ratio * (graph_h - 4));

        if (y_pos <= y_min) y_pos = y_min + 1;
        if (y_pos >= y_max) y_pos = y_max - 1;

        LCD_DrawPoint(x_min + i, y_pos, Yellow);

        if (i > 0 && y_pos != prev_y)
        {
            LCD_SetTextColor(Yellow);
            if (y_pos > prev_y)
                LCD_DrawLine(x_min + i, prev_y, y_pos - prev_y, 1);
            else
                LCD_DrawLine(x_min + i, y_pos, prev_y - y_pos, 1);
        }
        prev_y = y_pos;
    }

    /* Frequency axis labels */
    OS_String_Show(x_min + 2, y_max - 18, 16, 1, "0Hz");
    sprintf(label, "%.0fkHz", nyquist_hz / 1000.0f);
    OS_String_Show(x_max - 56, y_max - 18, 16, 1, label);
    OS_String_Show(x_min + graph_w / 2 - 24, y_max - 18, 16, 1, "Spect");

    /* Mark top harmonic peaks with red vertical lines + labels */
    {
        HarmonicInfo_t harms[MAX_HARMONICS];
        float sr = nyquist_hz * 2.0f;  /* sample_rate = 2 * nyquist */
        uint8_t n = FFT_GetHarmonics(harms, 4, sr);
        uint8_t k;

        /* Diagnostic: show peak count and max spectrum value */
        float spec_max = 0.0f;
        uint16_t si;
        for (si = 1; si < FFT_NUM_BINS; si++)
            if (Spectrum[si] > spec_max) spec_max = Spectrum[si];
        sprintf(label, "Peaks:%d max:%.0f", n, spec_max);
        LCD_SetTextColor(Cyan);
        OS_String_Show(x_min + 2, y_min + 2, 16, 1, label);

        for (k = 0; k < n; k++)
        {
            uint16_t px = x_min + (uint32_t)harms[k].bin * graph_w / FFT_NUM_BINS;
            if (px < x_min + 2) px = x_min + 2;
            if (px > x_max - 2) px = x_max - 2;

            /* Red dashed vertical line at peak */
            LCD_SetTextColor(Red);
            uint16_t yy;
            for (yy = y_min + 2; yy < y_max - 4; yy += 4)
                LCD_DrawPoint(px, yy, Red);

            /* Frequency label above peak */
            if (harms[k].freq >= 1000.0f)
                sprintf(label, "%.1fk", harms[k].freq / 1000.0f);
            else
                sprintf(label, "%.0f", harms[k].freq);
            OS_String_Show(px - 16, y_min + 20 + k * 18, 16, 1, label);
        }
    }
}

/* ===================================================================== *
 *  FFT_DrawTriggeredWaveform  --  display N complete periods (1-5)
 *  Software trigger: find rising zero-crossing, extract N periods.
 *  AC-coupled, Y-axis auto-scaled, with grid + voltage/time labels.
 *  Implements G-tile functional requirement 1.
 * ===================================================================== */
void FFT_DrawTriggeredWaveform(uint16_t x_min, uint16_t y_min,
                               uint16_t x_max, uint16_t y_max,
                               float sample_rate, float freq,
                               uint8_t num_periods, float gain)
{
    uint16_t graph_w = x_max - x_min;
    uint16_t graph_h = y_max - y_min;
    uint16_t mid_y   = y_min + graph_h / 2;
    uint16_t i;
    uint16_t prev_y  = mid_y;
    const float ADC_VREF   = 3.3f;
    const float ADC_MAXVAL = 4095.0f;

    /* Clear graph area */
    LCD_Appoint_Clear(x_min, y_min, x_max, y_max, Black);

    /* Draw border */
    LCD_SetTextColor(White);
    LCD_DrawLine(x_min, y_min, graph_w, 0);   /* top    */
    LCD_DrawLine(x_min, y_max, graph_w, 0);   /* bottom */
    LCD_DrawLine(x_min, y_min, graph_h, 1);   /* left   */
    LCD_DrawLine(x_max, y_min, graph_h, 1);   /* right  */

    /* --- Determine display range --- */
    uint32_t samples_per_period = 0;
    uint32_t total_samples;

    if (freq < 1.0f || freq > sample_rate / 2.0f)
    {
        total_samples = FFT_SIZE;
    }
    else
    {
        samples_per_period = (uint32_t)(sample_rate / freq + 0.5f);
        total_samples = samples_per_period * num_periods;
        if (total_samples > FFT_SIZE) total_samples = FFT_SIZE;
        if (total_samples < 8) total_samples = 8;
    }

    /* --- Software trigger: find first rising zero-crossing --- */
    uint16_t trig_idx = 0;
    uint8_t  found = 0;
    uint16_t search_end = FFT_SIZE - total_samples;
    if (search_end < 2) search_end = 2;

    for (i = 2; i < search_end; i++)
    {
        if (FFT_DMA_Buf[i - 1] < 2048 && FFT_DMA_Buf[i] >= 2048)
        {
            trig_idx = i;
            found = 1;
            break;
        }
    }
    if (!found) trig_idx = 0;

    /* --- First pass: find min/max/mean of displayed samples (AC auto-scale) --- */
    uint16_t smin = 4095, smax = 0;
    uint32_t sum_raw = 0;
    uint32_t cnt = 0;
    uint16_t scan_n = (total_samples < FFT_SIZE) ? total_samples : FFT_SIZE;
    for (i = 0; i < scan_n; i++)
    {
        uint16_t idx = trig_idx + i;
        if (idx >= FFT_SIZE) break;
        u16 raw = FFT_DMA_Buf[idx];
        if (raw < smin) smin = raw;
        if (raw > smax) smax = raw;
        sum_raw += raw;
        cnt++;
    }
    if (cnt == 0) cnt = 1;
    uint16_t smean = (uint16_t)(sum_raw / cnt);

    /* AC amplitude: half of peak-to-peak, with minimum */
    int32_t sig_amp = (int32_t)(smax - smin) / 2;
    if (sig_amp < 4) sig_amp = 4;
    /* Add 25% headroom so waveform doesn't touch edges */
    int32_t y_scale = sig_amp + sig_amp / 4;
    if (y_scale < 1) y_scale = 1;

    /* --- Draw grid (GRAY lines) --- */
    LCD_SetTextColor(GRAY);
    /* Horizontal: center + quarter lines */
    LCD_DrawLine(x_min + 1, mid_y, graph_w - 2, 0);
    LCD_DrawLine(x_min + 1, y_min + graph_h / 4, graph_w - 2, 0);
    LCD_DrawLine(x_min + 1, y_min + graph_h * 3 / 4, graph_w - 2, 0);
    /* Vertical: period boundaries (if known) */
    if (samples_per_period > 0 && num_periods > 1)
    {
        uint16_t p;
        for (p = 1; p < num_periods; p++)
        {
            uint16_t px = x_min + (uint32_t)p * graph_w / num_periods;
            LCD_DrawLine(px, y_min + 1, graph_h - 2, 1);
        }
    }

    /* --- Draw waveform (AC-coupled, auto-scaled, linear interpolation) --- */
    int32_t y_half = graph_h / 2 - 6;   /* 6px margin from top/bottom border */
    for (i = 0; i < graph_w; i++)
    {
        /* Use floating-point index for smooth interpolation */
        float f_idx = (float)i * (float)total_samples / (float)graph_w;
        uint16_t idx0 = trig_idx + (uint16_t)f_idx;
        uint16_t idx1 = trig_idx + (uint16_t)f_idx + 1;
        if (idx0 >= FFT_SIZE) idx0 = FFT_SIZE - 1;
        if (idx1 >= FFT_SIZE) idx1 = FFT_SIZE - 1;
        float frac = f_idx - (float)(uint16_t)f_idx;

        /* Linear interpolation between adjacent samples */
        float raw_f = (float)FFT_DMA_Buf[idx0] * (1.0f - frac)
                    + (float)FFT_DMA_Buf[idx1] * frac;

        /* AC: subtract mean, scale to fit graph */
        int32_t y_off = ((int32_t)raw_f - (int32_t)smean) * y_half / y_scale;
        /* Clamp */
        if (y_off > y_half) y_off = y_half;
        if (y_off < -y_half) y_off = -y_half;

        uint16_t y_pos = (uint16_t)(mid_y - y_off);

        /* Draw 2-pixel thick: y_pos and y_pos-1 (clamped) */
        LCD_DrawPoint(x_min + i, y_pos, Green);
        if (y_pos > y_min + 1)
            LCD_DrawPoint(x_min + i, y_pos - 1, Green);

        if (i > 0 && y_pos != prev_y)
        {
            LCD_SetTextColor(Green);
            if (y_pos > prev_y)
            {
                LCD_DrawLine(x_min + i, prev_y, y_pos - prev_y, 1);
                if (x_min + i + 1 < x_max)
                    LCD_DrawLine(x_min + i + 1, prev_y, y_pos - prev_y, 1);
            }
            else
            {
                LCD_DrawLine(x_min + i, y_pos, prev_y - y_pos, 1);
                if (x_min + i + 1 < x_max)
                    LCD_DrawLine(x_min + i + 1, y_pos, prev_y - y_pos, 1);
            }
        }
        prev_y = y_pos;
    }

    /* --- Y-axis labels (voltage in mV, AC-coupled, divided by chain gain) --- */
    {
        char yl[16];
        float v_max_mv = (float)y_scale * ADC_VREF / ADC_MAXVAL * 1000.0f / gain;
        LCD_SetTextColor(White);
        sprintf(yl, "+%.0fmV", v_max_mv);
        OS_String_Show(x_min + 2, y_min + 2, 16, 1, yl);
        sprintf(yl, "0");
        OS_String_Show(x_min + 2, mid_y - 8, 16, 1, yl);
        sprintf(yl, "-%.0fmV", v_max_mv);
        OS_String_Show(x_min + 2, y_max - 18, 16, 1, yl);
    }

    /* --- X-axis label (periods + total time + frequency) --- */
    {
        char xl[40];
        float total_ms = (float)total_samples / sample_rate * 1000.0f;
        if (freq >= 1000.0f)
            sprintf(xl, "%dT  %.2fms  %.1fkHz", num_periods, total_ms, freq / 1000.0f);
        else if (freq >= 1.0f)
            sprintf(xl, "%dT  %.2fms  %.0fHz", num_periods, total_ms, freq);
        else
            sprintf(xl, "%dT  %.2fms", num_periods, total_ms);
        LCD_SetTextColor(Yellow);
        OS_String_Show(x_min + graph_w / 2 - 60, y_max - 18, 16, 1, xl);

        /* Warning: insufficient samples per period */
        if (samples_per_period > 0 && samples_per_period < 8)
        {
            /* Low sample */
            LCD_SetTextColor(Red);
            OS_String_Show(x_min + 2, y_min + 20, 16, 1,
                           "Low sample!");
        }
    }
}

/* ===================================================================== *
 *  FFT_GetHarmonics  --  extract top-N spectral peaks
 *  Returns local maxima from FFT_Output (linear magnitude).
 *  Implements G-tile functional requirement 4.
 * ===================================================================== */
uint8_t FFT_GetHarmonics(HarmonicInfo_t *harmonics, uint8_t max_count,
                         float sample_rate)
{
    uint16_t i, k;
    uint8_t  count = 0;
    float    global_max = 0.0f;

    if (harmonics == 0 || max_count == 0) return 0;

    /* Find global max (skip DC bin 0 and bin 1) */
    for (i = 2; i < FFT_NUM_BINS - 1; i++)
    {
        if (Spectrum[i] > global_max)
            global_max = Spectrum[i];
    }
    if (global_max < 1e-10f) global_max = 1e-10f;

    /* Scan for local maxima (3-point check for robustness) */
    for (i = 3; i < FFT_NUM_BINS - 2 && count < max_count; i++)
    {
        /* Local maximum check: must be greater than both neighbours */
        if (Spectrum[i] > Spectrum[i - 1] &&
            Spectrum[i] >= Spectrum[i + 1])
        {
            float ratio = Spectrum[i] / global_max;

            /* Only keep peaks above -40 dB (1% of max) */
            if (ratio < 0.01f) continue;

            /* Check not too close to an already-found peak (within 5 bins) */
            uint8_t too_close = 0;
            for (k = 0; k < count; k++)
            {
                int diff = (int)i - (int)harmonics[k].bin;
                if (diff < 0) diff = -diff;
                if (diff < 5)
                {
                    /* Keep the larger one */
                    if (Spectrum[i] > Spectrum[harmonics[k].bin])
                    {
                        harmonics[k].bin      = i;
                        harmonics[k].freq     = (float)i * sample_rate / (float)FFT_SIZE;
                        harmonics[k].mag_norm = ratio;
                        harmonics[k].mag_db   = 20.0f * log10f(ratio);
                    }
                    too_close = 1;
                    break;
                }
            }
            if (!too_close)
            {
                harmonics[count].bin      = i;
                harmonics[count].freq     = (float)i * sample_rate / (float)FFT_SIZE;
                harmonics[count].mag_norm = ratio;
                harmonics[count].mag_db   = 20.0f * log10f(ratio);
                count++;
            }
        }
    }

    /* Fallback: if no local maxima found (e.g. flat-top or noisy spectrum),
     * return the global maximum as the sole peak so the caller always gets
     * at least the fundamental frequency. */
    if (count == 0)
    {
        uint16_t gmax_bin = 2;
        float    gmax_val = 0.0f;
        for (i = 2; i < FFT_NUM_BINS - 1; i++)
        {
            if (Spectrum[i] > gmax_val)
            {
                gmax_val = Spectrum[i];
                gmax_bin = i;
            }
        }
        if (gmax_val > 1e-10f)
        {
            harmonics[0].bin      = gmax_bin;
            harmonics[0].freq     = (float)gmax_bin * sample_rate / (float)FFT_SIZE;
            harmonics[0].mag_norm = 1.0f;
            harmonics[0].mag_db   = 0.0f;
            count = 1;
        }
    }

    /* Sort by magnitude descending (simple selection sort) */
    for (k = 0; k < count - 1; k++)
    {
        uint8_t best = k;
        uint8_t j;
        for (j = k + 1; j < count; j++)
        {
            if (harmonics[j].mag_norm > harmonics[best].mag_norm)
                best = j;
        }
        if (best != k)
        {
            HarmonicInfo_t tmp = harmonics[k];
            harmonics[k] = harmonics[best];
            harmonics[best] = tmp;
        }
    }

    return count;
}

/* ===================================================================== *
 *  AD9240 external 14-bit ADC support
 * ===================================================================== */

void FFT_SetAD9240VRef(float vref)
{
    g_ad9240_vref = vref;
}

int FFT_AD9240_IsReady(void)
{
    return 1;  /* driver is always available once compiled in */
}

int FFT_SingleShot_AD9240(float sample_rate, FFT_Result_t *result)
{
    uint16_t i;
    int ret;
    uint16_t combined[FFT_SIZE];

    /* Init RFFT instance once */
    if (!fft_instance_ready)
    {
        arm_rfft_fast_init_f32(&FFT_Instance, FFT_SIZE);
        fft_instance_ready = 1;
    }

    /* One-shot hardware acquisition via AD9240 */
    ret = AD9240_Start((uint32_t)sample_rate);
    if (ret != 0)
        return ret;

    /* Combine dual-port GPIO reads into 14-bit values */
    AD9240_Combine(combined, FFT_SIZE);

    /* Convert 14-bit (0–16383) to 12-bit (0–4095) for FFT_DMA_Buf.
     * Also zero out any sensor noise below 2 LSBs. */
    {
        uint16_t min_val = 16383, max_val = 0;
        for (i = 0; i < FFT_SIZE; i++)
        {
            if (combined[i] < min_val) min_val = combined[i];
            if (combined[i] > max_val) max_val = combined[i];
        }
        /* Scale to 0–4095 range */
        for (i = 0; i < FFT_SIZE; i++)
        {
            uint16_t val = combined[i];
            /* Right-shift 2 bits: 14b -> 12b */
            val >>= 2;
            FFT_DMA_Buf[i] = val;
        }
    }

    /* Run the standard FFT computation (uses internal ADC VREF = 3.3V).
     * We then correct the voltage outputs for AD9240's VREF. */
    if (result)
        FFT_Compute(sample_rate, result);

    /* Correct voltage scaling: FFT_Compute used 3.3V/4095.
     * AD9240 uses g_ad9240_vref / 4095 (for 12-bit scaled data).
     * Multiply by (g_ad9240_vref / 3.3) to convert. */
    if (result)
    {
        float v_corr = g_ad9240_vref / 3.3f;
        result->vpp  *= v_corr;
        result->vrms *= v_corr;
        result->vdc  *= v_corr;
    }

    return 0;
}

/* ===================================================================== *
 *  FFT_LoadTestData  --  generate synthetic sine wave for algorithm test
 *  Fills FFT_DMA_Buf with a pure sine wave + 1.65V DC offset, then
 *  runs FFT_Compute() to produce all results (Vpp, Vrms, freq, spectrum).
 *  No ADC hardware needed.
 * ===================================================================== */
int FFT_LoadTestData(float freq, float vpp, float sample_rate,
                     FFT_Result_t *result)
{
    uint16_t i;
    float amplitude = vpp / 2.0f;          /* peak voltage     */
    float dc_offset = 1.65f;              /* virtual DC bias   */
    float vref = 3.3f;

    if (freq < 1.0f || freq > sample_rate / 2.0f)
        return -1;

    /* Init FFT instance once.  Force re-init in case FFT_SIZE changed. */
    arm_rfft_fast_init_f32(&FFT_Instance, FFT_SIZE);
    fft_instance_ready = 1;

    /* Generate synthetic sine wave into DMA buffer */
    for (i = 0; i < FFT_SIZE; i++)
    {
        float t = (float)i / sample_rate;
        float voltage = dc_offset + amplitude * sinf(2.0f * 3.14159265f * freq * t);
        int adc_val = (int)(voltage / vref * 4095.0f + 0.5f);
        if (adc_val < 0) adc_val = 0;
        if (adc_val > 4095) adc_val = 4095;
        FFT_DMA_Buf[i] = (uint16_t)adc_val;
    }

    /* Run the same compute path as real ADC data */
    if (result)
        FFT_Compute(sample_rate, result);

    return 0;
}
