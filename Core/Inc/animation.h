/**
 * @file animation.h
 * @brief 320x320 20fps animation playback using DMA2D
 *
 * Frames are pre-rotated 90° CCW on PC side, so DMA2D does a direct
 * rectangle copy to the physical framebuffer (no rotation needed).
 * Timer-driven 50ms callback + semaphore for frame pacing.
 */
#ifndef __ANIMATION_H__
#define __ANIMATION_H__

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Frame dimensions (square, pre-rotated) */
#define ANIM_FRAME_WIDTH    320
#define ANIM_FRAME_HEIGHT   320
#define ANIM_FRAME_PIXELS   (ANIM_FRAME_WIDTH * ANIM_FRAME_HEIGHT)   /* 102,400 */
#define ANIM_FRAME_BYTES    (ANIM_FRAME_PIXELS * 2)                  /* 204,800 */

/* Playback timing */
#define ANIM_FPS            20
#define ANIM_PERIOD_MS      50
#define ANIM_DURATION_SEC   5
#define ANIM_TOTAL_FRAMES   100

/* Phase 2: test with 5 embedded frames */
#define ANIM_TEST_FRAMES    5

/* Physical framebuffer destination: centered 320x320 on 800x480 landscape */
#define ANIM_DST_X          240   /* (800 - 320) / 2 */
#define ANIM_DST_Y          80    /* (480 - 320) / 2 */

void AnimTimerCallback(void *argument);
void StartAnimationTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* __ANIMATION_H__ */