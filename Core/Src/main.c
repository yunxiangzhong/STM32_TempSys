/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include<stdio.h>
#include<string.h>
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* 角色定义 */
typedef enum {
    ROLE_SLAVE = 0, // 默认是从机（安全起见，防止两个主机上电冲突）
    ROLE_MASTER = 1
} SystemRole_TypeDef;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define RS485_TX_EN() HAL_GPIO_WritePin(GPIOE, GPIO_PIN_6, GPIO_PIN_SET)
#define RS485_RX_EN() HAL_GPIO_WritePin(GPIOE, GPIO_PIN_6, GPIO_PIN_RESET)
#define AT24C02_ADDR 0xA0
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

RTC_HandleTypeDef hrtc;

TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/* 全局变量 */
SystemRole_TypeDef current_role = ROLE_SLAVE; // 当前角色
uint8_t rx_buffer[128]; // 接收缓冲�???????
uint8_t rx_byte;        // 单字节接收缓�???????
uint8_t rx_flag = 0;    // 接收完成标志
uint8_t rx_idx = 0;     // 接收索引

uint32_t last_poll_time = 0; // 主机轮询计时�???????
uint32_t last_key_time = 0;

// 数码管显示缓冲：[0]=模式�???, [1]=十位, [2]=个位
uint8_t disp_buf[3] = {0, 17, 17};

// 上电默认值设�?? 255 (表示还没读或者无�??)
uint8_t saved_temp = 255;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_RTC_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */

uint16_t ModBus_CRC16(uint8_t *buffer, uint8_t len);
uint16_t Get_ADC_Value(void);
void Display_Scan_Loop(void);

