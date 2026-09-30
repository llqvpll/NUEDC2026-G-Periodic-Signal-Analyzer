/*
*********************************************************************************************************
                                               _01_App
    File       : app_fft.h
    platform   : STM32F407ZG
    function   : FFT spectrum analysis module interface (G-tuned)
*********************************************************************************************************
*/
#ifndef __APP_FFT_H
#define __APP_FFT_H

#include "User_header.h"

/*
 * FFT point count.
 * 2048-point RFFT: freq_resolution = sample_rate / 2048
 *   - 500 kHz sample -> 244 Hz resolution (meets G-tile <=500 Hz)
 *   - 1   MHz sample -> 488 Hz resolution (meets G-tile <=500 Hz)
 */
#define FFT_SIZE           2048
#define FFT_NUM_BINS       (FFT_SIZE / 2)

/* Default sample rate for u_a mode (signal up to 200 kHz) */
#define FFT_DEFAULT_FS     500000.0f

/* Signal chain voltage gain: OPA847(5x) -> NE5532(1x) -> LPF -> ADC
 * Measured: 100mV input -> ~500mV at ADC.
 * Display divides by this to show the original input signal amplitude. */
#define SIGNAL_CHAIN_GAIN  5.0f

extern float Spectrum[FFT_NUM_BINS];

/* FFT measurement result struct */
typedef struct {
    float peak_freq;        /* detected fundamental frequency (Hz)    */
    uint16_t peak_bin;      /* raw FFT bin index (debug)              */
    float peak_mag;         /* magnitude at peak bin (normalised 0..1)*/
    float vpp;              /* peak-to-peak voltage (V) from raw ADC  */
    float vrms;             /* true RMS (V) from ADC samples           */
    float vdc;              /* DC offset (V)                           */
    float freq_resolution;  /* Hz per bin                              */
    float zc_freq;          /* zero-crossing frequency estimate (Hz)   */
} FFT_Result_t;

/* Start a single FFT acquisition + compute, blocking until done.
 * sample_rate: desired sampling frequency in Hz (e.g. 500000 or 1000000)
 * result:      pointer to receive measurement results
 * Returns 0 on success, non-zero on error.
 */
int  FFT_SingleShot(float sample_rate, FFT_Result_t *result);

/* Draw real-time waveform from the latest ADC samples.
 * Call after FFT_SingleShot() to visualize the time-domain signal.
 * Graph area: (x_min, y_min) to (x_max, y_max). */
void FFT_DrawWaveform(uint16_t x_min, uint16_t y_min, uint16_t x_max, uint16_t y_max);

/* Draw frequency spectrum from the latest FFT computation.
 * Call after FFT_SingleShot().  Shows qualitative spectral lines.
 * nyquist_hz: half the sample rate, for axis labeling. */
void FFT_DrawSpectrum(uint16_t x_min, uint16_t y_min, uint16_t x_max, uint16_t y_max, float nyquist_hz);

/* Draw triggered waveform showing exactly 1-5 complete periods.
 * Uses software trigger: finds rising zero-crossing, then extracts
 * num_periods cycles from the ADC buffer.
 * freq: signal frequency (Hz), from comparator or FFT.
 * sample_rate: ADC sampling rate (Hz).
 * num_periods: 1-5 (per G-tile functional requirement 1).
 * gain: signal chain gain for Y-axis voltage labels (1.0 = no correction).
 * Graph area: (x_min, y_min) to (x_max, y_max). */
void FFT_DrawTriggeredWaveform(uint16_t x_min, uint16_t y_min,
                               uint16_t x_max, uint16_t y_max,
                               float sample_rate, float freq,
                               uint8_t num_periods, float gain);

/* Harmonic / spectral component information */
#define MAX_HARMONICS  5
typedef struct {
    float    freq;       /* frequency (Hz)                              */
    float    mag_norm;   /* normalised magnitude 0..1 (rel to peak)     */
    float    mag_db;     /* magnitude in dB (rel to peak, 0 = peak)     */
    uint16_t bin;        /* FFT bin index                                */
} HarmonicInfo_t;

/* Extract top-N harmonic peaks from the latest FFT computation.
 * Returns number of harmonics found (0..max_count).
 * Call after FFT_SingleShot(). */
uint8_t FFT_GetHarmonics(HarmonicInfo_t *harmonics, uint8_t max_count,
                         float sample_rate);

/* Legacy entry: continuous FFT display loop (menu 5).
 * Uses FFT_DEFAULT_FS.  Press Back to exit.
 */
void Start_FFT_Analysis(void);

/* Load synthetic test data into ADC buffer and run FFT.
 * Bypasses real ADC for algorithm verification.
 * freq: signal frequency (Hz), vpp: peak-to-peak voltage (V)
 * sample_rate: virtual sampling rate (Hz)
 * Fills result struct same as FFT_SingleShot. */
int FFT_LoadTestData(float freq, float vpp, float sample_rate,
                     FFT_Result_t *result);

/* ---- AD9240 external 14-bit ADC support ---- */

/* One-shot acquisition using AD9240 instead of internal ADC.
 * sample_rate: AD9240 clock frequency (Hz), e.g. 500000 or 1000000.
 * result: populated with amplitude/frequency/spectrum data.
 * Returns 0 on success, non-zero on error. */
int  FFT_SingleShot_AD9240(float sample_rate, FFT_Result_t *result);

/* AD9240 voltage reference. Default 2.5V; set before calling
 * FFT_SingleShot_AD9240 if your module uses a different VREF. */
void FFT_SetAD9240VRef(float vref);

/* Check if AD9240 driver is initialised (hardware present). */
int  FFT_AD9240_IsReady(void);

#endif
