/**
 * @file animation.c
 * @brief 320x320 20fps animation playback engine
 *
 * Phase 2: plays 5 test frames from internal flash.
 * Uses FreeRTOS 50ms timer + semaphore for frame pacing.
 */

#include "animation.h"
#include "lcd.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <string.h>

/* Linker symbols: test frame data embedded via test_frames.o */
extern const uint16_t _binary__Users_lemonliu_IOT_STM32_H743_ALIENTEK_CubeIDE_Core_Src_frames_test_5_bin_start;
#define test_frames ((const uint16_t *)&_binary__Users_lemonliu_IOT_STM32_H743_ALIENTEK_CubeIDE_Core_Src_frames_test_5_bin_start)

/* Semaphore for timer-to-task synchronization */
static osSemaphoreId_t anim_sem_id = NULL;

/* Current frame index (incremented by timer callback) */
static volatile int frame_index = 0;

/*---------------------------------------------------------------------------
 * CPU copy (line-by-line to handle framebuffer stride)
 *---------------------------------------------------------------------------
 * DMA2D M2M mode is deferred due to startup crash issues.
 * CPU memcpy is ~2ms for 200KB on 400MHz H7, well within 50ms budget.
 */
static void CopyFrame(const uint16_t *src, uint16_t *dst)
{
    for (int y = 0; y < ANIM_FRAME_HEIGHT; y++)
    {
        memcpy(dst + y * LCD_FB_STRIDE, src + y * ANIM_FRAME_WIDTH,
               ANIM_FRAME_WIDTH * 2);
    }
}

/*---------------------------------------------------------------------------
 * Timer callback (50ms = 20fps)
 *---------------------------------------------------------------------------
 */
void AnimTimerCallback(void *argument)
{
    (void)argument;
    frame_index = (frame_index + 1) % ANIM_TEST_FRAMES;
    osSemaphoreRelease(anim_sem_id);
}

/*---------------------------------------------------------------------------
 * Animation task
 *---------------------------------------------------------------------------
 */
void StartAnimationTask(void *argument)
{
    (void)argument;

    /* Give defaultTask time to finish init (LCD, USART, etc.) */
    osDelay(500);

    /* Create binary semaphore: max count = 1, initial count = 0 */
    anim_sem_id = osSemaphoreNew(1, 0, NULL);

    /* Clear screen to black */
    LCD_Clear(LCD_COLOR_BLACK);

    /* Destination pointer: physical framebuffer centered at (240, 80) */
    uint16_t *dst = (uint16_t *)LCD_FB_BASE
                  + ANIM_DST_Y * LCD_FB_STRIDE
                  + ANIM_DST_X;

    printf("Animation started: %d test frames, %dx%d at %dfps\r\n",
           ANIM_TEST_FRAMES, ANIM_FRAME_WIDTH, ANIM_FRAME_HEIGHT, ANIM_FPS);

    for (;;)
    {
        /* Wait for 50ms timer to signal next frame */
        osSemaphoreAcquire(anim_sem_id, osWaitForever);

        /* Copy current frame to framebuffer */
        CopyFrame(&test_frames[frame_index * ANIM_FRAME_PIXELS], dst);
    }
}