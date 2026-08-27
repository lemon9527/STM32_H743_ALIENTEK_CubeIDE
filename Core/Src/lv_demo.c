/* lv_demo.c - Minimal LVGL demo: creates a centered "Hello, LVGL!" label */
#include "lv_demo.h"
#include "lv_port_indev.h"
#include "tim.h"
#include "icon_resource/brightness_icons.h"
#include "icon_resource/air_quality_icons.h"
#include "icon_resource/filter_icons.h"
#include "fonts.h"
#include <stdio.h>

/* Demo helper removed: lvgl_demo_create is unused. */

/*---------------------------------------------------------------------------
 * Metrics1 value labels (updated by lvgl_metrics1_update)
 *---------------------------------------------------------------------------*/
static lv_obj_t *m1_air_index_lbl = NULL;   /* Card 0: Air Index value */
static lv_obj_t *m1_pm25_val_lbl = NULL;     /* Card 1: PM2.5 value */
static lv_obj_t *m1_pm25_unit_lbl = NULL;    /* Card 1: PM2.5 unit */
static lv_obj_t *m1_tvoc_val_lbl = NULL;     /* Card 2: TVOC value */
static lv_obj_t *m1_tvoc_unit_lbl = NULL;    /* Card 2: TVOC unit */
static lv_obj_t *m1_co2_val_lbl = NULL;      /* Card 3: CO2 value */
static lv_obj_t *m1_co2_unit_lbl = NULL;     /* Card 3: CO2 unit */

/*---------------------------------------------------------------------------
 * Metrics2 value labels (updated by lvgl_metrics2_update)
 *---------------------------------------------------------------------------*/
static lv_obj_t *m2_air_index_lbl = NULL;   /* Card 0: Air Index value */
static lv_obj_t *m2_temp_val_lbl = NULL;     /* Card 1: Temperature value */
static lv_obj_t *m2_temp_unit_lbl = NULL;    /* Card 1: Temperature unit */
static lv_obj_t *m2_humid_val_lbl = NULL;    /* Card 2: Humidity value */
static lv_obj_t *m2_humid_unit_lbl = NULL;   /* Card 2: Humidity unit */
static lv_obj_t *m2_press_val_lbl = NULL;    /* Card 3: Pressure value */
static lv_obj_t *m2_press_unit_lbl = NULL;   /* Card 3: Pressure unit */

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
    100      /* Off */
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
        /* Option Button is now KEY_UP: use LV_KEY_UP to change brightness */
        if (key == LV_KEY_UP)
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

