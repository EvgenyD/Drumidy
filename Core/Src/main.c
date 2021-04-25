/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <string.h>
#include <stdio.h>
#include <usbd_cdc.h>
//#include "midi.h"
#include "drumidy.h"


/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define OFF_DELAY_MS 200
#define TAB 0x09
#define FLASH_USER_START_ADDR 	0x0801F800 		//0x0801 F800
const volatile uint32_t *userConfig=(const volatile uint32_t *)FLASH_USER_START_ADDR;



uint8_t ASCIILOGO[] = "\n"\
"  ___                 _    _\n"\
" |   \\ _ _ _  _ _ __ (_)__| |_  _ \n"\
" | |) | '_| || | '  \\| / _` | || |\n"\
" |___/|_|  \\_,_|_|_|_|_\\__,_|\\_, |\n"\
"     .-.,     ,--. ,--.      |__/ \n"\
"    `/|~\\     \\__/T`--'     . \n"\
"    x |`' __   ,-~^~-.___ ==I== \n"\
"      |  |--| /       \\__}  | \n"\
"      |  |  |{   /~\\   }    | \n"\
"     /|\\ \\__/ \\  \\_/  /|   /|\\ \n"\
"    / | \\|  | /`~-_-~'X.\\ //| \\ \n\n"\
"= Send any char for configuration =\n";



/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
DRUM channel[9];	// array of drums

int cnt;

// ADC buffers0
uint32_t adc_buf[5];
// channel values
uint16_t adc_val[10];

GPIO_PinState aux_current_state[10];

uint32_t saved_config[64];

char buffer_out[1000];			// USB Buffers
uint8_t buffer_in[64];
//char buffer_ret[16];

// MIDI operation
uint8_t upd_active_sens = 0;	//flag for active sense, triggered every 300ms
uint8_t config_Mode[1] = {0};		// flag for activating config over serial
uint32_t custom_timer = 0;

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;
DMA_HandleTypeDef hdma_adc1;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim4;
TIM_HandleTypeDef htim6;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM4_Init(void);
static void MX_TIM2_Init(void);
static void MX_ADC2_Init(void);
static void MX_TIM6_Init(void);
/* USER CODE BEGIN PFP */
uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len);
uint8_t CDC_Receive_FS(uint8_t* Buf, uint16_t Len);

//static void tx_com( uint8_t *_buffer, uint16_t len );

void getAuxState(GPIO_PinState *_state);

void sendMidiAS  ();
//void sendMidiGEN (uint8_t note, uint8_t vel);
void sendMidi    (uint8_t note, uint8_t vel);
void sendMidiGEN (uint8_t note, uint8_t vel);
void sendMidiAT  (uint8_t note, uint8_t vel);
void sendMidiOFF (uint8_t note);
void sendMidi2   (uint8_t note1, uint8_t vel1,uint8_t note2, uint8_t vel2);

void sendMidiHHPedalOn();
void sendMidiHHPedalOff();

void tx_midi  (uint8_t *_buffer, uint16_t len);
void sendDebug (uint8_t _ch, uint8_t _aux);
uint8_t Save_Setting(uint8_t _rst);
uint8_t Load_Setting();
int get_num_from_uart(uint8_t _len);
uint8_t UART_CFG();
//static void tx_usb( uint8_t *_buffer, uint16_t len );

//static void tx_midi( uint8_t _note, uint8_t _velocity, uint8_t _volume);

uint32_t dataReceivedSize = 0;
volatile char flag_New_Settings;
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


