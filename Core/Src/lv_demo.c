/* lv_demo.c - Minimal LVGL demo: creates a centered "Hello, LVGL!" label */
#include "lv_demo.h"

void lvgl_demo_create(void)
{
    lv_obj_t *label = lv_label_create(lv_scr_act());
    if (label)
    {
        lv_label_set_text(label, "Hello, LVGL!");
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    }
}
