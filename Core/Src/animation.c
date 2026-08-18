/**
 * @file animation.c
 * @brief 320x320 20fps raw RGB565 animation playback engine
 *
 * Phase 6: Raw RGB565 frames stored on QSPI Flash (no JPEG decode).
 * DMA2D copies directly from QSPI memory-mapped region (0x90000000)
 * to the inactive framebuffer. No JPEG decode, no intermediate buffer.
 *
 * Uses FreeRTOS 50ms timer + semaphore for frame pacing.
 */

#include "animation.h"
#include "lcd.h"
#include "ltdc.h"
#include "qspi_video.h"
#include "qspi_flash.h"
#include "dma2d.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <string.h>

/* Number of frames (read from QSPI Flash at init) */
static uint32_t num_frames = 0;

/* Semaphore for timer-to-task synchronization */
static osSemaphoreId_t anim_sem_id = NULL;

/* Current frame index (incremented by timer callback) */
static volatile int frame_index = 0;

/* DMA2D handle (initialized by CubeMX in dma2d.c) */
extern DMA2D_HandleTypeDef hdma2d;

/* LTDC handle (initialized by CubeMX in ltdc.c) */
extern LTDC_HandleTypeDef hltdc;

/*---------------------------------------------------------------------------
 * Double buffering
 *---------------------------------------------------------------------------
 * Two full framebuffers in SDRAM, 1.5MB each (1600*480*2).
 * The animation writes to the INACTIVE buffer, then swaps the LTDC
 * layer address at VSYNC using the shadow register mechanism.
 */
#define FB_FRONT       0xC0000000U
#define FB_BACK        0xC0180000U

static uint32_t fb_active = FB_FRONT;

/*---------------------------------------------------------------------------
 * Swap LTDC layer to a new framebuffer address (immediate)
 *---------------------------------------------------------------------------
 * Since we write to the INACTIVE buffer and only switch after DMA2D
 * completes, the LTDC is never reading the buffer being written to.
 * Immediate switch is safe — no tearing, no VSYNC wait jitter.
 */
static void SwapFramebuffer(uint32_t new_fb)
{
    LTDC_Layer1->CFBAR = new_fb;
    fb_active = new_fb;
}

/*---------------------------------------------------------------------------
 * CPU: 90 deg CW rotate + copy animation frame from QSPI to SDRAM
 *---------------------------------------------------------------------------
 * Source: QSPI memory-mapped (0x90000000 + offset), 320x320 landscape
 * Destination: physical framebuffer at (ANIM_DST_X, ANIM_DST_Y), rotated
 *
 * The panel is physically mounted 90 deg CCW relative to the LTDC output.
 * We pre-rotate data 90 deg CW so the panel rotation cancels out and
 * produces an upright portrait display.
 *
 * Rotation: source (sx, sy) -> physical (ANIM_DST_X + (319-sy), ANIM_DST_Y + sx)
 * Read source sequentially (row-major) for best QSPI throughput.
 */
static void CopyFrame(const uint16_t *src, uint16_t *dst_base)
{
    for (int sy = 0; sy < ANIM_FRAME_HEIGHT; sy++)
    {
        for (int sx = 0; sx < ANIM_FRAME_WIDTH; sx++)
        {
            int px = ANIM_DST_X + (ANIM_FRAME_WIDTH - 1 - sy);  /* 319 - sy */
            int py = ANIM_DST_Y + sx;
            dst_base[py * LCD_FB_STRIDE + px] = src[sy * ANIM_FRAME_WIDTH + sx];
        }
    }
}

static const uint8_t digit_font[10][16] = {
    {0x00,0x00,0x00,0x00,0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00,0x00,0x00,0x00,0x00}, /* 0 */
    {0x00,0x00,0x00,0x00,0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00,0x00,0x00,0x00,0x00}, /* 1 */
    {0x00,0x00,0x00,0x00,0x3C,0x66,0x06,0x0C,0x18,0x30,0x7E,0x00,0x00,0x00,0x00,0x00}, /* 2 */
    {0x00,0x00,0x00,0x00,0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00,0x00,0x00,0x00,0x00}, /* 3 */
    {0x00,0x00,0x00,0x00,0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0x00,0x00,0x00,0x00,0x00}, /* 4 */
    {0x00,0x00,0x00,0x00,0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00,0x00,0x00,0x00,0x00}, /* 5 */
    {0x00,0x00,0x00,0x00,0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00,0x00,0x00,0x00,0x00}, /* 6 */
    {0x00,0x00,0x00,0x00,0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x00,0x00,0x00,0x00,0x00}, /* 7 */
    {0x00,0x00,0x00,0x00,0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00,0x00,0x00,0x00,0x00}, /* 8 */
    {0x00,0x00,0x00,0x00,0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00,0x00,0x00,0x00,0x00}, /* 9 */
};

