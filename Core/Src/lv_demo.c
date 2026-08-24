/* lv_demo.c - Minimal LVGL demo: creates a centered "Hello, LVGL!" label */
#include "lv_demo.h"
#include "lv_port_indev.h"
#include "tim.h"
#include "icon_resource/brightness_icons.h"

void lvgl_demo_create(lv_obj_t *scr)
{
    /********************学习任务1 Hello World — 标签 (Label) *******************/

    // 使用传入的屏幕对象，而不是 lv_scr_act()
    lv_obj_t *src = scr;

    /* Set screen background color and make it opaque so it covers
     * the animation framebuffer beneath. Change the hex value to
     * the desired RGB888 color (e.g. 0x000000=black, 0xFFFFFF=white).
     */
    lv_obj_set_style_bg_color(src, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(src, LV_OPA_COVER, 0);

    /* 2. 创建与 animation 页面相同的 320x480 矩形框（蓝色边框，2px，透明背景）
     *    并把 Hello 标签放在该矩形中心。
     */
    lv_obj_t *rect = lv_obj_create(src);
    lv_obj_set_size(rect, 320, 480);
    lv_obj_align(rect, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(rect, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rect, 2, 0);
    lv_obj_set_style_border_color(rect, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_border_opa(rect, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(rect, 0, 0); /* 关键：直角 */

    /* 3. 创建Label，作为矩形的子对象，居中显示 */
    lv_obj_t *label_hello = lv_label_create(rect);
    lv_label_set_text(label_hello, "Hello, LVGL!");
    lv_obj_center(label_hello);

    // 创建一个样式对象
    static lv_style_t style_label_font;
    lv_style_init(&style_label_font);

    // 设置样式对象的中的字体属性
    lv_style_set_text_font(&style_label_font, &lv_font_montserrat_24);

    // 设置样式对象的中的字体颜色属性
    lv_style_set_text_color(&style_label_font, lv_palette_main(LV_PALETTE_BLUE));

    // 将该样式应用到Label上
    lv_obj_add_style(label_hello, &style_label_font, 0);

    /********************学习任务2 按钮 (Button) *******************/
    // 创建一个按钮对象，作为矩形的子对象，居中显示    
    lv_obj_t *btn = lv_btn_create(rect);

    // 设置按钮的大小和位置
    lv_obj_set_size(btn, 120, 40);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 0);

    // 暂时不绑定回调事件

    // 在按钮上创建一个标签，显示"Click me!"文本
    lv_obj_t *label_btn = lv_label_create(btn);
    lv_label_set_text(label_btn, "Click me!");
    lv_obj_center(label_btn);
}

/*---------------------------------------------------------------------------
 * Brightness card selection
 *---------------------------------------------------------------------------
 * Stores the selected card index (0=High, 1=Medium, 2=Off) and updates
 * icon/text images to show active (selected) or normal (unselected).
 */
static int brightness_selected = 0;
static lv_obj_t *brightness_icon_objs[3] = {NULL};
static lv_obj_t *brightness_text_objs[3] = {NULL};

/* Active / Normal image descriptors for each card */
static const lv_img_dsc_t *icon_active[3] = {
    &ic_brightness_high_active,
    &ic_brightness_medium_high,
    &ic_brightness_off_active
};
static const lv_img_dsc_t *icon_normal[3] = {
    &ic_brightness_high_normal,
    &ic_brightness_medium_normal,
    &ic_brightness_off_normal
};
static const lv_img_dsc_t *text_active[3] = {
    &txt_brightness_high_active,
    &txt_brightness_medium_active,
    &txt_brightness_off_active
};
static const lv_img_dsc_t *text_normal[3] = {
    &txt_brightness_high_normal,
    &txt_brightness_medium_normal,
    &txt_brightness_off_normal
};

/* 亮度等级 PWM 占空比：High=100%, Medium=50%, Off=0% */
static const uint16_t brightness_levels[3] = {
    999,   /* High */
    400,   /* Medium */
    0      /* Off */
};

static void brightness_update_selection(int sel)
{
    brightness_selected = sel;
    for (int i = 0; i < 3; i++)
    {
        int active = (i == sel);
        lv_img_set_src(brightness_icon_objs[i], active ? icon_active[i] : icon_normal[i]);
        lv_img_set_src(brightness_text_objs[i], active ? text_active[i] : text_normal[i]);
    }

    /* 调节 LCD 背光 PWM 占空比 */
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, brightness_levels[sel]);
}

static void brightness_key_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);

    /* Only process keys when the brightness screen is the active screen */
    if (lv_scr_act() != lv_obj_get_screen(obj)) return;

    if (code == LV_EVENT_KEY)
    {
        uint32_t key = *(uint32_t *)lv_event_get_param(e);
        if (key == LV_KEY_UP)
        {
            int new_sel = (brightness_selected > 0) ? brightness_selected - 1 : 2;
            brightness_update_selection(new_sel);
        }
        else if (key == LV_KEY_DOWN)
        {
            int new_sel = (brightness_selected < 2) ? brightness_selected + 1 : 0;
            brightness_update_selection(new_sel);
        }
    }
}

