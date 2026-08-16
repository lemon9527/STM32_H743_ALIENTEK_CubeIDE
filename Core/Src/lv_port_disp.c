/**
 * @file lv_port_disp.c
 * @brief LVGL display driver for STM32H743 + 4.3" RGB LCD (portrait mode)
 *
 * Physical panel: 800x480 landscape, RGB565, stride=1600
 * LVGL configured: 480x800 portrait (logical)
 * Rotation: 90° CCW — logical (lx, ly) → physical (ly, 479 - lx)
 * DMA2D M2M copy for flush (hardware accelerated)
 * Double buffering: two 480x80 pixel buffers in SDRAM
 */

#include "lv_port_disp.h"
#include "lcd.h"
#include "dma2d.h"

/* DMA2D handle (initialized by CubeMX in dma2d.c) */
extern DMA2D_HandleTypeDef hdma2d;

/*---------------------------------------------------------------------------
 * LVGL Display Buffers (double buffering in SDRAM)
 *---------------------------------------------------------------------------
 * Placed at 0xC1000000, after the primary framebuffer.
 * Buffer dimensions: 480 pixels wide x 80 lines tall (portrait logical)
 * Double buffering for smooth partial refresh.
 */
#define LVGL_BUF1_ADDR  0xC1000000
#define LVGL_BUF2_ADDR  0xC1012C00
#define LVGL_BUF_SIZE    (480 * 80)   /* 38,400 pixels */

static lv_color_t *disp_buf1 = (lv_color_t *)LVGL_BUF1_ADDR;
static lv_color_t *disp_buf2 = (lv_color_t *)LVGL_BUF2_ADDR;
static lv_disp_draw_buf_t draw_buf;

/*---------------------------------------------------------------------------
 * Flush callback: CPU pixel copy with 90° CCW rotation
 *---------------------------------------------------------------------------
 * LVGL renders at 480x800 portrait (logical). Each flush area is a
 * rectangle in logical coordinates. We transform to physical landscape:
 *   phys_x = log_y
 *   phys_y = 479 - log_x
 *
 * DMA2D M2M cannot transpose, so we use CPU copy. LVGL's partial flush
 * keeps areas small (typically one widget at a time), so CPU copy is
 * fast enough on a 480MHz Cortex-M7.
 */
static void lv_port_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area,
                                lv_color_t *color_p)
{
    uint32_t log_w = (uint32_t)(area->x2 - area->x1 + 1);
    uint32_t log_h = (uint32_t)(area->y2 - area->y1 + 1);

    uint16_t *dst = (uint16_t *)LCD_FB_BASE;
    uint16_t *src = (uint16_t *)color_p;

    for (uint32_t ly = 0; ly < log_h; ly++) {
        uint32_t phys_x = (uint32_t)(area->y1 + ly);
        for (uint32_t lx = 0; lx < log_w; lx++) {
            uint32_t phys_y = (uint32_t)(479 - (area->x1 + lx));
            dst[phys_y * LCD_FB_STRIDE + phys_x] = src[ly * log_w + lx];
        }
    }

    lv_disp_flush_ready(disp_drv);
}

/*---------------------------------------------------------------------------
 * LVGL display initialization
 *---------------------------------------------------------------------------
 * Configures LVGL for 480x800 portrait (logical), with DMA2D flush
 * callback that handles the 90° CCW rotation to physical landscape.
 * Double-buffered partial refresh (80 lines per buffer).
 */
void lv_port_disp_init(void)
{
    /* Initialize display draw buffer (double buffering) */
    lv_disp_draw_buf_init(&draw_buf, disp_buf1, disp_buf2, LVGL_BUF_SIZE);

    /* Initialize display driver */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    /* Logical resolution: 480x800 portrait */
    disp_drv.hor_res = 480;
    disp_drv.ver_res = 800;

    disp_drv.flush_cb  = lv_port_disp_flush;
    disp_drv.draw_buf  = &draw_buf;

    lv_disp_drv_register(&disp_drv);
}