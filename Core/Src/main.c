/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define BME_ADDR (0x76 << 1)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static uint16_t dig_T1, dig_P1;
static int16_t  dig_T2, dig_T3;
static int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
static uint8_t  dig_H1, dig_H3;
static int16_t  dig_H2, dig_H4, dig_H5;
static int8_t   dig_H6;
static int32_t  t_fine;
uint8_t chip_id = 0;
char msg[96];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static HAL_StatusTypeDef bme_read(uint8_t reg, uint8_t *buf, uint16_t len)
{
  return HAL_I2C_Mem_Read(&hi2c1, BME_ADDR, reg, I2C_MEMADD_SIZE_8BIT, buf, len, 100);
}

static HAL_StatusTypeDef bme_write(uint8_t reg, uint8_t val)
{
  return HAL_I2C_Mem_Write(&hi2c1, BME_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
}

static void bme_read_calibration(void)
{
  uint8_t c[26], h[7];

  bme_read(0x88, c, 26);
  dig_T1 = (c[1] << 8) | c[0];
  dig_T2 = (int16_t)((c[3] << 8) | c[2]);
  dig_T3 = (int16_t)((c[5] << 8) | c[4]);
  dig_P1 = (c[7] << 8) | c[6];
  dig_P2 = (int16_t)((c[9] << 8) | c[8]);
  dig_P3 = (int16_t)((c[11] << 8) | c[10]);
  dig_P4 = (int16_t)((c[13] << 8) | c[12]);
  dig_P5 = (int16_t)((c[15] << 8) | c[14]);
  dig_P6 = (int16_t)((c[17] << 8) | c[16]);
  dig_P7 = (int16_t)((c[19] << 8) | c[18]);
  dig_P8 = (int16_t)((c[21] << 8) | c[20]);
  dig_P9 = (int16_t)((c[23] << 8) | c[22]);
  dig_H1 = c[25];

  bme_read(0xE1, h, 7);
  dig_H2 = (int16_t)((h[1] << 8) | h[0]);
  dig_H3 = h[2];
  dig_H4 = (int16_t)(((int16_t)(int8_t)h[3] * 16) | (h[4] & 0x0F));
  dig_H5 = (int16_t)(((int16_t)(int8_t)h[5] * 16) | (h[4] >> 4));
  dig_H6 = (int8_t)h[6];
}

/* Sonuc: 0.01 derece C */
static int32_t comp_temp(int32_t adc_T)
{
  int32_t v1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * (int32_t)dig_T2) >> 11;
  int32_t v2 = (((((adc_T >> 4) - (int32_t)dig_T1) * ((adc_T >> 4) - (int32_t)dig_T1)) >> 12) * (int32_t)dig_T3) >> 14;
  t_fine = v1 + v2;
  return (t_fine * 5 + 128) >> 8;
}

/* Sonuc: Pa x 256 */
static uint32_t comp_press(int32_t adc_P)
{
  int64_t v1, v2, p;
  v1 = (int64_t)t_fine - 128000;
  v2 = v1 * v1 * (int64_t)dig_P6;
  v2 = v2 + ((v1 * (int64_t)dig_P5) << 17);
  v2 = v2 + (((int64_t)dig_P4) << 35);
  v1 = ((v1 * v1 * (int64_t)dig_P3) >> 8) + ((v1 * (int64_t)dig_P2) << 12);
  v1 = ((((int64_t)1 << 47) + v1) * (int64_t)dig_P1) >> 33;
  if (v1 == 0) return 0;
  p = 1048576 - adc_P;
  p = (((p << 31) - v2) * 3125) / v1;
  v1 = ((int64_t)dig_P9 * (p >> 13) * (p >> 13)) >> 25;
  v2 = ((int64_t)dig_P8 * p) >> 19;
  p = ((p + v1 + v2) >> 8) + ((int64_t)dig_P7 << 4);
  return (uint32_t)p;
}

/* Sonuc: %RH x 1024 */
static uint32_t comp_hum(int32_t adc_H)
{
  int32_t v = t_fine - 76800;
  v = (((((adc_H << 14) - ((int32_t)dig_H4 << 20) - ((int32_t)dig_H5 * v)) + 16384) >> 15) *
       (((((((v * (int32_t)dig_H6) >> 10) * (((v * (int32_t)dig_H3) >> 11) + 32768)) >> 10) + 2097152) *
         (int32_t)dig_H2 + 8192) >> 14));
  v = v - (((((v >> 15) * (v >> 15)) >> 7) * (int32_t)dig_H1) >> 4);
  if (v < 0) v = 0;
  if (v > 419430400) v = 419430400;
  return (uint32_t)(v >> 12);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  if (bme_read(0xD0, &chip_id, 1) != HAL_OK || (chip_id != 0x60 && chip_id != 0x58))
  {
    snprintf(msg, sizeof(msg), "Sensor bulunamadi! (adres/baglanti kontrol et)\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);
    while (1);
  }
  snprintf(msg, sizeof(msg), "Sensor ID: 0x%02X (%s)\r\n", chip_id, chip_id == 0x60 ? "BME280" : "BMP280");
  HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);

  bme_read_calibration();
  if (chip_id == 0x60) bme_write(0xF2, 0x01);  /* nem x1, ctrl_meas'ten once yazilmali */
  bme_write(0xF5, 0xA0);                       /* 1 sn bekleme */
  bme_write(0xF4, 0x27);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  uint8_t d[8];
	      if (bme_read(0xF7, d, 8) == HAL_OK)
	      {
	        int32_t adc_P = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | (d[2] >> 4);
	        int32_t adc_T = ((int32_t)d[3] << 12) | ((int32_t)d[4] << 4) | (d[5] >> 4);
	        int32_t adc_H = ((int32_t)d[6] << 8) | d[7];

	        int32_t  t = comp_temp(adc_T);          /* once sicaklik: t_fine buradan gelir */
	        uint32_t p = comp_press(adc_P) / 256;   /* Pa */
	        uint32_t h = comp_hum(adc_H) / 1024;    /* %RH */

	        if (chip_id == 0x60)
	          snprintf(msg, sizeof(msg), "T: %ld.%02ld C | P: %lu.%02lu hPa | H: %lu %%\r\n",
	                   (long)(t / 100), (long)abs(t % 100),
	                   (unsigned long)(p / 100), (unsigned long)(p % 100), (unsigned long)h);
	        else
	          snprintf(msg, sizeof(msg), "T: %ld.%02ld C | P: %lu.%02lu hPa\r\n",
	                   (long)(t / 100), (long)abs(t % 100),
	                   (unsigned long)(p / 100), (unsigned long)(p % 100));

	        HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);
	      }
	      HAL_Delay(1000);
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

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
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
#ifdef USE_FULL_ASSERT
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
