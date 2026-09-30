/* ****************************
 * Project description:
 *
 * A Project empty template file
 *
 * Author: ï¿½ï¿½ï¿½Â»ï¿½ï¿½ï¿½ -> 2019 Mao
 *
 * Creation Date: 2021/09/05
 *
 * UpDate:
 * 1-> ï¿½ï¿½ï¿½ï¿½ï¿½Ë¶ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿?
 * 3-> ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½×¢ï¿½ï¿½
 * Update date: 2021/11/03
 * ****************************/

/* AC6: source files use GBK encoding for Chinese font data, suppress warning */
#pragma clang diagnostic ignored "-Winvalid-source-encoding"

/* ***************************** Include & Define Part     	*****************************
 * Í·ï¿½Ä¼ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ê¶¨ï¿½ï¿½ï¿½ï¿½
 * */
#include "User.h"
#include "User_header.h"
#include "app_sweep.h"
#include "app_fft.h"
#include "app_measure.h"
#include "Drive_AD637.h"
#include "Drive_AD9959.h"

void AD9959_Drawselect(void);
void AD9959_Selet_Clear(void);
void AD9959_Changeval(uint8_t KeyNum);
uint8_t AD9959_NumScan(uint32_t Num);
void AD9959_Show(void);
void AD9959_Fre_Show(uint16_t x,uint16_t y,uint32_t fre);
float Read_Key(float Num);
void DAC_ValRefresh(void);
void DAC_Select(void);
float AD9959_SelectNum(void);
void AD9959_PopNum(float Num);
void Refresh(void);

/* ***************************** Variable definition Part   *****************************
 * ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
 * */

// ADC//DAC
float adc_val;
u16 adc;
u16 value;
float step = 0.5f;
uint8_t ADCstate=0;
float DAC_val[2];

float DAC1_val;
float DAC2_val;
//ADS1256
int ADS1256_ad;
float ADS1256_val;

//AD9959
volatile uint32_t channel_fre[4]={50000,50000,50000,50000};
volatile float channel_Amp[4] = {200.0f, 200.0f, 200.0f, 200.0f};
volatile uint16_t channel_Pha[4]={0,0,0,0};

volatile uint8_t state_channel;    //Í¨ï¿½ï¿½ï¿½ï¿½Ö¾
volatile uint8_t state_Allkindval; //ï¿½ï¿½ï¿½ï¿½Ñ¡ï¿½ï¿½


// ï¿½Ëµï¿½ï¿½ï¿½Å±ï¿½ï¿½ï¿?
volatile uint8_t MenuSign = 0;

/* ***************************** Main Part                  *****************************
 * ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
 * */
void User_main(void)
{
	// ï¿½ï¿½Ê¼ï¿½ï¿½È«ï¿½ï¿½
	Init_All();

	// ï¿½ï¿½Ê¾ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
	Disp_Main();

	while (1)
	{
		switch (MenuSign)
		{
		case 0:
			if (Ps2KeyValue != KeyValue_Null) // ï¿½ï¿½Î´Ñ¡ï¿½Ð²Ëµï¿½Ê±ï¿½Ð°ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
				Change_Menu(Ps2KeyValue);	  // ï¿½ï¿½ï¿½Ý°ï¿½ï¿½ï¿½ï¿½Ä±ï¿½Ëµï¿½ï¿½ï¿½ï¿½ï¿?
			;
			break;
		case 1:
			MenuHandler_6();
			break;
		case 2:
			MenuHandler_1();
			break;
		case 3:
			MenuHandler_2();
			break;
		case 4:
			MenuHandler_3();
			break;
		case 5:
			MenuHandler_4();
			break;
		case 6:
			MenuHandler_5();
			break;
		default:
			break;
		}

		delay_ms(10);
	}
}

/* ***************************** Initialization Part        *****************************
 * ï¿½ï¿½Ê¼ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
 * */

// ï¿½ï¿½Ê¼ï¿½ï¿½È«ï¿½ï¿½ ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
void Init_All()
{
	LCD_Clear(Black);

	 //AD DAï¿½ï¿½Ê¼ï¿½ï¿½
	 ADC1_Init();    //PA1
	 ADC2_Init();    //PA2
	 ADC3_Init();    //PA3
	 DAC1_Init();    //PA4
	 DAC2_Init();    //PA5
}

/* ***************************** Display Part               *****************************
 * ï¿½ï¿½Ê¾ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
 * */

