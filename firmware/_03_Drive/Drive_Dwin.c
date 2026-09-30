/*
 * Drive_Dwin.c  -- 迪文DGUS II 串口屏驱动 (匹配已烧录屏幕配置)
 *
 * 帧格式: 5A A5 [LenL LenH] 82 [VP_H VP_L] [Data...]
 * 数据均为小端 (含 float IEEE 754)
 * 发送缓冲 256B, 单帧最多 ~250B
 */
#include "Drive_Dwin.h"
#include "usart.h"
#include "string.h"

/* ========================================================
 *  内部状态
 * ======================================================== */
static u8  s_enabled = 1;
static u8  s_buf[256];

/* ========================================================
 *  底层发送
 * ======================================================== */

/* 单字节阻塞发送 */
static void _tx_byte(u8 b)
{
    while ((USART1->SR & 0x40) == 0);
    USART1->DR = b;
}

/* 发送缓冲区 */
static void _send(u16 len)
{
    u16 i;
    if (!s_enabled) return;
    for (i = 0; i < len; i++)
        _tx_byte(s_buf[i]);
}

/*
 * 帧头填充: 5A A5 + 长度(小端) + 82 + VP地址(大端)
 * 返回数据区写入指针, data_len 不含帧头/长度/命令/VP
 */
static u8 *_frame_start(u16 vp, u16 data_len)
{
    u8 *p = s_buf;
    u16 total = 1 + 2 + data_len;   /* 82(1B) + VP(2B) + Data */

    *p++ = DWIN_HEAD_H;             /* 5A */
    *p++ = DWIN_HEAD_L;             /* A5 */

    *p++ = (u8)(total & 0xFF);      /* 长度 小端 */
    *p++ = (u8)(total >> 8);

    *p++ = DWIN_CMD_WRITE;          /* 82 */

    *p++ = (u8)(vp >> 8);           /* VP 大端 */
    *p++ = (u8)(vp & 0xFF);

    return p;
}

/* ========================================================
 *  公共接口
 * ======================================================== */

/*
 * 初始化 USART1 @921600
 */
void DWIN_Init(void)
{
    uart_init(DWIN_BAUD);
    s_enabled = 1;
}

/*
 * 使能/禁用发送 (未接屏时禁用避免阻塞)
 */
void DWIN_Enable(u8 enable)
{
    s_enabled = enable ? 1 : 0;
}

/*
 * 页面切换: 向 0x0084 写 u16
 */
void DWIN_SwitchPage(u8 page)
{
    DWIN_WriteU16(DWIN_VP_PAGE, (u16)page);
}

/*
 * 写 u16 (大端, 用于 DGUS uint16 控件)
 */
void DWIN_WriteU16(u16 vp, u16 val)
{
    u8 *p = _frame_start(vp, 2);
    *p++ = (u8)(val >> 8);
    *p++ = (u8)(val & 0xFF);
    _send((u16)(p - s_buf));
}

/*
 * 写 float (IEEE 754 小端, 用于 DGUS float 控件)
 */
void DWIN_WriteFloat(u16 vp, float val)
{
    union { float f; u8 b[4]; } u;
    u8 *p;

    u.f = val;
    p = _frame_start(vp, 4);

    /* 小端: 低字节在前 */
    *p++ = u.b[0];
    *p++ = u.b[1];
    *p++ = u.b[2];
    *p++ = u.b[3];

    _send((u16)(p - s_buf));
}

/*
 * 设置模式指示: 写 u16 到 VP 0x100C
 */
void DWIN_SetMode(u8 item)
{
    if (item > 2) item = 0;
    DWIN_WriteU16(DWIN_VP_MODE, (u16)item);
}

/*
 * 一键同步参数到 DGUS 控件
 *
 * upp_mv / urms_mv : mV 实际值 → float mV (无转换)
 * f1_hz            : Hz 实际值 → float kHz (/1000)
 * fs_hz            : Hz 实际值 → float MHz (/1e6)
 * mode             : 0=Item1, 1=Item2, 2=Item3
 */
void DWIN_SyncParams(float upp_mv, float urms_mv, float f1_hz, float fs_hz, u8 mode)
{
    DWIN_WriteFloat(DWIN_VP_UPP,  upp_mv);
    DWIN_WriteFloat(DWIN_VP_URMS, urms_mv);
    DWIN_WriteFloat(DWIN_VP_F1,   f1_hz / 1000.0f);
    DWIN_WriteFloat(DWIN_VP_FS,   fs_hz / 1000000.0f);
    DWIN_SetMode(mode);
}

/*
 * 发送谐波分量
 *
 * DGUS 有 8 个固定槽位 H1~H8, 按 order[i] 映射到对应槽:
 *   H1: VP 0x1010(freq) + 0x1030(amp)
 *   H2: VP 0x1014(freq) + 0x1034(amp)
 *   ...
 *   H8: VP 0x102C(freq) + 0x104C(amp)
 *
 * 先全清零, 再填有效分量.
 * freq_hz: Hz → kHz (/1000), amp_mv: mV 直接写
 */
