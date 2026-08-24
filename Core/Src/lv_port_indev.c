/**
 * @file lv_port_indev.c
 * @brief LVGL input device driver for STM32H743
 *
 * Maps KEY0 (PH3)  → LV_KEY_ENTER  (active LOW)
 *       KEY_UP (PA0)  → LV_KEY_UP     (active HIGH)
 *       KEY_DOWN (PH2) → LV_KEY_DOWN   (active LOW)
 *
 * Level-triggered: reports PRESSED continuously while a key is held,
 *                  RELEASED once when released.
 */

#include "lv_port_indev.h"
#include "main.h"

/* Global indev pointer, used by lv_demo.c to set the keypad group */
lv_indev_t *g_lv_indev = NULL;

/*---------------------------------------------------------------------------
 * Button read callback
 *---------------------------------------------------------------------------
 * Polls all three keys.  Level polarity:
 *   KEY_UP  → active HIGH (pressed = GPIO_PIN_SET)
 *   Others  → active LOW  (pressed = GPIO_PIN_RESET)
 *
 * Priority: KEY0 > KEY_UP > KEY_DOWN
 *
 * LVGL keypad indev expects:
 *   - PRESSED reported continuously while a key is held
 *   - RELEASED reported once when the key is released
 */
static void lv_port_indev_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    static uint32_t prev_key = 0;
    uint32_t key = 0;

    /* KEY0 (PH3): active LOW */
    if (HAL_GPIO_ReadPin(KEY0_GPIO_Port, KEY0_Pin) == GPIO_PIN_RESET)
    {
        key = LV_KEY_ENTER;
    }
    /* KEY_UP (PA0): active HIGH */
    else if (HAL_GPIO_ReadPin(KEY_UP_GPIO_Port, KEY_UP_Pin) == GPIO_PIN_SET)
    {
        key = LV_KEY_UP;
    }
    /* KEY_DOWN (PH2): active LOW */
    else if (HAL_GPIO_ReadPin(KEY_DOWN_GPIO_Port, KEY_DOWN_Pin) == GPIO_PIN_RESET)
    {
        key = LV_KEY_DOWN;
    }

    if (key != 0)
    {
        /* A key is currently pressed — report PRESSED continuously */
        data->key = key;
        data->state = LV_INDEV_STATE_PRESSED;
        prev_key = key;
    }
    else if (prev_key != 0)
    {
        /* Key just released — report RELEASED once */
        data->key = prev_key;
        data->state = LV_INDEV_STATE_RELEASED;
        prev_key = 0;
    }
    else
    {
        /* No key activity */
        data->key = 0;
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

/*---------------------------------------------------------------------------
 * Input device initialization
 *---------------------------------------------------------------------------
 * Registers a keypad-style input device and stores the pointer.
 */
lv_indev_t *lv_port_indev_init(void)
{
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);

    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = lv_port_indev_read;

    g_lv_indev = lv_indev_drv_register(&indev_drv);
    return g_lv_indev;
}