void New_USB_Settings(uint8_t* _buf, uint16_t _len){

	for(int i = 0; i < 64; i++)
		if (i < _len)
			buffer_in[i] = _buf[i];
		else
			buffer_in[i] = 0;

	flag_New_Settings = 1;
	dataReceivedSize = _len;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{

	// 10kHz trigger, 0.1ms
	if (htim->Instance==htim4.Instance)
      {
		HAL_ADCEx_MultiModeStart_DMA(&hadc1,  (uint32_t *) adc_buf, 5);//
		cnt++;
      }

	// 3.33Hz active sensing, 300ms
	if (htim->Instance==htim2.Instance)
      {
		upd_active_sens = 1;
      }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	if (huart->Instance == huart2.Instance){
		buffer_in[15] = 1;
//		config_Mode = 1;
//		HAL_UART_Receive_IT (&huart2, &config_Mode, 1);
	}

}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
	if (hadc->Instance==hadc1.Instance)	{
//		uint8_t ch;
		//get all adc values from common array
//		for (ch = 0; ch < 5;ch++) {
//			adc_val[ch] = adc_buf[ch] >> 16;     // store the values in adc_val from buffer
//			adc_val[ch+5 ] = adc_buf[ch] & 0xFFFF;  // store the values in adc_val from buffer
//		}

		// resulting order: 5x[ADC2] + 4x{ADC1} + 1xDummy
		// [PA6] - [PA7] - [PF1] - [PA5] - [PA4] - {PA0} - {PA1} - {PF0} - {PB0} - {XX X}
		//	2		0		4		5		6		9		7		3		1
		adc_val[0] = adc_buf[1] >> 16;
		adc_val[1] = adc_buf[3] & 0xFFFF;
		adc_val[2] = adc_buf[0] >> 16;
		adc_val[3] = adc_buf[2] & 0xFFFF;
		adc_val[4] = adc_buf[2] >> 16;
		adc_val[5] = adc_buf[3] >> 16;
		adc_val[6] = adc_buf[4] >> 16;
		adc_val[7] = adc_buf[1] & 0xFFFF;
		adc_val[8] = adc_buf[0] & 0xFFFF;

		getAuxState(aux_current_state);

		STEP_TIME = HAL_GetTick();

		Update_channel(&channel[0], adc_val[0], aux_current_state[0]);
		Update_channel(&channel[1], adc_val[1], aux_current_state[1]);
		Update_channel(&channel[2], adc_val[2], aux_current_state[2]);
		Update_channel(&channel[3], adc_val[3], aux_current_state[3]);
		Update_channel(&channel[4], adc_val[4], aux_current_state[4]);
		Update_channel(&channel[5], adc_val[5], aux_current_state[5]);
		Update_channel(&channel[6], adc_val[6], aux_current_state[6]);
		Update_channel(&channel[7], adc_val[7], aux_current_state[7]);
		Update_channel(&channel[8], adc_val[8], aux_current_state[8]);

	}// end adc1
}


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
	int vol;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  HAL_Delay(1000);

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  MX_TIM4_Init();
  MX_TIM2_Init();
  MX_ADC2_Init();
  MX_USB_Device_Init();
  MX_TIM6_Init();
  /* USER CODE BEGIN 2 */


  HAL_ADC_Start (&hadc1);
  HAL_ADC_Start (&hadc2);
  HAL_Delay(200);

  /// **************************
  /// ******* Defaul CFG *******
  /// **************************
  getAuxState(aux_current_state);

  initDrum(&channel[0], HHOPEN, HHPEDAL	, CYMBAL_HIHAT	, aux_current_state[0]);
  	  channel[0].alt_voice = HHCLOSE;

  // KICK - MESH+PEDAL - OK
  initDrum(&channel[1], KICK  , KICK	, MESH_PAD_AUTOAUX		, aux_current_state[1]);
  	  channel[1].peak_volume_norm = 20;
  	  channel[1].peak_min_length = 3;

  // KICK - MESH/ALT+PEDAL - OK
  initDrum(&channel[2], SNARE , KICK 	, MESH_RIM_AUTOAUX		, aux_current_state[2]);
	  channel[2].alt_voice = SNRIM;

  initDrum(&channel[3], SNARE , KICK 	, MESH_RIM_AUTOAUX		, aux_current_state[3]);
	  channel[3].alt_voice = 0x40;

  initDrum(&channel[4], TOM1 , TOM1  	, MESH_PAD_AUTOAUX		, aux_current_state[3]);
  initDrum(&channel[5], TOM2 , TOM2  	, MESH_PAD_AUTOAUX		, aux_current_state[4]);
  initDrum(&channel[6], TOMF , TOMF  	, MESH_PAD_AUTOAUX		, aux_current_state[6]);

  // cymbals
  initDrum(&channel[7], CRASH, CRASH 	, CYMBAL_MUTE			, aux_current_state[7]);	// CH7 aux disabled
  initDrum(&channel[8], RIDE ,  BELL 	, CYMBAL_2_ZONE			, aux_current_state[8]);


  // === Previous Settings ===
  HAL_UART_Transmit(&huart2, ASCIILOGO, strlen((char *)ASCIILOGO) , 50);
  HAL_Delay(500);


  Load_Setting();




	// start waiting for serial commands
	HAL_Delay(200);
	config_Mode[0] = 0;
	HAL_UART_Receive_IT (&huart2, &config_Mode[0], 1);

	/// **************************
	/// ******* LETS ROCK! *******
	/// **************************
	HAL_TIM_Base_Start_IT(&htim2); //AS
	HAL_TIM_Base_Start_IT(&htim4); //ADC
