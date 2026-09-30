/*
*********************************************************************************************************
                                               _01_App
    File       : app_measure.h
    platform   : STM32F407ZG
    function   : G-topic periodic signal measurement (fusion: comparator + FFT software RMS)
*********************************************************************************************************
*/
#ifndef __APP_MEASURE_H
#define __APP_MEASURE_H

#include "User_header.h"
#include "app_fft.h"

/* Measurement modes — maps to G-topic signal types */
typedef enum {
    MEASURE_MODE_UA = 0,    /* u_a: 10kHz-200kHz, 100-250mVpp, Fs=500kHz  */
    MEASURE_MODE_UB,        /* u_b: 10kHz-500kHz, 50-250mVpp,  Fs=1MHz    */
    MEASURE_MODE_UB_J,      /* u_b+u_J: u_b + 200mVpp interference >=1MHz */
    MEASURE_MODE_COUNT
} MeasureMode_t;

/* All measurement results from one acquisition cycle */
typedef struct {
    /* Frequency */
    float freq_comparator;    /* from TIM1 input capture (Hz)             */
    float freq_fft;           /* from FFT peak bin + interpolation (Hz)   */
    /* Amplitude */
    float vpp;                /* peak-to-peak from ADC samples (V)        */
    float vrms_true;          /* true RMS from ADC samples (V), any wave  */
    float vrms_calc;          /* calculated RMS from Vpp (sine only) (V)  */
    float vdc;                /* DC offset (V)                             */
    /* Quality */
    float freq_resolution;    /* FFT bin width (Hz)                       */
    float form_factor;        /* Vrms / (Vpp/2) — 0.707 for pure sine     */
    uint16_t peak_bin;        /* raw FFT peak bin (debug)                 */
    float zc_freq;            /* zero-crossing frequency estimate (Hz)    */
    /* Comparator debug (for "no signal" diagnosis) */
    uint32_t cmp_sig_count;   /* signal edge count in last gate           */
    uint8_t  cmp_done;        /* 1=gate finished, 0=still running         */
} MeasureResult_t;

/* Mode metadata: sample rate, description */
typedef struct {
    float      sample_rate;   /* FFT sampling frequency (Hz)              */
    const char *name;         /* short name for display                   */
    const char *desc;         /* longer description                       */
} MeasureModeInfo_t;

/* Get mode info for a given mode */
const MeasureModeInfo_t* Measure_GetModeInfo(MeasureMode_t mode);

/* Start the G-topic measurement loop (blocking, press Back to exit) */
void Start_G_Measure(void);

#endif
