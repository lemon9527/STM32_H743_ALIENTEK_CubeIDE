#ifndef __LCD_H__
#define __LCD_H__

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Portrait mode: 480x800 logical on 800x480 physical panel */
#define LCD_WIDTH       480
#define LCD_HEIGHT      800
#define LCD_FB_STRIDE   1600    /* physical row stride in pixels (LTDC ImageWidth) */

/* Font scale factor (2x = 16x32 effective) */
#define LCD_FONT_SCALE  2

/* Framebuffer base address in SDRAM */
#define LCD_FB_BASE     0xC0000000

/* UI overlay buffer (LTDC Layer 2, ARGB1555, per-pixel alpha)
 * Address: 0xC0300000 (after the two animation framebuffers)
 * Stride: 800 pixels (continuous, no gap)
 */
#define UI_BUF_ADDR     0xC0300000
#define UI_BUF_STRIDE   800

/* RGB565 color macros */
#define LCD_COLOR_BLACK          0x0000
#define LCD_COLOR_WHITE          0xFFFF
#define LCD_COLOR_RED            0xF800
#define LCD_COLOR_GREEN          0x07E0
#define LCD_COLOR_BLUE           0x001F
#define LCD_COLOR_CYAN           0x07FF
#define LCD_COLOR_MAGENTA        0xF81F
#define LCD_COLOR_YELLOW         0xFFE0
#define LCD_COLOR_GRAY           0x8410
#define LCD_COLOR_ORANGE         0xFC00
#define LCD_COLOR_DARKBLUE       0x0010

/* RGB565 color from RGB components */
#define LCD_RGB565(r, g, b)     ((((uint16_t)(r) & 0xF8) << 8) | \
                                 (((uint16_t)(g) & 0xFC) << 3) | \
                                 (((uint16_t)(b) & 0xF8) >> 3))

/* 8x16 ASCII font (chars 0x20-0x7E, 95 chars, 16 bytes each) */
extern const uint8_t lcd_font_8x16[][16];

void LCD_Init(void);
void LCD_Clear(uint16_t color);
void LCD_Fill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void LCD_ShowString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg_color);

#ifdef __cplusplus
}
#endif

#endif /* __LCD_H__ */