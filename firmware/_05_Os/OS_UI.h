/*
*********************************************************************************************************
                                               _04_OS
    File			 : OS_UI.c
    By  			 : Wind
    platform   : STM32F407ZG
	Data   		 : 2018/7/16
*********************************************************************************************************
*/
/* 防止重调用 ----------------------------------------------------------------*/
#ifndef __OS_UI_H
#define __OS_UI_H

/* 头文件包含 ----------------------------------------------------------------*/
#include "User_header.h"

/* 全局宏定义 ----------------------------------------------------------------*/
//画笔颜色
#define WHITE         	 0xFFFF
#define BLACK         	 0x0000
#define BLUE         	 0x001F
#define BRED             0XF81F
#define GRED 			 0XFFE0
#define GBLUE			 0X07FF
#define RED           	 0xF800
#define MAGENTA       	 0xF81F
#define GREEN         	 0x07E0
#define CYAN          	 0x7FFF
#define YELLOW        	 0xFFE0

/*----------------兼容颜色---------------------------------------*/
/* LCD color */
#define White          0xFFFF //白色
#define Black          0x0000 //黑色
#define Grey           0xF7DE //灰色
#define Blue           0x001F //蓝色
#define Blue2          0x051F //深蓝色
#define Red            0xF800 //红色
#define Magenta        0xF81F //品红
#define Green          0x07E0 //绿色
#define Cyan           0x7FFF //蓝绿色
#define Yellow         0xFFE0 //黄色
#define BROWN 		   0XBC40 //棕色
#define GRAY  		   0X8430 //灰色

/* 尺寸 --------------------------------------------------------*/
#define OS_LCD_WHITH  	800	 //宽度	
#define OS_LCD_HEIGHT  	480	 //高度	
#define OS_ICO_COLOUR_BACK  GREEN //系统桌面 图标背景颜色
#define OS_ICO_COLOUR_TEXT  WHITE  //系统桌面 图标文字颜色

#define FONT_1   12
#define FONT_2   16
#define FONT_3   24
#define FONT1   FONT_1
#define FONT2   FONT_2
#define FONT3   FONT_3

#define IOC_SIZE 64



#ifndef OS_DEFAULT
#define OS_DEFAULT 	     	     NULL	  //缺省值
#endif


/* 兼容以前的代码 -----------------------------------------------*/

/* 程序需要 ----------------------------------------------------*/
#define OS_CLEAR_PURE				(0x01 << 0)
#define OS_CLEAR_IMAGE				 0x00
#define OS_CLEAR_WEAKNESS			(0x01 << 1)
#define OS_CLEAR_CONSTRAINT			 0x00

#define OSC_FORMAT_U8				0x01
#define OSC_FORMAT_U16				0x02
#define OSC_FORMAT_FLOAT			0x03
#define OSC_FORMAT_DOUBLE			0x04

#define OSC_MODE_WINDOWS_SHOW		0x01
#define OSC_MODE_LINE_SHOW			0x02

#define OSC_MODE_ERROR				0x00
#define OSC_MODE_NORMAL				0x01
#define OSC_MODE_FFT				0x02


/* 全局函数声明 --------------------------------------------------------------*/
int OS_LCD_Init(void);
int OS_LCD_Clear(u16 color);
unsigned int OS_Point_Read(u16 x,u16 y);
unsigned int OS_TextColor_Set(u16 _color);
unsigned int OS_BackColor_Set(u16 _color);
int OS_Point_Draw(u16 x,u16 y,u16 color);
int OS_Line_Draw(u16 x1, u16 y1, u16 x2, u16 y2,u16 color);
int OS_Circle_Draw(u16 x, u16 y, u8 r, u8 size, u16 color);
int OS_Rect_Draw(u16 x1, u16 y1, u16 x2, u16 y2, u8 size, u16 color);
int OS_Picture_Draw(u16 x1,u16 y1,u16 x2,u16 y2,u8 mode,const u8 *pic);

int OS_String_Show(u16 x, u16 y, u8 size, u8 mode, char *str);
int OS_Num_Show(u16 x,u16 y,u8 size,u8 mode,double num,char* format);


#endif
/*******************************(C) COPYRIGHT 2016 Wind（谢玉伸）*********************************/













