/*
*********************************************************************************************************
                                               _01_App
    File       : app_measure.c
    platform   : STM32F407ZG
    function   : G-topic periodic signal measurement
                 Fusion: Comparator(freq) + FFT(Vpp/Vrms/freq/spectrum)
*********************************************************************************************************
*/

#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Winvalid-source-encoding"

#include "User_header.h"
#include "User.h"
#include "app_measure.h"
#include "app_fft.h"
#include "Drive_Comparator.h"
#include "Drive_AD9240.h"
#include "TFT_LCD.h"
#include <math.h>

/* MenuSign is defined in User.c */
extern volatile uint8_t MenuSign;

/* ============================ Mode info table ========================= */

static const MeasureModeInfo_t s_mode_info[MEASURE_MODE_COUNT] = {
    {  500000.0f, "u_a",    "10k-200kHz 100-250mV" },
    { 1000000.0f, "u_b",    "10k-500kHz 50-250mV" },
    { 1000000.0f, "u_b+J",  "u_b+200mV noise >=1MHz" },
};

const MeasureModeInfo_t* Measure_GetModeInfo(MeasureMode_t mode)
{
    if (mode < MEASURE_MODE_COUNT)
        return &s_mode_info[mode];
    return &s_mode_info[0];
}

/* ============================ Display helpers ========================= */

/* Split-screen geometry (800x480 LCD)
 * Left panel: numeric data, Right panel: waveform graph */
#define G_MARGIN_X     16
#define G_TITLE_Y      8
#define G_ROW_H        42
#define G_ROW_Y(n)     (56 + G_ROW_H * (n))
#define G_RIGHT_EDGE   (800 - 16)
#define G_GRAPH_X      400
#define G_GRAPH_Y      68
#define G_GRAPH_RIGHT  784
#define G_GRAPH_BOT    432

/* Graph view mode: waveform (time domain) or spectrum (frequency domain) */
typedef enum { VIEW_WAVEFORM = 0, VIEW_SPECTRUM } G_ViewMode_t;
static G_ViewMode_t s_view = VIEW_WAVEFORM;
static uint8_t      s_periods = 3;   /* 1-5 complete periods, default 3 (Req.1) */
static uint8_t      s_test_mode = 0;  /* 0=real ADC, 1=synthetic test data */
static uint8_t      s_use_ad9240 = 0; /* 0=internal ADC, 1=AD9240 external */
static uint8_t      s_freeze = 0;       /* 1=display frozen (hold current frame) */

/* 4-frame sliding average ring buffers */
#define AVG_FRAMES 4
static float s_vpp_hist[AVG_FRAMES]    = {0};
static float s_vrms_hist[AVG_FRAMES]   = {0};
static float s_freq_hist[AVG_FRAMES]   = {0};
static uint8_t s_avg_idx = 0;
static uint8_t s_avg_cnt = 0;

/* Test data parameters (111kHz, 66mVpp sine wave) */
#define TEST_FREQ_HZ     111000.0f
#define TEST_VPP_V       0.066f

static void G_ClearScreen(void)
{
    LCD_Clear(Black);
    OS_BackColor_Set(Black);   /* match OS mode=1 text background */
}