/*---------------------------------------------------------------------------
 * RGB565 to ARGB1555 conversion (for LTDC Layer 2 UI buffer)
 *   ARGB1555: bit15=alpha(1=opaque), bits14-10=R, bits9-5=G, bits4-0=B
 *   RGB565:   bits15-11=R, bits10-5=G, bits4-0=B
 *---------------------------------------------------------------------------
 */
static inline uint16_t rgb565_to_argb1555(uint16_t rgb565)
{
    return 0x8000 | ((rgb565 & 0xF800) >> 1) | ((rgb565 & 0x07C0) >> 1) | (rgb565 & 0x001F);
}

/*---------------------------------------------------------------------------
 * Pre-render static overlay elements to LTDC Layer 2 UI buffer
 * Called once at startup. Draws bottom bar + clean_text on the UI buffer.
 * Layer 2 uses ARGB1555 format: alpha=1 for overlay, alpha=0 for transparent.
 *---------------------------------------------------------------------------
 */
static void DrawTextLineCenter(uint16_t log_y, const char *text, uint16_t color);

static void PreRenderUI(void)
{
    uint16_t *ui_buf = (uint16_t *)UI_BUF_ADDR;

    /* Bottom bar: 320x134 RGB565, 90 deg CCW rotation */
    const uint16_t *bar_src = (const uint16_t *)_binary_bottom_bar_raw_start;
    for (int sy = 0; sy < BAR_HEIGHT; sy++)
    {
        for (int sx = 0; sx < BAR_WIDTH; sx++)
        {
            /* 90 deg CCW: source (sx, sy) -> physical (BAR_DST_X + sy, BAR_DST_Y + (BAR_WIDTH-1-sx)) */
            int px = BAR_DST_X + sy;
            int py = BAR_DST_Y + (BAR_WIDTH - 1 - sx);
            ui_buf[py * UI_BUF_STRIDE + px] = rgb565_to_argb1555(bar_src[sy * BAR_WIDTH + sx]);
        }
    }

    /* Clean text: 140x116 RGB565, 90 deg CCW rotation, full image (with black bg) */
    const uint16_t *text_rgb = (const uint16_t *)_binary_clean_text_rgb_raw_start;
    for (int sy = 0; sy < OVLY_LOGICAL_H; sy++)
    {
        for (int sx = 0; sx < OVLY_LOGICAL_W; sx++)
        {
            /* 90 deg CCW: source (sx, sy) -> physical (OVLY_DST_X + sy, OVLY_DST_Y + (OVLY_LOGICAL_W-1-sx)) */
            int px = OVLY_DST_X + sy;
            int py = OVLY_DST_Y + (OVLY_LOGICAL_W - 1 - sx);
            ui_buf[py * UI_BUF_STRIDE + px] = rgb565_to_argb1555(text_rgb[sy * OVLY_LOGICAL_W + sx]);
        }
    }

    /*---------------------------------------------------------------------------
     * 320x480 blue border rectangle, 2px thick, centered on portrait display.
     *   Logical: (80, 160) to (400, 640) = 320x480, centered 480x800 portrait.
     *   After 90° CCW panel rotation to physical (800x480):
     *     Top edge    (logical y=160,161)  → physical x=160,161; y=79..399  (2px wide vertical)
     *     Bottom edge (logical y=639,640)  → physical x=639,640; y=79..399  (2px wide vertical)
     *     Left edge   (logical x=80,81)    → physical y=398,399; x=160..640 (2px thick horizontal)
     *     Right edge  (logical x=399,400)  → physical y=79,80;   x=160..640 (2px thick horizontal)
     *   Interior is transparent (ARGB1555=0x0000) → animation shows through.
     *   Drawn after bottom_bar/clean_text to be on top.
     *---------------------------------------------------------------------------
     */
    #define BLUE_BORDER_ARGB1555  0x801F   /* ARGB1555: A=1, R=0, G=0, B=31 */

    /* Top edge: physical vertical lines at x=160,161, y=79..399 */
    for (int y = 79; y <= 399; y++)
    {
        ui_buf[y * UI_BUF_STRIDE + 160] = BLUE_BORDER_ARGB1555;
        ui_buf[y * UI_BUF_STRIDE + 161] = BLUE_BORDER_ARGB1555;
    }
    /* Bottom edge: physical vertical lines at x=639,640, y=79..399 */
    for (int y = 79; y <= 399; y++)
    {
        ui_buf[y * UI_BUF_STRIDE + 639] = BLUE_BORDER_ARGB1555;
        ui_buf[y * UI_BUF_STRIDE + 640] = BLUE_BORDER_ARGB1555;
    }
    /* Left edge: physical horizontal lines at y=398,399, x=160..640 */
    for (int x = 160; x <= 640; x++)
    {
        ui_buf[398 * UI_BUF_STRIDE + x] = BLUE_BORDER_ARGB1555;
        ui_buf[399 * UI_BUF_STRIDE + x] = BLUE_BORDER_ARGB1555;
    }
    /* Right edge: physical horizontal lines at y=79,80, x=160..640 */
    for (int x = 160; x <= 640; x++)
    {
        ui_buf[79 * UI_BUF_STRIDE + x] = BLUE_BORDER_ARGB1555;
        ui_buf[80 * UI_BUF_STRIDE + x] = BLUE_BORDER_ARGB1555;
    }

    /*---------------------------------------------------------------------------
     * Three lines centered above the 320px rectangle.
     *   Logical y=70  → "animation demo"
     *   Logical y=102 → "STM32H743IIT6"
     *   Logical y=134 → "20fps"
     *   White text (0xFFFF), transparent background.
     *---------------------------------------------------------------------------
     */
    DrawTextLineCenter(70,  "animation demo",   0xFFFF);
    DrawTextLineCenter(102, "STM32H743IIT6",    0xFFFF);
    DrawTextLineCenter(134, "20fps",            0xFFFF);
}

