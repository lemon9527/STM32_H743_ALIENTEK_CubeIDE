# LVGL 学习计划

## 背景

当前项目已有：
- LTDC 双 Layer 硬件叠加（动画 Layer 1 + UI Layer 2，不闪烁）
- 320x320 20fps 动画循环播放
- Layer 2 上预渲染了 UI 元素（bottom_bar、clean_text、矩形框、标题文字、帧号）
- LVGL 源码已集成在项目中（`Middlewares/Third_Party/LVGL/`）
- LVGL 显示驱动（`lv_port_disp.c`）和输入驱动（`lv_port_indev.c`）已存在

## 目标

通过 KEY0 按键在"动画演示页面"和"LVGL 实验页面"之间切换，逐步学习 LVGL 的核心功能。

---

## 第一阶段：页面切换骨架

**目标**：实现 KEY0 按键切换，两个页面互不干扰。

### 任务 1.1：确认 LVGL 初始化状态
- [ ] 检查 `lv_port_disp.c`：确认 LVGL 的 flush 回调当前是否对接 LTDC Layer 2
- [ ] 检查 `lv_port_indev.c`：确认 KEY0 按键是否已注册为 LVGL 输入设备
- [ ] 确认 `lv_init()` 是否在 `main()` 中调用
- [ ] 确认 LVGL 心跳（`lv_tick_inc()`）是否在 SysTick 或 TIM 中断中调用

### 任务 1.2：页面状态管理
- [ ] 定义 `page_state` 枚举：`PAGE_ANIMATION` / `PAGE_LVGL`
- [ ] 在 `StartAnimationTask` 中检测 `page_state`：
  - `PAGE_ANIMATION`：当前动画循环正常运行
  - `PAGE_LVGL`：暂停动画，显示 LVGL 页面
- [ ] KEY0 按下时切换 `page_state`，并通知动画任务

### 任务 1.3：LVGL 显示内存规划
LVGL 需要自己的显示缓冲区。当前 SDRAM 使用情况：

| 地址 | 用途 | 大小 |
|------|------|------|
| 0xC0000000 | 动画前帧缓冲 | 1600x480x2 = 1.5MB |
| 0xC0180000 | 动画后帧缓冲 | 1600x480x2 = 1.5MB |
| 0xC0300000 | UI Layer 2 缓冲 | 800x480x2 = 768KB |
| 0xC03C0000 | **空闲** | ~28.5MB |

LVGL 显示缓冲区可放在空闲区域，例如 `0xC03C0000`。

- [ ] 确认 LVGL 缓冲区大小（建议 1/10 屏幕大小，即 480x80x2 = 76800 字节，或全屏 1/5）
- [ ] 配置 `lv_port_disp.c` 使用双缓冲或单缓冲 + 全刷新

---

## 第二阶段：LVGL 基础控件

**目标**：在一个空白页面上显示 LVGL 的各种基础控件，熟悉 API。

### 任务 2.1：Hello World — 标签 (Label)
- [ ] 创建一个 `lv_label`，显示 "Hello, LVGL!"
- [ ] 设置字体、颜色、对齐方式
- [ ] 学习 `lv_label_set_text()`、`lv_obj_set_style_text_color()`、`lv_obj_align()`

### 任务 2.2：按钮 (Button)
- [ ] 创建一个 `lv_btn`，绑定点击回调
- [ ] 点击时切换标签的文本（例如 "Button pressed!"）
- [ ] 学习 `lv_btn_create()`、`lv_obj_add_event_cb()`

### 任务 2.3：进度条 (Bar)
- [ ] 创建一个 `lv_bar`，用定时器递增其值（0→100 循环）
- [ ] 学习 `lv_bar_create()`、`lv_bar_set_value()`

### 任务 2.4：滑块 (Slider)
- [ ] 创建一个 `lv_slider`，拖动时实时更新标签显示数值
- [ ] 学习 `lv_slider_create()`、`LV_EVENT_VALUE_CHANGED`

### 任务 2.5：复选框 (Checkbox)
- [ ] 创建一个 `lv_checkbox`，状态变化时更新标签
- [ ] 学习 `lv_checkbox_create()`、`lv_obj_add_state()` / `lv_obj_clear_state()`

### 任务 2.6：开关 (Switch)
- [ ] 创建一个 `lv_switch`，开关状态影响背景色
- [ ] 学习 `lv_switch_create()`

---

## 第三阶段：布局与容器

**目标**：学习 LVGL 的布局系统，让界面更工整。

