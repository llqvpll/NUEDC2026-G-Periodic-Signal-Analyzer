/*
*********************************************************************************************************
                                               _04_OS
    File       : Drive_Comparator.h
    platform   : STM32F407ZG
    function   : 绛夌簿搴﹂?戠巼娴嬮噺 (TLV3501 鏁村舰 -> TIM1 杈撳叆鎹曡幏 PA8)
*********************************************************************************************************
*/
#ifndef __DRIVE_COMPARATOR_H
#define __DRIVE_COMPARATOR_H

#include "User_header.h"

/*
 * 闂搁棬鏃堕棿 (ms), 瀹忓畾涔夊彲璋?
 * 闂搁棬瓒婇暱浣庨?戣秺鍑?, 闂搁棬瓒婄煭楂橀?熷搷搴旇秺濂?
 */
#define GATE_TIME_MS    100

void  Comparator_Init(void);
float Get_Frequency(void);
void  Comparator_Restart(void);   /* 璇诲彇瀹屼竴娆＄粨鏋滃悗璋冪敤, 閲嶅惎涓嬩竴杞?娴嬮噺 */

/* Diagnostic: read raw counter / state for debugging "no signal" issues.
 * sig_count:  signal edge count in last gate period
 * done:       1 = gate finished, 0 = gate still running
 * gate_state: 0=idle, 1=measuring, 2=done-waiting-read */
void  Comparator_GetDebug(u32 *sig_count, u8 *done, u8 *gate_state);

#endif
