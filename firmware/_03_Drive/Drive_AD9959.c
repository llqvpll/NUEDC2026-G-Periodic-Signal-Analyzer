/************************************************************
                    AD9959 ????
					AD9959--???
????(????, ?? GPIOE/GPIOF/GPIOB):
          CS			锟斤拷PE5;
          SCLK 		锟斤拷PE3;
          UPDATE	锟斤拷PB0;
          SP0    	锟斤拷PF5;
          SP1			锟斤拷PF3;
          SP2			锟斤拷PF1;
          SP3			锟斤拷PE6;
          SDIO0		锟斤拷PE4;
          SDIO1		锟斤拷PE2;
          SDIO2		锟斤拷PE0;
          SDIO3		锟斤拷PE1;
   AD9959_PWR(PDC)锟斤拷PF4;
          RST			锟斤拷PF2;
          GND			--GND(0V)

**************************************************************/

#include "Drive_AD9959.h"
#include "delay.h"

u8 CSR_DATA0[1] = {0x10};
u8 CSR_DATA1[1] = {0x20};
u8 CSR_DATA2[1] = {0x40};
u8 CSR_DATA3[1] = {0x80};
u8 CSR_DATAall[1] = {0xF0};

u8 FR2_DATA[2] = {0x00,0x00};
u8 CFR_DATA[3] = {0x00,0x03,0x02};

u8 CPOW0_DATA0[2] = {0x00,0x00};
u8 CPOW0_DATA1[2] = {0x00,0x00};
u8 CPOW0_DATA2[2] = {0x00,0x00};
u8 CPOW0_DATA3[2] = {0x00,0x00};

u8 LSRR_DATA[2] = {0x00,0x00};
u8 RDW_DATA[4] = {0x00,0x00,0x00,0x00};
u8 FDW_DATA[4] = {0x00,0x00,0x00,0x00};

/* 鍓嶇疆澹版槑 */
void AD9959_Set_Fre(uint8_t Channel, uint32_t Freq);
void AD9959_Set_Amp(uint8_t Channel, uint16_t Ampli);
void AD9959_Set_Phase(uint8_t Channel, uint16_t Phase);
void Intserve(void);
void IntReset(void);
void WriteData_AD9959(u8 RegisterAddress, u8 NumberofRegisters, u8 *RegisterData, u8 temp);

// ??? ?????:FR1 ?? ???
void Init_AD9959(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    // 20?? (25MHz?500MHz),REF_CLK??,PLL??,???75锟紸,??????
    u8 FR1_DATA[3] = {0xD0, 0x40, 0x00};

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE | RCC_AHB1Periph_GPIOF | RCC_AHB1Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    // PE0~PE6
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 |
                                  GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6;
    GPIO_Init(GPIOE, &GPIO_InitStructure);

    // PF1~PF5
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_Init(GPIOF, &GPIO_InitStructure);

    // PB0
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    Intserve();
    delay_ms(50);
    IntReset();
    delay_ms(50);

    WriteData_AD9959(FR1_ADD, 3, FR1_DATA, 1);

    // ??????
    AD9959_Set_Fre(0, 50000);
    AD9959_Set_Fre(1, 50000);
    AD9959_Set_Fre(2, 50000);
    AD9959_Set_Fre(3, 50000);

    AD9959_Set_Phase(0, 0);
    AD9959_Set_Phase(1, 0);
    AD9959_Set_Phase(2, 0);
    AD9959_Set_Phase(3, 0);

    AD9959_Set_Amp(0, 200);
    AD9959_Set_Amp(1, 200);
    AD9959_Set_Amp(2, 200);
    AD9959_Set_Amp(3, 200);
}

void delay1(u32 length)
{
    length = length * 12;
    while (length--);
}

void Intserve(void)
{
    AD9959_PWR = 0;
    CS = 1;
    SCLK = 0;
    UPDATE = 0;
    PS0 = 0;
    PS1 = 0;
    PS2 = 0;
    PS3 = 0;
    SDIO0 = 0;
    SDIO1 = 0;
    SDIO2 = 0;
    SDIO3 = 0;
}

void IntReset(void)
{
    Reset = 0;
    delay1(1);
    Reset = 1;
    delay1(30);
    Reset = 0;
}

void IO_Update(void)
{
    UPDATE = 0;
    delay1(2);
    UPDATE = 1;
    delay1(4);
    UPDATE = 0;
}

void WriteData_AD9959(u8 RegisterAddress, u8 NumberofRegisters, u8 *RegisterData, u8 temp)
{
    u8 ControlValue = 0;
    u8 ValueToWrite = 0;
    u8 RegisterIndex = 0;
    u8 i = 0;

    ControlValue = RegisterAddress;
    SCLK = 0;
    CS = 0;
    for (i = 0; i < 8; i++)
    {
        SCLK = 0;
        if (0x80 == (ControlValue & 0x80))
            SDIO0 = 1;
        else
            SDIO0 = 0;
        SCLK = 1;
        ControlValue <<= 1;
    }
    SCLK = 0;
    for (RegisterIndex = 0; RegisterIndex < NumberofRegisters; RegisterIndex++)
    {
        ValueToWrite = RegisterData[RegisterIndex];
        for (i = 0; i < 8; i++)
        {
            SCLK = 0;
            if (0x80 == (ValueToWrite & 0x80))
                SDIO0 = 1;
            else
                SDIO0 = 0;
            SCLK = 1;
            ValueToWrite <<= 1;
        }
        SCLK = 0;
    }
    if (temp == 1)
        IO_Update();
    CS = 1;
}