void lvgl_filter_create(lv_obj_t *scr)
{
    lv_obj_t *src = scr;
    lv_obj_set_style_bg_color(src, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(src, LV_OPA_COVER, 0);

    lv_obj_t *rect = lv_obj_create(src);
    lv_obj_set_size(rect, 320, 480);
    lv_obj_align(rect, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(rect, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rect, 2, 0);
    lv_obj_set_style_border_color(rect, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_border_opa(rect, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(rect, 0, 0);
    lv_obj_set_style_pad_all(rect, 0, 0);
    lv_obj_add_flag(rect, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    /*---------------------------------------------------------------------------
     * Title "Filter" — 左上角, inter_semi_bold_32, 距左 24, 距顶 24
     *---------------------------------------------------------------------------*/
    lv_obj_t *title = lv_label_create(rect);
    lv_label_set_text(title, "Filter");
    static lv_style_t style_title;
    lv_style_init(&style_title);
    lv_style_set_text_color(&style_title, lv_color_white());
    lv_style_set_text_font(&style_title, &inter_semi_bold_32);
    lv_obj_add_style(title, &style_title, 0);
    lv_obj_set_pos(title, 24, 24);

    /*---------------------------------------------------------------------------
     * Top-right: mini icon + filter type label (default: Hybrid)
     *   mini icon:  icon_mini_Hybrid_24_24 (24x24)
     *               x=205 (left edge from rect left), y=32 (from rect top)
     *   label:      "Hybrid", inter_regular_17, same baseline as mini icon
     *               text left = mini icon right + 13 = 205 + 24 + 13 = 242
     *---------------------------------------------------------------------------*/
    lv_obj_t *mini_icon = lv_img_create(rect);
    lv_img_set_src(mini_icon, &icon_mini_Hybrid_24_24);
    lv_obj_set_pos(mini_icon, 205, 32);

    lv_obj_t *type_label = lv_label_create(rect);
    lv_label_set_text(type_label, "Hybrid");
    static lv_style_t style_type_label;
    lv_style_init(&style_type_label);
    lv_style_set_text_color(&style_type_label, lv_color_white());
    lv_style_set_text_font(&style_type_label, &inter_regular_16);
    lv_obj_add_style(type_label, &style_type_label, 0);
    /* Align text vertically with mini icon (y offset for centering 16px text in 24px height) */
    lv_obj_set_pos(type_label, 205 + 24 + 13, 32 + (24 - 16) / 2);

    /*---------------------------------------------------------------------------
     * Center: large icon (default: Hybrid, 123x179)
     *   horizontally centered, top = mini_icon top + mini_icon height + 58
     *---------------------------------------------------------------------------*/
    lv_obj_t *large_icon = lv_img_create(rect);
    lv_img_set_src(large_icon, &icon_large_HYBRID_Filter_AMIII_123_179);
    int large_icon_x = (320 - 123) / 2;
    int large_icon_y = 32 + 24 + 58;
    lv_obj_set_pos(large_icon, large_icon_x, large_icon_y);

    /*---------------------------------------------------------------------------
     * "Life Remaining" label — 距大 icon 底部 56, 距左 96, inter_regular_16
     *---------------------------------------------------------------------------*/
    lv_obj_t *life_label = lv_label_create(rect);
    lv_label_set_text(life_label, "Life Remaining");
    static lv_style_t style_life_label;
    lv_style_init(&style_life_label);
    lv_style_set_text_color(&style_life_label, lv_color_white());
    lv_style_set_text_font(&style_life_label, &inter_regular_16);
    lv_obj_add_style(life_label, &style_life_label, 0);
    lv_obj_set_pos(life_label, 96, large_icon_y + 179 + 56);

    /*---------------------------------------------------------------------------
     * "76%" value — 左对齐 Life Remaining, inter_bold_42
     *---------------------------------------------------------------------------*/
    lv_obj_t *life_val = lv_label_create(rect);
    lv_label_set_text(life_val, "76%");
    static lv_style_t style_life_val;
    lv_style_init(&style_life_val);
    lv_style_set_text_color(&style_life_val, lv_color_white());
    lv_style_set_text_font(&style_life_val, &inter_bold_42);
    lv_obj_add_style(life_val, &style_life_val, 0);
    lv_obj_set_pos(life_val, 96, large_icon_y + 179 + 56 + 16 + 14);
}

void lvgl_metrics1_create(lv_obj_t *scr)
{
    lv_obj_t *src = scr;
    lv_obj_set_style_bg_color(src, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(src, LV_OPA_COVER, 0);

    lv_obj_t *rect = lv_obj_create(src);
    lv_obj_set_size(rect, 320, 480);
    lv_obj_align(rect, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(rect, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rect, 1, 0);
    lv_obj_set_style_border_color(rect, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_border_opa(rect, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(rect, 0, 0);
    lv_obj_set_style_pad_all(rect, 0, 0);
    lv_obj_add_flag(rect, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    /* Four rounded cards with same style as Brightness */
    static lv_style_t style_card;
    lv_style_init(&style_card);
    lv_style_set_radius(&style_card, 12);
    lv_style_set_bg_color(&style_card, lv_color_hex(0x1A1A1A));
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_border_width(&style_card, 0);
    lv_style_set_shadow_opa(&style_card, 0);

    const int n = 4;
    const int card_w = 288;
    const int card_x = 16;
    const int card_gap = 8; /* gap between cards */
    const int card_1_y = 16; /* top padding inside 320x480 */
    /* Available height = 480 - top - bottom (16 + 16) = 448
     * Total gaps = (n-1) * card_gap
     * card_h = (available_height - total_gaps) / n
     */
    int card_h = (480 - 32 - (n - 1) * card_gap) / n;

    lv_obj_t *cards[n];
    for (int i = 0; i < n; i++)
    {
        lv_obj_t *card = lv_obj_create(rect);
        lv_obj_remove_style_all(card);
        lv_obj_add_style(card, &style_card, 0);
        lv_obj_set_size(card, card_w, card_h);
        lv_obj_set_pos(card, card_x, card_1_y + i * (card_h + card_gap));
        cards[i] = card;
    }

    /* Place PM2.5/TVOC/CO2 icons on the right side of the each card (cards[1]/[2]/[3]).
     * Right margin from card = 28px, desired display size = 53x53.
     * For the original icon size ~120px, apply zoom ~= 113 (256 * 53 / 120).
     */
    {
        /* Create PM2.5 image as child of `rect` (not the card) so it can be drawn above the card
         * and won't be clipped or occluded by card child ordering. The embedded image is already
         * 53x53, so no zoom is required. Position is calculated relative to `rect`. */
        lv_obj_t *pm_img = lv_img_create(rect);
        lv_img_set_src(pm_img, &icon_pm2_5);
        int img_w = 53;
        int img_h = 53;
        /* x: rect origin + card_x + (card_w - 28 - img_w) */
        int x = card_x + (card_w - 28 - img_w);
        /* y: rect origin + card_1_y + second card offset + vertical centering */
        int y = card_1_y + 1 * (card_h + card_gap) + (card_h - img_h) / 2;
        lv_obj_set_pos(pm_img, x, y);
        /* Ensure the image is rendered above the cards */
        lv_obj_move_foreground(pm_img);
    }

    {
        /* Create TVOC image on cards[2], same position/size as PM2.5 */
        lv_obj_t *tvoc_img = lv_img_create(rect);
        lv_img_set_src(tvoc_img, &icon_tvoc);
        int img_w = 53;
        int img_h = 53;
        int x = card_x + (card_w - 28 - img_w);
        int y = card_1_y + 2 * (card_h + card_gap) + (card_h - img_h) / 2;
        lv_obj_set_pos(tvoc_img, x, y);
        lv_obj_move_foreground(tvoc_img);
    }

    {
        /* Create CO2 image on cards[3], same position/size as PM2.5 */
        lv_obj_t *co2_img = lv_img_create(rect);
        lv_img_set_src(co2_img, &icon_co2);
        int img_w = 53;
        int img_h = 53;
        int x = card_x + (card_w - 28 - img_w);
        int y = card_1_y + 3 * (card_h + card_gap) + (card_h - img_h) / 2;
        lv_obj_set_pos(co2_img, x, y);
        lv_obj_move_foreground(co2_img);
    }

    /** Card 0 Current Air Quality **/
    /* Metrics1: display title in top-left of card 0
     * Position relative to card: left=28.41px, top=13.26px
    * Use inter_regular_18 font.
     */
    lv_obj_t *title_lbl = lv_label_create(cards[0]);
    lv_label_set_text(title_lbl, "Current Air Quality");
    static lv_style_t style_title;
    lv_style_init(&style_title);
    lv_style_set_text_color(&style_title, lv_color_hex(0xA7A7A7));
    lv_style_set_text_font(&style_title, &inter_regular_18);
    lv_obj_add_style(title_lbl, &style_title, 0);
    /* Set position inside the card (use integer pixels) */
    lv_obj_set_pos(title_lbl, 28, 13);

    /* Add Air Index label below the title, vertically centered
     * between the title bottom and the card bottom, left-aligned with title.
     */
    lv_obj_t *value_lbl = lv_label_create(cards[0]);
    m1_air_index_lbl = value_lbl;
    lv_label_set_text(value_lbl, "98%");
    static lv_style_t style_value;
    lv_style_init(&style_value);
    lv_style_set_text_color(&style_value, lv_color_white());
    lv_style_set_text_font(&style_value, &inter_bold_50);
    lv_obj_add_style(value_lbl, &style_value, 0);
    /* Compute vertical placement using known title Y and card height */
    int title_y = 13;
    int title_h = lv_obj_get_height(title_lbl);
    int area_top = title_y + title_h;
    int area_bottom = card_h; /* card_h computed above */
    int val_h = lv_obj_get_height(value_lbl);
    int val_y = (area_top + area_bottom - val_h) / 2;
    /* Move up by 10 pixels to avoid overflowing the bottom */
    val_y -= 10;
    if (val_y < area_top) val_y = area_top + 4;
    lv_obj_set_pos(value_lbl, 28, val_y);

    /** Card 1 PM 2.5 **/
    /* Metrics1: display title(PM 2.5) in top-left of card 1
     * Position relative to card: left=28.41px, top=13.26px
    * Use inter_regular_18 font.
    */
    lv_obj_t *title_lbl2 = lv_label_create(cards[1]);
    lv_label_set_text(title_lbl2, "PM 2.5");
    lv_style_set_text_color(&style_title, lv_color_hex(0xA7A7A7));
    lv_style_set_text_font(&style_title, &inter_regular_18);
    lv_obj_add_style(title_lbl2, &style_title, 0);
    /* Set position inside the card (use integer pixels) */
    lv_obj_set_pos(title_lbl2, 28, 17);

    /* Add PM 2.5 value label below the title, left-aligned with title.
     * PM 2.5 value use inter_bold_42 font.
     * PM 2.5 unit use inter_regular_17 font, positioned right after value with 4px gap.
    */
    lv_obj_t *value_lbl2 = lv_label_create(cards[1]);
    m1_pm25_val_lbl = value_lbl2;
    lv_label_set_text(value_lbl2, "12");
    lv_style_set_text_color(&style_value, lv_color_white());
    lv_style_set_text_font(&style_value, &inter_bold_42);
    lv_obj_add_style(value_lbl2, &style_value, 0);
    int val2_x = 28;
    int val2_y = 46;
    lv_obj_set_pos(value_lbl2, val2_x, val2_y);

    /* Unit label: positioned right after the value label with 8px gap,
     * vertically aligned by baseline (bottom of unit = bottom of value).
     */
    lv_obj_t *unit_lbl2 = lv_label_create(cards[1]);
    m1_pm25_unit_lbl = unit_lbl2;
    lv_label_set_text(unit_lbl2, "μg/m³");
    lv_style_set_text_color(&style_title, lv_color_hex(0xA7A7A7));
    lv_style_set_text_font(&style_title, &inter_regular_17);
    lv_obj_add_style(unit_lbl2, &style_title, 0);
    /* Use lv_txt_get_width() for reliable text width at creation time.
     * lv_obj_get_width() may return 0 because LVGL hasn't laid out the label yet.
     */
    int val2_w = lv_txt_get_width("12", 3, &inter_bold_42, 0, LV_TEXT_FLAG_NONE);
    int val2_h = lv_font_get_line_height(&inter_bold_42);
    int unit2_h = lv_font_get_line_height(&inter_regular_17);
    int unit2_x = val2_x + val2_w + 8;  /* 8px gap between value and unit */
    int unit2_y = (val2_y + val2_h) - unit2_h - 6;  /* align bottom (baseline) - 6px offset */
    lv_obj_set_pos(unit_lbl2, unit2_x, unit2_y);

    /** Card 2 TVOC **/
    lv_obj_t *title_lbl3 = lv_label_create(cards[2]);
    lv_label_set_text(title_lbl3, "TVOC");
    lv_style_set_text_color(&style_title, lv_color_hex(0xA7A7A7));
    lv_style_set_text_font(&style_title, &inter_regular_18);
    lv_obj_add_style(title_lbl3, &style_title, 0);
    lv_obj_set_pos(title_lbl3, 28, 17);

    lv_obj_t *value_lbl3 = lv_label_create(cards[2]);
    m1_tvoc_val_lbl = value_lbl3;
    lv_label_set_text(value_lbl3, "115");
    lv_style_set_text_color(&style_value, lv_color_white());
    lv_style_set_text_font(&style_value, &inter_bold_42);
    lv_obj_add_style(value_lbl3, &style_value, 0);
    int val3_x = 28;
    int val3_y = 46;
    lv_obj_set_pos(value_lbl3, val3_x, val3_y);

    lv_obj_t *unit_lbl3 = lv_label_create(cards[2]);
    m1_tvoc_unit_lbl = unit_lbl3;
    lv_label_set_text(unit_lbl3, "ppb");
    lv_style_set_text_color(&style_title, lv_color_hex(0xA7A7A7));
    lv_style_set_text_font(&style_title, &inter_regular_17);
    lv_obj_add_style(unit_lbl3, &style_title, 0);
    int val3_w = lv_txt_get_width("115", 4, &inter_bold_42, 0, LV_TEXT_FLAG_NONE);
    int val3_h = lv_font_get_line_height(&inter_bold_42);
    int unit3_h = lv_font_get_line_height(&inter_regular_17);
    int unit3_x = val3_x + val3_w + 8;
    int unit3_y = (val3_y + val3_h) - unit3_h - 6;
    lv_obj_set_pos(unit_lbl3, unit3_x, unit3_y);

    /** Card 3 CO2 **/
    lv_obj_t *title_lbl4 = lv_label_create(cards[3]);
    lv_label_set_text(title_lbl4, "CO2");
    lv_style_set_text_color(&style_title, lv_color_hex(0xA7A7A7));
    lv_style_set_text_font(&style_title, &inter_regular_18);
    lv_obj_add_style(title_lbl4, &style_title, 0);
    lv_obj_set_pos(title_lbl4, 28, 17);

    lv_obj_t *value_lbl4 = lv_label_create(cards[3]);
    m1_co2_val_lbl = value_lbl4;
    lv_label_set_text(value_lbl4, "500");
    lv_style_set_text_color(&style_value, lv_color_white());
    lv_style_set_text_font(&style_value, &inter_bold_42);
    lv_obj_add_style(value_lbl4, &style_value, 0);
    int val4_x = 28;
    int val4_y = 46;
    lv_obj_set_pos(value_lbl4, val4_x, val4_y);

    lv_obj_t *unit_lbl4 = lv_label_create(cards[3]);
    m1_co2_unit_lbl = unit_lbl4;
    lv_label_set_text(unit_lbl4, "ppm");
    lv_style_set_text_color(&style_title, lv_color_hex(0xA7A7A7));
    lv_style_set_text_font(&style_title, &inter_regular_17);
    lv_obj_add_style(unit_lbl4, &style_title, 0);
    int val4_w = lv_txt_get_width("500", 4, &inter_bold_42, 0, LV_TEXT_FLAG_NONE);
    int val4_h = lv_font_get_line_height(&inter_bold_42);
    int unit4_h = lv_font_get_line_height(&inter_regular_17);
    int unit4_x = val4_x + val4_w + 8;
    int unit4_y = (val4_y + val4_h) - unit4_h - 6;
    lv_obj_set_pos(unit_lbl4, unit4_x, unit4_y);
}

/*---------------------------------------------------------------------------
 * lvgl_metrics1_update — update Metrics1 labels from sensor data
 *---------------------------------------------------------------------------*/
void lvgl_metrics1_update(const sensor_data_t *data)
{
    if (!data) return;

    /* Card 0: Air Index */
    if (m1_air_index_lbl)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%u%%", (unsigned)data->air_index);
        lv_label_set_text(m1_air_index_lbl, buf);
    }

    /* Card 1: PM2.5 value + reposition unit */
    if (m1_pm25_val_lbl && m1_pm25_unit_lbl)
    {
        char buf[16];
        int len = snprintf(buf, sizeof(buf), "%u", (unsigned)data->pm2_5);
        lv_label_set_text(m1_pm25_val_lbl, buf);
        /* Reposition unit label after value */
        int val_w = lv_txt_get_width(buf, len, &inter_bold_42, 0, LV_TEXT_FLAG_NONE);
        int val_h = lv_font_get_line_height(&inter_bold_42);
        int unit_h = lv_font_get_line_height(&inter_regular_17);
        int unit_x = 28 + val_w + 8;
        int unit_y = (46 + val_h) - unit_h - 6;
        lv_obj_set_pos(m1_pm25_unit_lbl, unit_x, unit_y);
    }

    /* Card 2: TVOC value + reposition unit */
    if (m1_tvoc_val_lbl && m1_tvoc_unit_lbl)
    {
        char buf[16];
        int len = snprintf(buf, sizeof(buf), "%u", (unsigned)data->tvoc);
        lv_label_set_text(m1_tvoc_val_lbl, buf);
        int val_w = lv_txt_get_width(buf, len, &inter_bold_42, 0, LV_TEXT_FLAG_NONE);
        int val_h = lv_font_get_line_height(&inter_bold_42);
        int unit_h = lv_font_get_line_height(&inter_regular_17);
        int unit_x = 28 + val_w + 8;
        int unit_y = (46 + val_h) - unit_h - 6;
        lv_obj_set_pos(m1_tvoc_unit_lbl, unit_x, unit_y);
    }

    /* Card 3: CO2 value + reposition unit */
    if (m1_co2_val_lbl && m1_co2_unit_lbl)
    {
        char buf[16];
        int len = snprintf(buf, sizeof(buf), "%u", (unsigned)data->co2);
        lv_label_set_text(m1_co2_val_lbl, buf);
        int val_w = lv_txt_get_width(buf, len, &inter_bold_42, 0, LV_TEXT_FLAG_NONE);
        int val_h = lv_font_get_line_height(&inter_bold_42);
        int unit_h = lv_font_get_line_height(&inter_regular_17);
        int unit_x = 28 + val_w + 8;
        int unit_y = (46 + val_h) - unit_h - 6;
        lv_obj_set_pos(m1_co2_unit_lbl, unit_x, unit_y);
    }
}

void lvgl_metrics2_create(lv_obj_t *scr)
{
    lv_obj_t *src = scr;
    lv_obj_set_style_bg_color(src, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(src, LV_OPA_COVER, 0);

    lv_obj_t *rect = lv_obj_create(src);
    lv_obj_set_size(rect, 320, 480);
    lv_obj_align(rect, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(rect, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rect, 1, 0);
    lv_obj_set_style_border_color(rect, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_border_opa(rect, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(rect, 0, 0);
    lv_obj_set_style_pad_all(rect, 0, 0);
    lv_obj_add_flag(rect, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    /* Four rounded cards filled vertically */
    static lv_style_t style_card;
    lv_style_init(&style_card);
    lv_style_set_radius(&style_card, 12);
    lv_style_set_bg_color(&style_card, lv_color_hex(0x1A1A1A));
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_border_width(&style_card, 0);
    lv_style_set_shadow_opa(&style_card, 0);

    const int n = 4;
    const int card_w = 288;
    const int card_x = 16;
    const int card_gap = 8;
    const int card_1_y = 16;
    int card_h = (480 - 32 - (n - 1) * card_gap) / n;

    lv_obj_t *cards[n];
    for (int i = 0; i < n; i++)
    {
        lv_obj_t *card = lv_obj_create(rect);
        lv_obj_remove_style_all(card);
        lv_obj_add_style(card, &style_card, 0);
        lv_obj_set_size(card, card_w, card_h);
        lv_obj_set_pos(card, card_x, card_1_y + i * (card_h + card_gap));
        cards[i] = card;
    }

    /*---------------------------------------------------------------------------
     * Card 0 — "Current Air Quality"
     *   Copy of metrics1 card 0 content
     *---------------------------------------------------------------------------
     */
    lv_obj_t *title_lbl = lv_label_create(cards[0]);
    lv_label_set_text(title_lbl, "Current Air Quality");
    static lv_style_t style_title;
    lv_style_init(&style_title);
    lv_style_set_text_color(&style_title, lv_color_hex(0xA7A7A7));
    lv_style_set_text_font(&style_title, &inter_regular_18);
    lv_obj_add_style(title_lbl, &style_title, 0);
    lv_obj_set_pos(title_lbl, 28, 13);

    lv_obj_t *value_lbl = lv_label_create(cards[0]);
    m2_air_index_lbl = value_lbl;
    lv_label_set_text(value_lbl, "98%");
    static lv_style_t style_value;
    lv_style_init(&style_value);
    lv_style_set_text_color(&style_value, lv_color_white());
    lv_style_set_text_font(&style_value, &inter_bold_50);
    lv_obj_add_style(value_lbl, &style_value, 0);
    int title_y = 13;
    int title_h = lv_obj_get_height(title_lbl);
    int area_top = title_y + title_h;
    int area_bottom = card_h;
    int val_h = lv_obj_get_height(value_lbl);
    int val_y = (area_top + area_bottom - val_h) / 2;
    val_y -= 10;
    if (val_y < area_top) val_y = area_top + 4;
    lv_obj_set_pos(value_lbl, 28, val_y);

    /*---------------------------------------------------------------------------
     * Right-side icons on cards[1]/[2]/[3] — 53x53, same position as metrics1.
     *---------------------------------------------------------------------------
     */
    const int img_w = 53;
    const int img_h = 53;
    int icon_x = card_x + (card_w - 28 - img_w);

    /* Card 1 icon: temperature */
    lv_obj_t *img1 = lv_img_create(rect);
    lv_img_set_src(img1, &icon_temperature);
    int y1 = card_1_y + 1 * (card_h + card_gap) + (card_h - img_h) / 2;
    lv_obj_set_pos(img1, icon_x, y1);
    lv_obj_move_foreground(img1);

    /* Card 2 icon: humidity */
    lv_obj_t *img2 = lv_img_create(rect);
    lv_img_set_src(img2, &icon_humidity);
    int y2 = card_1_y + 2 * (card_h + card_gap) + (card_h - img_h) / 2;
    lv_obj_set_pos(img2, icon_x, y2);
    lv_obj_move_foreground(img2);

    /* Card 3 icon: air pressure */
    lv_obj_t *img3 = lv_img_create(rect);
    lv_img_set_src(img3, &icon_air_pressure);
    int y3 = card_1_y + 3 * (card_h + card_gap) + (card_h - img_h) / 2;
    lv_obj_set_pos(img3, icon_x, y3);
    lv_obj_move_foreground(img3);

    /*---------------------------------------------------------------------------
     * Card 1 — Temperature 21°C
     *---------------------------------------------------------------------------
     */
    lv_obj_t *t1 = lv_label_create(cards[1]);
    lv_label_set_text(t1, "Temperature");
    lv_style_set_text_color(&style_title, lv_color_hex(0xA7A7A7));
    lv_style_set_text_font(&style_title, &inter_regular_18);
    lv_obj_add_style(t1, &style_title, 0);
    lv_obj_set_pos(t1, 28, 17);

    lv_obj_t *v1 = lv_label_create(cards[1]);
    m2_temp_val_lbl = v1;
    lv_label_set_text(v1, "21");
    lv_style_set_text_color(&style_value, lv_color_white());
    lv_style_set_text_font(&style_value, &inter_bold_42);
    lv_obj_add_style(v1, &style_value, 0);
    int v1_x = 28;
    int v1_y = 46;
    lv_obj_set_pos(v1, v1_x, v1_y);

    lv_obj_t *u1 = lv_label_create(cards[1]);
    m2_temp_unit_lbl = u1;
    lv_label_set_text(u1, "°C");
    lv_style_set_text_color(&style_title, lv_color_hex(0xA7A7A7));
    lv_style_set_text_font(&style_title, &inter_regular_17);
    lv_obj_add_style(u1, &style_title, 0);
    int v1_w = lv_txt_get_width("21", 3, &inter_bold_42, 0, LV_TEXT_FLAG_NONE);
    int v1_h = lv_font_get_line_height(&inter_bold_42);
    int u1_h = lv_font_get_line_height(&inter_regular_17);
    int u1_x = v1_x + v1_w + 8;
    int u1_y = (v1_y + v1_h) - u1_h - 6;
    lv_obj_set_pos(u1, u1_x, u1_y);

    /*---------------------------------------------------------------------------
     * Card 2 — Humidity 25%
     *---------------------------------------------------------------------------
     */
    lv_obj_t *t2 = lv_label_create(cards[2]);
    lv_label_set_text(t2, "Humidity");
    lv_style_set_text_color(&style_title, lv_color_hex(0xA7A7A7));
    lv_style_set_text_font(&style_title, &inter_regular_18);
    lv_obj_add_style(t2, &style_title, 0);
    lv_obj_set_pos(t2, 28, 17);

    lv_obj_t *v2 = lv_label_create(cards[2]);
    m2_humid_val_lbl = v2;
    lv_label_set_text(v2, "25");
    lv_style_set_text_color(&style_value, lv_color_white());
    lv_style_set_text_font(&style_value, &inter_bold_42);
    lv_obj_add_style(v2, &style_value, 0);
    int v2_x = 28;
    int v2_y = 46;
    lv_obj_set_pos(v2, v2_x, v2_y);

    lv_obj_t *u2 = lv_label_create(cards[2]);
    m2_humid_unit_lbl = u2;
    lv_label_set_text(u2, "%");
    lv_style_set_text_color(&style_title, lv_color_hex(0xA7A7A7));
    lv_style_set_text_font(&style_title, &inter_regular_17);
    lv_obj_add_style(u2, &style_title, 0);
    int v2_w = lv_txt_get_width("25", 3, &inter_bold_42, 0, LV_TEXT_FLAG_NONE);
    int v2_h = lv_font_get_line_height(&inter_bold_42);
    int u2_h = lv_font_get_line_height(&inter_regular_17);
    int u2_x = v2_x + v2_w + 8;
    int u2_y = (v2_y + v2_h) - u2_h - 6;
    lv_obj_set_pos(u2, u2_x, u2_y);

    /*---------------------------------------------------------------------------
     * Card 3 — Air Pressure 995
     *---------------------------------------------------------------------------
     */
    lv_obj_t *t3 = lv_label_create(cards[3]);
    lv_label_set_text(t3, "Air Pressure");
    lv_style_set_text_color(&style_title, lv_color_hex(0xA7A7A7));
    lv_style_set_text_font(&style_title, &inter_regular_18);
    lv_obj_add_style(t3, &style_title, 0);
    lv_obj_set_pos(t3, 28, 17);

    lv_obj_t *v3 = lv_label_create(cards[3]);
    m2_press_val_lbl = v3;
    lv_label_set_text(v3, "995");
    lv_style_set_text_color(&style_value, lv_color_white());
    lv_style_set_text_font(&style_value, &inter_bold_42);
    lv_obj_add_style(v3, &style_value, 0);
    int v3_x = 28;
    int v3_y = 46;
    lv_obj_set_pos(v3, v3_x, v3_y);

    /* Unit label for Air Pressure (hPa) */
    lv_obj_t *u3 = lv_label_create(cards[3]);
    m2_press_unit_lbl = u3;
    lv_label_set_text(u3, "hPa");
    lv_style_set_text_color(&style_title, lv_color_hex(0xA7A7A7));
    lv_style_set_text_font(&style_title, &inter_regular_17);
    lv_obj_add_style(u3, &style_title, 0);
    int v3_w = lv_txt_get_width("995", 4, &inter_bold_42, 0, LV_TEXT_FLAG_NONE);
    int v3_h = lv_font_get_line_height(&inter_bold_42);
    int u3_h = lv_font_get_line_height(&inter_regular_17);
    int u3_x = v3_x + v3_w + 8;
    int u3_y = (v3_y + v3_h) - u3_h - 6;
    lv_obj_set_pos(u3, u3_x, u3_y);
}

/*---------------------------------------------------------------------------
 * lvgl_metrics2_update — update Metrics2 labels from sensor data
 *---------------------------------------------------------------------------*/
void lvgl_metrics2_update(const sensor_data_t *data)
{
    if (!data) return;

    /* Card 0: Air Index */
    if (m2_air_index_lbl)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%u%%", (unsigned)data->air_index);
        lv_label_set_text(m2_air_index_lbl, buf);
    }

    /* Card 1: Temperature value + reposition unit */
    if (m2_temp_val_lbl && m2_temp_unit_lbl)
    {
        char buf[16];
        int len = snprintf(buf, sizeof(buf), "%u", (unsigned)data->temperature);
        lv_label_set_text(m2_temp_val_lbl, buf);
        int val_w = lv_txt_get_width(buf, len, &inter_bold_42, 0, LV_TEXT_FLAG_NONE);
        int val_h = lv_font_get_line_height(&inter_bold_42);
        int unit_h = lv_font_get_line_height(&inter_regular_17);
        int unit_x = 28 + val_w + 8;
        int unit_y = (46 + val_h) - unit_h - 6;
        lv_obj_set_pos(m2_temp_unit_lbl, unit_x, unit_y);
    }

    /* Card 2: Humidity value + reposition unit */
    if (m2_humid_val_lbl && m2_humid_unit_lbl)
    {
        char buf[16];
        int len = snprintf(buf, sizeof(buf), "%u", (unsigned)data->humidity);
        lv_label_set_text(m2_humid_val_lbl, buf);
        int val_w = lv_txt_get_width(buf, len, &inter_bold_42, 0, LV_TEXT_FLAG_NONE);
        int val_h = lv_font_get_line_height(&inter_bold_42);
        int unit_h = lv_font_get_line_height(&inter_regular_17);
        int unit_x = 28 + val_w + 8;
        int unit_y = (46 + val_h) - unit_h - 6;
        lv_obj_set_pos(m2_humid_unit_lbl, unit_x, unit_y);
    }

    /* Card 3: Pressure value + reposition unit */
    if (m2_press_val_lbl && m2_press_unit_lbl)
    {
        char buf[16];
        int len = snprintf(buf, sizeof(buf), "%u", (unsigned)data->pressure);
        lv_label_set_text(m2_press_val_lbl, buf);
        int val_w = lv_txt_get_width(buf, len, &inter_bold_42, 0, LV_TEXT_FLAG_NONE);
        int val_h = lv_font_get_line_height(&inter_bold_42);
        int unit_h = lv_font_get_line_height(&inter_regular_17);
        int unit_x = 28 + val_w + 8;
        int unit_y = (46 + val_h) - unit_h - 6;
        lv_obj_set_pos(m2_press_unit_lbl, unit_x, unit_y);
    }
}