// ï¿½ï¿½Ê¾ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
void Disp_Main()
{
	uint8_t count;

	// Show title
	OS_String_Show(400 - 32 * 2, 16, 32, 0, TitleStr);
	
	// Draw line
	LCD_Appoint_Clear(0, 64, 800, 64 + 8, White);
	LCD_Appoint_Clear(0, 480 - 32 - 8, 800, 480 - 32, White);
	LCD_Appoint_Clear(250, 64 + 8, 250 + 2, 480 - 32 - 8, White);

	// Show model version str
	OS_String_Show(32, 480 - 16 - 8, 16, 0, ModelVerStr);
	// Show user version str
	OS_String_Show(632, 480 - 16 - 8, 16, 0, UserVerStr);

	// Disp menu
	for (count = 1; count < MenuChoiceNum + 1; count++)
		OS_String_Show(32, 32 + 64 * count, 32, 0, ">");
	for (count = 0; count < MenuChoiceNum; count++)
	{
		switch (count)
		{
		case 0:
			OS_String_Show(80, 96, 32, 0, Menu1Choice1);
			break;
		case 1:
			OS_String_Show(80, 96 + 64, 32, 0, Menu1Choice2);
			break;
		case 2:
			OS_String_Show(80, 96 + 64 * 2, 32, 0, Menu1Choice3);
			break;
		case 3:
			OS_String_Show(80, 96 + 64 * 3, 32, 0, Menu1Choice4);
			break;
		case 4:
			OS_String_Show(80, 96 + 64 * 4, 32, 0, Menu1Choice5);
			break;
		case 5:
			OS_String_Show(80, 96 + 64 * 5, 32, 0, Menu1Choice6);
			break;
		default:
			break;
		}
	}
	OS_String_Show(400-3*24,480-24,24,1,"ºþÄÏÎÄÀíÑ§Ôº");
}

// ï¿½ï¿½ï¿½ï¿½ï¿½Ö?ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½1>Î»ï¿½Ã£ï¿½2>ï¿½ï¿½Öµï¿½ï¿½3>ï¿½ï¿½ï¿½ï¿½ï¿½Ê?ï¿½ï¿½"ï¿½ï¿½Öµ%0.0f"
void Show_Val(uint8_t location, float value, char *str)
{

	if (location > 0 && location <= 10)
		OS_Num_Show(250 + 64, 96 + 32 * (location - 1), 32, 1, value, str);
	else if (location > 10 && location <= 20)
		OS_Num_Show(500 + 64, 96 + 32 * (location - 11), 32, 1, value, str);
	else
		OS_String_Show(250 + 64, 96, 32, 1, "ERROR");
}

// ï¿½Ð»ï¿½ï¿½Ëµï¿½Ò³ ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½1>ï¿½Ëµï¿½ï¿½ï¿½ï¿?1~5)
void Change_Menu(uint8_t menu_sign)
{
	uint8_t count;

	// ï¿½ï¿½ï¿½ï¿½ï¿½Ê¾ï¿½ï¿½ï¿½ï¿?
	LCD_Appoint_Clear(250 + 2, 64 + 8, 800, 480 - 32 - 8, Black);

	for (count = 1; count < MenuChoiceNum + 1; count++)
		OS_String_Show(32, 32 + 64 * count, 32, 1, ">");

	if (menu_sign > 0 && menu_sign <= MenuChoiceNum)
		OS_String_Show(32, 32 + 64 * menu_sign, 32, 1, ">");
	else
		menu_sign = 0;

	Ps2KeyValue = KeyValue_Null;
	MenuSign = menu_sign;
}

/* ***************************** Menu Handler Part     	 	 	*****************************
 * ï¿½Ëµï¿½Ö´ï¿½Ðºï¿½ï¿½ï¿½ï¿½ï¿½
 */

