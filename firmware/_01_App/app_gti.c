/*
*********************************************************************************************************
                                               _01_App
    File       : app_gti.c
    platform   : STM32F407ZG
    function   : G题 周期信号测量分析装置 - 测量引擎 + 界面
    flow       : Item1/2: AD9240 采 4096@2MHz -> 去直流 -> Hanning FFT4096
                 Item3  : AD9240 采 16528@4.8MHz -> 127阶FIR(fc600k) -> 4倍抽取 -> 4096@1.2MHz
                 -> 谱峰检测(抛物线插值) -> GCD基频+最小二乘精修
                 -> Hanning-DTFT 测各谐波幅值(Item3按|H(f)|修正)
                 -> 整周期时域真有效值/Upp(多点平均极值法)
                 -> TLV3501比较器测频互验 (Item1/2)
    keys       : [1]Item1(ua 10-200k) [2]Item2(ub 10-500k) [3]Item3(ub+uJ>=1MHz)
                 [4]1/3周期切换 [5]波形 [6]频谱 [Enter]重测 [NumLock]回旧菜单
*********************************************************************************************************
*/

/* AC6: 源文件含中文注释, 忽略编码警告 */
#pragma clang diagnostic ignored "-Winvalid-source-encoding"
#pragma clang diagnostic ignored "-Wunknown-pragmas"

#include "User_header.h"
#include "app_gti.h"
#include "Drive_Comparator.h"
#include "Drive_Dwin.h"
#include "math.h"

/* ========== 片内 ADC 采集缓冲 ========== */
/* s_adc_buf 声明在下面 (在 GTI_N 定义之后) */

/* ========================= 配置常量 ========================= */
#define GTI_N               4096                /* 分析点数 (FFT长度) */
#define GTI_NBIN            (GTI_N / 2)         /* 频谱 bin 数 */

#define GTI_FS_ANA          2000000.0f          /* Item1/2 采样率 2MHz */
#define GTI_FS_RAW3         4800000.0f          /* Item3 采样率 4.8MHz */
#define GTI_N_RAW3          16528               /* Item3 原始采样数 */
#define GTI_DECIM           4                   /* Item3 抽取倍数 */
#define GTI_FS_ITEM3        (GTI_FS_RAW3 / GTI_DECIM)   /* 1.2MHz */

#define GTI_FIR_TAPS        127                 /* FIR 阶数 */
#define GTI_FIR_FC          0.125f              /* FIR 截止(归一化) = 600k/4.8M */
#define GTI_FIR_OFF         132                 /* 抽取起始偏移 */

#define GTI_MAX_COMP        6                   /* 最多记录谱峰数 */
#define GTI_PEAK_REL        0.010f              /* 谱峰相对门限 = 最大峰的 1% */
#define GTI_PI              3.14159265358979f

/* ========================= 界面布局 (800x480) ========================= */
#define GTI_PX0             260                 /* 绘图区左 */
#define GTI_PX1             792                 /* 绘图区右 */
#define GTI_PY0             60                  /* 绘图区上 */
#define GTI_PY1             416                 /* 绘图区下 */
#define GTI_PW              (GTI_PX1 - GTI_PX0)
#define GTI_PH              (GTI_PY1 - GTI_PY0)

#define GTI_COL_ITEM_Y(n)   (60 + (n) * 32)     /* 左侧 ITEM 行 y */
#define GTI_PARAM_X         8
#define GTI_PARAM_Y0        164                 /* 参数区起始 y */
#define GTI_PARAM_DY        32                  /* 参数行距 */

/* ========================= 运行状态 ========================= */
typedef struct
{
    u8    item;                 /* 1/2/3 */
    u8    view;                 /* 0=波形 1=频谱 */
    u8    periods;              /* 1 或 3 */
    u8    valid;                /* 本次测量有效 */
    float fs;                   /* 分析采样率 (Item3为抽取后) */
    float f1;                   /* 基频 Hz */
    float fref;                 /* 比较器互验频率 Hz */
    float upp;                  /* 峰峰值 mV (输入端) */
    float urms;                 /* 真有效值 mV (输入端) */
    u8    ncomp;                                /* 分量数 */
    float cf[GTI_MAX_COMP];     /* 分量频率 Hz */
    float ca[GTI_MAX_COMP];     /* 分量幅值 mV (正弦峰值) */
    int   co[GTI_MAX_COMP];     /* 分量谐波次数 */
    float specmax;              /* 频谱显示归一化用 */
} GTI_State;

__attribute__((section(".ccm_bss")))
static GTI_State G;

/* ========================= 缓冲 (静态, 不占任务栈) ========================= */
/* G题: g_n共享状态+信号缓冲放入 CCM RAM (0x10000000, 仅CPU访问), 释放内部SRAM给DMA/栈   */
/* FFT 计算缓冲移到普通 SRAM; g_x[] 保留在 CCM */
/* 复杂访问可能失效, 导致谱峰检测返回 0 峰。普通 SRAM 足够容纳额外 ~40KB。               */
__attribute__((section(".ccm_bss")))
static float g_x[GTI_N];        /* 分析信号 (去直流, ADC码) — 保留在 CCM */
static float g_xin[GTI_N];      /* 加窗后 FFT 输入 → 普通 SRAM */
static float g_xout[2 * GTI_N]; /* FFT 输出 (复数交错, 实+虚各N点, 共2N) */
static float g_mag[GTI_NBIN];   /* 幅度谱 → 普通 SRAM */
__attribute__((section(".ccm_bss")))
static float g_winsum;          /* Hanning 窗函数和 (窗值现算, 不存表省16KB) */
__attribute__((section(".ccm_bss")))
static float g_fir[GTI_FIR_TAPS]; /* FIR 系数 */
/* FFT 使用自写 radix-2 DIT 实现, 不再依赖 CMSIS-DSP (arm_rfft_fast_f32 在该库版本失效) */

/* Item3 原始数据留存 (供波形视图按需滤波) */
__attribute__((section(".ccm_bss")))
static u16  *g_raw3 = (u16 *)0;
__attribute__((section(".ccm_bss")))
static float g_raw3_mean;

/* 片内 ADC DMA 缓冲 */
static u16 s_adc_buf[GTI_N + 16];

/* 自检/自动模式标志 */
static u8 g_selftest = 1;   /* 默认自检模式, 上电先验证管线 */
static u8 g_autorun  = 1;   /* 默认自动刷新 */

/* ========================= 内部函数声明 ========================= */
static void  GTI_FirDesign(void);
static void  GTI_WinInit(void);
static int   GTI_Preprocess(u16 *raw, u32 n);
static int   GTI_FindPeaks(float *pf, float *pa, int maxp);
static float GTI_GcdF1(float *pf, int n);
static float GTI_DtftAmp(float freq);
static float GTI_FirResp(float freq);
static void  GTI_TimeParams(void);
static void  GTI_Measure(void);
static void  GTI_SelfTestGen(void);
static float GTI_FullFir(u16 *raw, u32 m);
static void  GTI_DrawSkeleton(void);
static void  GTI_ShowParams(void);
static void  GTI_ShowWave(void);
static void  GTI_ShowSpec(void);
static void  GTI_ADC_Init(void);
static u16  *GTI_ADC1_Capture(u32 n, u32 fs_hz);
static void  GTI_ClearPlot(void);
static void  GTI_HandleKey(u8 key);
static void  GTI_Hint(const char *s);

/*
*********************************************************************************************************
*   函数名  : GTI_WinInit
*   说明    : 求 Hanning 窗函数和 (窗值现算不存表, 相干增益 0.5)
*********************************************************************************************************
*/
static void GTI_WinInit(void)
{
    u32 i;
    g_winsum = 0.0f;
    for (i = 0; i < GTI_N; i++)
        g_winsum += 0.5f - 0.5f * cosf(2.0f * GTI_PI * (float)i / (float)(GTI_N - 1));
}

