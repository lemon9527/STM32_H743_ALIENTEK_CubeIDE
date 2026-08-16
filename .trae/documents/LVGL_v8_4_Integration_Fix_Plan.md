# LVGL v8.4 集成修复计划

## 1. 摘要

LVGL 集成的大部分工作已完成（源码、配置、display/input 驱动、构建系统），但 `freertos.c` 中缺少 `StartLVGLTask` 和 `lvgl_tick_timer_cb` 两个函数的**定义**（仅声明了原型）。需要补全这两个函数并编译验证。

## 2. 当前状态分析

| 组件 | 状态 | 说明 |
|------|------|------|
| LVGL 源码 (`Middlewares/Third_Party/LVGL/src/`) | 已就绪 | 所有 core/draw/extra/font/hal/misc/widgets 文件齐全 |
| `lv_conf.h` | 已就绪 | RGB565, 800x480, 64KB 内存池, 自定义 tick |
| `lv_port_disp.c/h` | 已就绪 | DMA2D M2M flush, 双缓冲 (SDRAM), 90° 旋转 |
| `lv_port_indev.c/h` | 已就绪 | KEY0 → LV_KEY_ENTER 按键映射 |
| 构建系统 (subdir.mk, sources.mk, makefile) | 已就绪 | LVGL 121 个源文件 + demo 已加入编译 |
| `FreeRTOSConfig.h` | 已就绪 | `configTOTAL_HEAP_SIZE = 30720` (30KB) |
| `freertos.c` | **不完整** | 原型已声明，但函数体缺失 |

## 3. 需要修改的文件

### 3.1 `Core/Src/freertos.c` — 补全 LVGL 任务函数

**现状：**
- 第 54 行：`static osTimerId_t lvgl_tick_timer_id;` 已声明
- 第 63-69 行：`lvglTask` 任务属性已定义（8KB 栈）
- 第 73-74 行：函数原型已声明
- 第 101-102 行：定时器已创建并启动（1ms 周期）
- 第 115 行：`lvglTaskHandle` 已创建
- **第 207 行之后：只有空的 `/* USER CODE END Application */`，缺少函数定义**

**修改：** 在 `/* USER CODE BEGIN Application */` 和 `/* USER CODE END Application */` 之间添加两个函数定义：

```c
/* LVGL tick timer callback - called every 1ms */
static void lvgl_tick_timer_cb(void *argument)
{
    (void)argument;
    lv_tick_inc(1);
}

/* LVGL task - initializes LVGL and runs the widgets demo */
void StartLVGLTask(void *argument)
{
    (void)argument;

    lv_init();                  /* Initialize LVGL core */
    lv_port_disp_init();        /* Initialize display driver (DMA2D + rotation) */
    lv_port_indev_init();       /* Initialize input driver (KEY0) */
    lv_demo_widgets();          /* Launch LVGL built-in widgets demo */

    for (;;)
    {
        lv_timer_handler();     /* Let LVGL handle its timers */
        osDelay(5);             /* 5ms tick → ~200Hz refresh */
    }
}
```

## 4. 不需要修改的内容

- `lv_conf.h` — 配置正确，无需改动
- `lv_port_disp.c` / `lv_port_indev.c` — 实现正确，无需改动
- 构建系统文件 — 已正确配置，无需改动
- `FreeRTOSConfig.h` — 堆大小 30KB 已足够，无需改动
- `defaultTask` — 已正确调用 `LCD_Init()` 并保持 LED 闪烁，无需改动

## 5. 验证步骤

1. 编译项目：`cd Debug && make clean && make -j8`
2. 确认无编译错误
3. 烧录到开发板：
   ```bash
   stm32flash -w Debug/STM32_H743_ALIENTEK_CubeIDE.hex -b 115200 /dev/tty.usbserial-2110 -v -g 0
   ```
4. 预期行为：
   - 串口输出系统时钟和 SDRAM 测试信息
   - 屏幕显示 LVGL widgets demo 界面
   - 按下 KEY0 可在 LVGL UI 中导航
   - 绿色 LED (PB0) 继续以 500ms 周期闪烁