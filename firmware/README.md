AD DA + AD9959 + ADS1256 示例工程

一. 工程简介

本工程基于STM32F407和Keil MDK，提供片内AD/DA、AD9959 DDS和 ADS1256高精度ADC的测试程序。程序运行后通过LCD显示主菜单，通过PS2键盘选择功能页并输入参数。

主菜单包含 3 个功能：

1.AD DA：显示 PA1/PA2/PA3 的 ADC 电压，控制 PA4/PA5 的 DAC 输出
2.AD9959：设置 AD9959 四通道频率、幅度和相位
3.ADS1256：循环读取 ADS1256 的 AIN0 ~ AIN7 单端输入

二. 硬件连接

AD9959 和 ADS1256 模块在实验箱/单片机板上已经预留了接口。实际使用时，将模块的对应信号连接到单片机板上预留的同名接口即可；若接口标号和代码宏不一致，以驱动文件中的 GPIO 宏为准，或按实际硬件修改宏定义。

ADC1 输入：PA1
ADC2 输入：PA2
ADC3 输入：PA3
DAC1 输出：PA4
DAC2 输出：PA5

AD9959 当前代码连接：

#define CS			PEout(5)
#define SCLK		PEout(3)
#define UPDATE	    PBout(0)

#define PS0			PFout(5)
#define PS1			PFout(3)
#define PS2			PFout(1)
#define PS3			PEout(6)

#define SDIO0		PEout(4)
#define SDIO1		PEout(2)
#define SDIO2		PEout(0)
#define SDIO3		PEout(1)

#define AD9959_PWR	PFout(4)
#define Reset		PFout(2)

ADS1256 当前代码连接：

#define ADS1256_SCLK_PIN GPIO_Pin_8
#define ADS1256_DOUT_PIN GPIO_Pin_9
#define ADS1256_DIN_PIN GPIO_Pin_11
#define ADS1256_DRDY_PIN GPIO_Pin_13
#define ADS1256_CS_PIN GPIO_Pin_15
#define ADS1256_PDWN_PIN GPIO_Pin_0

三. 功能使用

1. AD DA

按下按键1，进入AD DA菜单后，LCD显示PA1/PA2/PA3的ADC电压，并控制PA4/PA5的 DAC 输出，DAC 输出范围限制为0.0 V ~ 3.3 V。
按键的功能如下：
数字键：输入当前 DAC 输出电压
+和-：当前 DAC 电压按 0.5 V 步进调整
NumLock：切换 DAC1/DAC2
Back：返回主菜单

显示参数如下
ADC1
ADC2
ADC3
DAC1
DAC2

2. AD9959

按下按键2，进入AD9959菜单后，程序调用Init_AD9959()初始化芯片，并显示 4 个通道的频率、幅度和相位。
频率：50000 Hz（0 ~ 500000000 Hz）
幅度：200 mV（0 ~ 576 mV）
相位：0（0 ~ 360）

按键功能如下：
NumLock：在频率、幅度、相位之间切换
/ ：切换 CH0 ~ CH3
+ / -：调整当前参数
数字键：直接输入当前参数
Back：返回主菜单

显示参数如下：
AD9959四个通道（CH0~CH3）的频率，幅值，相位

3. ADS1256
按下按键3，进入ADS1256菜单后，程序调用ADS1256_Init()初始化芯片，并循环读取 AIN0 ~ AIN7 对 AINCOM 的单端电压。默认增益为ADS1256_GAIN_1，采样率配置为 ADS1256_DRATE_1000SPS。

显示参数如下：
AD1256的AIN0~7

四. 使用注意

1. AD9959 和 ADS1256 的接口已经预留，接线时把模块信号接到单片机板上对应预留接口即可。
2. 所有外设必须与 STM32 共地，供电电压按模块要求连接。
3. 更换硬件接口时，只需要修改对应驱动文件里的 GPIO 宏和初始化引脚。
