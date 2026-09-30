/*
*********************************************************************************************************
                                               _04_OS
    File       : Drive_Comparator.c
    platform   : STM32F407ZG
    function   : 绛夌簿搴﹂?戠巼娴嬮噺 (TLV3501 鏁村舰 -> TIM1 杈撳叆鎹曡幏 PA8)
*********************************************************************************************************
*/
#include "Drive_Comparator.h"

/*
 * 瀹炵幇鎬濊矾:
 *   - TIM1 CH1 (PA8) 杈撳叆鎹曡幏, 瀵瑰緟娴嬩俊鍙蜂笂鍗囨部璁℃暟
 *   - TIM2 浣滀负 1MHz 鏍囧噯鏃堕挓 (鍐呴儴璁℃暟), 鎻愪緵闂搁棬鍩哄噯
 *   - 闂搁棬鏃堕棿鍒板悗, 鍦ㄤ腑鏂?涓?鍙?璇诲彇鍘熷?嬫暣鍨嬭?℃暟鍊?, 涓嶅仛娴?鐐硅繍绠?
 *   - 涓诲惊鐜?璋冪敤 Get_Frequency() 璁＄畻娴?鐐圭粨鏋?:
 *       Freq = (Signal_Count / Clock_Count) * 1000000.0f
 *
 * 璧勬簮鍗犵敤:
 *   - TIM1 (PA8 杈撳叆鎹曡幏 + 鏇存柊涓?鏂?)
 *   - TIM2 (1MHz 鏍囧噯鏃堕挓 + 鏇存柊涓?鏂?)
 *   - NVIC: TIM1_CC_IRQn / TIM1_UP_TIM10_IRQn / TIM2_IRQn
 */

#define COMP_TIM_APB1_HZ        84000000UL    /* TIM2 鎸傚湪 APB1 (84MHz)  */
#define COMP_TIM_APB2_HZ        168000000UL   /* TIM1 鎸傚湪 APB2 (168MHz) */
#define COMP_CLOCK_HZ           1000000UL     /* TIM2 鏍囧噯 1MHz          */

/*
 * 闂搁棬鏃堕棿瀵瑰簲鐨? TIM2 璁℃暟:
 *   TIM2 璁? 1 涓?鏁? = 1us, 闂搁棬 GATE_TIME_MS ms => GATE_TIME_MS*1000 涓?璁℃暟
 */
#define GATE_CLOCK_COUNT        ((u32)(GATE_TIME_MS) * 1000UL)

/*
 * TIM2 浜х敓 1MHz: 棰勫垎棰? = 84 - 1, 鍛ㄦ湡 = GATE_CLOCK_COUNT
 *   update 棰戠巼 = 84MHz / 84 / GATE_CLOCK_COUNT = 1MHz / GATE_CLOCK_COUNT
 *   => 姣? GATE_TIME_MS ms 浜х敓涓?娆? update 涓?鏂?
 */
#define TIM2_PRESCALER          (COMP_TIM_APB1_HZ / COMP_CLOCK_HZ - 1UL)
#define TIM2_PERIOD             (GATE_CLOCK_COUNT - 1UL)

/*
 * TIM1 瀵瑰緟娴嬩俊鍙蜂笂鍗囨部鎹曡幏璁℃暟, 棰勫垎棰?=0, 鍛ㄦ湡鍙栨渶澶у??
 * 璁℃暟婧㈠嚭鐢? update 涓?鏂?绱?鍔?, 瀹為檯寰呮祴淇″彿璁℃暟 = 婧㈠嚭娆℃暟*65536 + 褰撳墠鎹曡幏鍊?
 */
#define TIM1_PRESCALER          0
#define TIM1_PERIOD             0xFFFF

/* ---- 涓?鏂?涓庝富寰?鐜?鍏变韩鐨勫師濮嬭?℃暟鍙橀噺 (鍧囬渶 volatile) ---- */
static volatile u32 g_signal_count = 0;   /* 鏈?娆￠椄闂ㄥ唴寰呮祴淇″彿璁℃暟   */
static volatile u32 g_clock_count  = 0;   /* 鏈?娆￠椄闂ㄥ唴鏍囧噯鏃堕挓璁℃暟   */
static volatile u8  g_measure_done = 0;   /* 涓?娆℃祴閲忓畬鎴愭爣蹇?         */

/* TIM1 璁℃暟婧㈠嚭绱?璁? (鍦ㄤ腑鏂?涓?缁存姢) */
static volatile u32 g_tim1_overflow = 0;
/* TIM2 璁℃暟婧㈠嚭绱?璁? (鐞嗚?轰笂=闂搁棬娆℃暟, 鐣欎綔鎵╁睍) */
static volatile u32 g_tim2_overflow = 0;

