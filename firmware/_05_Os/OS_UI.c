/*
*********************************************************************************************************
                                               _04_OS
    File			 : OS_UI.c
    By  			 : Wind
    platform   : STM32F407ZG
	Data   		 : 2018/7/16
*********************************************************************************************************
*/

/* 头文件包含 ----------------------------------------------------------------*/
#include "OS_UI.h"

/* 私有宏定义 ----------------------------------------------------------------*/

/* 私有（静态）函数声明 -------------------------------------------------------*/
void OS_Char_Show(u16 x, u16 y, u8 num, u8 size, u8 mode);

/* 全局变量定义 --------------------------------------------------------------*/

/* 全局变量声明 --------------------------------------------------------------*/

/* 全局函数编写 --------------------------------------------------------------*/

/*============================================= 底层程序层 =========================================*/
/** ----------------------------------------------------------------------------
    @FunctionName  : OS_LCD_Init()
    @Description   : 初始化函数
    @Data          : 2016/3/22
    @Explain
    -------------------------------------------------------------------------------
    None
    ------------------------------------------------------------------------------*/
int OS_LCD_Init(void)
{
	TFT_LCD_Init();
	LCD_Clear(Black);//清屏
	if(font_init())
	{
		OS_String_Show(400, 200, 32, 1, "\xd7\xd6\xbf\xe2\xb4\xed\xce\xf3!!!"); //检查字库
		return -1;
	}
	TextColor = White;
	BackColor = Black;
	return 0;
}

/** ----------------------------------------------------------------------------
    @FunctionName  : OSTextColor_Set()
    @Description   : None
    @Data          : 2016/3/22
    @Explain
    -------------------------------------------------------------------------------
    None
    ------------------------------------------------------------------------------*/
unsigned int OS_TextColor_Set(u16 _color)
{
	uint16_t temp = TextColor;
	TextColor = _color;
	return temp;
}


/** ----------------------------------------------------------------------------
    @FunctionName  : OS_BackColor_Set()
    @Description   : None
    @Data          : 2016/3/22
    @Explain
    -------------------------------------------------------------------------------
    None
    ------------------------------------------------------------------------------*/
unsigned int OS_BackColor_Set(u16 _color)
{
	uint16_t temp = BackColor;
	BackColor = _color;
	return temp;
}

/** ----------------------------------------------------------------------------
    @FunctionName  : OS_LCD_Clear()
    @Description   : None
    @Data          : 2016/7/12
    @Explain
    -------------------------------------------------------------------------------
    None
    ------------------------------------------------------------------------------*/
int OS_LCD_Clear(u16 color)
{
	LCD_Clear(color);
	return 0;
}



/** ----------------------------------------------------------------------------
    @FunctionName  : OS_Point_Draw()
    @Description   : 画点
    @Data          : 2016/3/22
    @Explain
    -------------------------------------------------------------------------------
    x,y:坐标
    POINT_COLOR:此点的颜色
    ------------------------------------------------------------------------------*/
int OS_Point_Draw(u16 x, u16 y, u16 color)
{
	LCD_SetCursor(x, y); //设置光标位置
	LCD_WriteRAM_Prepare();
	LCD_WriteRAM(color);
	return 0;
}

/** ----------------------------------------------------------------------------
    @FunctionName  : OS_Char_Show()
    @Description   : 打印字符
    @Data          : 2016/7/14
    @Explain
    -------------------------------------------------------------------------------
    在指定位置显示一个字符
    x,y:起始坐标
    num:要显示的字符:" "--->"~"
    size:字体大小 16/24/32
    mode:叠加方式(1)还是非叠加方式(0)
    ------------------------------------------------------------------------------*/
void OS_Char_Show(u16 x, u16 y, u8 num, u8 size, u8 mode)
{
	u8 temp, t1, t;
	u16 y0 = y;
	u8 csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size / 2);		//得到字体一个字符对应点阵集所占的字节数
	//设置窗口
	num = num - ' '; //得到偏移后的值
	for(t = 0; t < csize; t++)
	{
		if(size == 16)
			temp = asc2_1608[num][t];	//调用1608字体
		else if(size == 24)
			temp = asc2_2412[num][t]; //调用2412字体
		else if(size == 32)
			temp = asc2_3216[num][t]; //调用3216字体
		else
			return;//没有的字库
		for(t1 = 0; t1 < 8; t1++)
		{
			if(temp & 0x80)
				OS_Point_Draw(x, y, TextColor);
			else if(mode == 1)
				OS_Point_Draw(x, y, BackColor);
			temp <<= 1;
			y++;
			if(y >= 480)
				return;		//超区域了
			if((y - y0) == size)
			{
				y = y0;
				x++;
				if(x >= 800)
					return;	//超区域了
				break;
			}
		}
	}
}

