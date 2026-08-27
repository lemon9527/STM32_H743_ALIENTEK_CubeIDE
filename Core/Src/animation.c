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
#include "gpio.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <string.h>
#include "stm32h7xx.h"
#include "uart_protocol.h"

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

/* Background colour used to fill Layer 1 framebuffers when switching to
 * Brightness or Animation page (RGB565). 0x0000 = black.
 */
#define BG_COLOR_RGB565 0x0000U

/* Current page state (default: animation page) */
volatile PageState_t current_page = PAGE_ANIMATION;

/* Previous button states for edge detection */
static uint8_t prev_key_up = 0;    /* KEY_UP: active HIGH */
static uint8_t prev_key_down = 0;  /* KEY_DOWN: active LOW */

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
 * Clear LTDC Layer 2 UI buffer to transparent (ARGB1555=0x0000)
 *---------------------------------------------------------------------------
 */
static void ClearLayer2_Transparent(void)
{
    uint16_t *ui_buf = (uint16_t *)UI_BUF_ADDR;
    for (uint32_t i = 0; i < 800 * 480; i++)
    {
        ui_buf[i] = 0x0000;
    }
    /* Clean D-Cache so LTDC reads the cleared (transparent) data from SDRAM.
     * Without this, LTDC sees stale SDRAM content from the previous page,
     * which can cause a black background when stale opaque pixels block
     * the white animation layer.
     */
    SCB_CleanDCache_by_Addr((uint32_t *)UI_BUF_ADDR, 800 * 480 * sizeof(uint16_t));
}

/*---------------------------------------------------------------------------
 * Fill LTDC Layer 2 UI buffer to opaque black (ARGB1555=0x8000)
 * Uses DMA2D R2M for fast fill. Two critical requirements:
 *   1) Color value must be ARGB8888 format with bit31=1 (alpha=255):
 *      0xFF000000 → correctly produces ARGB1555 0x8000 (opaque black)
 *      0x00008000 → bit31=0, alpha=0 → transparent fill!
 *   2) Local handle State must be HAL_DMA2D_STATE_RESET before Init,
 *      otherwise HAL_DMA2D_Init may silently skip configuration.
 *   3) HAL_DMA2D_Abort() must be called before reconfiguring to ensure
 *      DMA2D is in a clean state (not busy from a previous M2M transfer).
 *---------------------------------------------------------------------------
 */
static void FillLayer2_OpaqueBlack(void)
{
    HAL_DMA2D_Abort(&hdma2d);

    DMA2D_HandleTypeDef hdma2d_local = {0};
    hdma2d_local.Instance              = DMA2D;
    hdma2d_local.State                 = HAL_DMA2D_STATE_RESET;  /* must be RESET for Init to work */
    hdma2d_local.Init.Mode             = DMA2D_R2M;
    hdma2d_local.Init.ColorMode        = DMA2D_OUTPUT_ARGB1555;
    hdma2d_local.Init.OutputOffset     = 0;
    hdma2d_local.Init.AlphaInverted    = DMA2D_REGULAR_ALPHA;
    hdma2d_local.Init.RedBlueSwap      = DMA2D_RB_REGULAR;
    HAL_DMA2D_Init(&hdma2d_local);

    /* ARGB8888 opaque black = 0xFF000000 (Alpha=0xFF, R=0, G=0, B=0)
     * HAL DMA2D_SetConfig extracts alpha from bit31, so bit31=1 → alpha=255.
     */
    HAL_DMA2D_Start(&hdma2d_local, 0xFF000000, UI_BUF_ADDR, UI_BUF_STRIDE, 480);
    HAL_DMA2D_PollForTransfer(&hdma2d_local, 100);

    /*---------------------------------------------------------------------------
     * Invalidate D-Cache for Layer 2 buffer after DMA2D write.
     *
     * CRITICAL: During pre-creation of LVGL screens in StartAnimationTask(),
     * the CPU writes LVGL pixels to Layer 2 via the flush callback. These
     * writes go to D-Cache, NOT to SDRAM directly.
     *
     * When FillLayer2_OpaqueBlack() runs later (on page switch), DMA2D fills
     * Layer 2 directly in SDRAM, bypassing D-Cache. The D-Cache now contains
     * STALE data from the pre-creation phase.
     *
     * If the subsequent lv_refr_now(NULL) CPU writes trigger D-Cache evictions,
     * the stale cached data (old LVGL pixels) would be written back to SDRAM,
     * OVERWRITING the DMA2D fill. This causes visible white/colored pixels
     * appearing as a diagonal scroll on the first page switch.
     *
     * InvalidateDCache discards the stale cache lines, forcing the CPU to
     * read from SDRAM (which has the correct DMA2D fill data).
     *---------------------------------------------------------------------------
     */
    SCB_InvalidateDCache_by_Addr((uint32_t *)UI_BUF_ADDR, 800 * 480 * sizeof(uint16_t));
}

#include "fonts.h"