// ï¿½Ëµï¿½1Ö´ï¿½Ðºï¿½ï¿½ï¿½ ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
void MenuHandler_1()
{
	float adc1_val, adc2_val, adc3_val;
	Ps2KeyValue = KeyValue_Null;
	//ADCï¿½ï¿½Öµï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ê¾
	Refresh();
	DAC_Select();
	// ï¿½Ö±ï¿½ï¿½È¡ï¿½ï¿½ï¿½ï¿½ADCÍ¨ï¿½ï¿½ï¿½ï¿½Öµï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ê¾ï¿½ï¿½Í¬ï¿½Ä´ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
	adc1_val = ADC1_GetVoltage();
	adc2_val = ADC2_GetVoltage();
	adc3_val = ADC3_GetVoltage();
	OS_Num_Show(LCD_CONTENT_X, LCD_ROW_Y(1), 32, 1, adc1_val, "PA1 ADC1:  %.6fV");
	OS_Num_Show(LCD_CONTENT_X, LCD_ROW_Y(2), 32, 1, adc2_val, "PA2 ADC2:  %.6fV");
	OS_Num_Show(LCD_CONTENT_X, LCD_ROW_Y(3), 32, 1, adc3_val, "PA3 ADC3:  %.6fV");
	while (Ps2KeyValue != KeyValue_Back)
	{
		float Temp;
		OS_String_Show(LCD_LABEL_X, LCD_TITLE_Y, 32, 1, "AD");

		adc1_val = ADC1_GetVoltage();
		OS_Num_Show(LCD_CONTENT_X, LCD_ROW_Y(1), 32, 1, adc1_val, "PA1 ADC1:  %.6fV");

		adc2_val = ADC2_GetVoltage();
		OS_Num_Show(LCD_CONTENT_X, LCD_ROW_Y(2), 32, 1, adc2_val, "PA2 ADC2:  %.6fV");

		adc3_val = ADC3_GetVoltage();
		OS_Num_Show(LCD_CONTENT_X, LCD_ROW_Y(3), 32, 1, adc3_val, "PA3 ADC3:  %.6fV");

		if(Ps2KeyValue != KeyValue_Null)
		{
			if(Ps2KeyValue<=KeyValue_Point)
			{
				Temp=Read_Key(DAC_val[ADCstate]);
				if(Temp>3.3f)Temp=3.3f;
				if(Temp<0.0f)Temp=0.0f;
				DAC_val[ADCstate]=Temp;
				DAC_ValRefresh();
			}
			else if(Ps2KeyValue==KeyValue_Add)
			{
				DAC_val[ADCstate]+=0.5f;
				if(DAC_val[ADCstate]>3.3f)DAC_val[ADCstate]=3.3f;
				Ps2KeyValue = KeyValue_Null;
				DAC_ValRefresh();
			}
			else if(Ps2KeyValue==KeyValue_Minus)
			{
				DAC_val[ADCstate]-=0.5f;
				if(DAC_val[ADCstate]<0.0f)DAC_val[ADCstate]=0.0f;
				Ps2KeyValue = KeyValue_Null;
				DAC_ValRefresh();
			}
			else if(Ps2KeyValue==KeyValue_NumLock)
			{
				//DACÑ¡ï¿½ï¿½ï¿½ï¿½ï¿?
				ADCstate++;
				ADCstate%=2;
				DAC_Select();
				Ps2KeyValue = KeyValue_Null;
			}
		}
		//DACï¿½ï¿½ï¿?
		DAC1_Out(DAC_val[0]);
		DAC2_Out(DAC_val[1]);
		delay_ms(100);
	}

	Change_Menu(0);
}

// ï¿½Ëµï¿½2Ö´ï¿½Ðºï¿½ï¿½ï¿½ ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
void MenuHandler_2()
{
	Ps2KeyValue = KeyValue_Null;
	Init_AD9959();             
	AD9959_Drawselect();
	AD9959_Show();
	while (Ps2KeyValue != KeyValue_Back)
	{
		float Temp;
		uint8_t index;
		//Ñ¡ï¿½ï¿½Í¨ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Öµï¿½ï¿½ï¿½Ð¸ï¿½ï¿½ï¿½
		if(Ps2KeyValue==KeyValue_NumLock)
		{
			state_Allkindval++;
			state_Allkindval%=3;
			Ps2KeyValue=KeyValue_Null;
			AD9959_Drawselect();
		}
		//Ñ¡ï¿½ï¿½ï¿½Í¨ï¿½ï¿½ï¿½ï¿½ï¿½Ð¸ï¿½ï¿½ï¿?
		else if(Ps2KeyValue==KeyValue_Div)	
		{
			state_channel++;
			state_channel%=4;
			Ps2KeyValue=KeyValue_Null;
			AD9959_Drawselect();
		}
		//ï¿½ï¿½ï¿½ï¿½Îªï¿½Ó»ï¿½ï¿½Ê±ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Öµï¿½ï¿½ï¿½Ð¸Ä±ï¿?
		else if(Ps2KeyValue==KeyValue_Add||Ps2KeyValue==KeyValue_Minus)
		{
			AD9959_Changeval(Ps2KeyValue);
			Ps2KeyValue=KeyValue_Null;
		}
		//ï¿½ï¿½ï¿½ï¿½Îª0-9ï¿½ï¿½Ð¡ï¿½ï¿½ï¿½ï¿½Ê±ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
		else if(Ps2KeyValue<=KeyValue_Point)
		{
			Temp=AD9959_SelectNum();
			Temp=Read_Key(Temp);
			index=AD9959_NumScan(Temp);
			if(index>=8)
			{
				LCD_Appoint_Clear(16, 96 + 64 * 4, 250 + 2+32, 480 - 32 - 8, Black);
				LCD_Appoint_Clear(250, 64 + 8, 250 + 2, 480 - 32 - 8, White);
			}
			AD9959_PopNum(Temp);
		}
		//AD9959ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ê?
		AD9959_Show();
		delay_ms(10);
	}
	Change_Menu(0);
}

