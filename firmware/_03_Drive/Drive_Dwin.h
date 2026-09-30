/*
 * Drive_Dwin.h  -- 迪文DGUS II 7寸串口屏驱动 (匹配已烧录屏幕配置)
 * 串口: USART1 PA9(TX) @921600, 5A A5 帧头
 * 数据类型: float (IEEE 754 小端) + uint16 (大端)
 * 3页: Page0=参数 Page1=波形+参数 Page2=频谱+谐波
 * 纯显示模式, PS2键盘全控制
 */
#ifndef _DRIVE_DWIN_H
#define _DRIVE_DWIN_H

#include "sys.h"

#define DWIN_BAUD       921600

/* ========================================================
 *  帧定义
 * ======================================================== */
#define DWIN_HEAD_H         0x5A
#define DWIN_HEAD_L         0xA5
#define DWIN_CMD_WRITE      0x82

/* ========================================================
 *  系统控制
 * ======================================================== */
#define DWIN_VP_PAGE        0x0084   /* 页面切换: 写u16, 0~2 */

/* ========================================================
 *  数据变量 VP 地址 (匹配已烧录DGUS配置)
 * ======================================================== */
#define DWIN_VP_F1          0x1000   /* 基频      float kHz   Page0/1 */
#define DWIN_VP_UPP         0x1004   /* 峰峰值    float mV    Page0/1 */
#define DWIN_VP_URMS        0x1008   /* 真有效值  float mV    Page0/1 */
#define DWIN_VP_MODE        0x100C   /* 测量模式  uint16 0/1/2  Page0 */
#define DWIN_VP_FS          0x1054   /* 采样率    float MHz   Page0 */
#define DWIN_VP_NHARM       0x1050   /* 谐波个数  uint16      Page2 */

/* 谐波分量: 频率 0x1010+4*(i-1), 幅值 0x1030+4*(i-1), i=1~8, float */
#define DWIN_VP_HF_BASE     0x1010   /* H1频率 float kHz */
#define DWIN_VP_HA_BASE     0x1030   /* H1幅值 float mV  */

/* ========================================================
 *  触摸键值 (从 VP 0x1060 读取, 保留但本项目用键盘)
 * ======================================================== */
#define DWIN_VP_TOUCH       0x1060
#define DWIN_KEY_ITEM1      0x0001
#define DWIN_KEY_ITEM2      0x0002
#define DWIN_KEY_ITEM3      0x0003
#define DWIN_KEY_WAVE1      0x0004
#define DWIN_KEY_WAVE3      0x0005
#define DWIN_KEY_PARAMS     0x0006
#define DWIN_KEY_SPECTRUM   0x0007
#define DWIN_KEY_AMP_VAL    0x0008
#define DWIN_KEY_CALIB      0x0009

/* ========================================================
 *  曲线通道 (DGUS 曲线控件, 需与 DGUS Tool 配置一致)
 * ======================================================== */
#define DWIN_VP_CURVE_W     0x2000   /* 波形曲线数据区 VP */
#define DWIN_VP_CURVE_S     0x3000   /* 频谱曲线数据区 VP */
#define DWIN_CH_WAVE        0x01     /* 波形通道号 */
#define DWIN_CH_SPEC        0x02     /* 频谱通道号 */

/* ========================================================
 *  公共接口
 * ======================================================== */

void DWIN_Init(void);
void DWIN_Enable(u8 enable);

/* 页面切换 (0~2) */
void DWIN_SwitchPage(u8 page);

/* 写变量 */
void DWIN_WriteU16(u16 vp, u16 val);
void DWIN_WriteFloat(u16 vp, float val);       /* IEEE 754 小端 */

/* 模式指示: 写 u16 到 0x100C (0=Item1, 1=Item2, 2=Item3) */
void DWIN_SetMode(u8 item);

/* 一键同步参数: 原始单位 mV / Hz, 驱动内部转 kHz/MHz */
void DWIN_SyncParams(float upp_mv, float urms_mv, float f1_hz, float fs_hz, u8 mode);

/* 发送谐波分量: order=次数数组, freq_hz=频率Hz, amp_mv=幅值mV, n=个数 */
void DWIN_SyncComponents(const int *order, const float *freq_hz, const float *amp_mv, u8 n);

/* 读取触摸键值 (轮询, 读后自动回写0清除) */
u16  DWIN_ReadTouch(void);

/* 发送曲线数据: data=Y值数组(u16), count=点数 */
void DWIN_SendCurve(u16 vp, u16 ch, const u16 *data, u16 count);

#endif
