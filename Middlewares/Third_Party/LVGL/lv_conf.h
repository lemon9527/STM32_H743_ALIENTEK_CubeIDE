/**
 * @file lv_conf.h
 * LVGL v8.4 configuration for STM32H743 + 4.3" RGB LCD (800x480, RGB565)
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH          16
#define LV_COLOR_16_SWAP        0       /* STM32 little-endian RGB565 */
#define LV_COLOR_SCREEN_TRANSP  0

/*====================
   MEMORY SETTINGS
 *====================*/
#define LV_MEM_CUSTOM           0       /* Use LVGL's built-in memory manager */
#define LV_MEM_SIZE             (64U * 1024U)   /* 64 KB static pool */
#define LV_MEM_ADR              0

/*====================
   HAL SETTINGS
 *====================*/
#define LV_TICK_CUSTOM          0       /* We call lv_tick_inc() externally via FreeRTOS timer */
#define LV_DPI_DEF              130     /* Approximate DPI for 4.3" 800x480 */

/*====================
   DISPLAY SETTINGS
 *====================*/
#define LV_HOR_RES_MAX          800
#define LV_VER_RES_MAX          480
#define LV_LAYER_SIMPLE_BUF_SIZE    (24 * 1024)

/*====================
   DRAW SETTINGS
 *====================*/
#define LV_DRAW_COMPLEX         1
#define LV_SHADOW_CACHE_SIZE    0
#define LV_CIRCLE_CACHE_SIZE    4

/*====================
   GPU
 *====================*/
#define LV_USE_GPU_STM32_DMA2D  0   /* We use DMA2D manually in flush_cb */

/*====================
   LOGGING
 *====================*/
#define LV_USE_LOG              0

/*====================
   FEATURE USAGE
 *====================*/
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0
#define LV_USE_DEMO_WIDGETS         1

/*====================
   FONT USAGE
 *====================*/
#define LV_FONT_MONTSERRAT_10   1
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_24   1
#define LV_FONT_MONTSERRAT_32   0
#define LV_FONT_DEFAULT         &lv_font_montserrat_14
/* Enable compressed fonts (required for LVGL compressed font support) */
#define LV_USE_FONT_COMPRESSED  1

/*====================
   WIDGETS
 *====================*/
#define LV_USE_ARC              1
#define LV_USE_BAR              1
#define LV_USE_BTN              1
#define LV_USE_BTNMATRIX        1
#define LV_USE_CANVAS           1
#define LV_USE_CHECKBOX         1
#define LV_USE_DROPDOWN         1
#define LV_USE_IMG              1
#define LV_USE_PNG              1
#define LV_USE_LABEL            1
#define LV_USE_LINE             1
#define LV_USE_ROLLER           1
#define LV_USE_SLIDER           1
#define LV_USE_SWITCH           1
#define LV_USE_TEXTAREA         1
#define LV_USE_TABLE            1

/*====================
   EXTRA COMPONENTS
 *====================*/
#define LV_USE_ANIMIMG          1
#define LV_USE_CALENDAR         1
#define LV_USE_CHART            1
#define LV_USE_COLORWHEEL       1
#define LV_USE_IMGBTN           1
#define LV_USE_KEYBOARD         1
#define LV_USE_LED              1
#define LV_USE_LIST             1
#define LV_USE_MENU             1
#define LV_USE_METER            1
#define LV_USE_MSGBOX           1
#define LV_USE_SPAN             1
#define LV_USE_SPINBOX          1
#define LV_USE_SPINNER          1
#define LV_USE_TABVIEW          1
#define LV_USE_TILEVIEW         1
#define LV_USE_WIN              1

/*====================
   THEMES
 *====================*/
#define LV_USE_THEME_DEFAULT    1
#define LV_USE_THEME_BASIC      1
#define LV_USE_THEME_MONO       1
#define LV_THEME_DEFAULT_DARK   0
#define LV_THEME_DEFAULT_GROW   1

/*====================
   OTHER
 *====================*/
#define LV_SPRINTF_CUSTOM       0
#define LV_USE_USER_DATA        1
#define LV_BIG_ENDIAN_SYSTEM    0
#define LV_ATTRIBUTE_MEM_ALIGN_SIZE  4

#endif /* LV_CONF_H */