//	HAL_TIM_Base_Start_IT(&htim6);


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
	  while (config_Mode[0]){

		uint8_t rs = UART_CFG();

		if ((rs == 1) || (rs == 2)){
			rs = Save_Setting(0);
			sprintf(buffer_out, "New configuration saved (%X)\n", rs);
			HAL_UART_Transmit(&huart2,(uint8_t *)buffer_out, strlen((char const*)buffer_out) , 50);
		}
		if (rs == 99){
			rs = Save_Setting(1);
			sprintf(buffer_out, "Reset to default values, restart the device (%X)\n", rs);
			HAL_UART_Transmit(&huart2,(uint8_t *)buffer_out, strlen((char const*)buffer_out) , 50);
		}
		config_Mode[0] = 0;
		HAL_UART_Receive_IT (&huart2, &config_Mode[0], 1);

	  }

	  if (upd_active_sens){
		  upd_active_sens = 0;
		  sendMidiAS();
	  }

	  for (uint8_t ch = 0; ch<9; ch++){

		  if (channel[ch].main_rdy){
			  channel[ch].main_rdy = 0;

			  // custom volume calculation for mesh
			  if (channel[ch].chnl_type < 2){
				  vol = (int)(100.* (float)(channel[ch].main_rdy_height - PEAK_THRESHOLD) / 4096. * 100. / (float)channel[ch].peak_volume_norm);
				  if ((channel[ch].chnl_type == MESH_RIM_AUTOAUX)&&(channel[ch].main_rdy_usealt))
					  vol = vol*4;
			  } else {
				  //volume for cymbals
				  vol = (int)(100.* (float)(channel[ch].main_rdy_height - PEAK_THRESHOLD) / 4096. * 100. / (float)channel[ch].peak_volume_norm * 2);
			  }

			  if (vol > 127) vol = 127;
			  if (vol < 1  ) vol = 1;
			  channel[ch].main_rdy_volume = (uint8_t) vol;

			  uint8_t vc;
			  if (channel[ch].main_rdy_usealt)
				  vc = channel[ch].alt_voice;	//				  sendMidiGEN(channel[ch].alt_voice ,channel[ch].main_rdy_volume);
			  else
				  vc = channel[ch].main_voice;	//				  sendMidiGEN(channel[ch].main_voice,channel[ch].main_rdy_volume);


			  sendMidi(vc,channel[ch].main_rdy_volume);
			  channel[ch].main_last_on_voice 	= vc;
			  channel[ch].main_last_on_time 	= HAL_GetTick();

  			  sendDebug(ch,0);



		  }


		  if (channel[ch].aux_rdy){
			  channel[ch].aux_rdy = 0;
			  sendDebug(ch,1);

			  switch (channel[ch].chnl_type ){
			  case CYMBAL_HIHAT:
				  if (channel[ch].aux_rdy_state == CHANNEL_PEDAL_PRESSED)
					 sendMidiHHPedalOn();
//				  else
//					  sendMidiGEN(channel[ch].main_voice, 5);
				  break;

			  case CYMBAL_MUTE:
				  if (channel[ch].aux_rdy_state == CHANNEL_PEDAL_PRESSED)
					  sendMidi2(channel[ch].main_voice, 1, channel[ch].main_voice, 0);
				  break;

			  case CYMBAL_2_ZONE:
				  sendMidi2(channel[ch].main_voice, 1, channel[ch].main_voice, 0);
				  break;

			  // INDEPENDENT AUX INPUTS
			  default:
				  if (channel[ch].aux_type == AUX_TYPE_PAD)
					  sendMidi2(channel[ch].aux_voice,100, channel[ch].aux_voice,0);
				  else { //PEDAL
					  // PEDAL pressed
					  if (channel[ch].aux_rdy_state == CHANNEL_PEDAL_PRESSED)
						  sendMidi2(channel[ch].aux_voice,100, channel[ch].aux_voice,0);
					  // PEDAL RELEASED... IN CASE
//					  else
//						  sendMidi2(channel[ch].aux_voice, 1, channel[ch].aux_voice,0);
				  }
			  }


		  }
		  // send off command if needed
		  if (channel[ch].main_last_on_voice > 0){
			  if ( HAL_GetTick() > (channel[ch].main_last_on_time + OFF_DELAY_MS) ){
				  sendMidi(channel[ch].main_last_on_voice,0);
				  channel[ch].main_last_on_voice = 0;
			  }


		  }


	  }
	  /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSI48;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the peripherals clocks
  */
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2|RCC_PERIPHCLK_USB
                              |RCC_PERIPHCLK_ADC12;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
  PeriphClkInit.Adc12ClockSelection = RCC_ADC12CLKSOURCE_SYSCLK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */
  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.GainCompensation = 0;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 5;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  hadc1.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_DUALMODE_REGSIMULT;
  multimode.DMAAccessMode = ADC_DMAACCESSMODE_12_10_BITS;
  multimode.TwoSamplingDelay = ADC_TWOSAMPLINGDELAY_2CYCLES;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_6CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_10;
  sConfig.Rank = ADC_REGULAR_RANK_3;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_15;
  sConfig.Rank = ADC_REGULAR_RANK_4;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_TEMPSENSOR_ADC1;
  sConfig.Rank = ADC_REGULAR_RANK_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief ADC2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC2_Init(void)
{

  /* USER CODE BEGIN ADC2_Init 0 */

  /* USER CODE END ADC2_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC2_Init 1 */

  /* USER CODE END ADC2_Init 1 */
  /** Common config
  */
  hadc2.Instance = ADC2;
  hadc2.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc2.Init.Resolution = ADC_RESOLUTION_12B;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.GainCompensation = 0;
  hadc2.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc2.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  hadc2.Init.LowPowerAutoWait = DISABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.NbrOfConversion = 5;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.DMAContinuousRequests = DISABLE;
  hadc2.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  hadc2.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_6CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_10;
  sConfig.Rank = ADC_REGULAR_RANK_3;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_13;
  sConfig.Rank = ADC_REGULAR_RANK_4;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_17;
  sConfig.Rank = ADC_REGULAR_RANK_5;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC2_Init 2 */

  /* USER CODE END ADC2_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 5.1E7;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 0;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 17000;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 170;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 65535;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMAMUX1_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : DIG_IN7_Pin DIG_IN1_Pin DIG_IN3_Pin DIG_IN5_Pin */
  GPIO_InitStruct.Pin = DIG_IN7_Pin|DIG_IN1_Pin|DIG_IN3_Pin|DIG_IN5_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : DIG_IN9_Pin DIG_IN6_Pin DIG_IN4_Pin DIG_IN2_Pin */
  GPIO_InitStruct.Pin = DIG_IN9_Pin|DIG_IN6_Pin|DIG_IN4_Pin|DIG_IN2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */
/*
 * @brief  Write generic device register (platform dependent)
 *
 * @param  tx_buffer     buffer to trasmit
 * @param  len           number of byte to send
 *
 */

void sendMidiAS(){
  uint8_t bff[4] = {0x0F, 0xFE, 0x00, 0x00};
  tx_midi((uint8_t *)bff,4);
}

// MIDI generic ON/OFF message
void sendMidiGEN(uint8_t note, uint8_t vel){
  uint8_t bff[8] = {TAB,  0x99, 0, 0, TAB,  0x99,0, 0x00};
  bff[2] = 0x7f & note;
  bff[3] = 0x7f & vel;
  bff[6] = 0x7f & note;
  tx_midi((uint8_t *)bff,8);
}

// MIDI generic ON message
void sendMidi(uint8_t note, uint8_t vel){
  uint8_t bff[4] = {TAB,  0x99, 0x00, 0x00};
  bff[2] = 0x7f & note;
  bff[3] = 0x7f & vel;
  tx_midi((uint8_t *)bff,4);
}
// MIDI generic OFF message
void sendMidiOFF(uint8_t note){
  uint8_t bff[4] = {TAB,  0x89, 0x00, 0x00};
  bff[2] = 0x7f & note;
  bff[3] = 0x7f;
  tx_midi((uint8_t *)bff,4);
}


// aftertouch
void sendMidiAT(uint8_t note, uint8_t vel){
  uint8_t bff[4] = {TAB,  0xA9, 0x00, 0x00};
  bff[2] = 0x7f & note;
  bff[3] = 0x7f & vel;
  tx_midi((uint8_t *)bff,4);
}


// MIDI generic ON message
void sendMidi2(uint8_t note1, uint8_t vel1,uint8_t note2, uint8_t vel2){
  uint8_t bff[8] = {TAB,  0x99, 0x00, 0x00, TAB,  0x99, 0x00, 0x00};
  bff[2] = 0x7f & note1;
  bff[3] = 0x7f & vel1;
  bff[2+4] = 0x7f & note2;
  bff[3+4] = 0x7f & vel2;
  tx_midi((uint8_t *)bff,8);
}

// MIDI HiHat pedal press message
void sendMidiHHPedalOn(){
  uint8_t bff[20] = { TAB,  0xA9, HHOPEN , 0x7F, TAB,  0xA9, HHCLOSE, 0x7F, TAB, 0xA9, 0x15, 0x7F,
		  	  	  	  TAB,  0x99, HHPEDAL, 0x64, TAB,  0x99, HHPEDAL, 0x00};
  tx_midi((uint8_t *)bff, 20);
}

// pedal aftertouch for hihat
void sendMidiHHPedalOff(){
  uint8_t bff[4] = { TAB,  0xA9, HHPEDAL , 0x3F};
  tx_midi((uint8_t *)bff,4);
}

//
//static void tx_com(uint8_t *_buffer, uint16_t len)
//{
//  HAL_UART_Transmit(&huart2, _buffer, len, 10);
//
////  HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
//}


void tx_midi(uint8_t *_buffer, uint16_t len)
{
  uint8_t rt = USBD_BUSY;

  while( rt == USBD_BUSY) {
	  rt = CDC_Transmit_FS(_buffer, len);
  };

  TIM2->CNT = 0; // restart active sense timer

}

void sendDebug(uint8_t _ch, uint8_t _aux)
{
	uint8_t voice;
	uint8_t volume;
	uint8_t length;

  if (_aux){
	  voice = channel[_ch].aux_voice;

	  sprintf(buffer_out, ">>>AUX %d: %X %d [%d %d]\n", _ch,
	  			  voice, channel[_ch].aux_rdy_state,
	  			  channel[_ch].main_peaking, channel[_ch].aux_status);
  }else{
	  if (channel[_ch].main_rdy_usealt) voice = channel[_ch].alt_voice; else voice = channel[_ch].main_voice;
	  volume = channel[_ch].main_rdy_volume;
	  length = channel[_ch].main_rdy_length;
	  sprintf(buffer_out, ">>MAIN %d: voice %X (alt:%d), vol %d (%u/4096) t=%u; AUX = %d\n",_ch,
			  voice, channel[_ch].alt_voice, volume, channel[_ch].main_rdy_height,
			  length, channel[_ch].aux_status);
  }
  HAL_UART_Transmit(&huart2,(uint8_t *)buffer_out, strlen((char const*)buffer_out) , 5);

  HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);

}