/* 闂搁棬鎺у埗: 0=绛夊緟寮?鍚?, 1=娴嬮噺涓?, 2=娴嬮噺瀹屾垚绛夊緟璇诲彇 */
static volatile u8  g_gate_state = 0;

/*
*********************************************************************************************************
*   鍑芥暟鍚? : Comparator_Init
*   璇?  鏄? : 鍒濆?嬪寲 TIM1 (PA8 杈撳叆鎹曡幏) 鍜? TIM2 (1MHz 鏍囧噯鏃堕挓), 浣胯兘涓?鏂?
*   鍙?  鏁? : 鏃?
*   杩斿洖鍊? : 鏃?
*********************************************************************************************************
*/
void Comparator_Init(void)
{
    GPIO_InitTypeDef        GPIO_InitStructure;
    NVIC_InitTypeDef        NVIC_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruct;
    TIM_ICInitTypeDef       TIM_ICInitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* PA8 澶嶇敤涓? TIM1_CH1 杈撳叆 */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;   /* 涓嬫媺, 鏃犱俊鍙锋椂淇濇寔浣庣數骞? */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource8, GPIO_AF_TIM1);

    /* ---- NVIC 閰嶇疆 (涓庨」鐩?涓?鑷翠娇鐢? Group2) ---- */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    /* TIM1 鎹曡幏姣旇緝涓?鏂? */
    NVIC_InitStructure.NVIC_IRQChannel                   = TIM1_CC_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* TIM1 鏇存柊涓?鏂? (璁℃暟婧㈠嚭绱?鍔?) */
    NVIC_InitStructure.NVIC_IRQChannel                   = TIM1_UP_TIM10_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* TIM2 鏇存柊涓?鏂? (闂搁棬鏃堕棿鍒?) */
    NVIC_InitStructure.NVIC_IRQChannel                   = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;  /* 闂搁棬浼樺厛绾ф渶楂? */
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* ---- TIM1 鏃跺熀: 瀵瑰?栭儴淇″彿鑷?鐢辫?℃暟 ---- */
    TIM_TimeBaseInitStruct.TIM_Prescaler         = TIM1_PRESCALER;
    TIM_TimeBaseInitStruct.TIM_Period            = TIM1_PERIOD;
    TIM_TimeBaseInitStruct.TIM_ClockDivision     = TIM_CKD_DIV1;
    TIM_TimeBaseInitStruct.TIM_CounterMode       = TIM_CounterMode_Up;
    TIM_TimeBaseInitStruct.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseInitStruct);

    /* ---- TIM1 CH1 杈撳叆鎹曡幏 (涓婂崌娌?, 鐩磋繛, 涓嶅垎棰?) ---- */
    TIM_ICInitStructure.TIM_Channel     = TIM_Channel_1;
    TIM_ICInitStructure.TIM_ICPolarity  = TIM_ICPolarity_Rising;
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    TIM_ICInitStructure.TIM_ICFilter    = 0x6;  /* 数字滤波: 过滤ns级振铃, 不影响100kHz信号 */
    TIM_ICInit(TIM1, &TIM_ICInitStructure);

    /* ---- TIM2 鏃跺熀: 1MHz 鏍囧噯鏃堕挓, 闂搁棬鍛ㄦ湡 ---- */
    TIM_TimeBaseInitStruct.TIM_Prescaler     = TIM2_PRESCALER;
    TIM_TimeBaseInitStruct.TIM_Period        = TIM2_PERIOD;
    TIM_TimeBaseInitStruct.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStruct.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStruct);

    /* 娓呮爣蹇?, 閬垮厤涓?寮?涓?鏂?灏辫Е鍙? */
    TIM_ClearITPendingBit(TIM1, TIM_IT_CC1 | TIM_IT_Update);
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

    /* 寮?鍚?涓?鏂? */
    TIM_ITConfig(TIM1, TIM_IT_CC1 | TIM_IT_Update, ENABLE);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    /* 澶嶄綅璁℃暟鍣ㄤ笌鐘舵??, 鍑嗗?囩??涓?娆℃祴閲? */
    g_tim1_overflow = 0;
    g_tim2_overflow = 0;
    g_signal_count  = 0;
    g_clock_count   = 0;
    g_measure_done  = 0;
    g_gate_state    = 1;   /* 杩涘叆娴嬮噺涓? */

    /* 鍚屾椂鍚?鍔ㄤ袱涓?瀹氭椂鍣?, 闂搁棬寮?濮? */
    TIM_Cmd(TIM1, ENABLE);
    TIM_Cmd(TIM2, ENABLE);
}

