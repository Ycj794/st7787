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
#include "spi.h"
#include "gpio.h"
#include <stdio.h>
#include <string.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "st7789.h"
#include "pc_uart.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static uint8_t Dashboard_ImageInputRow[240];
static uint16_t Dashboard_ImageDisplayRow[ST7789_WIDTH];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static uint8_t Dashboard_ParseNumber(const char *text, uint8_t *number)
{
  uint16_t value = 0;
  uint8_t has_digit = 0;

  while ((*text >= '0') && (*text <= '9'))
  {
    has_digit = 1;
    value = (uint16_t)(value * 10U + (uint8_t)(*text - '0'));
    if (value > 100U)
    {
      return 0;
    }
    text++;
  }

  if (!has_digit || (*text != '\0'))
  {
    return 0;
  }
  *number = (uint8_t)value;
  return 1;
}

static void Dashboard_Update(uint8_t cpu, uint8_t memory,
                             const char *clock, const char *note)
{
  static uint8_t previous_cpu;
  static uint8_t previous_memory;
  static uint8_t initialized;
  static char previous_note[25];
  char text[25];
  uint8_t redraw_all = (uint8_t)!initialized;
  uint16_t old_width;
  uint16_t new_width;

  if (!initialized)
  {
    ST7789_Fill(16, 108, 223, 119, GRAYBLUE);
    ST7789_Fill(16, 164, 223, 175, GRAYBLUE);
    initialized = 1;
  }

  (void)snprintf(text, sizeof(text), "TIME %s", clock);
  ST7789_WriteString(16, 48, text, Font_11x18, WHITE, BLACK);

  if (redraw_all || (cpu != previous_cpu))
  {
    (void)snprintf(text, sizeof(text), "CPU %3u%%", cpu);
    ST7789_WriteString(16, 82, text, Font_11x18, YELLOW, BLACK);
  }

  old_width = (uint16_t)(207U * previous_cpu / 100U);
  new_width = (uint16_t)(207U * cpu / 100U);
  if (new_width > old_width)
  {
    ST7789_Fill((uint16_t)(16U + old_width), 108,
                (uint16_t)(15U + new_width), 119, CYAN);
  }
  else if (new_width < old_width)
  {
    ST7789_Fill((uint16_t)(16U + new_width), 108,
                (uint16_t)(15U + old_width), 119, GRAYBLUE);
  }
  previous_cpu = cpu;

  if (redraw_all || (memory != previous_memory))
  {
    (void)snprintf(text, sizeof(text), "MEM %3u%%", memory);
    ST7789_WriteString(16, 138, text, Font_11x18, YELLOW, BLACK);
  }
  old_width = (uint16_t)(207U * previous_memory / 100U);
  new_width = (uint16_t)(207U * memory / 100U);
  if (new_width > old_width)
  {
    ST7789_Fill((uint16_t)(16U + old_width), 164,
                (uint16_t)(15U + new_width), 175, GREEN);
  }
  else if (new_width < old_width)
  {
    ST7789_Fill((uint16_t)(16U + new_width), 164,
                (uint16_t)(15U + old_width), 175, GRAYBLUE);
  }
  previous_memory = memory;

  if (strcmp(previous_note, note) != 0)
  {
    (void)snprintf(text, sizeof(text), "%-24.24s", note);
    ST7789_WriteString(16, 210, text, Font_7x10, WHITE, BLACK);
    (void)snprintf(previous_note, sizeof(previous_note), "%-24.24s", note);
  }
}

static void Dashboard_Init(void)
{
  ST7789_Fill_Color(BLACK);
  ST7789_WriteString(16, 16, "PC DASHBOARD", Font_11x18, CYAN, BLACK);
  ST7789_WriteString(16, 82, "CPU", Font_11x18, YELLOW, BLACK);
  ST7789_WriteString(16, 138, "MEM", Font_11x18, YELLOW, BLACK);
  ST7789_WriteString(16, 191, "NOTE", Font_7x10, CYAN, BLACK);
  Dashboard_Update(0, 0, "--:--:--", "Waiting for PC...");
}

static void Dashboard_HandleLine(char *line)
{
  char *cpu_text = line;
  char *memory_text = strchr(cpu_text, ',');
  char *clock_text;
  char *note_text;
  uint8_t cpu;
  uint8_t memory;
  char clock[9];
  char note[25];
  size_t i;

  if (strcmp(line, "IMG") == 0)
  {
    uint16_t x;
    uint16_t y;

    for (y = 0; y < 120U; y++)
    {
      if (PC_UART_ReadBytes(Dashboard_ImageInputRow,
                            sizeof(Dashboard_ImageInputRow), 1000U) != HAL_OK)
      {
        ST7789_Fill_Color(BLACK);
        ST7789_WriteString(16, 110, "IMAGE RX ERROR", Font_11x18, RED, BLACK);
        return;
      }

      for (x = 0; x < 120U; x++)
      {
        uint16_t color = (uint16_t)(
            ((uint16_t)Dashboard_ImageInputRow[2U * x] << 8) |
            Dashboard_ImageInputRow[2U * x + 1U]);
        uint16_t display_color = (uint16_t)((color << 8) | (color >> 8));
        Dashboard_ImageDisplayRow[2U * x] = display_color;
        Dashboard_ImageDisplayRow[2U * x + 1U] = display_color;
      }

      ST7789_DrawImage(0, (uint16_t)(2U * y), ST7789_WIDTH, 1,
                       Dashboard_ImageDisplayRow);
      ST7789_DrawImage(0, (uint16_t)(2U * y + 1U), ST7789_WIDTH, 1,
                       Dashboard_ImageDisplayRow);
    }
    return;
  }

  if (memory_text == NULL)
  {
    return;
  }
  *memory_text++ = '\0';
  clock_text = strchr(memory_text, ',');
  if (clock_text == NULL)
  {
    return;
  }
  *clock_text++ = '\0';
  note_text = strchr(clock_text, ',');
  if (note_text == NULL)
  {
    return;
  }
  *note_text++ = '\0';

  if (!Dashboard_ParseNumber(cpu_text, &cpu) ||
      !Dashboard_ParseNumber(memory_text, &memory))
  {
    return;
  }
  for (i = 0; (i < sizeof(clock) - 1U) && (clock_text[i] != '\0'); i++)
  {
    clock[i] = clock_text[i];
  }
  clock[i] = '\0';
  for (i = 0; (i < sizeof(note) - 1U) && (note_text[i] != '\0'); i++)
  {
    note[i] = note_text[i];
  }
  note[i] = '\0';

  Dashboard_Update(cpu, memory, clock, note);
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
  MX_SPI1_Init();
  if (PC_UART_Init() != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN 2 */
  HAL_Delay(1000);
  ST7789_Init();
  Dashboard_Init();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    char line[96];
    int result = PC_UART_ReadLine(line, sizeof(line));
    if (result == 1)
    {
      Dashboard_HandleLine(line);
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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
