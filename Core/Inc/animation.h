/**
 * @file animation.h
 * @brief 320x320 20fps raw RGB565 animation playback
 *
 * Phase 6: Raw RGB565 frames stored on QSPI Flash.
 * DMA2D copies directly from QSPI memory-mapped region (0x90000000)
 * to the LCD framebuffer. No JPEG decode, no intermediate buffer.
 *
 * Timer-driven 50ms callback + semaphore for frame pacing.
 */
#ifndef __ANIMATION_H__
#define __ANIMATION_H__

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Frame dimensions (square) */
#define ANIM_FRAME_WIDTH    320
#define ANIM_FRAME_HEIGHT   320
#define ANIM_FRAME_PIXELS   (ANIM_FRAME_WIDTH * ANIM_FRAME_HEIGHT)   /* 102,400 */
#define ANIM_FRAME_BYTES    (ANIM_FRAME_PIXELS * 2)                  /* 204,800 */

/* Playback timing */
#define ANIM_FPS            20
#define ANIM_PERIOD_MS      50
#define ANIM_DURATION_SEC   5
#define ANIM_TOTAL_FRAMES   100

/*
 * Physical framebuffer destination for portrait mode (vertical screen).
 *
 * Physical panel: 800x480 landscape. Panel is physically rotated 90 deg CCW.
 * LTDC reads the physical framebuffer and outputs to the landscape panel,
 * which is then rotated 90 deg CCW by the physical mounting.
 *
 * To display upright in portrait mode, animation data is pre-rotated 90 deg CW
 * by CPU in CopyFrame/CopyBottomBar, then placed in the physical framebuffer.
 * The panel's 90 deg CCW rotation cancels the pre-rotation, producing upright
 * output.
 *
 * Logical (portrait) coordinate: 480x800
 *   Animation at logical (80, 160) to (400, 480)  = 320x320, centered, 160px from top
 *   lcd_rotate_coords(80, 160) -> physical (160, 399)
 *   lcd_rotate_coords(400, 480) -> physical (480, 79)
 *   Physical rectangle for pre-rotated data: (160, 80) to (479, 399) = 320x320
 *
 *   Bottom bar at logical (80, 506) to (400, 640) = 320x134, 160px from bottom
 *   lcd_rotate_coords(80, 506) -> physical (506, 399)
 *   lcd_rotate_coords(400, 640) -> physical (640, 79)
 *   After 90 deg CW rotation: 320x134 logical -> 134x320 physical
 *   Physical rectangle for pre-rotated data: (506, 80) to (639, 399) = 134x320
 */
#define ANIM_DST_X          160
#define ANIM_DST_Y          80

/*
 * clean_text overlay: 140x116 logical, centered on animation area.
 *   Animation logical: (80, 160) to (400, 480) = 320x320
 *   Overlay logical:   (170, 262) to (310, 378) = 140x116
 *   lcd_rotate_coords(170, 262) -> physical (262, 309)
 *   lcd_rotate_coords(310, 378) -> physical (378, 169)
 *   Physical rectangle for pre-rotated data: (262, 169) to (378, 308) = 116x140
 */
#define OVLY_LOGICAL_W      140
#define OVLY_LOGICAL_H      116
#define OVLY_DST_X          262
#define OVLY_DST_Y          169

/* External symbols for clean_text overlay data (embedded in Flash .rodata.video) */
extern const uint8_t _binary_clean_text_rgb_raw_start[];
extern const uint8_t _binary_clean_text_rgb_raw_end[];
/* clean_text alpha (1 byte each, 140x116 = 16240 bytes) */
extern const uint8_t _binary_clean_text_alpha_raw_start[];
extern const uint8_t _binary_clean_text_alpha_raw_end[];

/* Page state enumeration */
typedef enum {
    PAGE_ANIMATION  = 0,
    PAGE_LVGL       = 1,
    PAGE_BRIGHTNESS = 2,
} PageState_t;

extern volatile PageState_t current_page;

void AnimTimerCallback(void *argument);
void StartAnimationTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* __ANIMATION_H__ */