// READ Diginal state of aux channels
void getAuxState (GPIO_PinState *_state){
	_state[0] = HAL_GPIO_ReadPin(DIG_IN1_GPIO_Port, DIG_IN1_Pin);
	_state[1] = HAL_GPIO_ReadPin(DIG_IN2_GPIO_Port, DIG_IN2_Pin);
	_state[2] = HAL_GPIO_ReadPin(DIG_IN3_GPIO_Port, DIG_IN3_Pin);
	_state[3] = HAL_GPIO_ReadPin(DIG_IN4_GPIO_Port, DIG_IN4_Pin);

	_state[4] = HAL_GPIO_ReadPin(DIG_IN5_GPIO_Port, DIG_IN5_Pin);
	_state[5] = HAL_GPIO_ReadPin(DIG_IN6_GPIO_Port, DIG_IN6_Pin);
	_state[6] = HAL_GPIO_ReadPin(DIG_IN7_GPIO_Port, DIG_IN7_Pin);
	_state[7] = 0; //HAL_GPIO_ReadPin(DIG_IN8_GPIO_Port, DIG_IN8_Pin);

	_state[8] = HAL_GPIO_ReadPin(DIG_IN9_GPIO_Port, DIG_IN9_Pin);
	_state[9] = 0;
}

uint8_t Save_Setting(uint8_t _rst)
{
	uint32_t SavingBuff[64];
	uint8_t i;
	uint32_t error = 0;
	uint64_t val = 0;

	FLASH_EraseInitTypeDef FLASH_EraseInitStruct = {
	        .TypeErase = FLASH_TYPEERASE_PAGES,
			.Banks = FLASH_BANK_1,
	        .Page = 63,
			.NbPages = 1
	};

	for (i=0;i<64;i++)
			SavingBuff[i] = 0;
//112233445566778899 AABBCCDDEEFF
	if (_rst == 0)
		SavingBuff[0] = 0xC4C0FFEE; // load settings marker
	else
		SavingBuff[0] = 0xFFFFFFFF; // do not load marker
	SavingBuff[1] = 0xBB;

	// 0x11223344
	for (i = 1; i < 10; i++){
		// channel configuration settings
		SavingBuff[2*i    ]  = (channel[i-1].main_voice & 0xFF)*0x01000000;
		SavingBuff[2*i    ] += (channel[i-1].aux_voice  & 0xFF)*0x00010000;
		SavingBuff[2*i    ] += (channel[i-1].alt_voice  & 0xFF)*0x00000100;
		SavingBuff[2*i    ] += (channel[i-1].chnl_type   & 0xFF);
		// channel parameter settings
		SavingBuff[2*i + 1]  = (channel[i-1].peak_volume_norm 	& 0xFF)*0x01000000;
		SavingBuff[2*i + 1] += (channel[i-1].peak_min_length 	& 0xFF)*0x00010000;
		SavingBuff[2*i + 1] += (channel[i-1].peak_max_length  	& 0xFF)*0x00000100;
//		SavingBuff[2*i + 1] += (channel[i-1].peak2peak  & 0xFF);
	}

	HAL_StatusTypeDef err;
	uint8_t st = 0;
	err = HAL_FLASH_Unlock();
	if (err != HAL_OK)
		st += 0b10000000;
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR  | FLASH_FLAG_PGSERR);

	err = HAL_FLASHEx_Erase(&FLASH_EraseInitStruct, &error);
	if (err != HAL_OK)
		st += 0b01000000;

	for (i=0;i<32;i++)
	{
		val = (((uint64_t)SavingBuff[i*2+1])<<32) + SavingBuff[i*2];
		if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, FLASH_USER_START_ADDR + 8*i, val) != HAL_OK) st += 1;
	}
	if ( HAL_FLASH_Lock() != HAL_OK ) st += 0b00100000;

	for (i=0;i<64;i++)	saved_config[0] = 0;


	return st;
}