// ï¿½Ëµï¿½3Ö´ï¿½Ðºï¿½ï¿½ï¿½ ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
void MenuHandler_3()
{
	//ADS1256ï¿½ï¿½Ê¼ï¿½ï¿½
	ADS1256_Init();
	Ps2KeyValue = KeyValue_Null;
	while (Ps2KeyValue != KeyValue_Back)
	{
		//ï¿½ï¿½ï¿½Ï¸ï¿½ï¿½Ð»ï¿½ï¿½ADS1256ï¿½ï¿½Í¨ï¿½ï¿½ï¿½ï¿½Öµï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ð¸ï¿½ï¿½ï¿½
		ADS1256_ad = Moving_Average_Filter(ADS1256_MUXP_AIN0 | ADS1256_MUXN_AINCOM,5);
		ADS1256_val = Get_Val(ADS1256_ad);
		OS_String_Show(LCD_LEFT_MARGIN, LCD_TITLE_Y, 32, 1, "ADS1256");
		OS_Num_Show(LCD_LEFT_MARGIN, LCD_TITLE_Y + LCD_ROW_SPACING * 1, 32, 1, ADS1256_val, "Í¨µÀ0:  %.2fV ");
		ADS1256_ad = Moving_Average_Filter(ADS1256_MUXP_AIN1 | ADS1256_MUXN_AINCOM,5);
		ADS1256_val = Get_Val(ADS1256_ad);
		OS_Num_Show(LCD_LEFT_MARGIN, LCD_TITLE_Y + LCD_ROW_SPACING * 2, 32, 1, ADS1256_val, "Í¨µÀ1:  %.2fV ");
		ADS1256_ad = Moving_Average_Filter(ADS1256_MUXP_AIN2 | ADS1256_MUXN_AINCOM,5);
		ADS1256_val = Get_Val(ADS1256_ad);
		OS_Num_Show(LCD_LEFT_MARGIN, LCD_TITLE_Y + LCD_ROW_SPACING * 3, 32, 1, ADS1256_val, "Í¨µÀ2:  %.2fV ");
		ADS1256_ad = Moving_Average_Filter(ADS1256_MUXP_AIN3 | ADS1256_MUXN_AINCOM,5);
		ADS1256_val = Get_Val(ADS1256_ad);
		OS_Num_Show(LCD_LEFT_MARGIN, LCD_TITLE_Y + LCD_ROW_SPACING * 4, 32, 1, ADS1256_val, "Í¨µÀ3:  %.2fV ");
		ADS1256_ad = Moving_Average_Filter(ADS1256_MUXP_AIN4 | ADS1256_MUXN_AINCOM,5);
		ADS1256_val = Get_Val(ADS1256_ad);
		OS_Num_Show(LCD_LEFT_MARGIN, LCD_TITLE_Y + LCD_ROW_SPACING * 5, 32, 1, ADS1256_val, "Í¨µÀ4:  %.2fV ");	
		ADS1256_ad = Moving_Average_Filter(ADS1256_MUXP_AIN5 | ADS1256_MUXN_AINCOM,5);
		ADS1256_val = Get_Val(ADS1256_ad);
		OS_Num_Show(LCD_LEFT_MARGIN, LCD_TITLE_Y + LCD_ROW_SPACING * 6, 32, 1, ADS1256_val, "Í¨µÀ5:  %.2fV ");
		ADS1256_ad = Moving_Average_Filter(ADS1256_MUXP_AIN6 | ADS1256_MUXN_AINCOM,5);
		ADS1256_val = Get_Val(ADS1256_ad);
		OS_Num_Show(LCD_COL2_X, LCD_TITLE_Y + LCD_ROW_SPACING * 1, 32, 1, ADS1256_val, "Í¨µÀ6:  %.2fV ");
		ADS1256_ad = Moving_Average_Filter(ADS1256_MUXP_AIN7 | ADS1256_MUXN_AINCOM,5);		
		ADS1256_val = Get_Val(ADS1256_ad);
		OS_Num_Show(LCD_COL2_X, LCD_TITLE_Y + LCD_ROW_SPACING * 2, 32, 1, ADS1256_val, "Í¨µÀ7:  %.2fV ");
		delay_ms(10);
	}
	Change_Menu(0);
}


