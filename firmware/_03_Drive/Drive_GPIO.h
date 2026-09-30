#ifndef __DRIVER_GPIO_H
#define __DRIVER_GPIO_H

#include "User_header.h"

#define LED1 PBout(7)
#define LED2 PBout(8)
#define LED3 PDout(2)
#define LED4 PBout(1)



void LED_Init(void);
void GPIO_Key_Init(void);
void GPIO_POW_Init(void);
void GPIO_Data_Init(void);

#endif

