/*
*********************************************************************************************************
                                               _01_App
    File       : app_sweep.h
    platform   : STM32F407ZG
    function   : 骞咃拷?锟界壒鎬ф壂棰戠畻娉曟帴锟?
*********************************************************************************************************
*/
#ifndef __APP_SWEEP_H
#define __APP_SWEEP_H

#include "User_header.h"

#define SWEEP_START_FREQ    1000.0f
#define SWEEP_END_FREQ      10000000.0f
#define SWEEP_STEP_RATIO    1.0471f
#define SWEEP_MAX_POINTS    200

extern volatile uint8_t Sweep_Start_Flag;

void Start_Sweep_Scan(void);

extern float AD637_ReadVoltage(void);

#endif