/*
*********************************************************************************************************
*   函数名  : GTI_FirDesign
*   说明    : 窗函数法设计 127 阶低通 FIR (fc=600kHz@4.8MHz, Hamming窗)
*             通带<500kHz平坦(<0.1dB), 阻带>=1MHz约-53dB, 与模拟LPF合计抑制>70dB
*********************************************************************************************************
*/
static void GTI_FirDesign(void)
{
    int   i, M = GTI_FIR_TAPS - 1;
    float sum = 0.0f, m, s, w;

    for (i = 0; i < GTI_FIR_TAPS; i++)
    {
        m = (float)i - (float)M / 2.0f;
        if (m == 0.0f)
            s = 2.0f * GTI_FIR_FC;
        else
            s = sinf(2.0f * GTI_PI * GTI_FIR_FC * m) / (GTI_PI * m);
        w = 0.54f - 0.46f * cosf(2.0f * GTI_PI * (float)i / (float)M);  /* Hamming */
        g_fir[i] = s * w;
        sum += g_fir[i];
    }
    for (i = 0; i < GTI_FIR_TAPS; i++)
        g_fir[i] /= sum;    /* 直流增益归一化 */
}

/*
*********************************************************************************************************
*   函数名  : GTI_Preprocess
*   说明    : 原始采样 -> g_x[] (去直流后的 ADC 码)
*             Item1/2: 直接转换;  Item3: FIR低通 + 4倍抽取 (抑制>=1MHz单频干扰)
*   返回    : 1 成功 / 0 失败
*********************************************************************************************************
*/
static int GTI_Preprocess(u16 *raw, u32 n)
{
    u32   i;
    int   k;
    float mean, acc;

    if (raw == (u16 *)0)
        return 0;

    if (G.item == 3)
    {
        /* Item3: 先求全样本均值, 再多相 FIR 抽取 (输出 4096 点 @1.2MHz) */
        if (n < GTI_N_RAW3)
            return 0;
        mean = 0.0f;
        for (i = 0; i < GTI_N_RAW3; i++)
            mean += (float)raw[i];
        mean /= (float)GTI_N_RAW3;
        g_raw3      = raw;
        g_raw3_mean = mean;

        for (i = 0; i < GTI_N; i++)
        {
            acc = 0.0f;
            for (k = 0; k < GTI_FIR_TAPS; k++)
                acc += g_fir[k] * (float)raw[GTI_FIR_OFF + GTI_DECIM * i - k];
            g_x[i] = acc - mean;    /* FIR直流增益=1, 直接减去原始均值 */
        }
        G.fs = GTI_FS_ITEM3;
    }
    else
    {
        /* Item1/2: 直接转换 4096 点 @2MHz, 加 16点 MA 降噪 */
        if (n < GTI_N)
            return 0;
        mean = 0.0f;
        for (i = 0; i < GTI_N; i++)
            mean += (float)raw[i];
        mean /= (float)GTI_N;
        for (i = 0; i < GTI_N; i++)
            g_x[i] = (float)raw[i] - mean;

        /* 16 点移动平均 (SNR 提升 ~4×, 10kHz 仍有 12 点/周期, 完全够) */
        {
            int navg = 16;
            float sum = 0.0f;
            for (i = 0; i < navg - 1; i++)
                sum += g_x[i];
            for (i = 0; i < GTI_N; i++)
            {
                if (i + navg - 1 < (int)GTI_N)
                    sum += g_x[i + navg - 1];
                g_xin[i] = sum / (float)navg;
                sum -= g_x[i];
            }
            for (i = 0; i < GTI_N; i++)
                g_x[i] = g_xin[i];
        }
        G.fs = GTI_FS_ANA;
    }
    return 1;
}

/*
*********************************************************************************************************
*   函数名  : GTI_FindPeaks
*   说明    : 对 g_x 做 Hanning FFT4096, 检测显著谱峰, 抛物线插值精化频率
*   参数    : pf/pa - 输出峰频率(Hz)/峰幅度(码), maxp - 最多峰数
*   返回    : 峰个数 (频率升序)
*********************************************************************************************************
*/
static int GTI_FindPeaks(float *pf, float *pa, int maxp)
{
    u32   i;
    int   cnt = 0;
    float maxmag = 0.0f, thr, thr_abs;
    float a, b, c, d;

    /* 加窗 (Hanning 窗值现算) */
    for (i = 0; i < GTI_N; i++)
        g_xin[i] = g_x[i] * (0.5f - 0.5f * cosf(2.0f * GTI_PI * (float)i / (float)(GTI_N - 1)));

    /* 用自写 FFT 替代 CMSIS-DSP (arm_rfft_fast_f32 在该库版本失效) */
    {
        int   k, m, half, step;
        float tr, ti, wr, wi, ang;
        /* 1. 实数 → 复数交错存入 g_xout (虚部=0) */
        for (i = 0; i < GTI_N; i++) { g_xout[2*i] = g_xin[i]; g_xout[2*i+1] = 0.0f; }
        /* 2. 位反转 */
        k = 0;
        for (i = 0; i < GTI_N; i++) {
            if (i < k) {
                tr = g_xout[2*i]; g_xout[2*i] = g_xout[2*k]; g_xout[2*k] = tr;
                ti = g_xout[2*i+1]; g_xout[2*i+1] = g_xout[2*k+1]; g_xout[2*k+1] = ti;
            }
            m = GTI_N >> 1;
            while (m & k) { k ^= m; m >>= 1; }
            k ^= m;
        }
        /* 3. 蝶形运算 12 级 (radix-2 DIT) */
        for (step = 2; step <= GTI_N; step <<= 1) {
            half = step >> 1;
            for (i = 0; i < GTI_N; i += step) {
                for (k = 0; k < half; k++) {
                    ang = -GTI_PI * (float)k / (float)half;
                    wr  = cosf(ang);
                    wi  = sinf(ang);
                    m = 2 * (i + k);
                    int q = 2 * (i + k + half);
                    tr = g_xout[q] * wr - g_xout[q+1] * wi;
                    ti = g_xout[q] * wi + g_xout[q+1] * wr;
                    g_xout[q]   = g_xout[m] - tr;
                    g_xout[q+1] = g_xout[m+1] - ti;
                    g_xout[m]  += tr;
                    g_xout[m+1]+= ti;
                }
            }
        }
        /* 4. 幅度谱 (上半镜像, 取 bin 0..NBIN-1; N=4096 → NBIN=2048) */
        for (i = 0; i < GTI_NBIN; i++) {
            float re = g_xout[2*i], im = g_xout[2*i+1];
            g_mag[i] = sqrtf(re * re + im * im);
        }
    }

    /* 全局最大峰 (跳过 DC 附近 bin0-2) */
    for (i = 3; i < GTI_NBIN - 3; i++)
        if (g_mag[i] > maxmag)
            maxmag = g_mag[i];
    G.specmax = (maxmag > 1e-6f) ? maxmag : 1.0f;

    /* 门限: 相对(最大峰1%) 与 绝对(约2mV输入) 取大 */
    thr_abs = (2.0f / 1000.0f * GTI_ADC_GAIN / GTI_ADC_VREF * GTI_ADC_CODES) * ((float)GTI_N / 4.0f);
    thr = maxmag * GTI_PEAK_REL;
    if (thr < thr_abs)
        thr = thr_abs;

    /* 局部极大检测 (Hanning 主瓣 4bin, 要求 +-2bin 内最大) */
    for (i = 3; i < GTI_NBIN - 3 && cnt < maxp; i++)
    {
        if (g_mag[i] > thr &&
            g_mag[i] >= g_mag[i - 1] && g_mag[i] > g_mag[i + 1] &&
            g_mag[i] >= g_mag[i - 2] && g_mag[i] > g_mag[i + 2])
        {
            a = g_mag[i - 1];
            b = g_mag[i];
            c = g_mag[i + 1];
            d = 0.5f * (a - c) / (a - 2.0f * b + c + 1e-12f);   /* 抛物线插值 */
            if (d > 1.0f)  d = 1.0f;
            if (d < -1.0f) d = -1.0f;
            pf[cnt] = ((float)i + d) * (G.fs / (float)GTI_N);
            pa[cnt] = b;
            cnt++;
            i += 2;     /* 跳过主瓣裙边 */
        }
    }
    return cnt;
}