/* Font metrics for frame number (8x16 bitmap, 2x scaled) */
#define FONT_SCALE   2
#define CHAR_W       (8 * FONT_SCALE)   /* 16 px */

/* Use LVGL Montserrat 24 font — anti-aliased, much better than 8x16 bitmap */
#include "src/font/lv_font.h"
LV_FONT_DECLARE(lv_font_montserrat_24);
#define LV_FONT (&lv_font_montserrat_24)

#define FN_X         4
#define FN_Y         4

/*---------------------------------------------------------------------------
 * Draw a single text line horizontally centered above the 320px rectangle
 * Uses LVGL Montserrat 14 font (anti-aliased).
 *   log_y: logical y position (portrait coordinate)
 *   color: ARGB1555 color (must have A=1 bit set, e.g. 0xFFFF for white)
 *   Text is centered horizontally in the 320px rectangle (logical x=80..400).
 *   Each pixel is rotated from logical to physical via the 90° CCW mapping.
 *---------------------------------------------------------------------------
 */
static void DrawTextLineCenter(uint16_t log_y, const char *text, uint16_t color)
{
    uint16_t *ui_buf = (uint16_t *)UI_BUF_ADDR;

    /* First pass: calculate total width (sum of adv_w for each char) */
    int len = strlen(text);
    int total_w = 0;
    for (int i = 0; i < len; i++)
    {
        lv_font_glyph_dsc_t dsc;
        if (lv_font_get_glyph_dsc(LV_FONT, &dsc, (uint32_t)(uint8_t)text[i], 0))
            total_w += dsc.adv_w;
        else
            total_w += LV_FONT->line_height / 2; /* fallback */
    }

    /* Center horizontally in the 320px logical rectangle (x=80 to 400) */
    int log_x = 80 + ((400 - 80) - total_w) / 2;
    int cursor_x = log_x;

    /* Second pass: render each glyph */
    for (int i = 0; i < len; i++)
    {
        uint32_t letter = (uint32_t)(uint8_t)text[i];
        lv_font_glyph_dsc_t dsc;
        if (!lv_font_get_glyph_dsc(LV_FONT, &dsc, letter, 0))
        {
            cursor_x += LV_FONT->line_height / 2;
            continue;
        }

        const uint8_t *bmp = lv_font_get_glyph_bitmap(LV_FONT, letter);
        if (!bmp) continue;
        int bpp = dsc.bpp;

        /* Glyph top in logical coordinates.
         * LVGL formula: gpos_y = baseline - box_h - ofs_y
         *   baseline = log_y + (line_height - base_line)
         */
        int gpos_y = log_y + (LV_FONT->line_height - LV_FONT->base_line) - dsc.box_h - dsc.ofs_y;

        /* Render glyph bitmap (row-major: for each row, then each column).
         * LVGL stores bitmaps in row-major order with bpp bits per pixel.
         * For bpp=4: each byte = 2 pixels (high nibble first, low nibble second).
         */
        for (int row = 0; row < dsc.box_h; row++)
        {
            for (int col = 0; col < dsc.box_w; col++)
            {
                /* Extract pixel value from row-major bitmap */
                int pixel_idx = row * dsc.box_w + col;
                uint8_t pixel_val;

                if (bpp == 1)
                    pixel_val = (bmp[pixel_idx / 8] >> (7 - (pixel_idx % 8))) & 1;
                else if (bpp == 2)
                    pixel_val = (bmp[pixel_idx / 4] >> (6 - 2 * (pixel_idx % 4))) & 3;
                else if (bpp == 4)
                    pixel_val = (bmp[pixel_idx / 2] >> (4 * (1 - (pixel_idx % 2)))) & 0x0F;
                else /* bpp == 8 */
                    pixel_val = bmp[pixel_idx];

                if (pixel_val == 0) continue; /* transparent */

                /* Logical pixel position:
                 *   lx = cursor_x + ofs_x + col
                 *   ly = gpos_y + row
                 */
                int lx = cursor_x + dsc.ofs_x + col;
                int ly = gpos_y + row;

                /* Convert to physical: px = ly, py = 479 - lx */
                int px = ly;
                int py = 479 - lx;

                if (px < 800 && py < 480)
                {
                    /* For anti-aliased (bpp>1), dim the color for edge pixels */
                    if (bpp > 1)
                    {
                        int max_val = (1 << bpp) - 1;
                        int r = ((color >> 10) & 0x1F) * pixel_val / max_val;
                        int g = ((color >> 5) & 0x1F) * pixel_val / max_val;
                        int b = (color & 0x1F) * pixel_val / max_val;
                        ui_buf[py * UI_BUF_STRIDE + px] = 0x8000 | (r << 10) | (g << 5) | b;
                    }
                    else
                    {
                        ui_buf[py * UI_BUF_STRIDE + px] = color;
                    }
                }
            }
        }

        cursor_x += dsc.adv_w;
    }
}

