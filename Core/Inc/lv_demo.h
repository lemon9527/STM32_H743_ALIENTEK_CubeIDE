/* lv_demo.h - Minimal LVGL demo header */
#ifndef __LV_DEMO_H__
#define __LV_DEMO_H__

#include "lvgl.h"
#include "uart_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

void lvgl_brightness_create(lv_obj_t *scr);
void lvgl_filter_create(lv_obj_t *scr);
void lvgl_filter_update(int percent);
void lvgl_metrics1_create(lv_obj_t *scr);
void lvgl_metrics2_create(lv_obj_t *scr);

/* Update sensor values on the Metrics1 page (Air Index, PM2.5, TVOC, CO2) */
void lvgl_metrics1_update(const sensor_data_t *data);

/* Update sensor values on the Metrics2 page (Air Index, Temperature, Humidity, Pressure) */
void lvgl_metrics2_update(const sensor_data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* __LV_DEMO_H__ */