void AT24C02_WriteOneByte(uint8_t addr, uint8_t data);
uint8_t AT24C02_ReadOneByte(uint8_t addr);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void RS485_Send(uint8_t *data, uint16_t len) {
    RS485_TX_EN(); // 切为发�??
    HAL_UART_Transmit(&huart2, data, len, 100);
    RS485_RX_EN(); // 切回接收
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
  MX_ADC1_Init();
  MX_RTC_Init();
  MX_USART2_UART_Init();
  MX_USART1_UART_Init();
  MX_I2C1_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  HAL_GPIO_WritePin(LED_SEL_GPIO_Port, LED_SEL_Pin, GPIO_PIN_RESET);
  // 启动接收中断
  RS485_RX_EN();
  HAL_UART_Receive_IT(&huart2, &rx_byte, 1);

  //上电立刻读取 EEPROM 中的历史温度
    saved_temp = AT24C02_ReadOneByte(0x00);
    if (saved_temp > 99) {
          saved_temp = 0; // 或�?? 255 代表无效
      }
  // 初始化显示缓冲：默认 Slave(0), 温度不显�???(0xFF)
    disp_buf[0] = 0;
    disp_buf[1] = 17;
    disp_buf[2] = 17;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  Display_Scan_Loop();
	   /* ================= 模式切换 (KEY3) ================= */
	   if (HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin) == GPIO_PIN_RESET)
	   {
	       HAL_Delay(9);
	       if (HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin) == GPIO_PIN_RESET)
	       {
	            while(HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin) == GPIO_PIN_RESET) {
	            }

	            current_role = (current_role == ROLE_MASTER) ? ROLE_SLAVE : ROLE_MASTER;
	            rx_idx = 0;
	            disp_buf[0] = (current_role == ROLE_MASTER) ? 1 : 0;

	            // 切换到主机时，立即从 EEPROM 读取历史温度显示
	            if (current_role == ROLE_MASTER)
			   {
				   // 只要�??切换变成主机，马上把内存里存�?? saved_temp 显示出来
				   if (saved_temp <= 99) {
					   if(saved_temp < 10) {
						   disp_buf[1] = 17;
						   disp_buf[2] = saved_temp;
					   } else {
						   disp_buf[1] = saved_temp / 10;
						   disp_buf[2] = saved_temp % 10;
					   }
				   }
			   }
			   else // 变成从机
			   {
				   disp_buf[1] = 17;
				   disp_buf[2] = 17;
			   }
	       }
	   }

	   /* ================= 3. 主机逻辑 (MASTER) ================= */
	   if (current_role == ROLE_MASTER)
	   {
	        // A. 周期发�?? Modbus 查询 (1秒一�??)
	        if (HAL_GetTick() - last_poll_time >= 1000)
	        {
	            uint8_t tx_buf[8] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
	            uint16_t crc = ModBus_CRC16(tx_buf, 6);
	            tx_buf[6] = crc & 0xFF;
	            tx_buf[7] = (crc >> 8) & 0xFF;

	            RS485_Send(tx_buf, 8);
	            last_poll_time = HAL_GetTick();
	        }

	        // B. 接收并解�??
	        if (rx_idx >= 7)
	        {
	            uint16_t cal_crc = ModBus_CRC16(rx_buffer, rx_idx - 2);
	            uint16_t rec_crc = rx_buffer[rx_idx-2] | (rx_buffer[rx_idx-1] << 8);

	            if (rx_buffer[0] == 0x01 && rx_buffer[1] == 0x03 && cal_crc == rec_crc)
	            {
	                uint16_t adc_raw = (rx_buffer[3] << 8) | rx_buffer[4];
	                if (adc_raw == 0) adc_raw = 1;

	                // NTC 温度计算
	                float R_up = 10000.0f;
	                float R_ntc = R_up * (float)adc_raw / (4095.0f - (float)adc_raw);
	                float B = 3950.0f;
	                float T0 = 298.15f;
	                float temp_k = 1.0f / ((1.0f / T0) + (1.0f / B) * logf(R_ntc / 10000.0f));
	                float temp_c = temp_k - 273.15f;

	                int temp_show = (int)(temp_c + 0.5f);
	                if(temp_show < 0) temp_show = 0;
	                if(temp_show > 99) temp_show = 99;

	                // 串口打印
	                char msg_buf[32];
	                sprintf(msg_buf, "T=%d C\r\n", temp_show);
	                HAL_UART_Transmit(&huart1, (uint8_t*)msg_buf, strlen(msg_buf), 10);

	                //EEPROM 存储逻辑
	                // 只有当温度发生变化，且数值有效时才写入，保护芯片寿命，防止频繁写入�?�成的数码管闪烁
	                if (temp_show != saved_temp)
	                {
	                    AT24C02_WriteOneByte(0x00, (uint8_t)temp_show); // 写入地址 0x00
	                    saved_temp = (uint8_t)temp_show;
	                    //串口提示存储成功
	                    HAL_UART_Transmit(&huart1, (uint8_t*)"Saved to ROM\r\n", 14, 10);
	                }

	                // 更新数码�??
	                if (temp_show < 10) {
	                       disp_buf[1] = 17; // 十位熄灭
	                   } else {
	                       disp_buf[1] = temp_show / 10;
	                   }
	                disp_buf[2] = temp_show % 10;
	            }
	            rx_idx = 0;
	        }
	   }
	   /* ================= 4. 从机逻辑 (SLAVE) ================= */
	   else
	   {
	        disp_buf[1] = 17;
	        disp_buf[2] = 17;
	        if (rx_idx >= 8)
	        {
	            uint16_t cal_crc = ModBus_CRC16(rx_buffer, rx_idx - 2);
	            uint16_t rec_crc = rx_buffer[rx_idx-2] | (rx_buffer[rx_idx-1] << 8);

	            if (cal_crc == rec_crc && rx_buffer[0] == 0x01 && rx_buffer[1] == 0x03)
	            {
	                uint16_t my_adc = Get_ADC_Value();
	                uint8_t tx_buf[7] = {0x01, 0x03, 0x02, (my_adc >> 8), (my_adc & 0xFF), 0, 0};
	                uint16_t crc = ModBus_CRC16(tx_buf, 5);
	                tx_buf[5] = crc & 0xFF;
	                tx_buf[6] = (crc >> 8) & 0xFF;
	                HAL_Delay(2);
	                RS485_Send(tx_buf, 7);
	            }
	            rx_idx = 0;
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC|RCC_PERIPHCLK_ADC;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
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

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_15;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

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
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.AsynchPrediv = RTC_AUTO_1_SECOND;
  hrtc.Init.OutPut = RTC_OUTPUTSOURCE_ALARM;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 72-1;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 2000-1;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

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
  huart2.Init.BaudRate = 9600;
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
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, RS485_DE_Pin|A_Pin|B_Pin|C_Pin
                          |D_Pin|E_Pin|F_Pin|G_Pin
                          |H_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, SEL0_Pin|SEL1_Pin|SEL2_Pin|LED_SEL_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : RS485_DE_Pin A_Pin B_Pin C_Pin
                           D_Pin E_Pin F_Pin G_Pin
                           H_Pin */
  GPIO_InitStruct.Pin = RS485_DE_Pin|A_Pin|B_Pin|C_Pin
                          |D_Pin|E_Pin|F_Pin|G_Pin
                          |H_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : KEY3_Pin KEY2_Pin KEY1_Pin */
  GPIO_InitStruct.Pin = KEY3_Pin|KEY2_Pin|KEY1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : SEL0_Pin SEL1_Pin SEL2_Pin LED_SEL_Pin */
  GPIO_InitStruct.Pin = SEL0_Pin|SEL1_Pin|SEL2_Pin|LED_SEL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
// 串口接收中断回调
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        rx_buffer[rx_idx++] = rx_byte;
        // �??????单防溢出
        if (rx_idx >= 128) rx_idx = 0;
        // 继续接收
        HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
    }
}
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3)
    {
        Display_Scan_Loop(); // 在中断里扫描数码�?
    }
}
// ADC 读取函数 (读取 IN15)
uint16_t Get_ADC_Value(void)
{
    HAL_ADC_Start(&hadc1);                     // 启动 ADC
    HAL_ADC_PollForConversion(&hadc1, 10);     // 等待转换完成
    uint16_t val = HAL_ADC_GetValue(&hadc1);   // 获取数�??
    return val;
}