/** ----------------------------------------------------------------------------
    @FunctionName  : OS_String_Show()
    @Description   : 打印句子
    @Data          : 2016/3/22
    @Explain
    -------------------------------------------------------------------------------
    x,y 坐标    *code 语句指针   size 字体大小
    ------------------------------------------------------------------------------*/
void OS_HzMat_Get(char *code, uint8_t *mat, u8 size)
{
	uint8_t qh, ql;
	uint8_t i;
	uint32_t foffset;
	u8 csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size); //得到字体一个字符对应点阵集所占的字节数
	qh = *code;
	ql = *(++code);
	if(qh < 0x81 || ql < 0x40 || ql == 0xff || qh == 0xff) //非常用汉字
	{
		for(i = 0; i < csize; i++)
			*mat++ = 0x00; //填充满格
		return; //结束访问
	}
	if(ql < 0x7f)
		ql -= 0x40; //注意!
	else
		ql -= 0x41;
	qh -= 0x81;
	foffset = ((uint32_t)190 * qh + ql) * csize;	//得到字库中的字节偏移量
	switch(size)
	{
		case 32:
			W25Q64_Read(mat, foffset + ftinfo.f12addr, csize);
			break;
		case 16:
			W25Q64_Read(mat, foffset + ftinfo.f16addr, csize);
			break;
		case 24:
			W25Q64_Read(mat, foffset + ftinfo.f24addr, csize);
			break;
	}
}


/** ----------------------------------------------------------------------------
    @FunctionName  : OS_Font_Show()
    @Description   : 打印句子
    @Data          : 2016/3/22
    @Explain
    -------------------------------------------------------------------------------
    x,y 坐标    *str 语句指针   size 字体大小
    ------------------------------------------------------------------------------*/
void OS_Font_Show(u16 x, u16 y, char *font, u8 size, u8 mode)
{
	u8 temp, t, t1;
	u16 y0 = y;
	u8 dzk[144];
	u8 csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size); //得到字体一个字符对应点阵集所占的字节数
	if(size != 32 && size != 16 && size != 24)
		return;	//不支持的size
	OS_HzMat_Get(font, dzk, size);	//得到相应大小的点阵数据
	for(t = 0; t < csize; t++)
	{
		temp = dzk[t];			//得到点阵数据
		for(t1 = 0; t1 < 8; t1++)
		{
			if(temp & 0x80)
				OS_Point_Draw(x, y, TextColor);
			else if(mode == 1)
				OS_Point_Draw(x, y, BackColor);
			temp <<= 1;
			y++;
			if((y - y0) == size)
			{
				y = y0;
				x++;
				break;
			}
		}
	}
}



/*============================================= 应用程序层 =========================================*/
/** ----------------------------------------------------------------------------
    @FunctionName  : OS_Line_Draw()
    @Description   : 画线
    @Data          : 2016/7/14
    @Explain
    -------------------------------------------------------------------------------
    x1,y1	:起点坐标
    x2,y2	:终点坐标
    size	:线条宽度
    ------------------------------------------------------------------------------*/
int OS_Line_Draw(u16 x1, u16 y1, u16 x2, u16 y2, u16 color)
{
	u16 t;
	int xerr = 0, yerr = 0, delta_x, delta_y, distance;
	int incx, incy, uRow, uCol;
	if(x1 == x2 && y1 == y2)
		x1 = x2 + 1;
	delta_x = x2 - x1; //计算坐标增量
	delta_y = y2 - y1;
	uRow = x1;
	uCol = y1;
	if(delta_x > 0)
		incx = 1; //设置单步方向
	else if(delta_x == 0)
		incx = 0; //垂直线
	else
	{
		incx = -1;
		delta_x = -delta_x;
	}
	if(delta_y > 0)
		incy = 1;
	else if(delta_y == 0)
		incy = 0; //水平线
	else
	{
		incy = -1;
		delta_y = -delta_y;
	}
	if( delta_x > delta_y)
		distance = delta_x; //选取基本增量坐标轴
	else
		distance = delta_y;
	for(t = 0; t <= distance + 1; t++ ) //画线输出
	{
		OS_Point_Draw(uRow, uCol, color); //画点
		xerr += delta_x ;
		yerr += delta_y ;
		if(xerr > distance)
		{
			xerr -= distance;
			uRow += incx;
		}
		if(yerr > distance)
		{
			yerr -= distance;
			uCol += incy;
		}
	}
	return 0;
}