// ï¿½Ë…h4Ö´ï¿½Ðºï¿½ï¿½ï¿½ï¿½ï¿½É¨Æµï¿½ï¿½ï¿½ï¿½
void MenuHandler_4()
{
	float start_freq = 1000.0f;
	float end_freq   = 10000000.0f;
	u32   points     = 200;

	Ps2KeyValue = KeyValue_Null;

	LCD_Appoint_Clear(250 + 2, 64 + 8, 800, 480 - 32 - 8, Black);
	OS_String_Show(LCD_LABEL_X, LCD_TITLE_Y, 32, 1, "Sweep Mode (·ùÆµÌØÐÔÉ¨Æµ)");

	OS_String_Show(LCD_LABEL_X, LCD_ROW_Y(1), 32, 1, "Start Freq:");
	OS_Num_Show(LCD_CONTENT_X, LCD_ROW_Y(1), 32, 1, start_freq, "%0.0f Hz");

	OS_String_Show(LCD_LABEL_X, LCD_ROW_Y(2), 32, 1, "End Freq:");
	OS_Num_Show(LCD_CONTENT_X, LCD_ROW_Y(2), 32, 1, end_freq / 1000000.0f, "%0.2f MHz");

	OS_String_Show(LCD_LABEL_X, LCD_ROW_Y(3), 32, 1, "Points:");
	OS_Num_Show(LCD_CONTENT_X, LCD_ROW_Y(3), 32, 1, (float)points, "%d");

	OS_String_Show(LCD_LABEL_X, LCD_ROW_Y(5), 24, 1, "[Enter] Start Scan  [Back] Return");

	Init_AD9959();
	AD637_Init();

	while (Ps2KeyValue != KeyValue_Back)
	{
		if (Ps2KeyValue == KeyValue_Enter)
		{
			Ps2KeyValue = KeyValue_Null;
			OS_String_Show(LCD_LABEL_X, LCD_ROW_Y(5), 24, 1, "Scanning...           ");
			Sweep_Start_Flag = 1;
			Start_Sweep_Scan();
			OS_String_Show(LCD_LABEL_X, LCD_ROW_Y(5), 24, 1, "[Enter] Start Scan  [Back] Return");
		}
		delay_ms(10);
	}

	ADC1_Init();
	Change_Menu(0);
}

// ï¿½Ë…h5ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½FFT Æµï¿½×·ï¿½ï¿½ï¿½
void MenuHandler_5()
{
	Ps2KeyValue = KeyValue_Null;
	LCD_Appoint_Clear(250 + 2, 64 + 8, 800, 480 - 32 - 8, Black);
	OS_String_Show(LCD_LABEL_X, LCD_TITLE_Y, 32, 1, "FFT Spectrum Analysis");
	OS_String_Show(LCD_LABEL_X, LCD_ROW_Y(5), 24, 1, "[Back] Exit");

	Start_FFT_Analysis();

	/* Restore ADC1 for Menu 1 after FFT reconfigured it */
	ADC1_Init();
	ADC2_Init();
	ADC3_Init();
}

// Menu 6: G-topic periodic signal measurement
void MenuHandler_6()
{
	Ps2KeyValue = KeyValue_Null;
	Start_G_Measure();
	/* ADC1/2/3 restored inside Start_G_Measure() */
}

/* ***************************** Custom Function Part       *****************************
 * ï¿½ï¿½ï¿½ï¿½ï¿½Ô¶ï¿½ï¿½åº¯ï¿½ï¿½ï¿½ï¿½
 */

// ï¿½ï¿½PS2ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½1>Î´ï¿½ï¿½ï¿½ï¿½Ê±ï¿½ï¿½ï¿½Øµï¿½ï¿½ï¿½Öµ ï¿½ï¿½ï¿½Ø£ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ö?
float PS2_ReadNum(float num)
{
	uint8_t count = 0;
	uint8_t dec_sign = 0;
	float temp_num = 0;
	uint8_t tim6WasEnabled = (TIM6->CR1 & TIM_CR1_CEN) ? 1 : 0;

	if (Ps2KeyValue <= KeyValue_Point)
	{
		TIM_Cmd(TIM6, DISABLE);

		LCD_Appoint_Clear(16, 96 + 64 * 4, 250 + 1, 480 - 32 - 8, Black);
		OS_Rect_Draw(16, 96 + 64 * 4, 250, 96 + 64 * 5-16, 1, White);

		while (Ps2KeyValue <= KeyValue_Point)
		{
			if (dec_sign == 0)
			{
				if (Ps2KeyValue == KeyValue_Point)
				{
					dec_sign = 1;
					count = 0;
				}
				else
					temp_num = temp_num * 10 + Ps2KeyValue;
			}
			else
			{
				if (Ps2KeyValue != KeyValue_Point)
					temp_num = temp_num + (float)Ps2KeyValue / pow(10, count);
			}

			OS_Num_Show(16 + 16, 96 + 64 * 4 + 16, 32, 1, temp_num, "-> %0.2f");

			count++;
			Ps2KeyValue = KeyValue_Null;
			while (Ps2KeyValue == KeyValue_Null);
		}

		Ps2KeyValue = KeyValue_Null;
		LCD_Appoint_Clear(16, 96 + 64 * 4, 250 + 1, 480 - 32 - 8, Black);

		if(tim6WasEnabled)
			TIM_Cmd(TIM6, ENABLE);
	}
	else
		temp_num = num;

	Ps2KeyValue = KeyValue_Null;

	return temp_num;
}

