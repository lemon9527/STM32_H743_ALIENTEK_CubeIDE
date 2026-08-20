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
#include "stm32h7xx.h"

/* DMA2D handle (initialized by CubeMX in dma2d.c) */
extern DMA2D_HandleTypeDef hdma2d;

/* Helper: convert RGB565 -> ARGB1555 (alpha=1 opaque) */
static inline uint16_t rgb565_to_argb1555_local(uint16_t rgb565)
{
    return (uint16_t)(0x8000u | ((rgb565 & 0xF800u) >> 1) | ((rgb565 & 0x07C0u) >> 1) | (rgb565 & 0x001Fu));
}

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

    uint16_t *src = (uint16_t *)color_p;

    /* Write LVGL output into Layer 2 UI buffer (ARGB1555) at UI_BUF_ADDR.
     * We convert RGB565 (LVGL render) -> ARGB1555 (Layer2 format) and
     * perform the same 90deg CCW rotation mapping used elsewhere.
     */
    uint16_t *ui_buf = (uint16_t *)UI_BUF_ADDR;

    /* conversion uses file-scope helper rgb565_to_argb1555_local() */

    for (uint32_t ly = 0; ly < log_h; ly++) {
        uint32_t phys_x = (uint32_t)(area->y1 + ly);
        for (uint32_t lx = 0; lx < log_w; lx++) {
            uint32_t phys_y = (uint32_t)(479 - (area->x1 + lx));
            uint16_t pixel_rgb565 = src[ly * log_w + lx];
            uint16_t pixel_argb1555 = rgb565_to_argb1555_local(pixel_rgb565);
            ui_buf[phys_y * UI_BUF_STRIDE + phys_x] = pixel_argb1555;
        }
    }

    /* Clean D-Cache for the physical region we just wrote so LTDC/DMA
     * reads the updated pixels. Compute physical rectangle bounds.
     */
    uint32_t phys_x_min = (uint32_t)area->y1;
    uint32_t phys_x_max = (uint32_t)area->y2;
    uint32_t phys_y_min = (uint32_t)(479 - area->x2);
    uint32_t phys_y_max = (uint32_t)(479 - area->x1);

    uint32_t phys_w = phys_x_max - phys_x_min + 1; /* pixels per row written */
    /* Clean each written row individually to avoid touching unrelated memory */
    for (uint32_t py = phys_y_min; py <= phys_y_max; py++) {
        uint32_t *addr = (uint32_t *)&ui_buf[py * UI_BUF_STRIDE + phys_x_min];
        uint32_t size_bytes = phys_w * sizeof(uint16_t);
        /* Align address down to 32-byte boundary and size up */
        uint32_t start = ((uint32_t)addr) & ~0x1FU;
        uint32_t end = (((uint32_t)addr + size_bytes + 31) & ~0x1FU);
        SCB_CleanDCache_by_Addr((uint32_t *)start, end - start);
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