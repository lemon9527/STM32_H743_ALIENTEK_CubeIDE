#include "lcd.h"

/* Framebuffer pointer */
static uint16_t *lcd_framebuf = (uint16_t *)LCD_FB_BASE;

/**
 * @brief  Initialize LCD backlight
 */
void LCD_Init(void)
{
    /* Turn on LCD backlight (PB5) */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
}

/**
 * @brief  Fill entire screen with a single color
 */
void LCD_Clear(uint16_t color)
{
    LCD_Fill(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, color);
}

/**
 * @brief  Fill a rectangle with a single color (CPU-based)
 */
void LCD_Fill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    uint16_t x, y;

    for (y = y0; y <= y1; y++)
    {
        uint16_t *row = &lcd_framebuf[y * LCD_FB_STRIDE];
        for (x = x0; x <= x1; x++)
        {
            row[x] = color;
        }
    }
}

/**
 * @brief  Draw a single pixel
 */
void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    lcd_framebuf[y * LCD_FB_STRIDE + x] = color;
}