//DAï¿½ï¿½Öµï¿½ï¿½Ê¾ï¿½ï¿½ï¿½ï¿½Ë¢ï¿½ï¿½
void Refresh(void)
{
	OS_Rect_Draw(LCD_LEFT_MARGIN+13*16, LCD_ROW_Y(1), 800, LCD_ROW_Y(2), 32, Black);
	OS_String_Show(LCD_LEFT_MARGIN, LCD_DA_LABEL_Y, 32, 1, "DA");
	OS_Num_Show(LCD_CONTENT_X, LCD_DA1_Y, 32, 1, DAC_val[0], "PA4 DAC1:  %.2fV     ");
	OS_Num_Show(LCD_CONTENT_X, LCD_DA2_Y, 32, 1, DAC_val[1], "PA5 DAC2:  %.2fV     ");	
}

//AD9959ï¿½ï¿½Ñ¡ï¿½ï¿½ï¿½ï¿½
void AD9959_Drawselect(void)
{
	AD9959_Selet_Clear();
	if(state_channel==0)
	{
		switch(state_Allkindval)
		{
			case 0:
				OS_String_Show(LCD_DDS_CURSOR_X, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 1, 24, 1, ">");
			break;
			case 1:
				OS_String_Show(LCD_DDS_CURSOR_X, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 2, 24, 1, ">");
			break;
			case 2:
				OS_String_Show(LCD_DDS_CURSOR_X, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 3, 24, 1, ">");
			break;
			default:
				break;
		}
	}
	else if(state_channel==1)
	{
		switch(state_Allkindval)
		{
			case 0:
				OS_String_Show(LCD_DDS_CURSOR_X2, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 1, 24, 1, ">");
			break;
			case 1:
				OS_String_Show(LCD_DDS_CURSOR_X2, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 2, 24, 1, ">");
			break;
			case 2:
				OS_String_Show(LCD_DDS_CURSOR_X2, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 3, 24, 1, ">");
			break;
			default:
				break;
		}
	}
	else if(state_channel==2)
	{
		switch(state_Allkindval)
		{
			case 0:
				OS_String_Show(LCD_DDS_CURSOR_X, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 5 + 16, 24, 1, ">");
			break;
			case 1:
				OS_String_Show(LCD_DDS_CURSOR_X, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 6 + 16, 24, 1, ">");
			break;
			case 2:
				OS_String_Show(LCD_DDS_CURSOR_X, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 7 + 16, 24, 1, ">");
			break;
			default:
				break;
		}
	}
	else if(state_channel==3)
	{
		switch(state_Allkindval)
		{
			case 0:
				OS_String_Show(LCD_DDS_CURSOR_X2, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 5 + 16, 24, 1, ">");
			break;
			case 1:
				OS_String_Show(LCD_DDS_CURSOR_X2, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 6 + 16, 24, 1, ">");
			break;
			case 2:
				OS_String_Show(LCD_DDS_CURSOR_X2, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 7 + 16, 24, 1, ">");
			break;
			default:
				break;
		}
	}
}
//AD9959ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
void AD9959_Selet_Clear(void)
{
	OS_Rect_Draw(LCD_DDS_CURSOR_X, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 1, LCD_DDS_CURSOR_X + 16, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 7 + 16 + 24, 24, Black);
	OS_Rect_Draw(LCD_DDS_CURSOR_X2, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 1, LCD_DDS_CURSOR_X2 + 16, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 7 + 16 + 24, 24, Black);
}

