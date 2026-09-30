#include "Drive_ADS1256.h"


//***************************
//		Pin assign	   	
//		STM32						ADS1256

//		GPIOF_Pin_0 		---> SCLK
//		GPIOF_Pin_2			<--- DIN
//		GPIOF_Pin_4  		---> DOUT
//		GPIOF_Pin_6 		<--- DRDY
//		GPIOF_Pin_8 		---> CS
//		GPIOF_Pin_10  	<--- PDWN

//***************************	

/*端口定义*/ 
#define ADS1256_SCLK_PIN GPIO_Pin_8
#define ADS1256_DOUT_PIN GPIO_Pin_9
#define ADS1256_DIN_PIN GPIO_Pin_11
#define ADS1256_DRDY_PIN GPIO_Pin_13
#define ADS1256_CS_PIN GPIO_Pin_15
#define ADS1256_PDWN_PIN GPIO_Pin_0

#define ADS1256_CS_0() GPIO_ResetBits(GPIOF, ADS1256_CS_PIN)
#define ADS1256_CS_1() GPIO_SetBits(GPIOF, ADS1256_CS_PIN)

#define ADS1256_SCLK_0() GPIO_ResetBits(GPIOF, ADS1256_SCLK_PIN)
#define ADS1256_SCLK_1() GPIO_SetBits(GPIOF, ADS1256_SCLK_PIN)

#define ADS1256_DIN_0() GPIO_ResetBits(GPIOF, ADS1256_DOUT_PIN) // 注意这里，单片机的DOUT连接ADS1256的DIN
#define ADS1256_DIN_1() GPIO_SetBits(GPIOF, ADS1256_DOUT_PIN)

#define ADS1256_DOUT (GPIOF->IDR & ADS1256_DIN_PIN) //
#define ADS1256_DRDY (GPIOF->IDR & ADS1256_DRDY_PIN)
#define ADS1256_PDWN (GPIOG->IDR & ADS1256_PDWN_PIN)

/* DRDY 等待超时阈值，防止硬件故障导致死机 */
#define ADS1256_DRDY_TIMEOUT 1000000UL

/*
*********************************************************************************************************
*	函 数 名: ADS1256_WaitDRDY
*	功能说明: 等待 DRDY 变低，带超时保护
*	返 回 值: 0=正常, 1=超时
*********************************************************************************************************
*/
static uint8_t ADS1256_WaitDRDY(void)
{
	uint32_t timeout = ADS1256_DRDY_TIMEOUT;
	while(ADS1256_DRDY && timeout--) ;
	return (timeout == 0) ? 1 : 0;
}


/*
*********************************************************************************************************
*	函 数 名: Init_ADS1256_GPIO
*	功能说明: 初始化ADS1256 GPIO
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void Init_ADS1256_GPIO(void)
{

	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF | RCC_AHB1Periph_GPIOG, ENABLE);

	GPIO_InitStructure.GPIO_Pin = ADS1256_SCLK_PIN | ADS1256_DOUT_PIN | ADS1256_CS_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
	GPIO_Init(GPIOF, &GPIO_InitStructure);

	ADS1256_CS_1();

	GPIO_InitStructure.GPIO_Pin = ADS1256_DIN_PIN | ADS1256_DRDY_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOF, &GPIO_InitStructure);

	/* PDWN 在 GPIOG */
	GPIO_InitStructure.GPIO_Pin = ADS1256_PDWN_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOG, &GPIO_InitStructure);
}


void SPI_WriteByte(u8 TxData)
{
	u8 i;
	for(i = 0; i < 8; i++)
	{
		ADS1256_SCLK_1();
		delay_us(1);        // 恢复为 1us 安全延时
		
		if (TxData & 0x80)
			ADS1256_DIN_1();  
		else
			ADS1256_DIN_0();
		TxData <<= 1;
		
		ADS1256_SCLK_0();	
		delay_us(1);        // 恢复为 1us 安全延时
	}
} 

