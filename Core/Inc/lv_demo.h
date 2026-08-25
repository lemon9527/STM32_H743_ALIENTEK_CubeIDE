/* lv_demo.h - Minimal LVGL demo header */
#ifndef __LV_DEMO_H__
#define __LV_DEMO_H__

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void lvgl_brightness_create(lv_obj_t *scr);
void lvgl_filter_create(lv_obj_t *scr);
void lvgl_metrics1_create(lv_obj_t *scr);
void lvgl_metrics2_create(lv_obj_t *scr);

#ifdef __cplusplus
}
#endif

#endif /* __LV_DEMO_H__ */