u8 CFTW0_DATA[4] = {0x00, 0x00, 0x00, 0x00};

void AD9959_Set_Fre(uint8_t Channel, uint32_t Freq)
{
    u32 Temp;

    if (Freq >= 500000000) Freq = 499999999;
    Temp = (u32)((double)Freq * 8.589934592);
    CFTW0_DATA[3] = (u8)Temp;
    CFTW0_DATA[2] = (u8)(Temp >> 8);
    CFTW0_DATA[1] = (u8)(Temp >> 16);
    CFTW0_DATA[0] = (u8)(Temp >> 24);

    if (Channel == 0) {
        WriteData_AD9959(CSR_ADD, 1, CSR_DATA0, 1);
        WriteData_AD9959(CFTW0_ADD, 4, CFTW0_DATA, 1);
    } else if (Channel == 1) {
        WriteData_AD9959(CSR_ADD, 1, CSR_DATA1, 1);
        WriteData_AD9959(CFTW0_ADD, 4, CFTW0_DATA, 1);
    } else if (Channel == 2) {
        WriteData_AD9959(CSR_ADD, 1, CSR_DATA2, 1);
        WriteData_AD9959(CFTW0_ADD, 4, CFTW0_DATA, 1);
    } else if (Channel == 3) {
        WriteData_AD9959(CSR_ADD, 1, CSR_DATA3, 1);
        WriteData_AD9959(CFTW0_ADD, 4, CFTW0_DATA, 1);
    }
    AD9959_SyncPhase_All();
}

u8 ACR_DATA[3] = {0x00, 0x10, 0x00};

void AD9959_Set_Amp(uint8_t Channel, uint16_t Ampli)
{
    u32 asf;

    if (Ampli > 576) Ampli = 576;
    asf = (u32)((Ampli / 576.0f) * 1023);
    ACR_DATA[0] = 0x00;
    ACR_DATA[1] = 0x10 | (u8)(asf >> 8);
    ACR_DATA[2] = (u8)(asf & 0xFF);

    if (Channel == 0) {
        WriteData_AD9959(CSR_ADD, 1, CSR_DATA0, 1);
        WriteData_AD9959(ACR_ADD, 3, ACR_DATA, 1);
    } else if (Channel == 1) {
        WriteData_AD9959(CSR_ADD, 1, CSR_DATA1, 1);
        WriteData_AD9959(ACR_ADD, 3, ACR_DATA, 1);
    } else if (Channel == 2) {
        WriteData_AD9959(CSR_ADD, 1, CSR_DATA2, 1);
        WriteData_AD9959(ACR_ADD, 3, ACR_DATA, 1);
    } else if (Channel == 3) {
        WriteData_AD9959(CSR_ADD, 1, CSR_DATA3, 1);
        WriteData_AD9959(ACR_ADD, 3, ACR_DATA, 1);
    }
    AD9959_SyncPhase_All();
}

void AD9959_Set_Phase(uint8_t Channel, uint16_t Phase)
{
    u16 P_temp = 0;

    if (Phase > 359) Phase = 359;
    P_temp = (u16)(Phase * 45.511111);
    P_temp &= 0x3FFF;
    CPOW0_DATA0[1] = (u8)P_temp;
    CPOW0_DATA0[0] = (u8)(P_temp >> 8);

    if (Channel == 0) {
        WriteData_AD9959(CSR_ADD, 1, CSR_DATA0, 1);
        WriteData_AD9959(CPOW0_ADD, 2, CPOW0_DATA0, 1);
    } else if (Channel == 1) {
        WriteData_AD9959(CSR_ADD, 1, CSR_DATA1, 1);
        WriteData_AD9959(CPOW0_ADD, 2, CPOW0_DATA0, 1);
    } else if (Channel == 2) {
        WriteData_AD9959(CSR_ADD, 1, CSR_DATA2, 1);
        WriteData_AD9959(CPOW0_ADD, 2, CPOW0_DATA0, 1);
    } else if (Channel == 3) {
        WriteData_AD9959(CSR_ADD, 1, CSR_DATA3, 1);
        WriteData_AD9959(CPOW0_ADD, 2, CPOW0_DATA0, 1);
    } else if (Channel == 4) {
        WriteData_AD9959(CSR_ADD, 1, CSR_DATAall, 1);
        WriteData_AD9959(CPOW0_ADD, 2, CPOW0_DATA0, 1);
    }
    AD9959_SyncPhase_All();
}

void AD9959_SyncPhase_All(void)
{
    u8 FR2_CLEAR_ALL[2] = {0x10, 0x00};
    u8 FR2_NORMAL[2]    = {0x00, 0x00};
    WriteData_AD9959(FR2_ADD, 2, FR2_CLEAR_ALL, 1);
    WriteData_AD9959(FR2_ADD, 2, FR2_NORMAL, 1);
}