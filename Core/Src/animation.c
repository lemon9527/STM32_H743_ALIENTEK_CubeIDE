/**
 * @file animation.c
 * @brief 320x320 20fps animation playback engine
 *
 * Phase 2: plays 5 test frames from internal flash via DMA2D.
 * Uses FreeRTOS 50ms timer + semaphore for frame pacing.
 * Frames are pre-rotated 90° CCW on PC side.
 */

#include "animation.h"
#include "lcd.h"
#include "dma2d.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <string.h>

/* DMA2D handle (initialized by CubeMX in dma2d.c) */
extern DMA2D_HandleTypeDef hdma2d;

/* Linker symbols: test frame data embedded via test_frames.o */
extern const uint16_t _binary__Users_lemonliu_IOT_STM32_H743_ALIENTEK_CubeIDE_Core_Src_frames_test_5_bin_start;
#define test_frames ((const uint16_t *)&_binary__Users_lemonliu_IOT_STM32_H743_ALIENTEK_CubeIDE_Core_Src_frames_test_5_bin_start)

/* Semaphore for timer-to-task synchronization */
static osSemaphoreId_t anim_sem_id = NULL;

/* Current frame index (incremented by timer callback) */
static volatile int frame_index = 0;

/*---------------------------------------------------------------------------
 * DMA2D rectangle copy (no rotation, source is contiguous)
 *---------------------------------------------------------------------------
 * Copies a 320x320 RGB565 rectangle from src to the physical framebuffer.
 * Pattern: same as LCD_Clear in lcd.c — temporarily switch DMA2D mode,
 * perform the copy, then restore default M2M_PFC mode.
 */
static void DMA2D_CopyFrame(const uint16_t *src, uint16_t *dst)
{
    /* CPU memcpy line-by-line (handles stride). DMA2D optimization deferred. */
    for (int y = 0; y < ANIM_FRAME_HEIGHT; y++)
    {
        memcpy(dst + y * LCD_FB_STRIDE, src + y * ANIM_FRAME_WIDTH,
               ANIM_FRAME_WIDTH * 2);
    }
}

/*---------------------------------------------------------------------------
 * Timer callback (50ms = 20fps)
 *---------------------------------------------------------------------------
 * Called from FreeRTOS timer service task. Increments frame index and
 * signals the animation task to copy the next frame.
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
 * Initializes the display, creates the sync semaphore, then loops:
 * wait for timer → DMA2D copy current frame to framebuffer.
 */
void StartAnimationTask(void *argument)
{
    (void)argument;

    /* Create binary semaphore: max count = 1, initial count = 0 */
    anim_sem_id = osSemaphoreNew(1, 0, NULL);

    /* Clear screen to black using DMA2D R2M fill */
    LCD_Clear(LCD_COLOR_BLACK);

    /* Destination pointer: physical framebuffer at (240, 80) */
    uint16_t *dst = (uint16_t *)LCD_FB_BASE
                  + ANIM_DST_Y * LCD_FB_STRIDE
                  + ANIM_DST_X;

    printf("Animation started: %d test frames, %dx%d at %dfps\r\n",
           ANIM_TEST_FRAMES, ANIM_FRAME_WIDTH, ANIM_FRAME_HEIGHT, ANIM_FPS);

    for (;;)
    {
        /* Wait for 50ms timer to signal next frame */
        osSemaphoreAcquire(anim_sem_id, osWaitForever);

        /* Copy current frame to framebuffer via DMA2D */
        DMA2D_CopyFrame(&test_frames[frame_index * ANIM_FRAME_PIXELS], dst);
    }
}