void DWIN_SyncComponents(const int *order, const float *freq_hz, const float *amp_mv, u8 n)
{
    u8 i;
    int idx;

    /* 1. 清空全部 8 个槽位 */
    for (i = 0; i < 8; i++)
    {
        DWIN_WriteFloat(DWIN_VP_HF_BASE + i * 4, 0.0f);
        DWIN_WriteFloat(DWIN_VP_HA_BASE + i * 4, 0.0f);
    }

    /* 2. 写谐波个数 */
    DWIN_WriteU16(DWIN_VP_NHARM, (u16)n);

    /* 3. 填有效分量: 按次数映射到对应槽 */
    for (i = 0; i < n && i < 8; i++)
    {
        idx = order[i];
        if (idx < 1 || idx > 8) continue;   /* 仅支持 1~8 次 */

        idx--;  /* 转为0基 */
        DWIN_WriteFloat(DWIN_VP_HF_BASE + (u16)idx * 4, freq_hz[i] / 1000.0f);
        DWIN_WriteFloat(DWIN_VP_HA_BASE + (u16)idx * 4, amp_mv[i]);
    }
}

/*
 * 读取触摸键值: 通过 0x83 命令读 VP 0x1060 的 u16
 * 读完后回写 0x0000 清除, 避免重复触发
 *
 * 注: 本项目以 PS2 键盘为主, 此函数保留备用
 */
u16 DWIN_ReadTouch(void)
{
    u8 *p = s_buf;
    u16 val;

    if (!s_enabled) return 0;

    /* 读 VP 命令: 5A A5 04 83 [VP_H VP_L] [1] */
    *p++ = DWIN_HEAD_H;
    *p++ = DWIN_HEAD_L;
    *p++ = 0x04;                    /* 长度(小端): 83+VP+1字 = 4 */
    *p++ = 0x00;
    *p++ = 0x83;                    /* 读变量命令 */
    *p++ = (u8)(DWIN_VP_TOUCH >> 8);
    *p++ = (u8)(DWIN_VP_TOUCH & 0xFF);
    *p++ = 0x01;                    /* 读 1 个字 */
    _send((u16)(p - s_buf));

    /*
     * 等待屏返回 (简化实现: 直接读 USART1->DR)
     *  返回帧: 5A A5 06 83 [VP_H VP_L] [Data_H Data_L]
     *  超时 ~2ms @ 921600 (约 200 字节时间)
     */
    {
        u32 tmo = 200000;
        u8  rx[10];
        u8  ri = 0;

        while (tmo--)
        {
            if (USART1->SR & 0x20)   /* RXNE */
            {
                rx[ri++] = (u8)(USART1->DR & 0xFF);
                if (ri >= 9) break;  /* 5A A5 06 83 VP_H VP_L D_H D_L = 9B */
            }
        }

        if (ri >= 9 && rx[0] == 0x5A && rx[1] == 0xA5)
            val = ((u16)rx[7] << 8) | rx[8];
        else
            val = 0;
    }

    /* 清除键值 */
    if (val != 0)
        DWIN_WriteU16(DWIN_VP_TOUCH, 0x0000);

    return val;
}

/*
 * 发送曲线数据到 DGUS 曲线控件
 *
 * 协议: 5A A5 [Len] 82 [VP_H VP_L] [Ch_H Ch_L] [Count_H Count_L] [Y0_H Y0_L] ...
 * vp   : DGUS Tool 中曲线控件的"变量存储地址"
 * ch   : 曲线通道号 (0x01=波形, 0x02=频谱)
 * data : Y 值数组 (u16), DGUS 自动均分 X 轴
 * count: 数据点数 (建议 ≤ 200 避免一帧过长)
 *
 * 注: 波形数据需要调用方先归一化到曲线控件高度范围 (e.g. 0~340)
 */
void DWIN_SendCurve(u16 vp, u16 ch, const u16 *data, u16 count)
{
    u16 i;
    u16 data_len;
    u8 *p;

    if (count == 0 || data == 0) return;
    if (count > 200) count = 200;   /* 单帧上限 */

    data_len = 4 + count * 2;       /* Ch(2B) + Count(2B) + Y值 */

    p = _frame_start(vp, data_len);

    /* 通道号 (大端) */
    *p++ = (u8)(ch >> 8);
    *p++ = (u8)(ch & 0xFF);

    /* 点数 (大端) */
    *p++ = (u8)(count >> 8);
    *p++ = (u8)(count & 0xFF);

    /* Y 值序列 (大端) */
    for (i = 0; i < count; i++)
    {
        *p++ = (u8)(data[i] >> 8);
        *p++ = (u8)(data[i] & 0xFF);
    }

    _send((u16)(p - s_buf));
}
