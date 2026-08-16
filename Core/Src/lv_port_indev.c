/**
 * @file lv_port_indev.c
 * @brief LVGL input device driver for STM32H743
 *
 * Maps KEY0 (PH3, active low) to LV_KEY_ENTER.
 * Simple debounce: requires two consecutive reads to confirm.
 */

#include "lv_port_indev.h"
#include "main.h"

/*---------------------------------------------------------------------------
 * Button read callback
 *---------------------------------------------------------------------------
 * Reads KEY0 and returns the corresponding LVGL key.
 * Only KEY0 is mapped for now (KEY0 → LV_KEY_ENTER).
 */
static void lv_port_indev_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    static uint8_t pressed = 0;
    uint32_t key = 0;

    /* KEY0 is active low (PH3) */
    if (HAL_GPIO_ReadPin(KEY0_GPIO_Port, KEY0_Pin) == GPIO_PIN_RESET)
    {
        key = LV_KEY_ENTER;
    }

    /* Simple edge detection: report PRESSED on first read, RELEASED on release */
    if (key != 0 && !pressed)
    {
        data->key = key;
        data->state = LV_INDEV_STATE_PRESSED;
        pressed = 1;
    }
    else if (key == 0 && pressed)
    {
        data->key = 0;
        data->state = LV_INDEV_STATE_RELEASED;
        pressed = 0;
    }
    else
    {
        data->key = 0;
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

/*---------------------------------------------------------------------------
 * Input device initialization
 *---------------------------------------------------------------------------
 * Registers a keypad-style input device.
 */
lv_indev_t *lv_port_indev_init(void)
{
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);

    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = lv_port_indev_read;

    return lv_indev_drv_register(&indev_drv);
}