uint8_t Load_Setting()
{
	uint8_t i;
//	uint32_t LoadingBuff[64];

	for (i=0;i<64;i++){
		saved_config[i] = *(userConfig+i);
	}

	if (saved_config[0] != 0xC4C0FFEE) return 0;

	for (i = 1; i < 10; i++){
		channel[i-1].main_voice = 0xff & (uint8_t)(saved_config[2*i]>>24);
		channel[i-1].aux_voice 	= 0xff & (uint8_t)(saved_config[2*i]>>16);
		channel[i-1].alt_voice 	= 0xff & (uint8_t)(saved_config[2*i]>>8);
		channel[i-1].chnl_type 	= 0xff & (uint8_t)(saved_config[2*i]);

		//		channel[i-1].peak_threshold 	= 0xff & (uint8_t)(saved_config[2*i+1]>>24);
		channel[i-1].peak_volume_norm 	= 0xff & (uint8_t)(saved_config[2*i+1]>>24);
		channel[i-1].peak_min_length 	= 0xff & (uint8_t)(saved_config[2*i+1]>>16);
		channel[i-1].peak_max_length 	= 0xff & (uint8_t)(saved_config[2*i+1]>>8);
//		channel[i-1].time_between_peaks = 0xff & (uint8_t)(saved_config[2*i+1]);
	}

	sprintf(buffer_out, "........ Previous settings: .......\n%08lX %08lX %08lX %08lX\n%08lX %08lX %08lX %08lX\n%08lX %08lX %08lX %08lX\n%08lX %08lX %08lX %08lX\n%08lX %08lX %08lX %08lX\n",
	  saved_config[0] ,saved_config[1] ,saved_config[2] ,saved_config[3] ,
	  saved_config[4] ,saved_config[5] ,saved_config[6] ,saved_config[7] ,
	  saved_config[8] ,saved_config[9] ,saved_config[10],saved_config[11],
	  saved_config[12],saved_config[13],saved_config[14],saved_config[15],
	  saved_config[16],saved_config[17],saved_config[18],saved_config[19]);

	  HAL_UART_Transmit(&huart2,(uint8_t *)buffer_out, strlen((char const*)buffer_out) , 50);
	  HAL_Delay(500);

	return 1;
}

