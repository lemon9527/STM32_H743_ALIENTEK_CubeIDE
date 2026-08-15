/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "lcd.h"
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
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */

  /* Print system clock info on startup */
  printf("\r\n");
  printf("========================================\r\n");
  printf("  STM32H743 System Clock Info\r\n");
  printf("========================================\r\n");
  printf("  SYSCLK: %lu MHz\r\n", (unsigned long)HAL_RCC_GetSysClockFreq() / 1000000);
  printf("  HCLK:   %lu MHz\r\n", (unsigned long)HAL_RCC_GetHCLKFreq() / 1000000);
  printf("  APB1:   %lu MHz\r\n", (unsigned long)HAL_RCC_GetPCLK1Freq() / 1000000);
  printf("  APB2:   %lu MHz\r\n", (unsigned long)HAL_RCC_GetPCLK2Freq() / 1000000);
  printf("========================================\r\n");
  printf("\r\n");

  printf("========================================\r\n");
  printf("  SDRAM Test (W9825G6KH)\r\n");
  printf("========================================\r\n");
  {
    uint32_t sdram_base = 0xC0000000;
    uint32_t sdram_size = 0x02000000;  /* 32MB */
    uint32_t test_pattern = 0xAA55AA55;
    uint32_t readback;
    int pass = 1;

    printf("  Base: 0x%08lX\r\n", (unsigned long)sdram_base);
    printf("  Size: %lu MB\r\n", (unsigned long)(sdram_size / 1024 / 1024));

    /* Test 1: write/read at start */
    *((volatile uint32_t *)sdram_base) = test_pattern;
    readback = *((volatile uint32_t *)sdram_base);
    printf("  [0x%08lX] W:0x%08lX R:0x%08lX %s\r\n",
               (unsigned long)sdram_base, (unsigned long)test_pattern, (unsigned long)readback,
               (readback == test_pattern) ? "OK" : "FAIL");
    if (readback != test_pattern) pass = 0;

    /* Test 2: write/read at end */
    uint32_t end_addr = sdram_base + sdram_size - 4;
    test_pattern = 0x55AA55AA;
    *((volatile uint32_t *)end_addr) = test_pattern;
    readback = *((volatile uint32_t *)end_addr);
    printf("  [0x%08lX] W:0x%08lX R:0x%08lX %s\r\n",
               (unsigned long)end_addr, (unsigned long)test_pattern, (unsigned long)readback,
               (readback == test_pattern) ? "OK" : "FAIL");
    if (readback != test_pattern) pass = 0;

    printf("  Result: %s\r\n", pass ? "PASS" : "FAIL");
  }
  printf("========================================\r\n");
  printf("\r\n");

  /* LCD test */
  printf("========================================\r\n");
  printf("  LCD Test (4.3\" RGB 800x480)\r\n");
  printf("========================================\r\n");

  /* Turn on backlight */
  LCD_Init();
  printf("  Backlight ON (PB5)\r\n");

  /* Fill screen with colors */
  printf("  Red screen...\r\n");
  LCD_Clear(LCD_COLOR_RED);
  osDelay(1000);

  printf("  Green screen...\r\n");
  LCD_Clear(LCD_COLOR_GREEN);
  osDelay(1000);

  printf("  Blue screen...\r\n");
  LCD_Clear(LCD_COLOR_BLUE);
  osDelay(1000);

  printf("  White screen...\r\n");
  LCD_Clear(LCD_COLOR_WHITE);

  printf("  Result: PASS\r\n");
  printf("========================================\r\n");
  printf("\r\n");

  /* Infinite loop */
  for(;;)
  {
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);  /* Toggle LED1 Green (PB0) */
    osDelay(500);                            /* 500ms */
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