//ï¿½ï¿½ï¿½Ì¶ï¿½AD9959ï¿½ï¿½Ó¦ï¿½ï¿½Öµï¿½ï¿½ï¿½Ð¼Ó¼ï¿½
void AD9959_Changeval(uint8_t KeyNum)
{
	uint32_t FreStep = 1;
	//ï¿½ï¿½Öµï¿½ï¿½ï¿½ï¿½
	if(KeyNum==KeyValue_Minus)
	{
		switch(state_Allkindval)
		{
			case 0:
				if(channel_fre[state_channel] >= 1000000)FreStep = 1000000;
				else if(channel_fre[state_channel] >= 1000)FreStep = 1000;
				else FreStep = 1;
				if(channel_fre[state_channel] <= FreStep){channel_fre[state_channel]=0;}
				else{channel_fre[state_channel]-=FreStep;}
				AD9959_Set_Fre(state_channel,channel_fre[state_channel]);
				break;
			case 1:
				if(channel_Amp[state_channel] <= 10.0f){channel_Amp[state_channel]=0.0f;}
				else{channel_Amp[state_channel]-=10.0f;}
				AD9959_Set_Amp(state_channel,channel_Amp[state_channel]);
				break;
			case 2:
				if(channel_Pha[state_channel] <= 10){channel_Pha[state_channel]=0;}
				else{channel_Pha[state_channel]-=10;}
				AD9959_Set_Phase(state_channel,channel_Pha[state_channel]);
				break;
			default:
				break;
		}
	}
	//ï¿½ï¿½Öµï¿½ï¿½ï¿½ï¿½
	if(KeyNum==KeyValue_Add)
	{
		switch(state_Allkindval)
		{
			case 0:
				if(channel_fre[state_channel] >= 1000000)FreStep = 1000000;
				else if(channel_fre[state_channel] >= 1000)FreStep = 1000;
				else FreStep = 1;
				channel_fre[state_channel]+=FreStep;
				if(channel_fre[state_channel]>500000000){channel_fre[state_channel]=500000000;}
				AD9959_Set_Fre(state_channel,channel_fre[state_channel]);
				break;
			case 1:
				channel_Amp[state_channel]+=10;
				if(channel_Amp[state_channel]>576.0f){channel_Amp[state_channel]=576.0f;}
				AD9959_Set_Amp(state_channel,channel_Amp[state_channel]);
				break;
			case 2:
				channel_Pha[state_channel]+=10;
				if(channel_Pha[state_channel]>360){channel_Pha[state_channel]=360;}
				AD9959_Set_Phase(state_channel,channel_Pha[state_channel]);
				break;
			default:
				break;
		}
	}
	//AD9959ï¿½ï¿½ï¿½ï¿½Ó¦Î»ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿?
}
//AD9959Æµï¿½ï¿½Î»ï¿½ï¿½ï¿½Ð¶ï¿½
uint8_t AD9959_NumScan(uint32_t Num)
{
	uint8_t count=0;
	if(Num==0){return 1;}
	while(Num!=0)
	{
		Num/=10;
		count++;
	}
	return count;
}
//AD9959Æµï¿½ï¿½ï¿½ï¿½Ê¾
void AD9959_Fre_Show(uint16_t x,uint16_t y,uint32_t fre)
{
	if(fre>1000000)
	{
		OS_Num_Show(x,y,24,1,(float)fre/1000000,"ÆµÂÊ:%.3fMHz      ");
	}
	else if(fre>1000)
	{
		OS_Num_Show(x,y,24,1,(float)fre/1000,"ÆµÂÊ:%.3fKHz      ");
	}
	else
	{
		OS_Num_Show(x,y,24,1,fre,"ÆµÂÊ:%0.0fHz      ");
	}
}