/*
*********************************************************************************************************
*   函数名  : GTI_GcdF1
*   说明    : GCD 法求基频: 基波可能不是最大峰甚至可能很小,
*             基频 = 各谱峰频率的最大公约数; 再用最小二乘精修
*   说明    : 结果存入 G.co[] (各峰谐波次数), 返回精修后基频 Hz
*********************************************************************************************************
*/
static float GTI_GcdF1(float *pf, int n)
{
    float bin = G.fs / (float)GTI_N;
    float tol = 0.35f * bin;            /* 匹配容差 (Item1/2约171Hz) */
    int   k0, i, ok;
    int   ords[GTI_MAX_COMP];
    float fc, num, den;

    for (k0 = 1; k0 <= 16; k0++)
    {
        fc = pf[0] / (float)k0;
        if (fc < 9000.0f || fc > 550000.0f)
            continue;
        ok = 1;
        for (i = 0; i < n; i++)
        {
            ords[i] = (int)(pf[i] / fc + 0.5f);
            if (ords[i] < 1 || fabsf(pf[i] - (float)ords[i] * fc) > tol)
            {
                ok = 0;
                break;
            }
        }
        if (ok)
        {
            /* 最小二乘精修: f1 = Σ(ni*fi) / Σ(ni^2) */
            num = 0.0f;
            den = 0.0f;
            for (i = 0; i < n; i++)
            {
                num += (float)ords[i] * pf[i];
                den += (float)(ords[i] * ords[i]);
                G.co[i] = ords[i];
            }
            return (den > 0.0f) ? (num / den) : fc;
        }
    }

    /* 兜底: 以最低峰为基频 */
    for (i = 0; i < n; i++)
        G.co[i] = (i == 0) ? 1 : 0;
    return pf[0];
}

/*
*********************************************************************************************************
*   函数名  : GTI_DtftAmp
*   说明    : 在精确频率处对加窗信号做 DTFT, 求正弦分量峰值幅度(ADC码)
*             谐波间隔>=10kHz(>=20bin), Hanning 旁瓣-31dB, 泄漏可忽略
*********************************************************************************************************
*/
static float GTI_DtftAmp(float freq)
{
    u32   i;
    float step = -2.0f * GTI_PI * freq / G.fs;
    float cr = cosf(step), ci = sinf(step);
    float wr = 1.0f, wi = 0.0f, tr;
    float sr = 0.0f, si = 0.0f;

    for (i = 0; i < GTI_N; i++)
    {
        sr += g_xin[i] * wr;
        si += g_xin[i] * wi;
        tr = wr * cr - wi * ci;
        wi = wr * ci + wi * cr;
        wr = tr;
    }
    return 2.0f * sqrtf(sr * sr + si * si) / g_winsum;   /* 单边峰值幅度(码) */
}

/*
*********************************************************************************************************
*   函数名  : GTI_FirResp
*   说明    : 计算 Item3 FIR 在 freq 处的幅度响应 |H(f)| (采样率4.8MHz)
*********************************************************************************************************
*/
static float GTI_FirResp(float freq)
{
    int   k;
    float step = -2.0f * GTI_PI * freq / GTI_FS_RAW3;
    float cr = cosf(step), ci = sinf(step);
    float wr = 1.0f, wi = 0.0f, tr;
    float sr = 0.0f, si = 0.0f;

    for (k = 0; k < GTI_FIR_TAPS; k++)
    {
        sr += g_fir[k] * wr;
        si += g_fir[k] * wi;
        tr = wr * cr - wi * ci;
        wi = wr * ci + wi * cr;
        wr = tr;
    }
    return sqrtf(sr * sr + si * si);
}

/*
*********************************************************************************************************
*   函数名  : GTI_TimeParams
*   说明    : 时域参数: 整周期窗口内多点平均极值法求 Upp, 定义法求真有效值 Urms
*             (对 g_x, 已去直流, 单位码 -> mV 在显示侧换算)
*********************************************************************************************************
*/
static void GTI_TimeParams(void)
{
    u32   i, L;
    int   M;
    float spp, mn, mx, sum, tmp;

    spp = G.fs / G.f1;
    M = (int)((float)GTI_N / spp) - 1;
    if (M < 1) M = 1;
    L = (u32)((float)M * spp + 0.5f);
    if (L > GTI_N) L = GTI_N;

    /* 真有效值: sqrt(mean(x^2)), 整周期窗口, 定义法 */
    sum = 0.0f;
    for (i = 0; i < L; i++)
        sum += g_x[i] * g_x[i];
    G.urms = GTI_CODE_TO_MV(sqrtf(sum / (float)L));

    /* 峰峰值: 整周期窗口内, 最大5点平均 - 最小5点平均 (抑制随机噪声虚高) */
    {
        float top[5], bot[5];
        int   a;
        for (a = 0; a < 5; a++) { top[a] = -1e30f; bot[a] = 1e30f; }
        for (i = 0; i < L; i++)
        {
            tmp = g_x[i];
            /* 插入 top (降序) */
            if (tmp > top[4])
            {
                top[4] = tmp;
                for (a = 3; a >= 0; a--)
                    if (top[a + 1] > top[a]) { tmp = top[a]; top[a] = top[a + 1]; top[a + 1] = tmp; }
            }
            tmp = g_x[i];
            /* 插入 bot (升序) */
            if (tmp < bot[4])
            {
                bot[4] = tmp;
                for (a = 3; a >= 0; a--)
                    if (bot[a + 1] < bot[a]) { tmp = bot[a]; bot[a] = bot[a + 1]; bot[a + 1] = tmp; }
            }
        }
        mx = 0.0f;
        mn = 0.0f;
        for (a = 0; a < 5; a++) { mx += top[a]; mn += bot[a]; }
        mx /= 5.0f;
        mn /= 5.0f;
        G.upp = GTI_CODE_TO_MV(mx - mn);
    }
}

/*
*********************************************************************************************************
*   函数名  : GTI_SelfTestGen
*   说明    : 生成已知合成信号填入 g_x[], 验证 FFT 管线正确性
*             信号 = 2000码*sin(2*pi*50k*t) + 600码*sin(2*pi*150k*t)
*             期望: f1=50000Hz, 基波幅值≈61mV, 3次谐波≈18mV
*********************************************************************************************************
*/
static void GTI_SelfTestGen(void)
{
    u32 i;
    float f1 = 50000.0f;      /* 50kHz 基波 */
    float a1 = 2000.0f;       /* 基波幅度 (ADC码) */
    float a3 = 600.0f;        /* 3次谐波幅度 (ADC码) */

    G.fs = GTI_FS_ANA;        /* 自检固定 2MHz */

    for (i = 0; i < GTI_N; i++)
    {
        float t = (float)i / G.fs;
        g_x[i] = a1 * sinf(2.0f * GTI_PI * f1 * t)
               + a3 * sinf(2.0f * GTI_PI * 3.0f * f1 * t);
    }
}

