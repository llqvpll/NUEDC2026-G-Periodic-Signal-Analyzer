/* ****************************
 * Project description:
 *
 * A Project empty template head file
 *
 * Author: 创新基地 -> 2019 Mao
 *
 * Creation Date: 2021/09/05
 *
 * Update date: 2021/11/03
 * ****************************/
#ifndef USER_H
#define USER_H

/* ***************************** Include & Define Part     	*****************************
 * 头文件声明及宏定义区
 * */
#include "User_header.h"

#define TitleLength 4
#define TitleStr "测试程序"
#define ModelVerStr "MD:6-2021/10/29"
#define UserVerStr "User:1.0-2026/6/5"

#define MenuChoiceNum 6
#define Menu1Choice1 "G题信号测量"
#define Menu1Choice2 "AD DA"
#define Menu1Choice3 "AD9959"
#define Menu1Choice4 "ADS1256"
#define Menu1Choice5 "幅频特性扫频"
#define Menu1Choice6 "FFT 频谱分析"

/* LCD 坐标宏定义 - 替代硬编码的坐标计算 */
#define LCD_LEFT_MARGIN   (250 + 2 + 16)      /* 内容区左边距 */
#define LCD_TITLE_Y       (64 + 8 + 16)       /* 标题行Y坐标 */
#define LCD_LABEL_X       (250 + 2 + 16)      /* 标签X坐标 */
#define LCD_CONTENT_X     (250 + 2 + 16 + 24) /* 内容数值X坐标 */
#define LCD_ROW_SPACING   (32 + 16)           /* 行间距: 字号32 + 间距16 */
#define LCD_ROW_Y(n)      (LCD_TITLE_Y + 32 + 16 + LCD_ROW_SPACING * ((n) - 1)) /* 第n行Y坐标 */
/* DA 显示区坐标 (在ADC第3行之后) */
#define LCD_DA_LABEL_Y    (LCD_ROW_Y(3) + LCD_ROW_SPACING * 1)
#define LCD_DA1_Y         (LCD_ROW_Y(3) + LCD_ROW_SPACING * 2)
#define LCD_DA2_Y         (LCD_ROW_Y(3) + LCD_ROW_SPACING * 3)
#define LCD_DA_VALUE_X    (LCD_CONTENT_X + 16 * 11)  /* DA数值显示X坐标 */
#define LCD_COL2_X        (LCD_LEFT_MARGIN + 16 * 16) /* ADS1256第二列X坐标 */
/* AD9959 显示区坐标 */
#define LCD_DDS_X         (LCD_LEFT_MARGIN + 16)                    /* AD9959内容X坐标 */
#define LCD_DDS_COL2_X    (LCD_DDS_X + (800 - 250 - 2 - 16) / 2)    /* AD9959第二列X坐标 */
#define LCD_DDS_ITEM_SP   (16 + 16)                                 /* AD9959参数项间距 */
#define LCD_DDS_CH_Y      (LCD_TITLE_Y + LCD_ROW_SPACING * 1)       /* AD9959通道行Y坐标 */
#define LCD_DDS_CURSOR_X  (250 + 2 + 8)                             /* AD9959光标X坐标(左列) */
#define LCD_DDS_CURSOR_X2 (250 + 2 + (800 - 250 - 2 - 16) / 2 + 8)  /* AD9959光标X坐标(右列) */

/* ***************************** Function Declaration Part  *****************************
 * 函数声明区
 * */
void User_main(void);

void Init_All(void);

void Disp_Main(void);
void Show_Val( uint8_t location , float value , char *str );
void Change_Menu( uint8_t menu_sign );

void MenuHandler_1(void);
void MenuHandler_2(void);
void MenuHandler_3(void);
void MenuHandler_4(void);
void MenuHandler_5(void);
void MenuHandler_6(void);

float PS2_ReadNum( float num );

/* ***************************** Variable definition Part   *****************************/







#endif