/** ----------------------------------------------------------------------------
    @FunctionName  : OS_Circle_Draw()
    @Description   : 画圆
    @Data          : 2016/3/22
    @Explain
    -------------------------------------------------------------------------------
    (x,y)		:中心点
    r			:圆的半径
    size		:线条的宽度  	0,表示实心
    ------------------------------------------------------------------------------*/
int OS_Circle_Draw(u16 x, u16 y, u8 r, u8 size, u16 color)
{
	u16 temp = TextColor;
	TextColor = color;
	if(size == 0)
		LCD_DrawCircleS(x, y, r, color);
	else
	{
		while(size--)
			LCD_DrawCircle(x, y, r - size);
	}
	TextColor = temp;
	return 0;
}


/** ----------------------------------------------------------------------------
    @FunctionName  : OS_Rect_Draw()
    @Description   : 画矩形
    @Data          : 2016/3/22
    @Explain
    -------------------------------------------------------------------------------
    x1,y1,x2,y2 ：矩形两对角坐标
    size		：线条宽度   0,表示实心
    ------------------------------------------------------------------------------*/
int OS_Rect_Draw(u16 x1, u16 y1, u16 x2, u16 y2, u8 size, u16 color)
{
	u16 temp = TextColor;
	TextColor = color;
	if(size == 0)
		LCD_DrawRectS(x1, y1, x2, y2, color);
	else
	{
		while(size --)
		{
			OS_Line_Draw(x1, y1 + size, x2, y1 + size, color);
			OS_Line_Draw(x1, y2 - size, x2, y2 - size, color);
			OS_Line_Draw(x2 - size, y1, x2 - size, y2, color);
			OS_Line_Draw(x1 + size, y1, x1 + size, y2, color);
		}
	}
	TextColor = temp;
	return 0;
}



/** ----------------------------------------------------------------------------
    @FunctionName  : OS_Picture_Draw()
    @Description   : 显示图片
    @Data          : 2016/3/22
    @Explain
    -------------------------------------------------------------------------------
    RGB565  16位真彩显示
    mode :  0 不可放缩      1 可缩放       2 可缩放，不显示白色
		  3 可缩放，不显示白色,且变灰
    带头数据
    ------------------------------------------------------------------------------*/
int OS_Picture_Draw(u16 x1, u16 y1, u16 x2, u16 y2, u8 mode, const u8 *pic)
{
	u16 temp = TextColor;
	static u8 UeseState = 0;
	if(UeseState)
		return -1;
	UeseState = 1;
	if(mode == 0)
	{
		u16 x, y, p_color;
		u16 len_x = (pic[2] << 8) | pic[3];
		u16 len_y = (pic[4] << 8) | pic[5];
		for(y = 0; y < len_y; y++)
			for(x = 0; x < len_x * 2; x += 2)
			{
				if((u16)(x / 2 + x1) <= x2)
					if((u16)(y + y1) <= y2)
					{
						p_color = pic[x + 6 + y * len_x * 2] << 8 | pic[(x + 7) + y * len_x * 2];
						LCD_SetCursor(x / 2 + x1, y + y1); //设置光标位置
						LCD_WriteRAM_Prepare();
						LCD_WriteRAM(p_color);
					}
			}
	}
	else if(mode == 1)
	{
		u16 x, y, p_color;
		float density_x, density_y;
		u16 len_x = (pic[2] << 8) | pic[3];
		u16 len_y = (pic[4] << 8) | pic[5];
		density_x = (float)(x2 - x1) / len_x;
		density_y = (float)(y2 - y1) / len_y;
		for(y = 0; y < len_y; y++)
			for(x = 0; x < len_x * 2; x += 2)
			{
				if((u16)((x / 2)*density_x + x1) <= x2)
					if((u16)(y * density_y + y1) <= y2)
					{
						p_color = pic[x + 6 + y * len_x * 2] << 8 | pic[(x + 7) + y * len_x * 2];
						LCD_SetCursor((x / 2)*density_x + x1, y * density_y + y1); //设置光标位置
						LCD_WriteRAM_Prepare();
						LCD_WriteRAM(p_color);
					}
			}
	}
	else if(mode == 2)
	{
		u16 x, y, p_color;
		float density_x, density_y;
		u16 len_x = (pic[2] << 8) | pic[3];
		u16 len_y = (pic[4] << 8) | pic[5];
		density_x = (float)(x2 - x1) / len_x;
		density_y = (float)(y2 - y1) / len_y;
		for(y = 0; y < len_y; y++)
			for(x = 0; x < len_x * 2; x += 2)
			{
				if((u16)((x / 2)*density_x + x1) <= x2)
					if((u16)(y * density_y + y1) <= y2)
					{
						p_color = pic[x + 6 + y * len_x * 2] << 8 | pic[(x + 7) + y * len_x * 2];
						if(p_color > 1) //不显示白色背景
						{
							LCD_SetCursor((x / 2)*density_x + x1, y * density_y + y1); //设置光标位置
							LCD_WriteRAM_Prepare();
							LCD_WriteRAM(p_color);
						}
					}
			}
	}
	else if(mode == 3)
	{
		u16 x, y, p_color, BitRed, BitGreen, BitBlue;
		float density_x, density_y;
		u16 len_x = (pic[2] << 8) | pic[3];
		u16 len_y = (pic[4] << 8) | pic[5];
		density_x = (float)(x2 - x1) / len_x;
		density_y = (float)(y2 - y1) / len_y;
		for(y = 0; y < len_y; y++)
			for(x = 0; x < len_x * 2; x += 2)
			{
				if((u16)((x / 2)*density_x + x1) <= x2)
					if((u16)(y * density_y + y1) <= y2)
					{
						p_color = pic[x + 6 + y * len_x * 2] << 8 | pic[(x + 7) + y * len_x * 2];
						if(p_color > 1) //不显示黑色背景
						{
							p_color = pic[x + 6 + y * len_x * 2] << 8 | pic[(x + 7) + y * len_x * 2];
							BitRed   = (p_color & (0x1f << 11)) >> 11;
							BitGreen = (p_color & (0x3f << 5 )) >> 5;
							BitBlue = p_color & 0x1f;
							p_color = ((BitRed / 3) << 11) | ((BitGreen / 3) << 5) | (BitBlue / 3);
							LCD_SetCursor((x / 2)*density_x + x1, y * density_y + y1); //设置光标位置
							LCD_WriteRAM_Prepare();
							LCD_WriteRAM(p_color);
						}
					}
			}
	}
	TextColor = temp;
	UeseState = 0;
	return 0;
}