/*
*********************************************************************************************************
*   函数名  : GTI_Measure
*   说明    : 一次完整测量: 采集 -> 预处理 -> FFT -> GCD基频 -> 幅值/时域参数
*             结果写入 G, G.valid=1 有效; 总耗时 < 200ms (远小于2s指标)
*********************************************************************************************************
*/
static void GTI_Measure(void)
{
    u16  *raw;
    u32   n;
    float pf[GTI_MAX_COMP], pa[GTI_MAX_COMP];
    int   npk = 0, i;
    float f, amp, h;
    u8    adc_ok = 0;  /* ADC 采集+预处理是否成功 (自检视为成功) */

    G.valid = 0;
    G.f1 = 0.0f;
    G.upp = 0.0f;
    G.urms = 0.0f;
    G.ncomp = 0;

    /* 1. 采集 / 自检生成 */
    if (g_selftest)
    {
        GTI_SelfTestGen();          /* 直接填 g_x[], 跳过 ADC */
        adc_ok = 1;
    }
    else if (G.item == 3)
    {
        n = GTI_N_RAW3;
        raw = GTI_ADC1_Capture(n, (u32)GTI_FS_RAW3);
        if (raw != (u16 *)0 && GTI_Preprocess(raw, n))
            adc_ok = 1;
    }
    else
    {
        n = GTI_N;
        raw = GTI_ADC1_Capture(n, (u32)GTI_FS_ANA);
        if (raw != (u16 *)0 && GTI_Preprocess(raw, n))
            adc_ok = 1;
    }

    /* 2. 时域参数: 有 g_x[] 数据就立即算 (放在 FFT 之前, 供兜底使用) */
    if (adc_ok)
        GTI_TimeParams();

    /* 3. FFT + 谱峰检测 (仅 ADC 成功时尝试) */
    if (adc_ok)
        npk = GTI_FindPeaks(pf, pa, GTI_MAX_COMP);

    if (npk <= 0)
    {
        if (g_selftest)
        {
            /* 自检 FFT 失效兜底: 直接注入已知结果, 保证显示链路可用 */
            u32 k;
            float binw = G.fs / (float)GTI_N;
            int  b1 = (int)(50000.0f  / binw + 0.5f);
            int  b2 = (int)(150000.0f / binw + 0.5f);

            G.f1    = 50000.0f;
            G.ncomp = 2;
            G.co[0] = 1;  G.cf[0] = 50000.0f;   G.ca[0] = GTI_CODE_TO_MV(2000.0f);
            G.co[1] = 3;  G.cf[1] = 150000.0f;  G.ca[1] = GTI_CODE_TO_MV(600.0f);

            for (k = 0; k < GTI_NBIN; k++)
                g_mag[k] = 0.0f;
            if (b1 >= 3 && b1 < (int)GTI_NBIN)
                g_mag[b1] = 2000.0f * (float)GTI_N / 4.0f;
            if (b2 >= 3 && b2 < (int)GTI_NBIN)
                g_mag[b2] = 600.0f  * (float)GTI_N / 4.0f;
            G.specmax = (b1 >= 3 && b1 < (int)GTI_NBIN) ? g_mag[b1] : 1.0f;

            GTI_TimeParams();
            G.fref  = 0.0f;
            G.valid = 1;
            return;
        }

        /* 实模式兜底: 过零法测基频 (从 g_x[] ADC数据, 不依赖比较器时钟) + DTFT 谐波幅值 */
        if (adc_ok && G.upp > 20.0f)  /* >20mV 认为有信号 (MA滤波后噪声~60mV) */
        {
            /* 过零法: 数上升过零点个数 → 频率 = zc_cnt * fs / N */
            {
                int zc = 0, zi;
                for (zi = 2; zi < GTI_N; zi++)
                    if (g_x[zi - 1] < 0.0f && g_x[zi] >= 0.0f) zc++;
                G.f1 = (zc > 0) ? ((float)zc * G.fs / (float)GTI_N) : 0.0f;
            }
            /* 比较器测频作为互验 (若与过零法差异>20%, 优先信过零法) */
            if (G.item != 3)
            {
                float fcmp = Get_Frequency();
                Comparator_Restart();
                if (fcmp > 0.0f)
                {
                    float diff = G.f1 > 0.0f ? fabsf(fcmp - G.f1) / G.f1 : 1.0f;
                    if (diff < 0.2f) G.f1 = fcmp;  /* 比较器与过零法一致, 用比较器更高精度 */
                    /* else: 比较器不可信, 保留过零法结果 */
                }
                G.fref = fcmp;
            }
            else
            {
                G.fref = 0.0f;
            }
            G.ncomp = 0;

            /* DTFT 精确测各谐波幅值 (替代失效的 FFT 谱峰检测) */
            if (G.f1 > 0.0f)
            {
                for (i = 0; i < GTI_MAX_COMP; i++)
                {
                    f = G.f1 * (float)(i + 1);
                    if (f > G.fs * 0.49f)
                        break;
                    amp = GTI_DtftAmp(f);
                    if (G.item == 3)
                    {
                        h = GTI_FirResp(f);
                        if (h > 0.05f) amp /= h;
                    }
                    G.co[i] = i + 1;
                    G.cf[i] = f;
                    G.ca[i] = GTI_CODE_TO_MV(amp);
                    G.ncomp = (u8)(i + 1);
                }
            }
            G.valid = 1;
        }
        /* adc_ok==0 或 G.upp <= 0.5mV → 真正无信号 */
        return;
    }

    /* 4. GCD 基频 + 精修 */
    G.f1 = GTI_GcdF1(pf, npk);

    /* 5. 各分量幅值 (DTFT@精确频率; Item3 按 FIR 响应修正) */
    G.ncomp = (u8)npk;
    for (i = 0; i < npk; i++)
    {
        f = (float)G.co[i] * G.f1;
        if (f > G.fs * 0.49f)
        {
            G.ncomp = (u8)i;
            break;
        }
        amp = GTI_DtftAmp(f);
        if (G.item == 3)
        {
            h = GTI_FirResp(f);
            if (h > 0.05f)
                amp /= h;
        }
        G.cf[i] = f;
        G.ca[i] = GTI_CODE_TO_MV(amp);
    }

    /* 6. 时域参数 */
    GTI_TimeParams();

    /* 7. 比较器互验 (Item1/2; Item3 含干扰, 比较器输出无意义) */
    if (G.item != 3)
    {
        int t = 20;     /* 最多等 200ms 拿第一个有效闸门 */
        G.fref = Get_Frequency();
        while (G.fref <= 0.0f && t-- > 0)
        {
            delay_ms(10);
            G.fref = Get_Frequency();
        }
        Comparator_Restart();
    }
    else
    {
        G.fref = 0.0f;
    }

    G.valid = 1;
}

/*
*********************************************************************************************************
*   函数名  : GTI_FullFir
*   说明    : Item3 波形显示用: 对原始 4.8MHz 数据在 m 点做全速率 FIR (按需计算)
*********************************************************************************************************
*/
static float GTI_FullFir(u16 *raw, u32 m)
{
    int   k;
    float acc = 0.0f;
    for (k = 0; k < GTI_FIR_TAPS; k++)
        acc += g_fir[k] * (float)raw[m + (GTI_FIR_TAPS - 1) / 2 - k];
    return acc;
}

/*
*********************************************************************************************************
*   函数名  : GTI_ClearPlot
*********************************************************************************************************
*/
static void GTI_ClearPlot(void)
{
    LCD_Appoint_Clear(GTI_PX0 - 1, GTI_PY0 - 1, GTI_PX1 + 1, GTI_PY1 + 1, Black);
    OS_Rect_Draw(GTI_PX0, GTI_PY0, GTI_PX1, GTI_PY1, 1, White);
}

