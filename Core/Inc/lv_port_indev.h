#ifndef __LV_PORT_INDEV_H__
#define __LV_PORT_INDEV_H__

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_indev_t *lv_port_indev_init(void);

/* Global indev pointer, used by other modules to set keypad groups */
extern lv_indev_t *g_lv_indev;

#ifdef __cplusplus
}
#endif

#endif /* __LV_PORT_INDEV_H__ */