u8 SPI_ReadByte(void)
{
	u8 i;
	u8 read = 0;
	for (i = 0; i < 8; i++)
	{
		ADS1256_SCLK_1();
		delay_us(1);        // 恢复为 1us 安全延时
		read = read << 1;
		
		ADS1256_SCLK_0();
		delay_us(1);        // 恢复为 1us 安全延时
		
		if(ADS1256_DOUT)
				read++;
	}
	return read;
}

/*
*********************************************************************************************************
*	函 数 名: ADS1256WREG
*	功能说明: ADS1256 写数据
*	形    参: regaddr:寄存器地址
						databyte:待写入数据
*	返 回 值: 无
*********************************************************************************************************
*/
void ADS1256WREG(u8 regaddr,u8 databyte)
{
	
	ADS1256_CS_0();
	if(ADS1256_WaitDRDY()) { ADS1256_CS_1(); return; } // 超时保护
	
	//向寄存器写入数据地址
	SPI_WriteByte(ADS1256_CMD_WREG | (regaddr & 0x0F));
	
	//写入数据的个数n-1
	SPI_WriteByte(0x00);
	
	delay_us(10);
	
	//向regaddr地址指向的寄存器写入数据databyte
	SPI_WriteByte(databyte);
	
	ADS1256_CS_1();
}

/*
*********************************************************************************************************
*	函 数 名: ADS1256RREG
*	功能说明: ADS1256 读数据
*	形    参: regaddr:寄存器地址
*	返 回 值: 无
*********************************************************************************************************
*/
u8 ADS1256RREG(u8 regaddr)
{
	u8 databyte;
	
	ADS1256_CS_0();
	if(ADS1256_WaitDRDY()) { ADS1256_CS_1(); return 0; } // 超时保护
	
	//向寄存器写入数据地址
	SPI_WriteByte(ADS1256_CMD_RREG | (regaddr & 0x0F));
	
	//写入数据的个数n-1
	SPI_WriteByte(0x00);
	
	delay_us(10);
	
	//向regaddr地址指向的寄存器写入数据databyte
	databyte=SPI_ReadByte();
	
	ADS1256_CS_1();

  return databyte;
}


/*
*********************************************************************************************************
*	函 数 名: ADS1256_Init
*	功能说明: 初始化ADS1256
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void ADS1256_Init(void)
{
	Init_ADS1256_GPIO(); 																							  //ADS1256_GPIO初始化
	delay_ms(10);
	
	ADS1256_CS_0();
	delay_us(100);
	if(ADS1256_WaitDRDY()) { ADS1256_CS_1(); return; }
	SPI_WriteByte(ADS1256_CMD_REST);																		//复位
	delay_ms(10);

	if(ADS1256_WaitDRDY()) { ADS1256_CS_1(); return; }
	SPI_WriteByte(ADS1256_CMD_SDATAC);  // 0x0F
	delay_us(10);

	if(ADS1256_WaitDRDY()) { ADS1256_CS_1(); return; }
	SPI_WriteByte(ADS1256_CMD_SELFCAL);																	 //自动校准
	
	if(ADS1256_WaitDRDY()) { ADS1256_CS_1(); return; }                     		 								
	SPI_WriteByte(ADS1256_CMD_SYNC);                 										//同步
	SPI_WriteByte(ADS1256_CMD_WAKEUP);              										//同步唤醒
	
	if(ADS1256_WaitDRDY()) { ADS1256_CS_1(); return; }
	ADS1256WREG(ADS1256_STATUS,0x04);               										// 高位在前、不使用缓冲
	ADS1256WREG(ADS1256_MUX,ADS1256_MUXP_AIN0 | ADS1256_MUXN_AINCOM);		//设置通道
	ADS1256WREG(ADS1256_ADCON,ADS1256_GAIN_1);      										// 放大倍数1
	ADS1256WREG(ADS1256_DRATE,ADS1256_DRATE_1000SPS); 
	ADS1256WREG(ADS1256_IO,0x00);  																			//不使用数据IO口
	
	if(ADS1256_WaitDRDY()) { ADS1256_CS_1(); return; }
	SPI_WriteByte(ADS1256_CMD_SELFCAL);																	 //自动校准
	ADS1256_CS_1();

}             
/*
*********************************************************************************************************
*	函 数 名: ADS1256ReadData
*	功能说明: 读取24位AD值
*	形    参: channel:通道选择
*	返 回 值: AD值
*********************************************************************************************************
*/
u32 ADS1256ReadData(u8 channel)  
{
	u32 sum=0;

	if(ADS1256_WaitDRDY()) return 0;
	ADS1256WREG(ADS1256_MUX,channel);
	
	ADS1256_CS_0();
	delay_us(5);                                // 给写入寄存器一点缓冲时间
	
	SPI_WriteByte(ADS1256_CMD_SYNC);
	delay_us(5);                                // 连续发命令中间稍微停顿
	SPI_WriteByte(ADS1256_CMD_WAKEUP);
	
	delay_us(5);                                // 稍微等几微秒，让 DRDY 变为高电平
	if(ADS1256_WaitDRDY()) { ADS1256_CS_1(); return 0; }  // 等待 DRDY 重新变为低电平
	
	SPI_WriteByte(ADS1256_CMD_RDATA);
	delay_us(10);                               
	sum = (SPI_ReadByte() << 16);
	sum |= (SPI_ReadByte() << 8);
	sum |= SPI_ReadByte();

	ADS1256_CS_1();
	
	return sum;
}
/*
*********************************************************************************************************
*	函 数 名: Get_Val
*	功能说明: 将读取的24位AD值转化成电压值
*********************************************************************************************************
*/
float Get_Val(u32 addata)
{
	 u8 PGA = 1;
	 float VREF = 2.4918f;
	 int32_t signed_data;

	 // 符号扩展：如果第 24 位（符号位）为 1，说明是负数，将高 8 位全部补 1
	 if (addata & 0x800000) 
	 {
			addata |= 0xFF000000;
	 }
	 
	 // 强制转换为有符号的 32 位整型
	 signed_data = (int32_t)addata;

	 return (signed_data * 2.0f * VREF) / (PGA * 8388608.0f);
}