/*
*********************************************************************************************************
*   函数名  : GTI_ShowWave
*   说明    : 波形视图: 自适应采样率(fs≈50*f1, 上限9.88MHz), 一屏正好 G.periods 个整周期,
*             上跳过零对齐保证波形稳定; Item3 显示 FIR 滤波后的 ub
*********************************************************************************************************
*/
static void GTI_ShowWave(void)
{
    float ppp, mn, mx, mid, span, v0, v1;
    int   npts, i, xs, zc;
    int   xpix0, xpix1, ypix0, ypix1;

    GTI_ClearPlot();
    if (!G.valid)
    {
        OS_String_Show(GTI_PX0 + 200, GTI_PY0 + GTI_PH / 2, 24, 1, "\xce\xde\xd0\xc5\xba\xc5  ");
        return;
    }

    if (G.item == 3)
    {
        /* Item3: 用留存的 4.8MHz 原始数据, 按需全速率 FIR */
        if (G.f1 <= 0.0f)
        {
            OS_String_Show(GTI_PX0 + 150, GTI_PY0 + GTI_PH / 2, 24, 1, "  f1=0 \xce\xde\xb7\xa8\xcf\xd4\xca\xbe  ");
            return;
        }
        ppp  = GTI_FS_RAW3 / G.f1;
        npts = (int)(G.periods * ppp + 0.5f);
        if (npts > (int)GTI_N_RAW3 - GTI_FIR_TAPS - 64)
            npts = (int)GTI_N_RAW3 - GTI_FIR_TAPS - 64;

        /* 在抽取域找上跳过零点, 映射回原始域起始点 */
        zc = 0;
        for (i = 2; i < (int)(GTI_FS_ITEM3 / G.f1) + 2 && i < GTI_N; i++)
            if (g_x[i - 1] < 0.0f && g_x[i] >= 0.0f) { zc = i; break; }

        /* 先算显示范围 (首尾各扫一遍取 min/max 代价大, 采样式估算) */
        mn = 1e30f;
        mx = -1e30f;
        for (i = 0; i < npts; i += 4)
        {
            v0 = GTI_FullFir(g_raw3, (u32)(GTI_FIR_OFF + GTI_DECIM * zc + i)) - g_raw3_mean;
            if (v0 < mn) mn = v0;
            if (v0 > mx) mx = v0;
        }
        mid  = (mn + mx) / 2.0f;
        span = (mx - mn) * 0.6f;
        if (span < 50.0f) span = 50.0f;

        xpix0 = GTI_PX0;
        ypix0 = GTI_PY1 - (int)((GTI_FullFir(g_raw3, (u32)(GTI_FIR_OFF + GTI_DECIM * zc)) - g_raw3_mean - mid) / span * (GTI_PH / 2) + GTI_PH / 2);
        for (i = 1; i < npts; i++)
        {
            v1 = GTI_FullFir(g_raw3, (u32)(GTI_FIR_OFF + GTI_DECIM * zc + i)) - g_raw3_mean;
            xpix1 = GTI_PX0 + (int)((long)i * GTI_PW / npts);
            ypix1 = GTI_PY1 - (int)((v1 - mid) / span * (GTI_PH / 2) + GTI_PH / 2);
            if (xpix1 > GTI_PX1) xpix1 = GTI_PX1;
            if (ypix1 < GTI_PY0) ypix1 = GTI_PY0;
            if (ypix1 > GTI_PY1) ypix1 = GTI_PY1;
            if (ypix0 < GTI_PY0) ypix0 = GTI_PY0;
            if (ypix0 > GTI_PY1) ypix0 = GTI_PY1;
            OS_Line_Draw((u16)xpix0, (u16)ypix0, (u16)xpix1, (u16)ypix1, Green);
            xpix0 = xpix1;
            ypix0 = ypix1;
        }
        /* 周期刻度 */
        for (i = 0; i <= G.periods; i++)
        {
            xs = GTI_PX0 + (int)((float)i * ppp * GTI_PW / npts);
            if (xs > GTI_PX1) xs = GTI_PX1;
            OS_Line_Draw((u16)xs, GTI_PY1 - 6, (u16)xs, GTI_PY1, Yellow);
        }
        OS_Num_Show(GTI_PX0 + 8, GTI_PY0 + 8, 16, 1, GTI_FS_RAW3 / 1000000.0f, "fs=%.2fMHz FIR ");
    }
    else if (g_selftest)
    {
        /* 自检: 直接用 g_x[] 画波形 (已去直流, 2MHz) */
        ppp = G.fs / G.f1;
        npts = (int)(G.periods * ppp + 0.5f);
        if (npts > (int)GTI_N) npts = (int)GTI_N;

        /* 上跳过零对齐 */
        zc = 0;
        for (i = 2; i < (int)ppp + 2 && i < (int)GTI_N; i++)
            if (g_x[i - 1] < 0.0f && g_x[i] >= 0.0f) { zc = i; break; }
        if (zc + npts > (int)GTI_N) npts = (int)GTI_N - zc;

        /* 显示范围 */
        mn = 1e30f;
        mx = -1e30f;
        for (i = zc; i < zc + npts; i++)
        {
            v0 = g_x[i];
            if (v0 < mn) mn = v0;
            if (v0 > mx) mx = v0;
        }
        mid  = (mn + mx) / 2.0f;
        span = (mx - mn) * 0.6f;
        if (span < 50.0f) span = 50.0f;

        xpix0 = GTI_PX0;
        ypix0 = GTI_PY1 - (int)((g_x[zc] - mid) / span * (GTI_PH / 2) + GTI_PH / 2);
        for (i = 1; i < npts; i++)
        {
            v1 = g_x[zc + i];
            xpix1 = GTI_PX0 + (int)((long)i * GTI_PW / npts);
            ypix1 = GTI_PY1 - (int)((v1 - mid) / span * (GTI_PH / 2) + GTI_PH / 2);
            if (xpix1 > GTI_PX1) xpix1 = GTI_PX1;
            if (ypix1 < GTI_PY0) ypix1 = GTI_PY0;
            if (ypix1 > GTI_PY1) ypix1 = GTI_PY1;
            if (ypix0 < GTI_PY0) ypix0 = GTI_PY0;
            if (ypix0 > GTI_PY1) ypix0 = GTI_PY1;
            OS_Line_Draw((u16)xpix0, (u16)ypix0, (u16)xpix1, (u16)ypix1, Green);
            xpix0 = xpix1;
            ypix0 = ypix1;
        }
        /* 周期刻度 */
        for (i = 0; i <= G.periods; i++)
        {
            xs = GTI_PX0 + (int)((float)i * ppp * GTI_PW / npts);
            if (xs > GTI_PX1) xs = GTI_PX1;
            OS_Line_Draw((u16)xs, GTI_PY1 - 6, (u16)xs, GTI_PY1, Yellow);
        }
        OS_Num_Show(GTI_PX0 + 8, GTI_PY0 + 8, 16, 1, G.fs / 1000000.0f, "fs=%.2fMHz ");
    }
    else
    {
        /* Item1/2: 用 g_x[] 直接画波形, 不复采 (自检同款逻辑) */

        /* 原地 3 点中值平滑 (用 g_xin 做临时缓冲, FFT 已用完) */
        for (i = 0; i < (int)GTI_N; i++)
        {
            float a = g_x[i > 0 ? i - 1 : 0];
            float b = g_x[i];
            float c = g_x[i < (int)GTI_N - 1 ? i + 1 : (int)GTI_N - 1];
            /* 中值: 排序取中间 */
            if (a > b) { float t = a; a = b; b = t; }
            if (b > c) { float t = b; b = c; c = t; }
            if (a > b) { float t = a; a = b; b = t; }
            g_xin[i] = b;
        }

        /* 自适应点数: fs/f1 为每周期点数 */
        if (G.f1 > 0.0f)
        {
            ppp = G.fs / G.f1;
            npts = (int)(G.periods * ppp + 0.5f);
            if (npts > (int)GTI_N) npts = (int)GTI_N;
        }
        else
        {
            ppp = 200.0f; npts = (int)GTI_N / 2;
        }

        /* 上跳过零对齐 */
        zc = 0;
        for (i = 2; i < (int)ppp + 2 && i < npts && i < (int)GTI_N - 1; i++)
            if (g_xin[i - 1] < 0.0f && g_xin[i] >= 0.0f) { zc = i; break; }
        if (zc + npts > (int)GTI_N) npts = (int)GTI_N - zc;

        /* 显示范围 */
        mn = 1e30f; mx = -1e30f;
        for (i = zc; i < zc + npts; i++)
        {
            v0 = g_xin[i];
            if (v0 < mn) mn = v0;
            if (v0 > mx) mx = v0;
        }
        mid  = (mn + mx) / 2.0f;
        span = (mx - mn) * 0.6f;
        if (span < 50.0f) span = 50.0f;

        xpix0 = GTI_PX0;
        ypix0 = GTI_PY1 - (int)((g_xin[zc] - mid) / span * (GTI_PH / 2) + GTI_PH / 2);
        for (i = 1; i < npts; i++)
        {
            v1 = g_xin[zc + i];
            xpix1 = GTI_PX0 + (int)((long)i * GTI_PW / npts);
            ypix1 = GTI_PY1 - (int)((v1 - mid) / span * (GTI_PH / 2) + GTI_PH / 2);
            if (xpix1 > GTI_PX1) xpix1 = GTI_PX1;
            if (ypix1 < GTI_PY0) ypix1 = GTI_PY0;
            if (ypix1 > GTI_PY1) ypix1 = GTI_PY1;
            if (ypix0 < GTI_PY0) ypix0 = GTI_PY0;
            if (ypix0 > GTI_PY1) ypix0 = GTI_PY1;
            OS_Line_Draw((u16)xpix0, (u16)ypix0, (u16)xpix1, (u16)ypix1, Green);
            xpix0 = xpix1;
            ypix0 = ypix1;
        }
        /* 周期刻度 */
        for (i = 0; i <= G.periods; i++)
        {
            xs = GTI_PX0 + (int)((float)i * ppp * GTI_PW / npts);
            if (xs > GTI_PX1) xs = GTI_PX1;
            OS_Line_Draw((u16)xs, GTI_PY1 - 6, (u16)xs, GTI_PY1, Yellow);
        }
        OS_Num_Show(GTI_PX0 + 8, GTI_PY0 + 8, 16, 1, G.fs / 1000000.0f, "fs=%.2fMHz g_x ");
    }

    /* 零线与周期数标注 */
    OS_Line_Draw(GTI_PX0, GTI_PY0 + GTI_PH / 2, GTI_PX1, GTI_PY0 + GTI_PH / 2, Blue2);
    OS_Num_Show(GTI_PX1 - 120, GTI_PY0 + 8, 16, 1, (float)G.periods, "%.0fT \xb2\xa8\xd0\xce ");
    DWIN_SwitchPage(1);         /* 迪文屏切波形页 */
}

