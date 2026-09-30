#ifndef _Dirive_AD9959_H_
#define _Dirive_AD9959_H_
#include "sys.h"
#include "stdint.h"

//AD9959寄存器地址定义（保持不变）
#define CSR_ADD   0x00
#define FR1_ADD   0x01
#define FR2_ADD   0x02
#define CFR_ADD   0x03
#define CFTW0_ADD 0x04
#define CPOW0_ADD 0x05
#define ACR_ADD   0x06
#define LSRR_ADD  0x07
#define RDW_ADD   0x08
#define FDW_ADD   0x09

//AD9959管脚宏定义（保持不变）
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

/* 新驱动对外接口函数 */
void Init_AD9959(void);
void AD9959_Set_Fre(uint8_t Channel, uint32_t Freq);
void AD9959_Set_Amp(uint8_t Channel, uint16_t Ampli);
void AD9959_Set_Phase(uint8_t Channel, uint16_t Phase);
void AD9959_SyncPhase_All(void);

#endif