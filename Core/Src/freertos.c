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
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "animation.h"
#include "uart_protocol.h"
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
static osTimerId_t anim_timer_id;
static osTimerId_t lv_tick_timer_id;
osThreadId_t animTaskHandle;
const osThreadAttr_t animTask_attributes = {
  .name = "animation",
  .stack_size = 1024 * 8,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
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
void StartAnimationTask(void *argument);
void AnimTimerCallback(void *argument);
void LVTickCallback(void *argument);
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
  anim_timer_id = osTimerNew(AnimTimerCallback, osTimerPeriodic, NULL, NULL);
  osTimerStart(anim_timer_id, 50U);  /* 50ms period for 20fps animation */
  /* 1ms LVGL tick timer */
  lv_tick_timer_id = osTimerNew(LVTickCallback, osTimerPeriodic, NULL, NULL);
  osTimerStart(lv_tick_timer_id, 1U);  /* 1ms lv_tick_inc(1) */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  animTaskHandle = osThreadNew(StartAnimationTask, NULL, &animTask_attributes);
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
  uint32_t sysclk, hclk, apb1, apb2;
  uint32_t sdram_base = 0xC0000000;
  uint32_t sdram_size = 0x02000000;
  uint32_t test_pattern, readback1, readback2, end_addr;
  int sdram_pass = 1;
  int led_tick = 0;

  /* Get system clock info */
  sysclk = (unsigned long)HAL_RCC_GetSysClockFreq() / 1000000;
  hclk   = (unsigned long)HAL_RCC_GetHCLKFreq() / 1000000;
  apb1   = (unsigned long)HAL_RCC_GetPCLK1Freq() / 1000000;
  apb2   = (unsigned long)HAL_RCC_GetPCLK2Freq() / 1000000;

  /* ---------- Serial output ---------- */
  printf("\r\n");
  printf("========================================\r\n");
  printf("  STM32H743 System Clock Info\r\n");
  printf("========================================\r\n");
  printf("  SYSCLK: %lu MHz\r\n", sysclk);
  printf("  HCLK:   %lu MHz\r\n", hclk);
  printf("  APB1:   %lu MHz\r\n", apb1);
  printf("  APB2:   %lu MHz\r\n", apb2);
  printf("========================================\r\n\n");

  printf("========================================\r\n");
  printf("  SDRAM Test (W9825G6KH)\r\n");
  printf("========================================\r\n");
  printf("  Base: 0x%08lX\r\n", (unsigned long)sdram_base);
  printf("  Size: %lu MB\r\n", (unsigned long)(sdram_size / 1024 / 1024));

  test_pattern = 0xAA55AA55;
  *((volatile uint32_t *)sdram_base) = test_pattern;
  readback1 = *((volatile uint32_t *)sdram_base);
  printf("  [0x%08lX] W:0x%08lX R:0x%08lX %s\r\n",
             (unsigned long)sdram_base, (unsigned long)test_pattern, (unsigned long)readback1,
             (readback1 == test_pattern) ? "OK" : "FAIL");
  if (readback1 != test_pattern) sdram_pass = 0;

  end_addr = sdram_base + sdram_size - 4;
  test_pattern = 0x55AA55AA;
  *((volatile uint32_t *)end_addr) = test_pattern;
  readback2 = *((volatile uint32_t *)end_addr);
  printf("  [0x%08lX] W:0x%08lX R:0x%08lX %s\r\n",
             (unsigned long)end_addr, (unsigned long)test_pattern, (unsigned long)readback2,
             (readback2 == test_pattern) ? "OK" : "FAIL");
  if (readback2 != test_pattern) sdram_pass = 0;

  printf("  Result: %s\r\n", sdram_pass ? "PASS" : "FAIL");
  printf("========================================\r\n\n");

  printf("Animation starting...\r\n");

  /* Turn on LCD backlight (LVGL will manage the display) */
  LCD_Init();

  /* Initialize LVGL and its ports */
  lv_init();
  lv_port_disp_init();
  lv_port_indev_init();

  /* Initialize UART1 protocol (ControlPCB communication) */
  uart_protocol_init();
  printf("UART1 protocol started (115200 baud)\r\n");

  /* Infinite loop - LED blink + LVGL task handling */
  for(;;)
  {
    /* LED blink: toggle every 500ms (100 ticks * 5ms) */
    led_tick++;
    if (led_tick >= 100)
    {
      HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
      led_tick = 0;
    }

    /* LVGL task handler: call frequently (5ms) when Brightness page is active */
    if (lv_is_initialized() && current_page == PAGE_BRIGHTNESS)
    {
      lv_task_handler();
    }

    /* Process UART1 protocol data (ring buffer -> frame parser) */
    uart_protocol_process();

    /* Diagnostic: print UART1 RX stats every 2 seconds */
    {
        static uint32_t last_irq = 0;
        static int diag_tick = 0;
        diag_tick++;
        if (diag_tick >= 400)  /* 400 * 5ms = 2s */
        {
            diag_tick = 0;
            uint32_t cur_irq = g_uart1_rx_irq_count;
            if (cur_irq != last_irq)
            {
                printf("[UART1] RX IRQ: %lu, overflow: %lu, rb: %u\r\n",
                       cur_irq, g_uart1_rb_overflow,
                       uart_rb_available(&g_uart_rb));
                last_irq = cur_irq;
            }
        }
    }

    osDelay(5);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* LVGL 1ms tick callback - increments LVGL tick counter.
   Placed in USER CODE section so CubeMX won't overwrite it. */
void LVTickCallback(void *argument)
{
  (void)argument;
  lv_tick_inc(1);
}

/* USER CODE END Application */