/*
*********************************************************************************************************
                                               _04_OS
    File       : Drive_AD637.h
    platform   : STM32F407ZG
    function   : AD637 RMS-to-DC converter driver (ADC1 CH0, PA0)
*********************************************************************************************************
*/
#ifndef __DRIVE_AD637_H
#define __DRIVE_AD637_H

#include "User_header.h"

/*
 * 鏍″噯鎺ュ彛锛氱嚎鎬ф嫙鍚?
 *   y = AD637_SCALE * x + AD637_OFFSET
 * 鍚庣画鐢ㄦ爣鍑嗕俊鍙锋簮鏍囧畾鍚庝慨鏀硅繖涓や釜瀹忓嵆鍙?
 */
#define AD637_SCALE   1.0f
#define AD637_OFFSET  0.0f

void  AD637_Init(void);
float AD637_ReadVoltage(void);

#endif