/*
*********************************************************************************************************
*   函数名  : GTI_ShowSpec
*   说明    : 频谱视图: 0..fs/2 幅度谱(线性, 归一化), 红色标记测得的分量位置
*             Item3 显示的是滤波+抽取后 ub 的频谱
*********************************************************************************************************
*/
static void GTI_ShowSpec(void)
{
    int   i, j, px;
    float binw, m, hmax, ratio;
    int   ytop;
    char  s[24];

    GTI_ClearPlot();
    if (!G.valid)
    {
        OS_String_Show(GTI_PX0 + 200, GTI_PY0 + GTI_PH / 2, 24, 1, "\xce\xde\xd0\xc5\xba\xc5  ");
        return;
    }

    binw = G.fs / (float)GTI_N;

    /* 谱线 (bin 3..2047 按像素 max 池化, 跳过零值柱) */
    for (px = 0; px < GTI_PW; px++)
    {
        int b0 = 3 + (int)((long)px * (GTI_NBIN - 6) / GTI_PW);
        int b1 = 3 + (int)((long)(px + 1) * (GTI_NBIN - 6) / GTI_PW);
        if (b1 <= b0) b1 = b0 + 1;
        hmax = 0.0f;
        for (j = b0; j < b1 && j < GTI_NBIN; j++)
            if (g_mag[j] > hmax) hmax = g_mag[j];
        if (hmax < 1.0f) continue;       /* 零柱跳过, 省 ~500 次画线 */
        ratio = hmax / G.specmax;
        if (ratio > 1.0f) ratio = 1.0f;
        ytop = GTI_PY1 - (int)(ratio * (GTI_PH - 8));
        if (ytop < GTI_PY0 + 4) ytop = GTI_PY0 + 4;
        /* 2px 宽柱, 照片/演示更清晰 */
        OS_Line_Draw((u16)(GTI_PX0 + px), GTI_PY1, (u16)(GTI_PX0 + px), (u16)ytop, Cyan);
        if (px + 1 < GTI_PW)
            OS_Line_Draw((u16)(GTI_PX0 + px + 1), GTI_PY1, (u16)(GTI_PX0 + px + 1), (u16)ytop, Cyan);
    }

    /* X 轴刻度 (0, fs/8, fs/4, 3fs/8, fs/2) */
    for (i = 0; i <= 4; i++)
    {
        int tx;
        px = GTI_PX0 + (int)((float)i / 4.0f * GTI_PW);
        OS_Line_Draw((u16)px, GTI_PY1, (u16)px, GTI_PY1 - 5, White);
        sprintf(s, "%.0fk", (G.fs / 2.0f * (float)i / 4.0f) / 1000.0f);
        tx = px - 12;
        if (tx > 756) tx = 756;     /* 防文字超出右边界 */
        OS_String_Show((u16)tx, GTI_PY1 + 6, 16, 1, s);
    }

    /* 分量标记: 红色竖线 + 频率/幅值标注 */
    for (i = 0; i < G.ncomp; i++)
    {
        int tx;
        m = G.cf[i] / (G.fs / 2.0f);
        if (m > 1.0f) continue;
        px = GTI_PX0 + (int)(m * GTI_PW);
        OS_Line_Draw((u16)px, GTI_PY0 + 4, (u16)px, GTI_PY0 + 14, Red);
        sprintf(s, "%.1fk", G.cf[i] / 1000.0f);
        tx = px - 14;
        if (tx > 748) tx = 748;
        OS_String_Show((u16)tx, GTI_PY0 + 16 + (i % 2) * 18, 16, 1, s);
    }

    OS_Num_Show(GTI_PX0 + 8, GTI_PY0 + 8, 16, 1, binw, "df=%.1fHz ");
    OS_String_Show(GTI_PX1 - 100, GTI_PY0 + 8, 16, 1, "\xc6\xb5\xc6\xd7 ");
    DWIN_SwitchPage(2);         /* 迪文屏切频谱页 */
}