/*---------------------------------------------------------------------------
 * Draw frame number on LTDC Layer 2 UI buffer (ARGB1555)
 * Called every frame. White text on transparent background.
 * Uses UI_BUF_STRIDE (800) instead of LCD_FB_STRIDE (1600).
 *---------------------------------------------------------------------------
 */
static void DrawFrameNumberUI(int num)
{
    uint16_t *ui_buf = (uint16_t *)UI_BUF_ADDR;
    char buf[8];
    int len = sprintf(buf, "%d", num);
    int x, y, row, col, sx, sy;

    for (int i = 0; i < len; i++)
    {
        int idx = buf[i] - '0';
        if (idx < 0 || idx > 9) continue;

        for (row = 0; row < 16; row++)
        {
            uint8_t line = digit_font[idx][row];
            for (col = 0; col < 8; col++)
            {
                /* ARGB1555: 0xFFFF = white opaque, 0x0000 = transparent */
                uint16_t pixel = (line & (0x80 >> col)) ? 0xFFFF : 0x0000;
                for (sy = 0; sy < FONT_SCALE; sy++)
                {
                    for (sx = 0; sx < FONT_SCALE; sx++)
                    {
                        x = FN_X + i * CHAR_W + col * FONT_SCALE + sx;
                        y = FN_Y + row * FONT_SCALE + sy;
                        if (x < 800 && y < 480)
                            ui_buf[y * UI_BUF_STRIDE + x] = pixel;
                    }
                }
            }
        }
    }
}

/*---------------------------------------------------------------------------
 * Timer callback (50ms = 20fps)
 *---------------------------------------------------------------------------
 */
void AnimTimerCallback(void *argument)
{
    (void)argument;
    if (num_frames > 0)
    {
        frame_index = (frame_index + 1) % num_frames;
    }
    osSemaphoreRelease(anim_sem_id);
}

/*---------------------------------------------------------------------------
 * Animation task
 *---------------------------------------------------------------------------
 */