static void G_ShowHeader(MeasureMode_t mode)
{
    const MeasureModeInfo_t *info = Measure_GetModeInfo(mode);
    char buf[64];

    /* Title */
    if (s_test_mode)
    {
        /* test data */
        sprintf(buf, "G [TEST] 111kHz 66mV sine");
        OS_String_Show(G_MARGIN_X, G_TITLE_Y, 24, 1, buf);
        /* Red test label on right side of title */
        OS_TextColor_Set(Red);
        OS_String_Show(600, G_TITLE_Y, 24, 1, "[TEST]");
        OS_TextColor_Set(White);
    }
    else if (s_use_ad9240)
    {
        sprintf(buf, "G Periodic Sig [AD9240] %s %s", info->name, info->desc);
        OS_String_Show(G_MARGIN_X, G_TITLE_Y, 24, 1, buf);
        OS_TextColor_Set(Cyan);
        OS_String_Show(560, G_TITLE_Y, 24, 1, "[AD9240]");
        OS_TextColor_Set(White);
    }
    else
    {
        /* G Periodic Signal Measure */
        sprintf(buf, "G Periodic Sig  %s %s", info->name, info->desc);
        OS_String_Show(G_MARGIN_X, G_TITLE_Y, 24, 1, buf);
    }

    /* Separator under header */
    LCD_Appoint_Clear(G_MARGIN_X, 44, G_RIGHT_EDGE, 48, White);

    /* Graph label */
    if (s_view == VIEW_WAVEFORM)
    {
        char wl[24];
        /* Wave */
        sprintf(wl, "Wave %dT", s_periods);
        OS_String_Show(G_GRAPH_X, 48, 16, 1, wl);
    }
    else
        /* Spectrum */
        OS_String_Show(G_GRAPH_X, 48, 16, 1, "Spect");

    /* Separator above footer */
    LCD_Appoint_Clear(G_MARGIN_X, 436, G_RIGHT_EDGE, 440, White);

    /* Key hints */
    OS_String_Show(G_MARGIN_X, 446, 16, 1,
                   "[0/4]Test [1]ua [5]Swap [6]Cyc [7]AD9240 [8]Hold [Back]Exit");
}

static void G_ShowResult(const MeasureResult_t *r, MeasureMode_t mode)
{
    const MeasureModeInfo_t *info = Measure_GetModeInfo(mode);

    /* Row 1: Comparator frequency */
    /* CMP freq */
    if (r->freq_comparator > 0.5f)
    {
        if (r->freq_comparator >= 1000000.0f)
            OS_Num_Show(G_MARGIN_X, G_ROW_Y(1), 24, 1,
                        r->freq_comparator / 1000000.0f,
                        "CMP freq: %9.4f MHz ");
        else
            OS_Num_Show(G_MARGIN_X, G_ROW_Y(1), 24, 1,
                        r->freq_comparator / 1000.0f,
                        "CMP freq: %9.3f kHz ");
    }
    else
    {
        /* no signal
         * Show diagnostic: sig_count / done status */
        char dbg[48];
        /* CMP no signal:
           比较器频率: 无信号 cnt=XXX d=X */
        sprintf(dbg, "CMP: no sig cnt=%lu d=%u",
                r->cmp_sig_count, r->cmp_done);
        OS_String_Show(G_MARGIN_X, G_ROW_Y(1), 16, 1, dbg);
    }

    /* Row 2: FFT peak frequency */
    /* FFT freq */
    if (r->freq_fft > 0.5f && r->freq_fft < info->sample_rate / 2.0f)
    {
        OS_Num_Show(G_MARGIN_X, G_ROW_Y(2), 24, 1,
                    r->freq_fft / 1000.0f,
                    "FFT freq:  %9.3f kHz ");
    }
    else
    {
        /* no peak */
        OS_String_Show(G_MARGIN_X, G_ROW_Y(2), 24, 1,
                       "FFT freq:  --- no peak ---    ");
    }

    /* Row 3: Vpp */
    /* Vpp */
    OS_Num_Show(G_MARGIN_X, G_ROW_Y(3), 24, 1,
                r->vpp * 1000.0f,
                "Vpp:        %8.1f mV  ");

    /* Row 4: Vrms true (software computed from ADC samples) */
    /* Vrms true */
    OS_Num_Show(G_MARGIN_X, G_ROW_Y(4), 24, 1,
                r->vrms_true * 1000.0f,
                "Vrms true:  %8.1f mV  ");

    /* Row 5: Vrms calculated (sine assumption) */
    /* Vrms sine */
    OS_Num_Show(G_MARGIN_X, G_ROW_Y(5), 24, 1,
                r->vrms_calc * 1000.0f,
                "Vrms sine:  %8.1f mV  ");

    /* Row 6: DC offset */
    /* DC offset */
    OS_Num_Show(G_MARGIN_X, G_ROW_Y(6), 24, 1,
                r->vdc,
                "DC offset:  %8.4f V   ");

    /* Row 7: Form factor Kf = Vrms / (Vpp/2) */
    /* Crest factor */
    OS_Num_Show(G_MARGIN_X, G_ROW_Y(7), 24, 1,
                r->form_factor,
                "Crest fact: %8.4f    ");

    /* Row 8: Zero-crossing freq + raw peak bin (debug) */
    /* ZC freq */
    char bin_buf[48];
    if (r->zc_freq > 0.5f)
        sprintf(bin_buf, "ZC freq: %6.1fk  Bin:%4d ", r->zc_freq / 1000.0f, r->peak_bin);
    else
        sprintf(bin_buf, "ZC freq:---      Bin:%4d ", r->peak_bin);
    OS_String_Show(G_MARGIN_X, G_ROW_Y(8), 24, 1, bin_buf);
}