/*---------------------------------------------------------------------------
 * Pre-render static overlay elements to LTDC Layer 2 UI buffer
 * Called once at startup. Draws bottom bar icons + clean_text on UI buffer.
 * Layer 2 uses ARGB1555 format: alpha=1 for overlay, alpha=0 for transparent.
 *---------------------------------------------------------------------------
 */
static void DrawTextLineCenter(uint16_t log_y, const char *text, uint16_t color);
static void DrawTextAtCenter(uint16_t center_x, uint16_t log_y,
                             const char *text, const lv_font_t *font,
                             uint16_t color);
static void ClearNumberArea(int center_x, int num_top, int h, int w);

/*---------------------------------------------------------------------------
 * Pre-render static overlay elements to LTDC Layer 2 UI buffer
 * Called once at startup. Draws bottom bar icons + clean_text on UI buffer.
 * Layer 2 uses ARGB1555 format: alpha=1 for overlay, alpha=0 for transparent.
 *---------------------------------------------------------------------------
 */
static void PreRenderUI(void)
{
    uint16_t *ui_buf = (uint16_t *)UI_BUF_ADDR;

    /*---------------------------------------------------------------------------
     * Clean text: 140x116 RGB565, 90 deg CCW rotation, full image (with black bg)
     *---------------------------------------------------------------------------
     */
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
     * Bottom text: two lines of sensor labels inside the 320x480 rectangle.
     *   Line 1: "PM 2.5" / "TVOC" / "CO2" — inter_regular_17, white
     *     PM2.5 center: x = 80+66 = 146, top: y = 480+16 = 496
     *     TVOC center:  x = 240 (middle of 320px rect), top: y = 496
     *     CO2 center:   x = 400-69 = 331, top: y = 496
     *   Line 3: "μg/m³" / "ppb" / "ppm" — inter_regular_17, white
     *     Same horizontal centers as line 1.
     *     Text center: y = 640-36 = 604 → top = 604 - line_height/2
     *---------------------------------------------------------------------------
     */
    {
        const lv_font_t *f = &inter_regular_17;
        uint16_t col = 0xD294; /* ARGB1555: ~0xA7A7A7 (matches Metrics1 style) */
        int lh = lv_font_get_line_height(f);

        /* Line 1: PM 2.5, TVOC, CO2 */
        DrawTextAtCenter(146, 496, "PM 2.5", f, col);
        DrawTextAtCenter(240, 496, "TVOC",   f, col);
        DrawTextAtCenter(331, 496, "CO2",    f, col);

        /* Line 2: sensor values (between Line 1 and Line 3) — inter_bold_32, white
         * Vertically centered: midpoint of Line 1 bottom and Line 3 top.
         *   Line 1 bottom = 496 + lh
         *   Line 3 top    = 604 - lh/2
         *   midpoint = (496 + lh + 604 - lh/2) / 2 = 550 + lh/4
         *   number top = midpoint - lh_bold32 / 2
         */
        {
            const lv_font_t *f32 = &inter_bold_32;
            int lh_bold32 = lv_font_get_line_height(f32);
            int num_top = 550 + lh / 4 - lh_bold32 / 2;
            uint16_t white = 0xFFFF; /* ARGB1555 white */

            char buf[16];
            sensor_data_t data;
            uart_protocol_get_data(&data);

            /* Use default values when data is not yet received (startup) */
            unsigned pm2_5 = uart_protocol_has_data() ? (unsigned)data.pm2_5 : 12U;
            unsigned tvoc  = uart_protocol_has_data() ? (unsigned)data.tvoc  : 115U;
            unsigned co2   = uart_protocol_has_data() ? (unsigned)data.co2   : 500U;

            /* Clear entire number area before drawing to remove any previous
             * digit remnants (even when the new number is shorter than the old).
             * 100px width is enough for 3-digit numbers and safe from blue border. */
            ClearNumberArea(146, num_top, lh_bold32, 100);
            snprintf(buf, sizeof(buf), "%u", pm2_5);
            DrawTextAtCenter(146, num_top, buf, f32, white);

            ClearNumberArea(240, num_top, lh_bold32, 100);
            snprintf(buf, sizeof(buf), "%u", tvoc);
            DrawTextAtCenter(240, num_top, buf, f32, white);

            ClearNumberArea(331, num_top, lh_bold32, 100);
            snprintf(buf, sizeof(buf), "%u", co2);
            DrawTextAtCenter(331, num_top, buf, f32, white);
        }

        /* Line 3: μg/m³, ppb, ppm (centered vertically at y=604) */
        int line3_top = 604 - lh / 2;
        DrawTextAtCenter(146, line3_top, "μg/m³", f, col);
        DrawTextAtCenter(240, line3_top, "ppb",   f, col);
        DrawTextAtCenter(331, line3_top, "ppm",   f, col);
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

    /*---------------------------------------------------------------------------
     * Clean D-Cache for the Layer 2 UI buffer so LTDC reads the freshly
     * rendered data from SDRAM. Without this, the CPU's write-back cache
     * holds the data and LTDC (reading from SDRAM via AHB) sees stale data.
     *---------------------------------------------------------------------------
     */
    SCB_CleanDCache_by_Addr((uint32_t *)UI_BUF_ADDR, 800 * 480 * sizeof(uint16_t));
}

/*---------------------------------------------------------------------------
 * Helper: clear a horizontal strip in the Layer 2 UI buffer (with 90° CCW
 * rotation) to transparent. Used to erase entire number areas before
 * redrawing, preventing overlap when number length changes.
 *   center_x: logical x center of the area to clear
 *   num_top:  logical y top of the area
 *   h:        height of the area (line height of the font)
 *   w:        width of the area (must be wide enough for max digit count)
 *---------------------------------------------------------------------------
 */
static void ClearNumberArea(int center_x, int num_top, int h, int w)
{
    uint16_t *ui_buf = (uint16_t *)UI_BUF_ADDR;
    int half = w / 2;
    for (int row = 0; row < h; row++)
    {
        int ly = num_top + row;
        for (int col = -half; col < half; col++)
        {
            int lx = center_x + col;
            int px = ly;
            int py = 479 - lx;
            if (px >= 0 && px < UI_BUF_STRIDE && py >= 0 && py < LCD_HEIGHT)
                ui_buf[py * UI_BUF_STRIDE + px] = 0x0000;
        }
    }
}

/*---------------------------------------------------------------------------
 * Update sensor numbers on the animation page (called periodically from
 * StartDefaultTask when new UART data arrives).
 * Only re-renders the three number fields (PM2.5, TVOC, CO2) in the
 * bottom area, keeping all other Layer 2 UI content intact.
 * Must clean D-Cache after writing so LTDC sees the updated data.
 *---------------------------------------------------------------------------
 */
void UpdateAnimationSensorNumbers(void)
{
    /* Only update when animation page is active */
    if (current_page != PAGE_ANIMATION)
        return;

    /* Recalculate number position (same logic as PreRenderUI) */
    const lv_font_t *f = &inter_regular_17;
    int lh = lv_font_get_line_height(f);
    const lv_font_t *f32 = &inter_bold_32;
    int lh_bold32 = lv_font_get_line_height(f32);
    int num_top = 550 + lh / 4 - lh_bold32 / 2;
    uint16_t white = 0xFFFF; /* ARGB1555 white */

    /* Clear each number area before drawing to erase any previous digit
     * remnants. 100px width is enough for 3-digit numbers and safe from
     * the blue border. */
    ClearNumberArea(146, num_top, lh_bold32, 100);
    ClearNumberArea(240, num_top, lh_bold32, 100);
    ClearNumberArea(331, num_top, lh_bold32, 100);

    char buf[16];
    sensor_data_t data;
    uart_protocol_get_data(&data);

    snprintf(buf, sizeof(buf), "%u", (unsigned)data.pm2_5);
    DrawTextAtCenter(146, num_top, buf, f32, white);

    snprintf(buf, sizeof(buf), "%u", (unsigned)data.tvoc);
    DrawTextAtCenter(240, num_top, buf, f32, white);

    snprintf(buf, sizeof(buf), "%u", (unsigned)data.co2);
    DrawTextAtCenter(331, num_top, buf, f32, white);

    /* Clean D-Cache so LTDC reads the updated numbers from SDRAM */
    SCB_CleanDCache_by_Addr((uint32_t *)UI_BUF_ADDR, 800 * 480 * sizeof(uint16_t));
}

/* Font metrics for frame number (8x16 bitmap, 2x scaled) */
#define FONT_SCALE   2
#define CHAR_W       (8 * FONT_SCALE)   /* 16 px */

/* Use LVGL Montserrat 24 font — anti-aliased, much better than 8x16 bitmap */
#include "src/font/lv_font.h"
LV_FONT_DECLARE(lv_font_montserrat_24);
#define LV_FONT (&lv_font_montserrat_24)

#include "lv_demo.h"

/* LVGL screen references for page switching (create once, load on switch) */
/* LVGL screen references for page switching (create once, load on switch) */
static lv_obj_t *brightness_screen = NULL;
static lv_obj_t *filter_screen = NULL;
static lv_obj_t *metrics1_screen = NULL;
static lv_obj_t *metrics2_screen = NULL;

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

                if (px >= 0 && px < UI_BUF_STRIDE && py >= 0 && py < LCD_HEIGHT)
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
 * Draw a number string centered at a specific logical x position.
 * Uses Montserrat 32 font (anti-aliased) for the bottom bar sensor values.
 *   center_x: logical x of the number's center
 *   log_y:    logical y position (top of the text line)
 *   color:    ARGB1555 color (must have A=1 bit set, e.g. 0xFFFF for white)
 *---------------------------------------------------------------------------
 */

/*---------------------------------------------------------------------------
 * Draw a text string centered at a specific logical x position.
 * Accepts any LVGL font. Supports UTF-8 (handles multi-byte characters like μ).
 *   center_x: logical x of the text's center (portrait coordinate)
 *   log_y:    logical y position (top of the text line)
 *   font:     LVGL font to use (e.g. &inter_regular_18)
 *   color:    ARGB1555 color (must have A=1 bit set, e.g. 0xFFFF for white)
 *---------------------------------------------------------------------------
 */
static void DrawTextAtCenter(uint16_t center_x, uint16_t log_y,
                             const char *text, const lv_font_t *font,
                             uint16_t color)
{
    uint16_t *ui_buf = (uint16_t *)UI_BUF_ADDR;
    const uint8_t *p = (const uint8_t *)text;

    /* First pass: calculate total width (UTF-8 aware) */
    int total_w = 0;
    while (*p)
    {
        uint32_t c = 0;
        int len = 1;

        /* UTF-8 decode: 1-byte (0xxxxxxx), 2-byte (110xxxxx 10xxxxxx), etc. */
        if ((*p & 0xF0) == 0xF0) {
            /* 4-byte UTF-8 */
            c = ((uint32_t)(*p & 0x07) << 18) |
                ((uint32_t)(*(p+1) & 0x3F) << 12) |
                ((uint32_t)(*(p+2) & 0x3F) << 6) |
                ((uint32_t)(*(p+3) & 0x3F));
            len = 4;
        } else if ((*p & 0xE0) == 0xE0) {
            /* 3-byte UTF-8 */
            c = ((uint32_t)(*p & 0x0F) << 12) |
                ((uint32_t)(*(p+1) & 0x3F) << 6) |
                ((uint32_t)(*(p+2) & 0x3F));
            len = 3;
        } else if ((*p & 0xC0) == 0xC0) {
            /* 2-byte UTF-8 (covers μ=0xC2 0xB5, characters up to U+07FF) */
            c = ((uint32_t)(*p & 0x1F) << 6) |
                ((uint32_t)(*(p+1) & 0x3F));
            len = 2;
        } else {
            /* 1-byte ASCII */
            c = (uint32_t)*p;
            len = 1;
        }

        lv_font_glyph_dsc_t dsc;
        if (lv_font_get_glyph_dsc(font, &dsc, c, 0))
            total_w += dsc.adv_w;
        else
            total_w += font->line_height / 2;

        p += len;
    }

    /* Center at the given position */
    int cursor_x = (int)center_x - total_w / 2;

    /* Second pass: render each glyph (UTF-8 decode again) */
    p = (const uint8_t *)text;
    while (*p)
    {
        uint32_t c = 0;
        int len = 1;

        if ((*p & 0xF0) == 0xF0) {
            c = ((uint32_t)(*p & 0x07) << 18) |
                ((uint32_t)(*(p+1) & 0x3F) << 12) |
                ((uint32_t)(*(p+2) & 0x3F) << 6) |
                ((uint32_t)(*(p+3) & 0x3F));
            len = 4;
        } else if ((*p & 0xE0) == 0xE0) {
            c = ((uint32_t)(*p & 0x0F) << 12) |
                ((uint32_t)(*(p+1) & 0x3F) << 6) |
                ((uint32_t)(*(p+2) & 0x3F));
            len = 3;
        } else if ((*p & 0xC0) == 0xC0) {
            c = ((uint32_t)(*p & 0x1F) << 6) |
                ((uint32_t)(*(p+1) & 0x3F));
            len = 2;
        } else {
            c = (uint32_t)*p;
            len = 1;
        }

        lv_font_glyph_dsc_t dsc;
        if (!lv_font_get_glyph_dsc(font, &dsc, c, 0))
        {
            cursor_x += font->line_height / 2;
            p += len;
            continue;
        }

        const uint8_t *bmp = lv_font_get_glyph_bitmap(font, c);
        if (!bmp) {
            /* Space or other non-bitmap glyph: advance cursor by adv_w */
            cursor_x += dsc.adv_w;
            p += len;
            continue;
        }

        int bpp = dsc.bpp;
        int gpos_y = log_y + (font->line_height - font->base_line) - dsc.box_h - dsc.ofs_y;

        /* Clear the entire glyph box to transparent (0x0000) before drawing.
         * This erases any previous glyph remnants at the exact glyph position,
         * preventing flicker (no separate clear pass) and avoiding accidental
         * erasure of nearby UI elements like the blue border. */
        for (int row = 0; row < dsc.box_h; row++)
        {
            for (int col = 0; col < dsc.box_w; col++)
            {
                int lx = cursor_x + dsc.ofs_x + col;
                int ly = gpos_y + row;
                int px = ly;
                int py = 479 - lx;
                if (px >= 0 && px < UI_BUF_STRIDE && py >= 0 && py < LCD_HEIGHT)
                    ui_buf[py * UI_BUF_STRIDE + px] = 0x0000;
            }
        }

        /* Now draw the glyph pixels (non-transparent only) */
        for (int row = 0; row < dsc.box_h; row++)
        {
            for (int col = 0; col < dsc.box_w; col++)
            {
                int pixel_idx = row * dsc.box_w + col;
                uint8_t pixel_val;

                if (bpp == 1)
                    pixel_val = (bmp[pixel_idx / 8] >> (7 - (pixel_idx % 8))) & 1;
                else if (bpp == 2)
                    pixel_val = (bmp[pixel_idx / 4] >> (6 - 2 * (pixel_idx % 4))) & 3;
                else if (bpp == 4)
                    pixel_val = (bmp[pixel_idx / 2] >> (4 * (1 - (pixel_idx % 2)))) & 0x0F;
                else
                    pixel_val = bmp[pixel_idx];

                if (pixel_val == 0) continue;

                int lx = cursor_x + dsc.ofs_x + col;
                int ly = gpos_y + row;

                /* 90° CCW rotation: physical (px, py) = (ly, 479 - lx) */
                int px = ly;
                int py = 479 - lx;

                if (px >= 0 && px < UI_BUF_STRIDE && py >= 0 && py < LCD_HEIGHT)
                {
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
        p += len;
    }
}

/*---------------------------------------------------------------------------
 * Switch back to animation page: restore UI overlays on Layer 2
 *---------------------------------------------------------------------------
 */
static void SwitchToAnimationPage(void)
{
    /* STEP 1: Fill both animation framebuffers to black FIRST.
     * This ensures the background is already black before any UI elements
     * appear on Layer 2. Doing this after PreRenderUI causes a visible
     * flicker: white background + UI elements → background turns black.
     */
    hdma2d.Init.Mode         = DMA2D_R2M;
    hdma2d.Init.ColorMode    = DMA2D_OUTPUT_RGB565;
    hdma2d.Init.OutputOffset = 0;
    hdma2d.Init.AlphaInverted = DMA2D_REGULAR_ALPHA;
    hdma2d.Init.RedBlueSwap   = DMA2D_RB_REGULAR;
    HAL_DMA2D_Init(&hdma2d);

    /* Clear back buffer via DMA2D R2M */
    HAL_DMA2D_Abort(&hdma2d);
    HAL_DMA2D_Start(&hdma2d, 0x00000000, FB_BACK, LCD_FB_STRIDE, 480);
    HAL_DMA2D_PollForTransfer(&hdma2d, 100);
    SCB_CleanDCache_by_Addr((uint32_t *)FB_BACK, LCD_FB_STRIDE * LCD_HEIGHT * sizeof(uint16_t));

    /* Clear front buffer via DMA2D R2M */
    HAL_DMA2D_Abort(&hdma2d);
    HAL_DMA2D_Start(&hdma2d, 0x00000000, FB_FRONT, LCD_FB_STRIDE, 480);
    HAL_DMA2D_PollForTransfer(&hdma2d, 100);
    SCB_CleanDCache_by_Addr((uint32_t *)FB_FRONT, LCD_FB_STRIDE * LCD_HEIGHT * sizeof(uint16_t));

    /* Ensure LTDC uses the front buffer and update tracking variable */
    LTDC_Layer1->CFBAR = FB_FRONT;
    fb_active = FB_FRONT;

    /* STEP 2: Now clear Layer 2 to transparent and re-render UI overlays.
     * The background is already black, so UI elements will appear on a
     * dark background with no flicker.
     */
    ClearLayer2_Transparent();
    PreRenderUI();

    printf("PAGE: switched to Animation (buffers cleared)\r\n");
    current_page = PAGE_ANIMATION;
}

/*---------------------------------------------------------------------------
 * Switch to Brightness page
 *---------------------------------------------------------------------------
 */
static void SwitchToBrightnessPage(void)
{
    /*---------------------------------------------------------------------------
     * Pre-fill Layer 2 with opaque black before
     * loading the LVGL screen, preventing the band-by-band background paint
     * flash. All three pages use black backgrounds, so no white flash.
     *---------------------------------------------------------------------------
     */
    FillLayer2_OpaqueBlack();

    /* Fill both animation framebuffers (Layer 1) with black. Since Layer 2 is
     * already filled with opaque black, the content of Layer 1 is completely
     * hidden. We fill Layer 1 to black for consistency and to avoid stale
     * animation pixels in case of future layer reconfiguration.
     */
    hdma2d.Init.Mode         = DMA2D_R2M;
    hdma2d.Init.ColorMode    = DMA2D_OUTPUT_RGB565;
    hdma2d.Init.OutputOffset = 0;
    hdma2d.Init.AlphaInverted = DMA2D_REGULAR_ALPHA;
    hdma2d.Init.RedBlueSwap   = DMA2D_RB_REGULAR;
    HAL_DMA2D_Init(&hdma2d);

    uint32_t inactive_fb = (fb_active == FB_FRONT) ? FB_BACK : FB_FRONT;
    uint32_t other_fb = (inactive_fb == FB_FRONT) ? FB_BACK : FB_FRONT;

    HAL_DMA2D_Abort(&hdma2d);
    HAL_DMA2D_Start(&hdma2d, BG_COLOR_RGB565, inactive_fb, LCD_FB_STRIDE, 480);
    HAL_DMA2D_PollForTransfer(&hdma2d, 100);
    SCB_CleanDCache_by_Addr((uint32_t *)inactive_fb, LCD_FB_STRIDE * LCD_HEIGHT * sizeof(uint16_t));

    LTDC_Layer1->CFBAR = inactive_fb;
    fb_active = inactive_fb;

    HAL_DMA2D_Abort(&hdma2d);
    HAL_DMA2D_Start(&hdma2d, BG_COLOR_RGB565, other_fb, LCD_FB_STRIDE, 480);
    HAL_DMA2D_PollForTransfer(&hdma2d, 100);
    SCB_CleanDCache_by_Addr((uint32_t *)other_fb, LCD_FB_STRIDE * LCD_HEIGHT * sizeof(uint16_t));

    /* Set up Brightness LVGL screen (create once, load on switch)
     * Same approach as Animation page: no Layer 2 disable/enable,
     * no lv_refr_now(NULL), let the LVGL task handler render naturally.
     */
    if (lv_is_initialized()) {
        if (brightness_screen == NULL) {
            brightness_screen = lv_obj_create(NULL);
            lvgl_brightness_create(brightness_screen);
        }
        lv_scr_load(brightness_screen);
        /* Critical invalidation: without this, lv_scr_load() may return early
         * if the screen is already active, and the LVGL task handler won't
         * re-render.
         */
        lv_obj_invalidate(brightness_screen);
    }

    printf("PAGE: switched to Brightness\r\n");
    current_page = PAGE_BRIGHTNESS;
}

/*---------------------------------------------------------------------------
 * Switch to Filter page
 *--------------------------------------------------------------------------*/
static void SwitchToFilterPage(void)
{
    FillLayer2_OpaqueBlack();

    hdma2d.Init.Mode         = DMA2D_R2M;
    hdma2d.Init.ColorMode    = DMA2D_OUTPUT_RGB565;
    hdma2d.Init.OutputOffset = 0;
    hdma2d.Init.AlphaInverted = DMA2D_REGULAR_ALPHA;
    hdma2d.Init.RedBlueSwap   = DMA2D_RB_REGULAR;
    HAL_DMA2D_Init(&hdma2d);

    uint32_t inactive_fb = (fb_active == FB_FRONT) ? FB_BACK : FB_FRONT;
    uint32_t other_fb = (inactive_fb == FB_FRONT) ? FB_BACK : FB_FRONT;

    HAL_DMA2D_Abort(&hdma2d);
    HAL_DMA2D_Start(&hdma2d, BG_COLOR_RGB565, inactive_fb, LCD_FB_STRIDE, 480);
    HAL_DMA2D_PollForTransfer(&hdma2d, 100);
    SCB_CleanDCache_by_Addr((uint32_t *)inactive_fb, LCD_FB_STRIDE * LCD_HEIGHT * sizeof(uint16_t));

    LTDC_Layer1->CFBAR = inactive_fb;
    fb_active = inactive_fb;

    HAL_DMA2D_Abort(&hdma2d);
    HAL_DMA2D_Start(&hdma2d, BG_COLOR_RGB565, other_fb, LCD_FB_STRIDE, 480);
    HAL_DMA2D_PollForTransfer(&hdma2d, 100);
    SCB_CleanDCache_by_Addr((uint32_t *)other_fb, LCD_FB_STRIDE * LCD_HEIGHT * sizeof(uint16_t));

    if (lv_is_initialized()) {
        if (filter_screen == NULL) {
            filter_screen = lv_obj_create(NULL);
            lvgl_filter_create(filter_screen);
        }
        lv_scr_load(filter_screen);
        lv_obj_invalidate(filter_screen);
        /* Force immediate LVGL refresh and sync D-Cache so DMA2D/LTDC see pixels */
        lv_refr_now(NULL);
        SCB_CleanDCache_by_Addr((uint32_t *)UI_BUF_ADDR, 800 * 480 * sizeof(uint16_t));
    }

    printf("PAGE: switched to Filter\r\n");
    current_page = PAGE_FILTER;
}

/*---------------------------------------------------------------------------
 * Switch to Metrics page (Main Screen secondary)
 *--------------------------------------------------------------------------*/
static void SwitchToMetrics1Page(void)
{
    FillLayer2_OpaqueBlack();

    hdma2d.Init.Mode         = DMA2D_R2M;
    hdma2d.Init.ColorMode    = DMA2D_OUTPUT_RGB565;
    hdma2d.Init.OutputOffset = 0;
    hdma2d.Init.AlphaInverted = DMA2D_REGULAR_ALPHA;
    hdma2d.Init.RedBlueSwap   = DMA2D_RB_REGULAR;
    HAL_DMA2D_Init(&hdma2d);

    uint32_t inactive_fb = (fb_active == FB_FRONT) ? FB_BACK : FB_FRONT;
    uint32_t other_fb = (inactive_fb == FB_FRONT) ? FB_BACK : FB_FRONT;

    HAL_DMA2D_Abort(&hdma2d);
    HAL_DMA2D_Start(&hdma2d, BG_COLOR_RGB565, inactive_fb, LCD_FB_STRIDE, 480);
    HAL_DMA2D_PollForTransfer(&hdma2d, 100);
    SCB_CleanDCache_by_Addr((uint32_t *)inactive_fb, LCD_FB_STRIDE * LCD_HEIGHT * sizeof(uint16_t));

    LTDC_Layer1->CFBAR = inactive_fb;
    fb_active = inactive_fb;

    HAL_DMA2D_Abort(&hdma2d);
    HAL_DMA2D_Start(&hdma2d, BG_COLOR_RGB565, other_fb, LCD_FB_STRIDE, 480);
    HAL_DMA2D_PollForTransfer(&hdma2d, 100);
    SCB_CleanDCache_by_Addr((uint32_t *)other_fb, LCD_FB_STRIDE * LCD_HEIGHT * sizeof(uint16_t));

    if (lv_is_initialized()) {
        if (metrics1_screen == NULL) {
            metrics1_screen = lv_obj_create(NULL);
            lvgl_metrics1_create(metrics1_screen);
        }
        lv_scr_load(metrics1_screen);
        lv_obj_invalidate(metrics1_screen);
        lv_refr_now(NULL);
        SCB_CleanDCache_by_Addr((uint32_t *)UI_BUF_ADDR, 800 * 480 * sizeof(uint16_t));
    }

    printf("PAGE: switched to Metrics1\r\n");
    current_page = PAGE_METRICS1;
}

/*---------------------------------------------------------------------------
 * Switch to Metrics2 page
 *--------------------------------------------------------------------------*/
static void SwitchToMetrics2Page(void)
{
    FillLayer2_OpaqueBlack();

    hdma2d.Init.Mode         = DMA2D_R2M;
    hdma2d.Init.ColorMode    = DMA2D_OUTPUT_RGB565;
    hdma2d.Init.OutputOffset = 0;
    hdma2d.Init.AlphaInverted = DMA2D_REGULAR_ALPHA;
    hdma2d.Init.RedBlueSwap   = DMA2D_RB_REGULAR;
    HAL_DMA2D_Init(&hdma2d);

    uint32_t inactive_fb = (fb_active == FB_FRONT) ? FB_BACK : FB_FRONT;
    uint32_t other_fb = (inactive_fb == FB_FRONT) ? FB_BACK : FB_FRONT;

    HAL_DMA2D_Abort(&hdma2d);
    HAL_DMA2D_Start(&hdma2d, BG_COLOR_RGB565, inactive_fb, LCD_FB_STRIDE, 480);
    HAL_DMA2D_PollForTransfer(&hdma2d, 100);
    SCB_CleanDCache_by_Addr((uint32_t *)inactive_fb, LCD_FB_STRIDE * LCD_HEIGHT * sizeof(uint16_t));

    LTDC_Layer1->CFBAR = inactive_fb;
    fb_active = inactive_fb;

    HAL_DMA2D_Abort(&hdma2d);
    HAL_DMA2D_Start(&hdma2d, BG_COLOR_RGB565, other_fb, LCD_FB_STRIDE, 480);
    HAL_DMA2D_PollForTransfer(&hdma2d, 100);
    SCB_CleanDCache_by_Addr((uint32_t *)other_fb, LCD_FB_STRIDE * LCD_HEIGHT * sizeof(uint16_t));

    if (lv_is_initialized()) {
        if (metrics2_screen == NULL) {
            metrics2_screen = lv_obj_create(NULL);
            lvgl_metrics2_create(metrics2_screen);
        }
        lv_scr_load(metrics2_screen);
        lv_obj_invalidate(metrics2_screen);
        lv_refr_now(NULL);
        SCB_CleanDCache_by_Addr((uint32_t *)UI_BUF_ADDR, 800 * 480 * sizeof(uint16_t));
    }

    printf("PAGE: switched to Metrics2\r\n");
    current_page = PAGE_METRICS2;
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
 * Page switch request mechanism
 *
 * The AnimationTask handles physical key presses but must NOT call LVGL
 * functions directly (SwitchTo*), because the default task also calls
 * lv_task_handler() every 5ms. Both tasks accessing LVGL state causes
 * race conditions leading to HardFault.
 *
 * Instead, AnimationTask sets g_requested_page + g_page_switch_pending,
 * and the default task calls ProcessPageSwitch() to execute the actual
 * switch (where LVGL operations are safe).
 *---------------------------------------------------------------------------
 */
volatile PageState_t g_requested_page = PAGE_ANIMATION;
volatile uint8_t     g_page_switch_pending = 0;

void ProcessPageSwitch(void)
{
    if (!g_page_switch_pending)
        return;
    g_page_switch_pending = 0;

    /* Classify the current page to determine which switch function to call.
     * We use the same logic as the AnimationTask key handler. */
    PageState_t target = g_requested_page;

    /* Handle the case where the request is a main-page cycle */
    if (target == PAGE_ANIMATION)       SwitchToAnimationPage();
    else if (target == PAGE_FILTER)     SwitchToFilterPage();
    else if (target == PAGE_BRIGHTNESS) SwitchToBrightnessPage();
    else if (target == PAGE_METRICS1)   SwitchToMetrics1Page();
    else if (target == PAGE_METRICS2)   SwitchToMetrics2Page();
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
    /* Clean D-cache for the cleared back buffer */
    SCB_CleanDCache_by_Addr((uint32_t *)FB_BACK, LCD_FB_STRIDE * LCD_HEIGHT * sizeof(uint16_t));

    /* Pre-render static overlays (bottom_bar, clean_text, rectangle, title text)
     * to LTDC Layer 2 UI buffer immediately after clearing the framebuffers.
     * This ensures UI elements are visible from the very first frame, before
     * any potentially slow QSPI initialization.
     */
    PreRenderUI();

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

    /*---------------------------------------------------------------------------
     * Pre-create LVGL screens during initialization.
     * This ensures the slow initial widget creation and LVGL style setup
     * happens during power-up, NOT during the user's first page switch.
     * Without this, the first switch triggers a full LVGL render + flush
     * that is visible as a diagonally scrolling white flash.
     *---------------------------------------------------------------------------
     */
    if (lv_is_initialized()) {
        if (brightness_screen == NULL) {
            brightness_screen = lv_obj_create(NULL);
            lvgl_brightness_create(brightness_screen);
        }
        if (filter_screen == NULL) {
            filter_screen = lv_obj_create(NULL);
            lvgl_filter_create(filter_screen);
        }
        if (metrics1_screen == NULL) {
            metrics1_screen = lv_obj_create(NULL);
            lvgl_metrics1_create(metrics1_screen);
        }
        if (metrics2_screen == NULL) {
            metrics2_screen = lv_obj_create(NULL);
            lvgl_metrics2_create(metrics2_screen);
        }
    }

    for (;;)
    {
        /* Wait for 50ms timer to signal next frame */
        osSemaphoreAcquire(anim_sem_id, osWaitForever);

        /*--------------------------------------------------------------------
         * Poll KEY_UP (Screen Button) and KEY_DOWN (Option Button)
         * KEY_UP  = PA0 -> active HIGH
         * KEY_DOWN= PH2 -> active LOW
         * - KEY_UP cycles main pages: Animation -> Filter -> Brightness
         * - KEY_DOWN when on Animation switches to Metrics (secondary page)
         *   KEY_DOWN on Brightness is handled by LVGL event handler.
         *--------------------------------------------------------------------
         */
        {
            uint8_t key_up = (HAL_GPIO_ReadPin(KEY_UP_GPIO_Port, KEY_UP_Pin) == GPIO_PIN_SET) ? 1 : 0;
            uint8_t key_down = (HAL_GPIO_ReadPin(KEY_DOWN_GPIO_Port, KEY_DOWN_Pin) == GPIO_PIN_RESET) ? 1 : 0;

            if (key_up != prev_key_up || key_down != prev_key_down) {
                printf("DEBUG_KEYS: key_up=%d key_down=%d\r\n", key_up, key_down);
            }

            /* Screen Button: KEY_DOWN press (active low -> key_down==1) -> advance among main pages */
            if (prev_key_down == 0 && key_down == 1)
            {
                PageState_t base = current_page;
                if (base == PAGE_METRICS1 || base == PAGE_METRICS2) base = PAGE_ANIMATION; /* treat metrics as main=animation */

                if (base == PAGE_ANIMATION)
                    { g_requested_page = PAGE_FILTER;     g_page_switch_pending = 1; }
                else if (base == PAGE_FILTER)
                    { g_requested_page = PAGE_BRIGHTNESS; g_page_switch_pending = 1; }
                else if (base == PAGE_BRIGHTNESS)
                    { g_requested_page = PAGE_ANIMATION;  g_page_switch_pending = 1; }
            }

            /* Option Button: KEY_UP press (rising edge) when on Animation -> Metrics
             * Other pages should handle KEY_UP via LVGL event handlers. */
            if (prev_key_up == 0 && key_up == 1)
            {
                if (current_page == PAGE_ANIMATION)
                {
                    /* Animation -> Metrics1 */
                    g_requested_page = PAGE_METRICS1; g_page_switch_pending = 1;
                }
                else if (current_page == PAGE_METRICS1)
                {
                    /* Metrics1 -> Metrics2 */
                    g_requested_page = PAGE_METRICS2; g_page_switch_pending = 1;
                }
                else if (current_page == PAGE_METRICS2)
                {
                    /* Metrics2 -> Animation */
                    g_requested_page = PAGE_ANIMATION; g_page_switch_pending = 1;
                }
                /* else: let LVGL handle KEY_UP events on pages that use it */
            }

            prev_key_up = key_up;
            prev_key_down = key_down;
        }

        /* Skip rendering when not on the animation page (keep polling buttons) */
        if (current_page != PAGE_ANIMATION)
        {
            continue;
        }

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
        /* Clean D-cache for the destination framebuffer so LTDC reads updated data */
        SCB_CleanDCache_by_Addr((uint32_t *)fb_dst, LCD_FB_STRIDE * LCD_HEIGHT * sizeof(uint16_t));
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