/*
*********************************************************************************************************
*   鍑芥暟鍚? : Get_Frequency
*   璇?  鏄? : 杩斿洖鏈?鏂版祴寰楃殑棰戠巼鍊? (Hz), 鐢变富寰?鐜?璋冪敤
*           鑻ュ綋鍓嶆棤鏈夋晥娴嬮噺缁撴灉, 杩斿洖 0
*   鍙?  鏁? : 鏃?
*   杩斿洖鍊? : 棰戠巼 (Hz)
*********************************************************************************************************
*/
float Get_Frequency(void)
{
    u32 sig, clk;
    u8  done;

    /* 鍏充腑鏂?璇诲彇涓?缁勪竴鑷寸殑鏁版嵁 */
    __disable_irq();
    sig  = g_signal_count;
    clk  = g_clock_count;
    done = g_measure_done;
    __enable_irq();

    if (done == 0 || clk == 0)
    {
        return 0.0f;
    }

    /* 娴?鐐硅繍绠楁斁鍦ㄤ富寰?鐜?, 涓?鏂?閲屽彧缁存姢鏁存暟璁℃暟 */
    return ((float)sig / (float)clk) * 1000000.0f;
}

/*
*********************************************************************************************************
*   鍑芥暟鍚? : TIM1_CC_IRQHandler
*   璇?  鏄? : TIM1 鎹曡幏涓?鏂?, 姣忎釜寰呮祴淇″彿涓婂崌娌胯Е鍙戜竴娆?, 绱?鍔犱俊鍙疯?℃暟
*********************************************************************************************************
*/
void TIM1_CC_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM1, TIM_IT_CC1) != RESET)
    {
        TIM_ClearITPendingBit(TIM1, TIM_IT_CC1);

        if (g_gate_state == 1)
        {
            g_signal_count++;
        }
    }
}

/*
*********************************************************************************************************
*   鍑芥暟鍚? : TIM1_UP_TIM10_IRQHandler
*   璇?  鏄? : TIM1 鏇存柊涓?鏂?, 16浣嶈?℃暟鍣ㄦ孩鍑虹疮鍔? (鐞嗚?轰笂闂搁棬鍐呭緢灏戞孩鍑?, 浠呬綔淇濇姢)
*********************************************************************************************************
*/
void TIM1_UP_TIM10_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM1, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
        if (g_gate_state == 1)
        {
            g_tim1_overflow++;
        }
    }
}

/*
*********************************************************************************************************
*   鍑芥暟鍚? : TIM2_IRQHandler
*   璇?  鏄? : TIM2 鏇存柊涓?鏂?, 闂搁棬鏃堕棿鍒?, 缁撴潫鏈?娆℃祴閲忓苟鍑嗗?囦笅涓?娆?
*********************************************************************************************************
*/
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

        if (g_gate_state == 1)
        {
            /* 闂搁棬缁撴潫: 璁板綍鏈?娆℃爣鍑嗘椂閽熻?℃暟, 缃?瀹屾垚鏍囧織 */
            g_clock_count  = GATE_CLOCK_COUNT;
            g_measure_done = 1;
            g_gate_state   = 2;   /* 绛夊緟涓诲惊鐜?璇诲彇 */

            /* 鍋? TIM1 鍋滄?㈣?℃暟, 涓诲惊鐜?璇诲彇鍚庝細閲嶅惎 */
            TIM_Cmd(TIM1, DISABLE);
        }
    }
}

/*
*********************************************************************************************************
*   鍑芥暟鍚? : Comparator_Restart (渚涗富寰?鐜?璋冪敤, 闈炰腑鏂?)
*   璇?  鏄? : 璇诲彇瀹屼竴娆＄粨鏋滃悗, 閲嶅惎涓嬩竴杞?娴嬮噺
*********************************************************************************************************
*/
void Comparator_Restart(void)
{
    __disable_irq();
    g_signal_count  = 0;
    g_clock_count   = 0;
    g_measure_done  = 0;
    g_tim1_overflow = 0;
    g_tim2_overflow = 0;
    g_gate_state    = 1;

    TIM_SetCounter(TIM1, 0);
    TIM_SetCounter(TIM2, 0);
    TIM_ClearITPendingBit(TIM1, TIM_IT_CC1 | TIM_IT_Update);
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

    TIM_Cmd(TIM1, ENABLE);
    TIM_Cmd(TIM2, ENABLE);
    __enable_irq();
}

/*
*********************************************************************************************************
*   Comparator_GetDebug -- diagnostic for "no signal" troubleshooting
*********************************************************************************************************
*/
void Comparator_GetDebug(u32 *sig_count, u8 *done, u8 *gate_state)
{
    __disable_irq();
    if (sig_count)  *sig_count  = g_signal_count;
    if (done)       *done       = g_measure_done;
    if (gate_state) *gate_state = g_gate_state;
    __enable_irq();
}
