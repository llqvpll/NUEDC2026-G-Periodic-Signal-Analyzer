/*
*********************************************************************************************************
                                               _03_Drive
    File       : Drive_AD9240.h
    platform   : STM32F407ZG
    function   : AD9240 14-bit parallel ADC driver (TIM8 dual-DMA capture)
*********************************************************************************************************
*/

#ifndef __DRIVE_AD9240_H
#define __DRIVE_AD9240_H

#include "User_header.h"

/* Buffer size: must match FFT_SIZE (2048) */
#define AD9240_BUF_SIZE   2048

/* Default sample rate (Hz) */
#define AD9240_DEFAULT_FS 500000UL

/*
 * External data buffers — filled by DMA, read by FFT module.
 * ad9240_buf_lo[] : PE0-PE6  -> bits D0-D6
 * ad9240_buf_hi[] : PF1-5 + PF8-9 -> bits D7-D13
 * Combine: hi_packed = ((hi>>1)&0x1F) | ((hi>>3)&0x60)
 */
extern volatile uint16_t ad9240_buf_lo[AD9240_BUF_SIZE];
extern volatile uint16_t ad9240_buf_hi[AD9240_BUF_SIZE];

/* Pin mapping summary (AD9240 module silkscreen → STM32F407)
 *
 *   模块左侧:  +5V | GND | CLK→PC6 | D13→PF8 | D11→PF4 | D9→PF2 | D7→PE6 | D5→PE4 | D3→PE2 | D1→PE0
 *   模块右侧:  +5V | GND | D14→PF9 | D12→PF5 | D10→PF3 | D8→PF1 | D6→PE5 | D4→PE3 | D2→PE1 | OR→PF10
 *
 *   CLK:      PC6  (TIM8_CH1, PWM out)
 *   D0-D6:    PE0-PE6  (lo port, DMA reads GPIOE->IDR)
 *   D7-D13:   PF1-PF5 + PF8-PF9  (hi port, DMA reads GPIOF->IDR, packed in Combine)
 *   OTR:      PF10 (optional)
 *   NOTE: no PF0/PF6/PF7 on this board, skipped
 *
 * DMA channels
 *   DMA2_Stream1 Ch7 (TIM8_UP)  -> reads GPIOE->IDR -> ad9240_buf_lo
 *   DMA2_Stream3 Ch7 (TIM8_CH2) -> reads GPIOF->IDR -> ad9240_buf_hi
 *
 * Voltage reference: AD9240 VREF = 2.5V (typical)
 *   14-bit: V = adc_val / 16383 * 2.5V  (single-ended, 0-2.5V range)
 *
 * Usage: press [7] in app_measure G-tile to toggle AD9240 mode.
 */

/* ---- Public API ---- */

/* Init hardware once: GPIO, TIM8, DMA. Must call before AD9240_Start. */
void AD9240_Init(uint32_t sample_rate_hz);

/* Start one-shot acquisition. DMA fills buffers, blocks until complete.
 * Returns 0 on success, -1 on invalid params, -2 on timeout. */
int  AD9240_Start(uint32_t sample_rate_hz);

/* Stop hardware (disable timer + DMA). Call after reading buffers. */
void AD9240_Stop(void);

/* Combine lo/hi buffers into 14-bit ADC values in dst[0..count-1].
 * Call after AD9240_Start completes successfully. */
void AD9240_Combine(uint16_t *dst, uint16_t count);

/* Get the actual configured sample rate (may differ from requested). */
uint32_t AD9240_GetSampleRate(void);

#endif
