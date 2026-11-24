/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : AC Current and Temperature Monitoring with NanoEdge AI
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "NanoEdgeAI.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* DHT22 connected to PD0 */
#define DHT22_PORT              GPIOD
#define DHT22_PIN               GPIO_PIN_0

/* ACS712-5A parameters */
#define ACS712_VREF             3.3f
#define ACS712_ADC_MAX          4095.0f
#define ACS712_SENSITIVITY      0.185f

/* AC RMS measurement */
#define RMS_SAMPLE_COUNT        200U
#define RMS_SAMPLE_DELAY_MS     1U

/* NanoEdge AI anomaly threshold */
#define ANOMALY_THRESHOLD       80U

/* DHT22 minimum reading interval */
#define DHT22_INTERVAL_MS       2000U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* ADC and current measurement variables */
uint32_t adc_value = 0;
float voltage = 0.0f;
float current = 0.0f;

/* DHT22 measurement variables */
float temperature = 0.0f;
float humidity = 0.0f;

/* NanoEdge AI variables */
enum neai_state state;
bool use_pretrained = true;
uint8_t similarity = 0;
uint8_t anomaly = 0;

/* Two input axes: current RMS and temperature */
float input_signal[NEAI_INPUT_SIGNAL_LENGTH *
                   NEAI_INPUT_AXIS_NUMBER];

/* UART transmission buffer */
char uart_message[100];

/* DWT timing variables */
uint32_t cycles_per_us = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

static void DWT_Init(void);
static void delay_us(uint32_t us);

static void DHT22_Set_Pin_Output(void);
static void DHT22_Set_Pin_Input(void);

static uint8_t DHT22_Wait_For_Level(GPIO_PinState level,
                                    uint32_t timeout_us);

static uint8_t DHT22_Read(float *temperature_value,
                          float *humidity_value);

static uint8_t Read_Current(float *current_value);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief Initialize DWT counter for microsecond timing.
 */
static void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    cycles_per_us = SystemCoreClock / 1000000U;
}

/**
 * @brief Generate a microsecond delay using DWT.
 */
static void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t delay_cycles = us * cycles_per_us;

    while ((DWT->CYCCNT - start) < delay_cycles)
    {
    }
}

/**
 * @brief Configure DHT22 pin as open-drain output.
 */
static void DHT22_Set_Pin_Output(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = DHT22_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(DHT22_PORT, &GPIO_InitStruct);
}

/**
 * @brief Configure DHT22 pin as input.
 */
static void DHT22_Set_Pin_Input(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = DHT22_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;

    HAL_GPIO_Init(DHT22_PORT, &GPIO_InitStruct);
}

/**
 * @brief Wait until DHT22 pin reaches requested logic level.
 */
static uint8_t DHT22_Wait_For_Level(GPIO_PinState level,
                                    uint32_t timeout_us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t timeout_cycles = timeout_us * cycles_per_us;

    while (HAL_GPIO_ReadPin(DHT22_PORT, DHT22_PIN) != level)
    {
        if ((DWT->CYCCNT - start) > timeout_cycles)
        {
            return 0;
        }
    }

    return 1;
}

/**
 * @brief Read temperature and humidity from DHT22.
 */
static uint8_t DHT22_Read(float *temperature_value,
                          float *humidity_value)
{
    uint8_t data[5] = {0};
    uint8_t i;
    uint8_t j;

    uint32_t pulse_start;
    uint32_t pulse_duration;

    uint16_t raw_temperature;
    uint16_t raw_humidity;

    if ((temperature_value == NULL) ||
        (humidity_value == NULL) ||
        (cycles_per_us == 0U))
    {
        return 0;
    }

    /* Send start signal */
    DHT22_Set_Pin_Output();

    HAL_GPIO_WritePin(DHT22_PORT,
                      DHT22_PIN,
                      GPIO_PIN_RESET);

    HAL_Delay(2);

    /* Release data line */
    HAL_GPIO_WritePin(DHT22_PORT,
                      DHT22_PIN,
                      GPIO_PIN_SET);

    delay_us(30);

    /* Switch to input mode */
    DHT22_Set_Pin_Input();

    /* Check sensor response */
    if (!DHT22_Wait_For_Level(GPIO_PIN_RESET, 100))
        return 0;

    if (!DHT22_Wait_For_Level(GPIO_PIN_SET, 100))
        return 0;

    if (!DHT22_Wait_For_Level(GPIO_PIN_RESET, 100))
        return 0;

    /* Read 40 data bits */
    for (i = 0; i < 5; i++)
    {
        for (j = 0; j < 8; j++)
        {
            if (!DHT22_Wait_For_Level(GPIO_PIN_SET, 100))
                return 0;

            pulse_start = DWT->CYCCNT;

            if (!DHT22_Wait_For_Level(GPIO_PIN_RESET, 100))
                return 0;

            pulse_duration =
                (DWT->CYCCNT - pulse_start) / cycles_per_us;

            data[i] <<= 1;

            /* Longer high pulse represents bit 1 */
            if (pulse_duration > 40U)
            {
                data[i] |= 1U;
            }
        }
    }

    /* Verify checksum */
    if ((uint8_t)(data[0] + data[1] +
                  data[2] + data[3]) != data[4])
    {
        return 0;
    }

    /* Convert raw humidity */
    raw_humidity =
        ((uint16_t)data[0] << 8) | data[1];

    /* Convert raw temperature */
    raw_temperature =
        ((uint16_t)(data[2] & 0x7F) << 8) | data[3];

    *humidity_value = raw_humidity / 10.0f;
    *temperature_value = raw_temperature / 10.0f;

    /* Check temperature sign */
    if (data[2] & 0x80U)
    {
        *temperature_value = -*temperature_value;
    }

    return 1;
}

