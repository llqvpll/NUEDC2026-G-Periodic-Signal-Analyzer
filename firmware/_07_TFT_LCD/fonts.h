/**
  ******************************************************************************
    @file    fonts.h
    @author  MCD Application Team/longwei
    @version V4.5.1
    @date    19-9-2012
    @brief   Header for fonts.c
*/
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __FONTS_H__
#define __FONTS_H__
/* define ------------------------------------------------------------------*/
#define __FONTS 2

#include <stdint.h>
/** @defgroup FONTS_Exported_Types
    @{
*/
typedef struct _tFont
{
	const unsigned char (*table)[64];
	uint8_t Height;
	uint8_t Width;
	uint8_t Mode;//设置字符是否叠加，默认为非叠加

} sFONT;

extern sFONT Font16x08;
#if __FONTS==2
extern sFONT Font32x16;
extern sFONT Font16x08x;
extern sFONT Font24x12;
#endif

/** @defgroup FONTS_Exported_Constants
    @{
*/






/******************* (C) COPYRIGHT 2011 longwei *****END OF FILE***************/



/*******************************(C) COPYRIGHT 2016 Wind（谢玉伸）*********************************/
extern const unsigned char asc2_1608[95][16];
extern const unsigned char asc2_2412[95][36];
extern const unsigned char asc2_3216[95][64];



/*******************************(C) COPYRIGHT 2016 Wind（谢玉伸）*********************************/




#endif /* __FONTS_H */