/* --- Harmonic component list (G-tile Req.4: display spectral amplitudes) - */
static void G_ShowHarmonicList(uint16_t x_min, uint16_t y_min,
                               uint16_t x_max, uint16_t y_max,
                               float sample_rate)
{
    HarmonicInfo_t harms[MAX_HARMONICS];
    uint8_t n, k;
    char buf[48];

    /* Clear area */
    LCD_Appoint_Clear(x_min, y_min, x_max, y_max, Black);

    /* Border */
    LCD_SetTextColor(White);
    LCD_DrawLine(x_min, y_min, x_max - x_min, 0);
    LCD_DrawLine(x_min, y_max, x_max - x_min, 0);

    /* Title */
    /* Harmonics */
    OS_String_Show(x_min + 4, y_min + 2, 16, 1, "Harmonics:");

    /* Get top harmonics */
    n = FFT_GetHarmonics(harms, MAX_HARMONICS, sample_rate);

    if (n == 0)
    {
        /* No peak found */
        OS_String_Show(x_min + 4, y_min + 22, 16, 1, "No peak found");
        return;
    }

    /* List each harmonic: "H1: 100.0kHz  -3.2dB" */
    for (k = 0; k < n; k++)
    {
        uint16_t y = y_min + 20 + k * 18;
        if (y + 16 > y_max) break;

        if (harms[k].freq >= 1000.0f)
            sprintf(buf, "H%d: %.2fkHz  %.1fdB",
                    k + 1, harms[k].freq / 1000.0f, harms[k].mag_db);
        else
            sprintf(buf, "H%d: %.0fHz  %.1fdB",
                    k + 1, harms[k].freq, harms[k].mag_db);

        /* Highlight fundamental (H1) in yellow, others in cyan/white */
        if (k == 0)
            OS_String_Show(x_min + 4, y, 16, 1, buf);
        else
            OS_String_Show(x_min + 4, y, 16, 1, buf);
    }

    /* THD: sqrt(H2^2 + ... + Hn^2) / H1 * 100% */
    if (n >= 2)
    {
        float thd_sum_sq = 0.0f;
        for (k = 1; k < n; k++)
            thd_sum_sq += harms[k].mag_norm * harms[k].mag_norm;
        float thd = sqrtf(thd_sum_sq) / harms[0].mag_norm * 100.0f;

        uint16_t thd_y = y_min + 20 + n * 18;
        if (thd_y + 16 <= y_max)
        {
            OS_TextColor_Set(Yellow);
            sprintf(buf, "THD: %.1f%%", thd);
            OS_String_Show(x_min + 4, thd_y, 16, 1, buf);
            OS_TextColor_Set(White);
        }
    }
}

/* ============================ Main measurement loop =================== */