/**
 * @brief Read AC current using ACS712 and calculate RMS.
 *
 * RMS voltage is calculated after removing the DC offset.
 * RMS current = RMS voltage / ACS712 sensitivity.
 */
static uint8_t Read_Current(float *current_value)
{
    uint32_t i;
    uint32_t adc_sample;

    float sample_voltage;
    float voltage_sum = 0.0f;
    float voltage_squared_sum = 0.0f;
    float mean_voltage;
    float mean_square_voltage;
    float rms_voltage_squared;
    float rms_voltage;

    if (current_value == NULL)
    {
        return 0;
    }

    /* Acquire ADC samples */
    for (i = 0; i < RMS_SAMPLE_COUNT; i++)
    {
        if (HAL_ADC_Start(&hadc1) != HAL_OK)
        {
            return 0;
        }

        if (HAL_ADC_PollForConversion(&hadc1, 100) != HAL_OK)
        {
            HAL_ADC_Stop(&hadc1);
            return 0;
        }

        adc_sample = HAL_ADC_GetValue(&hadc1);

        if (HAL_ADC_Stop(&hadc1) != HAL_OK)
        {
            return 0;
        }

        /* Convert ADC value into voltage */
        sample_voltage =
            ((float)adc_sample * ACS712_VREF) /
            ACS712_ADC_MAX;

        voltage_sum += sample_voltage;
        voltage_squared_sum +=
            sample_voltage * sample_voltage;

        HAL_Delay(RMS_SAMPLE_DELAY_MS);
    }

    /* Calculate mean voltage */
    mean_voltage =
        voltage_sum / (float)RMS_SAMPLE_COUNT;

    /* Calculate mean squared voltage */
    mean_square_voltage =
        voltage_squared_sum / (float)RMS_SAMPLE_COUNT;

    /* Remove DC offset */
    rms_voltage_squared =
        mean_square_voltage -
        (mean_voltage * mean_voltage);

    if (rms_voltage_squared < 0.0f)
    {
        rms_voltage_squared = 0.0f;
    }

    /* Calculate AC RMS voltage */
    rms_voltage = sqrtf(rms_voltage_squared);

    /* Convert RMS voltage into RMS current */
    *current_value =
        rms_voltage / ACS712_SENSITIVITY;

    return 1;
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
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

    /* Initialize DWT for microsecond timing */
    DWT_Init();

    /* Initialize NanoEdge AI model */
    state = neai_anomalydetection_init(use_pretrained);

    if (state != NEAI_OK)
    {
        Error_Handler();
    }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    while (1)
    {
        /* Read AC current from ACS712 */
        if (!Read_Current(&current))
        {
            Error_Handler();
        }

        /* Read temperature and humidity from DHT22 */
        if (DHT22_Read(&temperature, &humidity))
        {
            /* Prepare NanoEdge AI input axes */
            input_signal[0] = current;
            input_signal[1] = temperature;

            /* Run anomaly detection */
            state = neai_anomalydetection_detect(
                input_signal,
                &similarity
            );

            if (state != NEAI_OK)
            {
                Error_Handler();
            }

            /* Determine anomaly status */
            anomaly =
                (similarity < ANOMALY_THRESHOLD) ? 1U : 0U;

            /* Control anomaly indicator on PD12 */
            if (anomaly == 1U)
            {
                HAL_GPIO_WritePin(GPIOD,
                                  GPIO_PIN_12,
                                  GPIO_PIN_SET);
            }
            else
            {
                HAL_GPIO_WritePin(GPIOD,
                                  GPIO_PIN_12,
                                  GPIO_PIN_RESET);
            }

            /* Prepare measurements for ESP8266 */
            int32_t current_x100 =
                (int32_t)(current * 100.0f);

            int32_t temperature_x10 =
                (int32_t)(temperature * 10.0f);

            int32_t humidity_x10 =
                (int32_t)(humidity * 10.0f);

            int uart_length = snprintf(
                uart_message,
                sizeof(uart_message),
                "I_x100=%ld;T_x10=%ld;H_x10=%ld;S=%u;A=%u\r\n",
                (long)current_x100,
                (long)temperature_x10,
                (long)humidity_x10,
                (unsigned int)similarity,
                (unsigned int)anomaly
            );

            /* Send data through USART2 */
            if ((uart_length > 0) &&
                (uart_length < (int)sizeof(uart_message)))
            {
                if (HAL_UART_Transmit(
                        &huart2,
                        (uint8_t *)uart_message,
                        (uint16_t)uart_length,
                        1000) != HAL_OK)
                {
                    Error_Handler();
                }
            }

            /* Toggle status LED on PD13 */
            HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_13);
        }
        else
        {
            /* DHT22 reading error */
            const char error_message[] = "DHT22_ERROR\r\n";

            HAL_UART_Transmit(
                &huart2,
                (uint8_t *)error_message,
                sizeof(error_message) - 1U,
                1000
            );
        }

        /* DHT22 requires approximately 2 seconds between readings */
        HAL_Delay(DHT22_INTERVAL_MS);

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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
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

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

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
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12|GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0, GPIO_PIN_SET);

  /*Configure GPIO pins : PD12 PD13 */
  GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : PD0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

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

    /* User can add implementation to report file and line */

  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