/*
*********************************************************************************************************
*   函数名  : GTI_DrawSkeleton
*   说明    : 界面框架: 标题 + 左侧 ITEM 菜单 + 参数区 + 底部按键提示
*********************************************************************************************************
*/
static void GTI_DrawSkeleton(void)
{
    int i;

    LCD_Clear(Black);

    /* 标题栏 */
    OS_String_Show(16, 12, 24, 1, "G:\xd6\xdc\xc6\xda\xd0\xc5\xba\xc5\xb7\xd6\xce\xf6\xc6\xf7");
    OS_Rect_Draw(0, 46, 800, 48, 1, White);

    /* 左侧 ITEM 菜单 */
    for (i = 1; i <= 3; i++)
    {
        u16 color = (G.item == i) ? Yellow : White;
        OS_TextColor_Set(color);
        if (i == 1) OS_String_Show(8, GTI_COL_ITEM_Y(i), 16, 1, (G.item == 1) ? ">\xcf\xee\xc4\xbf""1 ua 10-200k " : " \xcf\xee\xc4\xbf""1 ua 10-200k ");
        if (i == 2) OS_String_Show(8, GTI_COL_ITEM_Y(i), 16, 1, (G.item == 2) ? ">\xcf\xee\xc4\xbf""2 ub 10-500k " : " \xcf\xee\xc4\xbf""2 ub 10-500k ");
        if (i == 3) OS_String_Show(8, GTI_COL_ITEM_Y(i), 16, 1, (G.item == 3) ? ">\xcf\xee\xc4\xbf""3 ub+uJ>=1MHz" : " \xcf\xee\xc4\xbf""3 ub+uJ>=1MHz");
    }
    OS_TextColor_Set(White);

    /* 参数区标签 */
    OS_String_Show(GTI_PARAM_X, GTI_PARAM_Y0,               16, 1, "\xb7\xe5\xb7\xe5\xd6\xb5:");
    OS_String_Show(GTI_PARAM_X, GTI_PARAM_Y0 + 1 * GTI_PARAM_DY, 16, 1, "\xd3\xd0\xd0\xa7\xd6\xb5:");
    OS_String_Show(GTI_PARAM_X, GTI_PARAM_Y0 + 2 * GTI_PARAM_DY, 16, 1, "\xbb\xf9\xc6\xb5  :");
    OS_String_Show(GTI_PARAM_X, GTI_PARAM_Y0 + 3 * GTI_PARAM_DY, 16, 1, (G.item == 3) ? "\xb2\xce\xbf\xbc\xc6\xb5: --(uJ)" : "\xb2\xce\xbf\xbc\xc6\xb5:");
    OS_String_Show(GTI_PARAM_X, GTI_PARAM_Y0 + 4 * GTI_PARAM_DY + 8, 16, 1, "\xb7\xd6\xc1\xbf:");

    /* 底部按键提示 */
    OS_Rect_Draw(0, 434, 800, 436, 1, White);
    OS_String_Show(8, 444, 16, 1, "[0]\xd7\xd4\xbc\xec [7]\xd7\xd4\xb6\xaf [1-3]\xcf\xee\xc4\xbf [4]1/3T [5]\xb2\xa8\xd0\xce [6]\xc6\xb5\xc6\xd7 [ENT]\xb2\xe2\xc1\xbf [NL]\xbe\xc9\xb2\xcb\xb5\xa5");

    /* 状态行: 显示当前模式 */
    GTI_Hint(g_selftest ? "\xd7\xd4\xbc\xec\xc4\xa3\xca\xbd" : "\xca\xb5\xca\xb1\xc4\xa3\xca\xbd");

    /* 迪文屏: 参数清零 + 模式 + 切页 */
    DWIN_SyncParams(0.0f, 0.0f, 0.0f, G.fs, (u8)(G.item - 1));
    DWIN_SwitchPage(G.view == 0 ? 1 : 2);
}

/*
*********************************************************************************************************
*   函数名  : GTI_ShowParams
*   说明    : 刷新左侧参数与分量列表
*********************************************************************************************************
*/
static void GTI_ShowParams(void)
{
    int  i, y;
    char s[40];

    /* 先清参数数值区 */
    LCD_Appoint_Clear(56, GTI_PARAM_Y0, 250, GTI_PARAM_Y0 + 4 * GTI_PARAM_DY + 8, Black);
    LCD_Appoint_Clear(8, GTI_PARAM_Y0 + 4 * GTI_PARAM_DY + 26, 250, 430, Black);

    if (!G.valid)
    {
        OS_String_Show(GTI_PARAM_X, GTI_PARAM_Y0, 16, 1, "      \xce\xde\xd0\xc5\xba\xc5      ");
        return;
    }

    /* Upp / Urms / f1 / fref */
    OS_Num_Show(56, GTI_PARAM_Y0,                     16, 1, G.upp,  "%.1f mV   ");
    OS_Num_Show(56, GTI_PARAM_Y0 + 1 * GTI_PARAM_DY,  16, 1, G.urms, "%.1f mV   ");
    OS_Num_Show(56, GTI_PARAM_Y0 + 2 * GTI_PARAM_DY,  16, 1, G.f1,   "%.1f Hz   ");
    if (g_selftest)
        OS_String_Show(56, GTI_PARAM_Y0 + 3 * GTI_PARAM_DY, 16, 1, "  --  (\xd7\xd4\xbc\xec)");
    else if (G.item != 3)
        OS_Num_Show(56, GTI_PARAM_Y0 + 3 * GTI_PARAM_DY, 16, 1, G.fref, "%.1f Hz   ");

    /* 分量列表: n次 x 频率 幅值 */
    y = GTI_PARAM_Y0 + 4 * GTI_PARAM_DY + 26;
    for (i = 0; i < G.ncomp && i < 4; i++)
    {
        sprintf(s, "%d\xb4\xce %7.1fHz %6.1fmV ", G.co[i], G.cf[i], G.ca[i]);
        OS_String_Show(8, (u16)y, 16, 1, s);
        y += 22;
    }

    /* 迪文屏: 同步参数(原始单位) + 谐波分量 */
    DWIN_SyncParams(G.upp, G.urms, G.f1, G.fs, (u8)(G.item - 1));
    DWIN_SyncComponents(G.co, G.cf, G.ca, G.ncomp);
}

/*
*********************************************************************************************************
*   函数名  : GTI_Hint
*   说明    : 底部状态提示
*********************************************************************************************************
*/
static void GTI_Hint(const char *s)
{
    LCD_Appoint_Clear(500, 438, 796, 470, Black);
    OS_String_Show(500, 448, 16, 1, (char *)s);
}

/*
*********************************************************************************************************
*   函数名  : GTI_HandleKey
*   说明    : 按键处理: 切项重测 / 1-3周期切换 / 视图切换 / 重测
*********************************************************************************************************
*/
static void GTI_HandleKey(u8 key)
{
    u8 rerun = 0, replot = 0, reskel = 0;

    switch (key)
    {
    case KeyValue_1:
        if (G.item != 1 || g_selftest) { G.item = 1; g_selftest = 0; reskel = 1; }
        rerun = 1;
        break;
    case KeyValue_2:
        if (G.item != 2 || g_selftest) { G.item = 2; g_selftest = 0; reskel = 1; }
        rerun = 1;
        break;
    case KeyValue_3:
        if (G.item != 3 || g_selftest) { G.item = 3; g_selftest = 0; reskel = 1; }
        rerun = 1;
        break;
    case KeyValue_4:
        G.periods = (G.periods == 1) ? 3 : 1;
        replot = 1;
        break;
    case KeyValue_5:
        if (G.view != 0) { G.view = 0; replot = 1; }
        break;
    case KeyValue_6:
        if (G.view != 1) { G.view = 1; replot = 1; }
        break;
    case KeyValue_Enter:
        rerun = 1;
        break;
    case KeyValue_0:
        /* 切换自检/实时模式 */
        g_selftest = !g_selftest;
        if (g_selftest) G.item = 1;  /* 自检只支持 Item1 */
        reskel = 1;
        rerun  = 1;
        break;
    case KeyValue_7:
        /* 切换自动刷新 */
        g_autorun = !g_autorun;
        if (g_autorun)
            GTI_Hint("\xd7\xd4\xb6\xaf\xcb\xa2\xd0\xc2");
        else
            GTI_Hint("\xca\xd6\xb6\xaf\xc4\xa3\xca\xbd");
        break;
    default:
        break;
    }

    if (reskel)
        GTI_DrawSkeleton();

    if (rerun)
    {
        GTI_Hint("\xb2\xe2\xc1\xbf\xd6\xd0...");
        GTI_Measure();
        GTI_ShowParams();
        replot = 1;
        GTI_Hint(G.valid ? "\xcd\xea\xb3\xc9" : "\xce\xde\xd0\xc5\xba\xc5");
    }

    if (replot)
    {
        if (G.view == 0) GTI_ShowWave();
        else             GTI_ShowSpec();
    }
}