/*
*********************************************************************************************************
*	函 数 名: Moving_Average_Filter
*	功能说明: 滑动平均滤波
*	形    参: channel:通道选择
* times:数据个数 (必须大于等于3，且小于等于100)
*	返 回 值: 滤波后的AD值
*********************************************************************************************************
*/
u32 Moving_Average_Filter(u8 channel, u16 times) 
{
	 u16 i;
	 u32 sum = 0, data[100];
	 u32 max, min;

	 // 1. 安全保护：限制采样次数，防止数组越界或分母溢出
	 if(times > 100) times = 100;
	 if(times < 3) return ADS1256ReadData(channel); // 次数太少，直接走单次读取

	 if(ADS1256_WaitDRDY()) return 0;
	 ADS1256WREG(ADS1256_MUX, channel);  // 写入新通道
	 
	 ADS1256_CS_0();
	 delay_us(5);
	 SPI_WriteByte(ADS1256_CMD_SYNC);    // 同步
	 delay_us(5);
	 SPI_WriteByte(ADS1256_CMD_WAKEUP);  // 唤醒并重新启动转换
	 delay_us(25);                       // 给 DRDY 留出拉高的时间

	 // 3. 连续、高速地读取 times 次数据
	 for(i = 0; i < times; i++)
	 {
			if(ADS1256_WaitDRDY()) { ADS1256_CS_1(); return 0; }            
			
			__disable_irq();             
			
			SPI_WriteByte(ADS1256_CMD_RDATA); // 发出读数据指令
			delay_us(10);
			
			data[i] = (SPI_ReadByte() << 16); 
			data[i] |= (SPI_ReadByte() << 8);
			data[i] |= SPI_ReadByte();
			
			__enable_irq();      
	 }
	 
	 ADS1256_CS_1();                     // 读取完毕，释放片选

	 // 4. 掐头去尾求平均
	 max = min = data[0];
	 for(i = 0; i < times; i++)
	 {
		  if(data[i] > max) max = data[i];
		  if(data[i] < min) min = data[i];
		  sum += data[i];
	 }

	 return (sum - max - min) / (times - 2);
}