void lvgl_brightness_create(lv_obj_t *scr)
{
    /******************** Brightness 页面 — 使用 PNG 图像 *******************/

    lv_obj_t *src = scr;

    /* Set screen background color to black */
    lv_obj_set_style_bg_color(src, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(src, LV_OPA_COVER, 0);

    /* 创建 320x480 矩形框（蓝色边框，1px，透明背景） */
    lv_obj_t *rect = lv_obj_create(src);
    lv_obj_set_size(rect, 320, 480);
    lv_obj_align(rect, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(rect, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rect, 1, 0);
    lv_obj_set_style_border_color(rect, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_border_opa(rect, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(rect, 0, 0);
    lv_obj_set_style_pad_all(rect, 0, 0);       /* 清除默认 padding，确保子对象在 320x480 内定位 */
    lv_obj_add_flag(rect, LV_OBJ_FLAG_CLICK_FOCUSABLE); /* 确保可接收焦点 */

    /* 左上角标题：txt_brightness_title.png (已预缩放至 196x38) */
    lv_obj_t *title_img = lv_img_create(rect);
    lv_img_set_src(title_img, &txt_brightness_title);
    lv_obj_set_pos(title_img, 20, 24);

    /*---------------------------------------------------------------------------
     * 3 个圆角卡片，每个卡片内左侧为 icon (h=59)，右侧为文本
     *   标题底部 y=62 → 间距 26px → 卡片 1 起始 y=88
     *   卡片宽度 288px (居中：x=16)，高度 120px，间距 8px
     *   3 * 120 + 3 * 8 = 392 → 卡片 3 底部 y=480
     *---------------------------------------------------------------------------
     */
    /* 卡片样式：纯色圆角矩形，无边框无阴影 (LVGL v8 无 selector 参数) */
    static lv_style_t style_card;
    lv_style_init(&style_card);
    lv_style_set_radius(&style_card, 12);
    lv_style_set_bg_color(&style_card, lv_color_hex(0x1A1A1A));
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_border_width(&style_card, 0);    /* 无边框，纯色 */
    lv_style_set_shadow_opa(&style_card, 0);      /* 无阴影，禁用发光效果 */

    #define CARD_W     288
    #define CARD_H     120
    #define CARD_X     16   /* (320 - 288) / 2 */
    #define CARD_GAP   8
    #define CARD_1_Y   88   /* 标题底部 62 + 26px 间距 */

    /* 预计算 icon 宽度，用于 text 的 x 偏移 */
    static const int icon_w[3] = { 60, 58, 58 };
    /* 各卡片文本高度：High 为 45，Medium/Off 为 35 */
    static const int text_h[3] = { 45, 35, 35 };

    for (int i = 0; i < 3; i++)
    {
        lv_obj_t *card = lv_obj_create(rect);
        lv_obj_remove_style_all(card);          /* 清除默认样式 */
        lv_obj_add_style(card, &style_card, 0);
        lv_obj_set_size(card, CARD_W, CARD_H);
        lv_obj_set_pos(card, CARD_X, CARD_1_Y + i * (CARD_H + CARD_GAP));

        /* 卡片内左侧 icon，高度 59px，垂直居中 */
        lv_obj_t *icon = lv_img_create(card);
        lv_img_set_src(icon, icon_normal[i]);
        lv_obj_set_pos(icon, 10, (CARD_H - 59) / 2);

        /* icon 右侧的文本，垂直居中，与 icon 间距 23px */
        lv_obj_t *label = lv_img_create(card);
        lv_img_set_src(label, text_normal[i]);
        lv_obj_set_pos(label, 10 + icon_w[i] + 23, (CARD_H - text_h[i]) / 2);

        /* 保存对象指针，供按键切换时更新 */
        brightness_icon_objs[i] = icon;
        brightness_text_objs[i] = label;
    }

    /*---------------------------------------------------------------------------
     * 按键事件处理：KEY_UP / KEY_DOWN 切换选中卡片
     *   将 rect 添加到组中，并绑定按键事件处理器
     *---------------------------------------------------------------------------
     */
    lv_obj_add_event_cb(rect, brightness_key_handler, LV_EVENT_KEY, NULL);

    /* 创建 LVGL 组，将 rect 加入组，并设置到输入设备上 */
    lv_group_t *g = lv_group_create();
    lv_group_add_obj(g, rect);
    lv_indev_set_group(g_lv_indev, g);

    /* 默认选中 High（索引 0） */
    brightness_selected = 0;
    brightness_update_selection(0);
}