void Start_G_Measure(void)
{
    MeasureMode_t   mode = MEASURE_MODE_UA;
    const MeasureModeInfo_t *info;
    MeasureResult_t result;
    FFT_Result_t    fft_res;
    float           freq;
    uint16_t        wait_cnt;
    u8              need_redraw = 1;

    /* Clear entire screen (full-screen mode) */
    G_ClearScreen();
    G_ShowHeader(mode);

    /*
     * Disable DMA2_Stream4 — Drive_ADC's ADC1_Init() set it up in
     * circular mode for ADC1.  FFT uses DMA2_Stream0 for ADC1.
     * Both configured for ADC1 = conflict if both enabled.
     */
    DMA_Cmd(DMA2_Stream4, DISABLE);

    /* Init AD9240 external ADC (idle until toggled on) */
    AD9240_Init(500000);

    /* Start comparator (TIM1 input capture + TIM2 gate, 100ms) */
    Comparator_Init();

    /* Wait for first gate to complete */
    delay_ms(110);
    freq = Get_Frequency();
    Comparator_Restart();

    while (Ps2KeyValue != KeyValue_Back)
    {
        info = Measure_GetModeInfo(mode);

        /* --- Mode switching --- */
        if (Ps2KeyValue == KeyValue_0 || Ps2KeyValue == KeyValue_4)
        {
            /* [0] or [4] toggles synthetic test data mode */
            s_test_mode = !s_test_mode;
            Ps2KeyValue = KeyValue_Null;
            need_redraw = 1;
        }
        else if (Ps2KeyValue == KeyValue_1)
        {
            mode = MEASURE_MODE_UA;
            Ps2KeyValue = KeyValue_Null;
            need_redraw = 1;
        }
        else if (Ps2KeyValue == KeyValue_2)
        {
            mode = MEASURE_MODE_UB;
            Ps2KeyValue = KeyValue_Null;
            need_redraw = 1;
        }
        else if (Ps2KeyValue == KeyValue_3)
        {
            mode = MEASURE_MODE_UB_J;
            Ps2KeyValue = KeyValue_Null;
            need_redraw = 1;
        }
        else if (Ps2KeyValue == KeyValue_5)
        {
            s_view = (s_view == VIEW_WAVEFORM) ? VIEW_SPECTRUM : VIEW_WAVEFORM;
            Ps2KeyValue = KeyValue_Null;
            need_redraw = 1;
        }
        else if (Ps2KeyValue == KeyValue_6)
        {
            /* Cycle period count: 1->2->3->4->5->1 (G-tile Req.1: 1T & 3T) */
            s_periods++;
            if (s_periods > 5) s_periods = 1;
            Ps2KeyValue = KeyValue_Null;
            need_redraw = 1;
        }
        else if (Ps2KeyValue == KeyValue_7)
        {
            /* Toggle AD9240 external 14-bit ADC */
            s_use_ad9240 = !s_use_ad9240;
            Ps2KeyValue = KeyValue_Null;
            need_redraw = 1;
        }
        else if (Ps2KeyValue == KeyValue_8)
        {
            s_freeze = !s_freeze;
            Ps2KeyValue = KeyValue_Null;
            need_redraw = 1;
        }

        if (need_redraw)
        {
            G_ClearScreen();
            G_ShowHeader(mode);
            /* Show freeze indicator */
            if (s_freeze)
            {
                OS_TextColor_Set(Yellow);
                OS_String_Show(G_GRAPH_X, G_GRAPH_Y + 4, 24, 1, "[HOLD]");
                OS_TextColor_Set(White);
            }
            s_avg_cnt = 0;  /* reset sliding average on any mode/view change */
            need_redraw = 0;
        }

        /* Freeze: skip acquisition, just keep displaying last frame */
        if (s_freeze)
        {
            delay_ms(50);
            continue;
        }

        /* --- Data acquisition: real ADC or synthetic test data --- */
        int fft_ret = 0;
        if (s_test_mode)
            FFT_LoadTestData(TEST_FREQ_HZ, TEST_VPP_V,
                             info->sample_rate, &fft_res);
        else if (s_use_ad9240)
            fft_ret = FFT_SingleShot_AD9240(info->sample_rate, &fft_res);
        else
            fft_ret = FFT_SingleShot(info->sample_rate, &fft_res);

        /* If real ADC acquisition failed, show error and skip display */
        if (fft_ret != 0 && !s_test_mode)
        {
            char err_buf[48];
            /* acquire fail */
            sprintf(err_buf, "ADC acq fail err=%d  ", fft_ret);
            OS_String_Show(G_MARGIN_X, G_ROW_Y(2), 24, 1, err_buf);
            /* Show comparator freq if available */
            if (freq > 0.5f)
            {
                if (freq >= 1000000.0f)
                    OS_Num_Show(G_MARGIN_X, G_ROW_Y(1), 24, 1,
                                freq / 1000000.0f,
                                "CMP freq: %9.4f MHz ");
                else
                    OS_Num_Show(G_MARGIN_X, G_ROW_Y(1), 24, 1,
                                freq / 1000.0f,
                                "CMP freq: %9.3f kHz ");
            }
            else
            {
                u32 sc; u8 dn; u8 gs;
                Comparator_GetDebug(&sc, &dn, &gs);
                sprintf(err_buf, "CMP: no sig cnt=%lu d=%d  ",
                        (unsigned long)sc, dn);
                OS_String_Show(G_MARGIN_X, G_ROW_Y(1), 24, 1, err_buf);
            }
            delay_ms(50);
            if (Ps2KeyValue == KeyValue_Back) break;
            continue;
        }

        /* --- Assemble results --- *
         * In real ADC mode, divide amplitude by SIGNAL_CHAIN_GAIN to show  *
         * the original input signal amplitude (not the amplified ADC level).*
         * Test mode data already represents input-level amplitude.          *
         * AD9240: no external amplifier, gain=1.0; VREF handled by FFT.     */
        float volt_scale;
        if (s_test_mode || s_use_ad9240)
            volt_scale = 1.0f;
        else
            volt_scale = SIGNAL_CHAIN_GAIN;

        result.freq_comparator = s_test_mode ? 0.0f : freq;
        /* Comparator debug info */
        if (!s_test_mode)
        {
            u32 sc; u8 dn; u8 gs;
            Comparator_GetDebug(&sc, &dn, &gs);
            result.cmp_sig_count = sc;
            result.cmp_done      = dn;
        }
        else
        {
            result.cmp_sig_count = 0;
            result.cmp_done      = 0;
        }
    result.freq_fft        = fft_res.peak_freq;
    result.peak_bin        = fft_res.peak_bin;
    result.vpp             = fft_res.vpp / volt_scale;
        result.vrms_true       = fft_res.vrms / volt_scale;
        result.vrms_calc       = fft_res.vpp / (2.0f * 1.41421356f) / volt_scale;
        result.vdc             = fft_res.vdc;
        result.freq_resolution = fft_res.freq_resolution;
        result.zc_freq         = fft_res.zc_freq;
        result.form_factor     = (fft_res.vpp > 0.001f)
                                  ? (fft_res.vrms / (fft_res.vpp / 2.0f))
                                  : 0.0f;

        /* --- 4-frame sliding average smooth --- */
        {
            uint8_t ai = s_avg_idx;
            s_vpp_hist[ai]  = result.vpp;
            s_vrms_hist[ai] = result.vrms_true;
            s_freq_hist[ai] = (result.freq_comparator > 0.5f)
                              ? result.freq_comparator : result.freq_fft;

            s_avg_idx = (ai + 1) % AVG_FRAMES;
            if (s_avg_cnt < AVG_FRAMES) s_avg_cnt++;

            if (s_avg_cnt >= 2)  /* apply smoothing from 2nd frame onward */
            {
                float sum_vpp = 0, sum_vrms = 0, sum_freq = 0;
                uint8_t j;
                for (j = 0; j < s_avg_cnt; j++)
                {
                    sum_vpp  += s_vpp_hist[j];
                    sum_vrms += s_vrms_hist[j];
                    sum_freq += s_freq_hist[j];
                }
                result.vpp       = sum_vpp  / s_avg_cnt;
                result.vrms_true = sum_vrms / s_avg_cnt;
                result.vrms_calc = result.vpp / (2.0f * 1.41421356f);
                /* Smooth displayed frequency */
                if (s_avg_cnt >= 4)
                    result.freq_fft = sum_freq / s_avg_cnt;
            }
        }

        /* --- Display --- */
        G_ShowResult(&result, mode);

        /* --- Real-time graph (waveform or spectrum) --- */
        if (s_view == VIEW_WAVEFORM)
        {
            /* Triggered waveform: use comparator freq if valid, else FFT */
            float disp_freq = (!s_test_mode && freq > 0.5f) ? freq : fft_res.peak_freq;
            FFT_DrawTriggeredWaveform(G_GRAPH_X, G_GRAPH_Y, G_GRAPH_RIGHT, G_GRAPH_BOT,
                                      info->sample_rate, disp_freq, s_periods,
                                      s_test_mode ? 1.0f : (s_use_ad9240 ? 1.0f : SIGNAL_CHAIN_GAIN));
        }
        else
        {
            /* Spectrum view: harmonic list on top, spectrum graph below */
            G_ShowHarmonicList(G_GRAPH_X, G_GRAPH_Y, G_GRAPH_RIGHT,
                               G_GRAPH_Y + 116, info->sample_rate);
            FFT_DrawSpectrum(G_GRAPH_X, G_GRAPH_Y + 120, G_GRAPH_RIGHT, G_GRAPH_BOT,
                             info->sample_rate / 2.0f);
        }

        /* --- Wait for comparator gate (skip in test mode) --- *
         * Gate = 100 ms.  FFT + display took ~10 ms.            *
         * Poll Get_Frequency() until non-zero or timeout.       */
        if (!s_test_mode)
        {
            wait_cnt = 0;
            while (Get_Frequency() == 0.0f && wait_cnt < 50)
            {
                if (Ps2KeyValue == KeyValue_Back) break;
                delay_ms(5);
                wait_cnt++;
            }
            freq = Get_Frequency();

            /* Restart comparator for next cycle */
            Comparator_Restart();
        }

        delay_ms(5);
    }

    /* ============================ Cleanup ============================ */

    /* Stop AD9240 hardware */
    if (s_use_ad9240)
        AD9240_Stop();

    /* Stop comparator timers + interrupts */
    TIM_Cmd(TIM1, DISABLE);
    TIM_Cmd(TIM2, DISABLE);
    TIM_ITConfig(TIM1, TIM_IT_CC1 | TIM_IT_Update, DISABLE);
    TIM_ITConfig(TIM2, TIM_IT_Update, DISABLE);
    NVIC_DisableIRQ(TIM1_CC_IRQn);
    NVIC_DisableIRQ(TIM1_UP_TIM10_IRQn);
    NVIC_DisableIRQ(TIM2_IRQn);

    /* Re-enable DMA2_Stream0 interrupt (disabled by FFT_DMA_Init) */
    NVIC_EnableIRQ(DMA2_Stream0_IRQn);

    /* Restore ADC1/2/3 for Menu 1 (re-enables DMA2_Stream4 circular) */
    ADC1_Init();
    ADC2_Init();
    ADC3_Init();

    /* Full-screen exit: clear and redraw the menu */
    LCD_Clear(Black);
    Disp_Main();
    Ps2KeyValue = KeyValue_Null;
    MenuSign = 0;
}