/** ----------------------------------------------------------------------------
    @FunctionName  : OS_String_Show()
    @Description   : 打印句子
    @Data          : 2016/3/22
    @Explain
    -------------------------------------------------------------------------------
    x,y 		坐标
    str 		语句指针
    size 		字体大小 16 24 32
    mode 		0,无背景色，1带背景色
    return 		0,打印完成		-1,打印被占用
    ------------------------------------------------------------------------------*/
int OS_String_Show(u16 x, u16 y, u8 size, u8 mode, char *str)
{
	uint16_t x0 = x;
	uint8_t bHz = 0;   //字符或者中文
	while(*str != 0)		//数据未结束
	{
		if(!bHz)
		{
			if((signed char)*str < 0)
				bHz = 1; //中文
			else              //字符
			{
				if(*str == '\n') //换行符号
				{
					y += size;
					x = x0;
					str++;
				}
				OS_Char_Show(x, y, *str, size, mode);
				str++;
				x += size / 2; //字符,为全字的一半
			}
		}
		else //中文
		{
			bHz = 0; //有汉字库
			OS_Font_Show(x, y, str, size, mode);
			str += 2;
			x += size; //下一个汉字偏移
		}
	}
	return 0;
}




/** ----------------------------------------------------------------------------
    @FunctionName  : OS_Num_Show()
    @Description   : 多功能打印数值
    @Data          : 2017/7/12
    @Explain
    -------------------------------------------------------------------------------
    x,y 		坐标
    num 		数值
    size 		字体大小 12 16 24 32
    mode 		0,无背景色，1带背景色
    format		--------------------------------
			%d 十进制有符号整数，有BUG，请用%0.0f替代
			%u 十进制无符号整数
			%f 浮点数	%lf 长浮点数
			%s 字符串
			%c 单个字符
			%p 指针的值
			%e 指数形式的浮点数
			%x,%X 无符号以十六进制表示的整数
			%o 无符号以八进制表示的整数
			%g,NULL 自动选择合适的表示法
			%p 输出地址符
			--------------------------------
    return 		0,打印完成		-1,打印被占用
    注：format还有巧妙的用法，例如："时间：%0.0f ms"
    ------------------------------------------------------------------------------*/
int OS_Num_Show(u16 x, u16 y, u8 size, u8 mode, double num, char *format)
{
	char temp[100] = {0};
	if(format == NULL)
		sprintf(temp, "%g", num); //自动选择格式
	else
		sprintf(temp, format, num);
	OS_String_Show(x, y, size, mode, temp);
	return 0;
}

/*******************************(C) COPYRIGHT 2016 Wind（谢玉伸）*********************************/



