### 任务 3.1：Flex 布局
- [ ] 在容器中使用 `lv_obj_set_flex_flow()` 横向/纵向排列多个控件
- [ ] 学习 `LV_FLEX_FLOW_ROW_WRAP`、`LV_FLEX_FLOW_COLUMN`

### 任务 3.2：网格布局 (Grid)
- [ ] 使用 `lv_obj_set_grid_dsc_array()` 创建 3x3 网格
- [ ] 在每个格子中放置不同颜色的方块
- [ ] 学习 `lv_obj_set_grid_cell()`

### 任务 3.3：页面滚动 (lv_page / lv_roller)
- [ ] 创建一个可以滚动的列表（`lv_list` 或 `lv_roller`）
- [ ] 学习滚动事件的监听

---

## 第四阶段：高级控件

**目标**：学习更复杂的控件，接近实际应用。

### 任务 4.1：下拉列表 (Dropdown)
- [ ] 创建一个 `lv_dropdown`，选择项后更新标签
- [ ] 学习 `lv_dropdown_create()`、`lv_dropdown_get_selected()`

### 任务 4.2：文本输入 (Textarea)
- [ ] 创建一个 `lv_textarea`，显示键盘输入
- [ ] 学习 `lv_textarea_create()`、`lv_textarea_set_text()`

### 任务 4.3：日历 (Calendar)
- [ ] 创建一个 `lv_calendar`，显示当前日期
- [ ] 学习 `lv_calendar_create()`、`lv_calendar_set_today_date()`

### 任务 4.4：图表 (Chart)
- [ ] 创建一个 `lv_chart`，显示折线图或柱状图
- [ ] 学习 `lv_chart_create()`、`lv_chart_add_series()`、`lv_chart_set_next_value()`

### 任务 4.5：动画 (lv_anim)
- [ ] 使用 `lv_anim` 对一个对象进行位置/透明度/大小动画
- [ ] 学习 `lv_anim_set_*()` 系列函数

---

## 第五阶段：样式与主题

**目标**：美化界面，学习 LVGL 的样式系统。

### 任务 5.1：自定义样式
- [ ] 创建 `lv_style_t`，设置边框、圆角、阴影、背景渐变
- [ ] 应用到按钮和标签上

### 任务 5.2：状态样式
- [ ] 为按钮的 `LV_STATE_PRESSED`、`LV_STATE_DISABLED` 设置不同样式

### 任务 5.3：主题切换
- [ ] 定义暗色/亮色两套主题，通过按钮切换
- [ ] 学习 `lv_theme_default_init()` 或 `lv_theme_mono_init()`

---

## 第六阶段：综合练习

**目标**：综合运用所学知识，实现一个实用的小工具。

### 任务 6.1：简易仪表盘
- [ ] 显示 CPU 频率、SDRAM 使用率、帧率等系统信息
- [ ] 使用仪表（`lv_meter`）或进度条展示
- [ ] 每 500ms 更新一次数据

### 任务 6.2：图片显示
- [ ] 将一张图片转换为 LVGL 的 C 数组格式
- [ ] 使用 `lv_img` 显示在屏幕上
- [ ] 学习 `lv_img_create()`、`lv_img_set_src()`

---

## 项目文件清单

学习过程中需要修改/创建的文件：

| 文件 | 说明 |
|------|------|
| `Core/Src/animation.c` | 添加页面状态切换逻辑，KEY0 回调 |
| `Core/Src/lv_port_disp.c` | 确认/修改 LVGL 显示驱动 |
| `Core/Src/lv_port_indev.c` | 确认/修改 KEY0 按键驱动 |
| `Core/Src/main.c` | 确认 LVGL 初始化 |
| `Core/Src/freertos.c` | 可能需要创建 LVGL 任务 |
| `Core/Inc/animation.h` | 添加页面状态枚举声明 |

---

## 注意事项

1. **LTDC 双 Layer 共存**：LVGL 绘制到自己的缓冲区后，通过 DMA2D 拷贝到 Layer 2。不能影响 Layer 1 的动画。
2. **切换页面时**：切换到 LVGL 页面时暂停动画（停止定时器或跳过帧），切回动画时恢复。
3. **内存管理**：LVGL 默认使用 `malloc`/`free`，确认 heap 空间足够（建议在 RAM_D1 中，当前链接脚本已配置）。
4. **心跳**：LVGL 需要 1-5ms 的 `lv_tick_inc()` 调用，确认当前 SysTick 或 TIM 中断中已实现。
5. **KEY0 防抖**：如果 KEY0 在 `lv_port_indev.c` 中作为 LVGL 输入设备注册，可复用 LVGL 的防抖逻辑；否则需要手动防抖。