//AD9959ï¿½ï¿½Ê¾
void AD9959_Show(void)
{
	OS_String_Show(LCD_DDS_X, LCD_TITLE_Y, 32, 1, "AD9959");
	OS_String_Show(LCD_DDS_X, LCD_DDS_CH_Y, 24, 1, "Í¨µÀ0");
	AD9959_Fre_Show(LCD_DDS_X, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 1, channel_fre[0]);
	OS_Num_Show(LCD_DDS_X, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 2, 24, 1, channel_Amp[0], "·ù¶È:%.2fmV      ");
	OS_Num_Show(LCD_DDS_X, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 3, 24, 1, channel_Pha[0], "ÏàÎ»:%0.0f        ");	

	OS_String_Show(LCD_DDS_COL2_X, LCD_DDS_CH_Y, 24, 1, "Í¨µÀ1");
	AD9959_Fre_Show(LCD_DDS_COL2_X, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 1, channel_fre[1]);
	OS_Num_Show(LCD_DDS_COL2_X, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 2, 24, 1, channel_Amp[1], "·ù¶È:%.2fmV      ");
	OS_Num_Show(LCD_DDS_COL2_X, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 3, 24, 1, channel_Pha[1], "ÏàÎ»:%0.0f      ");	

	OS_String_Show(LCD_DDS_X, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 4 + 16, 24, 1, "Í¨µÀ2");
	AD9959_Fre_Show(LCD_DDS_X, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 5 + 16, channel_fre[2]);
	OS_Num_Show(LCD_DDS_X, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 6 + 16, 24, 1, channel_Amp[2], "·ù¶È:%.2fmV      ");
	OS_Num_Show(LCD_DDS_X, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 7 + 16, 24, 1, channel_Pha[2], "ÏàÎ»:%0.0f");	

	OS_String_Show(LCD_DDS_COL2_X, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 4 + 16, 24, 1, "Í¨µÀ3");
	AD9959_Fre_Show(LCD_DDS_COL2_X, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 5 + 16, channel_fre[3]);
	OS_Num_Show(LCD_DDS_COL2_X, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 6 + 16, 24, 1, channel_Amp[3], "·ù¶È:%.2fmV      ");
	OS_Num_Show(LCD_DDS_COL2_X, LCD_DDS_CH_Y + LCD_DDS_ITEM_SP * 7 + 16, 24, 1, channel_Pha[3], "ÏàÎ»:%0.0f       ");	
}
//ï¿½ï¿½ï¿½AD9959ï¿½ï¿½Ç°ï¿½ï¿½ï¿½Ö¸ï¿½ï¿½ï¿½Öµ
float AD9959_SelectNum(void)
{
	switch(state_Allkindval)
	{
		case 0:
			return channel_fre[state_channel];
		case 1:
			return channel_Amp[state_channel];
		case 2:
			return channel_Pha[state_channel];
		default:
			break;
	}
	return 0;
}
//AD9959ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ÖµÐ´ï¿½ëµ±Ç°ï¿½ï¿½ï¿½Ö¸ï¿½ï¿½ï¿½Ö?
void AD9959_PopNum(float Num)
{
	switch(state_Allkindval)
	{
		case 0:
			if(Num>500000000.0f)Num=500000000.0f;
			if(Num<0.0f)Num=0.0f;
			channel_fre[state_channel]=Num;
			AD9959_Set_Fre(state_channel,channel_fre[state_channel]);
			break;
		case 1:
			if(Num>576.0f)Num=576.0f;
			if(Num<0.0f)Num=0.0f;
			channel_Amp[state_channel]=Num;
			AD9959_Set_Amp(state_channel,channel_Amp[state_channel]);
			break;
		case 2:
			if(Num>360.0f)Num=360.0f;
			if(Num<0.0f)Num=0.0f;
			channel_Pha[state_channel]=Num;
			AD9959_Set_Phase(state_channel,channel_Pha[state_channel]);
			break;
		default:
			break;
	}
}
//ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½È¡
float Read_Key(float Num)
{
		float Num1=0;
		if(Ps2KeyValue<=KeyValue_Point)
		while(Ps2KeyValue!=KeyValue_Enter)
		{			
			if(Ps2KeyValue<=KeyValue_Point)
			{
				Num1=PS2_ReadNum(0);
				Ps2KeyValue = KeyValue_Null;
				return Num1;
			}	
			delay_ms(10);
		}	
		else 
		{
			return Num;
		}
		return Num;
}
//DACÖµï¿½ï¿½ï¿½ï¿½Ó¦Î»ÖµË¢ï¿½ï¿½
void DAC_ValRefresh(void)
{
	if(ADCstate==0)
	{
		OS_Num_Show(LCD_DA_VALUE_X, LCD_DA1_Y, 32, 1, DAC_val[0], "%.2fV       ");
	}
	else if(ADCstate==1)
	{
		OS_Num_Show(LCD_DA_VALUE_X, LCD_DA2_Y, 32, 1, DAC_val[1], "%.2fV       ");
	}
}
//DACï¿½ï¿½ï¿?
void DAC_Select(void)
{
	//ï¿½Ô¹ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿?
	OS_Rect_Draw(LCD_LEFT_MARGIN, LCD_DA1_Y, LCD_CONTENT_X, LCD_DA2_Y+24, 24, Black);
	if(ADCstate==0)
	{
		OS_String_Show(LCD_LEFT_MARGIN, LCD_DA1_Y, 24, 1, ">");
	}
	else if(ADCstate==1)
	{
		OS_String_Show(LCD_LEFT_MARGIN, LCD_DA2_Y, 24, 1, ">");
	}
}

/* ***************************** IRQHandler Part    	     	*****************************
 * ï¿½Ð¶ï¿½Ö´ï¿½Ðºï¿½ï¿½ï¿½ï¿½ï¿½
 */

/* ***************************** 						END 	   	     	*****************************/