//receive number from serial or a given max length
int get_num_from_uart(uint8_t _len){
	uint8_t i;
	int val = 0;
	for (i = 0; i<_len+1; i++)
		buffer_in[i] = 0;


	HAL_UART_Receive_IT (&huart2, &buffer_in[0], _len);
	while (buffer_in[0] == 0) {HAL_Delay(1);};
	HAL_Delay(2); // wait for the rest of the message

	val = 0;
	for (i = 0; i<_len; i++){
		if ((buffer_in[i] == 0) || (buffer_in[i] == 10) || (buffer_in[i] == 13)) break;
		if ((buffer_in[0]>='0') && (buffer_in[0]<='9'))
			val = val*10 + (buffer_in[i]-'0');
		else{
			val = -1;
			break;
		}
	}
	HAL_UART_AbortReceive(&huart2);
	return val;
}

uint8_t UART_CFG(){

	int val = 0;

	uint8_t rtrn = 0;

	sprintf(buffer_out, "\nConfig mode.\nType number of the pad [1..9], or hit the drum (x - reset to default):\n");
	HAL_UART_Transmit(&huart2,(uint8_t *)buffer_out, strlen((char const*)buffer_out) , 50);

	buffer_in[0] = 0;
	HAL_UART_Receive_IT (&huart2, &buffer_in[0], 1);

	uint8_t chnl = 10;
	while (chnl == 10){
		  for (uint8_t ch = 0; ch<9; ch++)
			  if ((channel[ch].main_rdy)||(channel[ch].aux_rdy)){
				  channel[ch].main_rdy = 0;
				  channel[ch].aux_rdy = 0;
				  chnl = ch;
				  HAL_UART_AbortReceive(&huart2);
			  }
		  if (buffer_in[0]>0){
			  if ((buffer_in[0]>='1') && (buffer_in[0]<='9'))
				  chnl = buffer_in[0]-'1';
			  else
				  chnl = 255;

			  if (buffer_in[0]=='x')
				  return 99;
		  }
	}

	if (chnl == 255) {
		HAL_UART_AbortReceive(&huart2);
		sprintf(buffer_out, "Ciao\n");
		HAL_UART_Transmit(&huart2,(uint8_t *)buffer_out, strlen((char const*)buffer_out) , 50);
		config_Mode[0] = 0;
		HAL_UART_Receive_IT (&huart2, &config_Mode[0], 1);
		return 0;
	}

	// got the correct channel.
	// print current values
	sprintf(buffer_out, "Current values CH#%d:\n\tVoices: main %d, aux %d, alt %d\n\tTimings: peak min %d max %d\n\tChannel type: %d,volume norm %d\n",
			chnl+1, channel[chnl].main_voice, channel[chnl].aux_voice, channel[chnl].alt_voice,
			(int)channel[chnl].peak_min_length, (int)channel[chnl].peak_max_length,
			channel[chnl].aux_type, (int)channel[chnl].peak_volume_norm);
	HAL_UART_Transmit(&huart2,(uint8_t *)buffer_out, strlen((char const*)buffer_out) , 50);
	HAL_Delay(200);

	// Starting to change the values
	// main voicepeak_volume_norm
	sprintf(buffer_out, "\nCH#%d Change main voice from %d:\t",chnl+1, channel[chnl].main_voice);
	HAL_UART_Transmit(&huart2,(uint8_t *)buffer_out, strlen((char const*)buffer_out) , 50);
	HAL_Delay(200);

	 val = get_num_from_uart(2);
	if ((val>25)&&(val<90)){
		channel[chnl].main_voice = val & 0xFF;
		sprintf(buffer_out, "New main voice: %d\n", channel[chnl].main_voice);
	}else
		sprintf(buffer_out, "Keeping the old value: %d\n", channel[chnl].main_voice);
	HAL_UART_Transmit(&huart2,(uint8_t *)buffer_out, strlen((char const*)buffer_out) , 50);
	HAL_Delay(200);

	// aux voice
	sprintf(buffer_out, "\nCH#%d Change aux input voice from %d:\t",chnl+1, channel[chnl].aux_voice);
	HAL_UART_Transmit(&huart2,(uint8_t *)buffer_out, strlen((char const*)buffer_out) , 50);
	HAL_Delay(200);

	 val = get_num_from_uart(2);
	if ((val>25)&&(val<90)){
		channel[chnl].aux_voice = val & 0xFF;
		sprintf(buffer_out, "New aux voice: %d\n", channel[chnl].aux_voice);
	}else
		sprintf(buffer_out, "Keeping the old value: %d\n", channel[chnl].aux_voice);
	HAL_UART_Transmit(&huart2,(uint8_t *)buffer_out, strlen((char const*)buffer_out) , 50);
	HAL_Delay(200);

	// main alt voice
	sprintf(buffer_out, "\nCH#%d Change main alt voice (when pedal pressed) from %d:\t",chnl+1, channel[chnl].alt_voice);
	HAL_UART_Transmit(&huart2,(uint8_t *)buffer_out, strlen((char const*)buffer_out) , 50);
	HAL_Delay(200);

	 val = get_num_from_uart(2);
	if ((val>25)&&(val<90)){
		channel[chnl].alt_voice = val & 0xFF;
		sprintf(buffer_out, "New alt voice: %d\n", channel[chnl].alt_voice);
	}else
		sprintf(buffer_out, "Keeping the old value: %d\n", channel[chnl].alt_voice);
	HAL_UART_Transmit(&huart2,(uint8_t *)buffer_out, strlen((char const*)buffer_out) , 50);
	HAL_Delay(200);

	// channel type
	sprintf(buffer_out, "\nCH#%d Change aux type from %d to:\n\tAUX - auto, MAIN - Mesh(0), Mesh with rim(1), or Cymbal(2),\n\t HiHat(3) with pedal, Cymbal with 2 zones(4), Cymabal with mute button(5)\n", chnl+1,  channel[chnl].chnl_type);
	HAL_UART_Transmit(&huart2,(uint8_t *)buffer_out, strlen((char const*)buffer_out) , 50);
	HAL_Delay(200);

	val = get_num_from_uart(1);
	if ((val>=0)&&(val<=4)){
		channel[chnl].chnl_type = val & 0xFF;
		sprintf(buffer_out, "New channel type: %d\n", channel[chnl].chnl_type);
	}else
		sprintf(buffer_out, "Keeping the old value: %d\n", channel[chnl].chnl_type);
	HAL_UART_Transmit(&huart2,(uint8_t *)buffer_out, strlen((char const*)buffer_out) , 50);
	HAL_Delay(200);

	rtrn = 1;
	sprintf(buffer_out, "\nAdjust timing? y - yes, n - save settings and exit\n");
	HAL_UART_Transmit(&huart2,(uint8_t *)buffer_out, strlen((char const*)buffer_out) , 50);
	HAL_Delay(200);


	buffer_in[0] = 0;
	HAL_UART_Receive_IT (&huart2, &buffer_in[0], 1);
	while (buffer_in[0] == 0){HAL_Delay(1);}
	if (buffer_in[0] == 'y'){

		// Peak threshold
		sprintf(buffer_out, "\nCH#%d Volume norm = %d (default 50, 0..255) (full volume point, 100~4096). New:\t",chnl+1,(int) channel[chnl].peak_volume_norm);
		HAL_UART_Transmit(&huart2,(uint8_t *)buffer_out, strlen((char const*)buffer_out) , 50);
		HAL_Delay(200);

		val = get_num_from_uart(3);
		if ((val>0)&&(val<256)){
			channel[chnl].peak_volume_norm = val;
			sprintf(buffer_out, "New threshold = %d\n", (int)channel[chnl].peak_volume_norm);
		}else
			sprintf(buffer_out, "Keeping the old value: %d\n", (int)channel[chnl].peak_volume_norm);
		HAL_UART_Transmit(&huart2,(uint8_t *)buffer_out, strlen((char const*)buffer_out) , 50);
		HAL_Delay(200);

		// min peak len
		sprintf(buffer_out, "\nCH#%d Peak min length = %d (default mesh 15, cymbal 4, 1..99) [x0.1ms]. New:\t",chnl+1,(int) channel[chnl].peak_min_length);
		HAL_UART_Transmit(&huart2,(uint8_t *)buffer_out, strlen((char const*)buffer_out) , 50);
		HAL_Delay(200);

		val = get_num_from_uart(2);
		if ((val>0)&&(val<100)){
			channel[chnl].peak_min_length = val;
			sprintf(buffer_out, "New min length = %d\n", (int)channel[chnl].peak_min_length);
		}else
			sprintf(buffer_out, "Keeping the old value: %d\n", (int)channel[chnl].peak_min_length);
		HAL_UART_Transmit(&huart2,(uint8_t *)buffer_out, strlen((char const*)buffer_out) , 50);
		HAL_Delay(200);

		// max peak len
		sprintf(buffer_out, "\nCH#%d Peak max length = %d (default 200, 1..255) [x0.1ms]. New:\t",chnl+1, (int)channel[chnl].peak_max_length);
		HAL_UART_Transmit(&huart2,(uint8_t *)buffer_out, strlen((char const*)buffer_out) , 50);
		HAL_Delay(200);

		val = get_num_from_uart(3);
		if ((val>0)&&(val<256)){
			channel[chnl].peak_max_length = val;
			sprintf(buffer_out, "New max length = %d\n", (int)channel[chnl].peak_max_length);
		}else
			sprintf(buffer_out, "Keeping the old value: %d\n", (int)channel[chnl].peak_max_length);
		HAL_UART_Transmit(&huart2,(uint8_t *)buffer_out, strlen((char const*)buffer_out) , 50);
		HAL_Delay(200);
		rtrn = 2;
	}
	return rtrn;
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