void StartAnimationTask(void *argument)
{
    (void)argument;

    /* Give defaultTask time to finish init (LCD, USART, QSPI, etc.) */
    osDelay(500);

    /* Create binary semaphore: max count = 1, initial count = 0 */
    anim_sem_id = osSemaphoreNew(1, 0, NULL);

    /* Clear both framebuffers to black */
    LCD_Clear(LCD_COLOR_BLACK);

    /* Clear back buffer using DMA2D R2M */
    hdma2d.Init.Mode         = DMA2D_R2M;
    hdma2d.Init.ColorMode    = DMA2D_OUTPUT_RGB565;
    hdma2d.Init.OutputOffset = 0;
    hdma2d.Init.AlphaInverted = DMA2D_REGULAR_ALPHA;
    hdma2d.Init.RedBlueSwap   = DMA2D_RB_REGULAR;
    HAL_DMA2D_Init(&hdma2d);
    HAL_DMA2D_Start(&hdma2d, 0x00000000, FB_BACK, LCD_FB_STRIDE, 480);
    HAL_DMA2D_PollForTransfer(&hdma2d, 10);

    /* Write header + frame table to QSPI Flash (first boot only, uses direct QSPI commands) */
    if (QSPI_Video_WriteToFlash() != HAL_OK)
    {
        printf("ANIM: QSPI header write failed\r\n");
        osDelay(1000);
        NVIC_SystemReset();
    }

    /* Check if frame data is present (direct QSPI read, no mmap needed) */
    if (!QSPI_Video_IsDataProgrammed())
    {
        printf("ANIM: Frame data not found, entering programming mode...\r\n");
        if (QSPI_Video_ProgramFrameData() != HAL_OK)
        {
            printf("ANIM: Frame data programming failed\r\n");
            osDelay(1000);
            NVIC_SystemReset();
        }
        printf("ANIM: Frame data programmed, resetting...\r\n");
        osDelay(500);
        NVIC_SystemReset();
    }

    /* Enable QSPI memory-mapped mode for DMA2D direct access */
    if (QSPI_EnableMemoryMappedMode() != HAL_OK)
    {
        printf("ANIM: QSPI mmap failed\r\n");
        osDelay(1000);
        NVIC_SystemReset();
    }

    /* Get number of frames from QSPI Flash (now mmap is active) */
    num_frames = QSPI_Video_GetFrameCount();

    /* Enable DWT cycle counter for profiling */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    printf("Animation Phase 6: Raw RGB565 QSPI, %lu frames, %dx%d at %dfps\r\n",
           num_frames, ANIM_FRAME_WIDTH, ANIM_FRAME_HEIGHT, ANIM_FPS);

    /* Pre-render static overlays (bottom_bar, clean_text) to LTDC Layer 2 UI buffer
     * This is done once at startup. Layer 2 blends hardware over Layer 1 animation.
     */
    PreRenderUI();

    for (;;)
    {
        /* Wait for 50ms timer to signal next frame */
        osSemaphoreAcquire(anim_sem_id, osWaitForever);

        uint32_t t0 = DWT->CYCCNT;

        /* Get QSPI memory-mapped address for this frame */
        uint32_t src_addr = QSPI_Video_GetFrameAddr(frame_index);
        if (src_addr == 0)
        {
            printf("ANIM: Invalid frame address for frame %d\r\n", frame_index);
            continue;
        }

        uint32_t t1 = DWT->CYCCNT;

        /* Determine the inactive framebuffer */
        uint32_t fb_dst = (fb_active == FB_FRONT) ? FB_BACK : FB_FRONT;

        /* CPU rotate + copy animation frame (CopyFrame adds ANIM_DST_X/Y internally) */
        CopyFrame((const uint16_t *)src_addr, (uint16_t *)fb_dst);
        uint32_t t2 = DWT->CYCCNT;

        /* Draw frame number on LTDC Layer 2 UI buffer (ARGB1555, transparent bg) */
        DrawFrameNumberUI(frame_index);

        /* Swap LTDC Layer 1 (animation) to the inactive buffer */
        SwapFramebuffer(fb_dst);
        uint32_t t3 = DWT->CYCCNT;

        /* Print timing every 20 frames (HCLK=200MHz → 1 tick = 5ns) */
        if ((frame_index % 20) == 0)
        {
            printf("  T:%d addr=%luus CPUrot=%luus swap=%luus\r\n",
                   frame_index,
                   (t1 - t0) / 200,
                   (t2 - t1) / 200,
                   (t3 - t2) / 200);
        }
    }
}