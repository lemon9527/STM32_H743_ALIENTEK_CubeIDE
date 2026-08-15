#ifndef __LCD_H__
#define __LCD_H__

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* LCD parameters */
#define LCD_WIDTH       800
#define LCD_HEIGHT      480
#define LCD_FB_STRIDE   1600    /* framebuffer row stride in pixels (ImageWidth) */

/* Framebuffer base address in SDRAM */
#define LCD_FB_BASE     0xC0000000

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

void LCD_Init(void);
void LCD_Clear(uint16_t color);
void LCD_Fill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color);

#ifdef __cplusplus
}
#endif

#endif /* __LCD_H__ */