// ModBus CRC16 计算函数
uint16_t ModBus_CRC16(uint8_t *buffer, uint8_t len)
{
    uint16_t crc = 0xFFFF;
    for (int pos = 0; pos < len; pos++)
    {
        crc ^= (uint16_t)buffer[pos];
        for (int i = 8; i != 0; i--)
        {
            if ((crc & 0x0001) != 0)
            {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    // ModBus 协议�?????? CRC 是低字节在前，高字节在后，但计算结果通常不用交换�??????
    // 发�?�时拆分即可�??????
    return crc;
}
// 共阴/共阳极段码表 (0-9, A, B, C, D, E, F, -)
const uint8_t SEG_CODE[] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
    0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71, 0x40
};



// 写段码 (控制 A-H 引脚)
void Write_Segment(uint8_t code)
{
    // 使用命名的信�??? A-H
    HAL_GPIO_WritePin(A_GPIO_Port, A_Pin, (code & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(B_GPIO_Port, B_Pin, (code & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(C_GPIO_Port, C_Pin, (code & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(D_GPIO_Port, D_Pin, (code & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(E_GPIO_Port, E_Pin, (code & 0x10) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(F_GPIO_Port, F_Pin, (code & 0x20) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(G_GPIO_Port, G_Pin, (code & 0x40) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(H_GPIO_Port, H_Pin, (code & 0x80) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}


// 硬件位�?�输出函�???
void Select_Hardware_Digit(uint8_t val)
{
    // 这里�??? 0x01, 0x02, 0x04 对应 SEL0(A), SEL1(B), SEL2(C)
    HAL_GPIO_WritePin(SEL0_GPIO_Port, SEL0_Pin, (val & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEL1_GPIO_Port, SEL1_Pin, (val & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEL2_GPIO_Port, SEL2_Pin, (val & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

// 扫描函数
void Display_Scan_Loop(void)
{
    static uint8_t step = 0;
    // 强制消隐 (关断段�??) —�?? 解决重叠和显�???8的核心！
    Write_Segment(0x00);

    // 加一点微小的延时，给硬件反应时间 (消影死区)
    for(int i=0; i<50; i++) { __NOP(); }

    //切换位�?? (74HC138)
    switch(step)
    {
        case 0:
            Select_Hardware_Digit(7);
            // 写入数据
            Write_Segment(SEG_CODE[disp_buf[0]]);
            break;

        case 1:
            Select_Hardware_Digit(0);
            // 写入数据
            Write_Segment(SEG_CODE[disp_buf[1]]);
            break;

        case 2:
            Select_Hardware_Digit(1);
            // 写入数据
            Write_Segment(SEG_CODE[disp_buf[2]]);
            break;
    }
    // 第三步：发光延时
    HAL_Delay(2);

    // 切换下一�???
    step++;
    if(step > 2) step = 0;
}

// 写入�??个字节到 EEPROM
void AT24C02_WriteOneByte(uint8_t addr, uint8_t data)
{
    // 等待芯片就绪（重�? 3 次），防止上�?次写入还没结�?
    if(HAL_I2C_IsDeviceReady(&hi2c1, AT24C02_ADDR, 3, 100) == HAL_OK)
    {
        HAL_I2C_Mem_Write(&hi2c1, AT24C02_ADDR, addr, I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
        // 【关键�?�必须延�? 5ms，这�? AT24C02 的物理特性！
        HAL_Delay(5);
    }
}

uint8_t AT24C02_ReadOneByte(uint8_t addr)
{
    uint8_t data = 0;
    // 读取前也�?测一下，防止紧接�?写入后的读取失败o
    if(HAL_I2C_IsDeviceReady(&hi2c1, AT24C02_ADDR, 3, 100) == HAL_OK)
    {
        if(HAL_I2C_Mem_Read(&hi2c1, AT24C02_ADDR, addr, I2C_MEMADD_SIZE_8BIT, &data, 1, 100) != HAL_OK)
        {
            return 0xFF; // 如果读取失败，返�? 0xFF 表示错误
        }
    }
    return data;
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
