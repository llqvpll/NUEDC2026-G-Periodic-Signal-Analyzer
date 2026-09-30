/*
*********************************************************************************************************
                                               _01_App
    File       : app_sweep.c
    platform   : STM32F407ZG
    function   : 幅频特性扫频功能
*********************************************************************************************************
*/
#include "app_sweep.h"
#include "TFT_LCD.h"
#include "Drive_AD9959.h"      // AD9959驱动
#include <math.h>

volatile uint8_t Sweep_Start_Flag = 0;

#define GRAPH_X_MIN         (250 + 2 + 16)
#define GRAPH_X_MAX         (800 - 16)
#define GRAPH_Y_MIN         (64 + 8 + 32)
#define GRAPH_Y_MAX         (480 - 32 - 16 - 32)

#define GRAPH_WIDTH         (GRAPH_X_MAX - GRAPH_X_MIN)
#define GRAPH_HEIGHT        (GRAPH_Y_MAX - GRAPH_Y_MIN)

#define LOG_START           3.0f
#define LOG_END             7.0f
#define LOG_RANGE           (LOG_END - LOG_START)

#define MAX_VOLTAGE         3.3f

static void Draw_Axis(void)
{
    LCD_SetTextColor(White);

    LCD_DrawLine(GRAPH_X_MIN, GRAPH_Y_MAX, GRAPH_WIDTH, 0);
    LCD_DrawLine(GRAPH_X_MIN, GRAPH_Y_MIN, GRAPH_HEIGHT, 1);

    OS_String_Show(GRAPH_X_MIN, GRAPH_Y_MAX + 8, 16, 1, "Freq(Hz)");
    OS_String_Show(GRAPH_X_MIN - 48, GRAPH_Y_MIN + GRAPH_HEIGHT / 2, 16, 1, "Amplitude(V)");

    float freq_mark = 1000.0f;
    float log_mark;
    uint16_t x_pos;
    char freq_str[16];

    while (freq_mark <= SWEEP_END_FREQ)
    {
        log_mark = log10f(freq_mark);
        x_pos = GRAPH_X_MIN + (uint16_t)((log_mark - LOG_START) / LOG_RANGE * GRAPH_WIDTH);

        LCD_DrawLine(x_pos, GRAPH_Y_MAX, 4, 1);

        if (freq_mark < 1000000.0f)
            sprintf(freq_str, "%dk", (uint32_t)(freq_mark / 1000));
        else
            sprintf(freq_str, "%.1fM", freq_mark / 1000000.0f);

        OS_String_Show(x_pos - 16, GRAPH_Y_MAX + 16, 16, 1, freq_str);

        if (freq_mark < 10000.0f)
            freq_mark *= 10.0f;
        else
            freq_mark *= 5.0f;
    }

    for (uint8_t i = 0; i <= 5; i++)
    {
        uint16_t y_pos = GRAPH_Y_MAX - (uint16_t)((float)i / 5.0f * GRAPH_HEIGHT);
        LCD_DrawLine(GRAPH_X_MIN, y_pos, 4, 0);

        sprintf(freq_str, "%.1f", (float)i / 5.0f * MAX_VOLTAGE);
        OS_String_Show(GRAPH_X_MIN - 40, y_pos - 8, 16, 1, freq_str);
    }
}

static uint16_t Freq_to_X(float freq)
{
    float log_freq = log10f(freq);
    float ratio = (log_freq - LOG_START) / LOG_RANGE;

    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    return GRAPH_X_MIN + (uint16_t)(ratio * GRAPH_WIDTH);
}

static uint16_t Volt_to_Y(float voltage)
{
    float ratio = voltage / MAX_VOLTAGE;

    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    return GRAPH_Y_MAX - (uint16_t)(ratio * GRAPH_HEIGHT);
}

void Start_Sweep_Scan(void)
{
    float freq = SWEEP_START_FREQ;
    float voltage;
    uint16_t x, y;
    uint16_t last_x = 0, last_y = 0;
    uint8_t first_point = 1;
    uint8_t point_count = 0;

    LCD_Appoint_Clear(GRAPH_X_MIN, GRAPH_Y_MIN, GRAPH_X_MAX, GRAPH_Y_MAX, Black);

    Draw_Axis();

    LCD_SetTextColor(Green);

    while (freq <= SWEEP_END_FREQ && point_count < SWEEP_MAX_POINTS)
    {
        // 设置频率
        AD9959_Set_Fre(0, (uint32_t)freq);

        OSTimeDly(5);

        voltage = AD637_ReadVoltage();

        x = Freq_to_X(freq);
        y = Volt_to_Y(voltage);

        if (first_point)
        {
            first_point = 0;
            LCD_DrawPoint(x, y, Green);
        }
        else
        {
            LCD_DrawuniLine(last_x, last_y, x, y);
            LCD_DrawPoint(x, y, Green);
        }

        last_x = x;
        last_y = y;

        freq *= SWEEP_STEP_RATIO;
        point_count++;
    }

    LCD_SetTextColor(White);
    OS_String_Show(GRAPH_X_MIN + GRAPH_WIDTH / 2 - 48, GRAPH_Y_MIN - 24, 24, 1, "Complete!");

    Sweep_Start_Flag = 0;
}