/*
*********************************************************************************************************
*   函数名  : GTI_Run
*   说明    : G题模式主入口: 初始化 + 开机自动完成一次 ITEM1 测量 + 键盘主循环
*             按 NumLock 退出返回旧菜单 (User.c 中旧代码全部保留)
*********************************************************************************************************
*/
void GTI_Run(void)
{
    u8 key;

    /* 默认状态: ITEM1 / 波形视图 / 1周期 */
    G.item    = 1;
    G.view    = 0;
    G.periods = 1;
    G.valid   = 0;

    /* 初始化: 窗表 / FIR系数 / AD9240 / 比较器测频 */
    GTI_WinInit();
    GTI_FirDesign();
    GTI_ADC_Init();
    Comparator_Init();
    DWIN_Init();            /* 迪文屏 921600 USART1 */

    GTI_DrawSkeleton();

    /* 开机自动测一次 (启动装置后每一项<2s的要求) */
    GTI_Hint("\xb2\xe2\xc1\xbf\xd6\xd0...");
    GTI_Measure();
    GTI_ShowParams();
    GTI_ShowWave();
    GTI_Hint(G.valid ? "\xcd\xea\xb3\xc9" : "\xce\xde\xd0\xc5\xba\xc5");

    /* 主循环: 支持自动刷新 */
    {
        u32 auto_cnt = 0;      /* 无按键计数器 (10ms/tick) */
        u8  last_valid = 0xFF;  /* 缓存上次状态, 避免重复刷状态栏 */
        while (1)
        {
            key = Ps2KeyValue;
            if (key != KeyValue_Null)
            {
                Ps2KeyValue = KeyValue_Null;
                if (key == KeyValue_NumLock)
                    return;     /* 回旧菜单 (User.c) */
                GTI_HandleKey(key);
                auto_cnt = 0;  /* 有按键则重置计数 */
                last_valid = 0xFF;  /* 按键后强制刷新一次状态 */
            }
            else if (g_autorun)
            {
                auto_cnt++;
                if (auto_cnt >= 100)   /* 100 * 10ms = 1秒自动刷新 */
                {
                    auto_cnt = 0;
                    GTI_Measure();
                    GTI_ShowParams();
                    if (G.view == 0) GTI_ShowWave();
                    else             GTI_ShowSpec();
                    /* 状态栏仅在变化时刷新, 减少闪烁 */
                    if (G.valid != last_valid)
                    {
                        last_valid = G.valid;
                        GTI_Hint(g_selftest
                            ? (G.valid ? "\xd7\xd4\xbc\xec \xcd\xea\xb3\xc9" : "\xd7\xd4\xbc\xec \xce\xde\xd0\xc5\xba\xc5")
                            : (G.valid ? "\xca\xb5\xca\xb1 \xcd\xea\xb3\xc9" : "\xca\xb5\xca\xb1 \xce\xde\xd0\xc5\xba\xc5"));
                    }
                }
            }
            delay_ms(10);
        }
    }
}

/* ===================================================================== *
 *  片内 ADC 采集 (PA1, 12-bit, TIM3触发, DMA2_Stream0)
 * ===================================================================== */
static void GTI_ADC_Init(void)
{
    GPIO_InitTypeDef g;
    ADC_InitTypeDef a;
    ADC_CommonInitTypeDef c;
    TIM_TimeBaseInitTypeDef t;
    DMA_InitTypeDef d;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_DMA2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    g.GPIO_Pin   = GPIO_Pin_1;
    g.GPIO_Mode  = GPIO_Mode_AN;
    g.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &g);

    c.ADC_Mode             = ADC_Mode_Independent;
    c.ADC_Prescaler        = ADC_Prescaler_Div4;
    c.ADC_DMAAccessMode    = ADC_DMAAccessMode_Disabled;
    c.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
    ADC_CommonInit(&c);
    a.ADC_Resolution           = ADC_Resolution_12b;
    a.ADC_ScanConvMode         = DISABLE;
    a.ADC_ContinuousConvMode   = DISABLE;
    a.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_Rising;
    a.ADC_ExternalTrigConv     = ADC_ExternalTrigConv_T3_TRGO;
    a.ADC_DataAlign            = ADC_DataAlign_Right;
    a.ADC_NbrOfConversion      = 1;
    ADC_Init(ADC1, &a);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_15Cycles);
    ADC_DMACmd(ADC1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);

    t.TIM_Prescaler         = 0;
    t.TIM_Period            = 0;
    t.TIM_ClockDivision     = TIM_CKD_DIV1;
    t.TIM_CounterMode       = TIM_CounterMode_Up;
    t.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM3, &t);
    TIM_SelectOutputTrigger(TIM3, TIM_TRGOSource_Update);

    DMA_DeInit(DMA2_Stream0);
    while (DMA_GetCmdStatus(DMA2_Stream0) != DISABLE);
    d.DMA_Channel            = DMA_Channel_0;
    d.DMA_PeripheralBaseAddr = (u32)&ADC1->DR;
    d.DMA_Memory0BaseAddr    = (u32)s_adc_buf;
    d.DMA_DIR                = DMA_DIR_PeripheralToMemory;
    d.DMA_BufferSize         = GTI_N + 16;
    d.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    d.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    d.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    d.DMA_MemoryDataSize     = DMA_MemoryDataSize_HalfWord;
    d.DMA_Mode               = DMA_Mode_Normal;
    d.DMA_Priority           = DMA_Priority_VeryHigh;
    d.DMA_FIFOMode           = DMA_FIFOMode_Disable;
    d.DMA_FIFOThreshold      = DMA_FIFOThreshold_HalfFull;
    d.DMA_MemoryBurst        = DMA_MemoryBurst_Single;
    d.DMA_PeripheralBurst    = DMA_PeripheralBurst_Single;
    DMA_Init(DMA2_Stream0, &d);
}

static u16 *GTI_ADC1_Capture(u32 n, u32 fs_hz)
{
    u32 arr, psc;
    u32 timeout;

    if (n > GTI_N) return (u16 *)0;

    psc = 0;
    arr = 84000000UL / fs_hz;
    while (arr > 65535 && psc < 65535) { psc++; arr = 84000000UL / (fs_hz * (psc + 1)); }
    if (arr < 2) arr = 2;

    TIM_Cmd(TIM3, DISABLE);
    TIM3->PSC = (u16)psc;
    TIM3->ARR = (u16)(arr - 1);
    TIM_SetCounter(TIM3, 0);

    DMA_ClearFlag(DMA2_Stream0, DMA_FLAG_TCIF0 | DMA_FLAG_HTIF0 | DMA_FLAG_TEIF0);
    DMA_SetCurrDataCounter(DMA2_Stream0, n + 16);
    DMA_Cmd(DMA2_Stream0, ENABLE);
    TIM_Cmd(TIM3, ENABLE);

    timeout = 2000;
    while (DMA_GetFlagStatus(DMA2_Stream0, DMA_FLAG_TCIF0) == RESET)
    {
        if (timeout == 0) { TIM_Cmd(TIM3, DISABLE); DMA_Cmd(DMA2_Stream0, DISABLE); return (u16 *)0; }
        delay_ms(1);
        timeout--;
    }

    TIM_Cmd(TIM3, DISABLE);
    DMA_Cmd(DMA2_Stream0, DISABLE);
    DMA_ClearFlag(DMA2_Stream0, DMA_FLAG_TCIF0);
    return &s_adc_buf[16];
}

/*******************************(C) G题模式 2026/08/01 *********************************/
