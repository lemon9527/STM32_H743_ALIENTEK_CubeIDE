# LVGL v8.4 集成计划

## Context

在 STM32H743 项目中集成 LVGL v8.4 图形库，当前项目已有 LTDC（800×480 RGB565）+ DMA2D + SDRAM + FreeRTOS 基础。LVGL 将接管 LCD 显示，提供完整的 UI 控件框架。

## 架构概览

```
LVGL Core (渲染、控件、旋转)
    |
    v
lv_port_disp.c (flush_cb 通过 DMA2D M2M 拷贝)
    |
    v
物理 Framebuffer (SDRAM 0xC0000000, 800x480 RGB565, stride=1600)
    |
    v
LTDC 硬件输出到 LCD 面板
```

- **LVGL 配置为 800×480（物理分辨率）**
- **`lv_disp_set_rotation(disp, LV_DISP_ROT_90)` 实现竖屏（480×800 逻辑坐标）**
- **flush_cb 收到的是 LVGL 已旋转好的数据，DMA2D 直接拷贝，无需坐标变换**
- **LVGL 双缓冲在 SDRAM（0xC1000000），每个 480×80 像素**

## 文件变更清单

### 新增文件

| 文件 | 说明 |
|------|------|
| `Middlewares/Third_Party/LVGL/lvgl.h` | LVGL 主头文件（从 release 复制） |
| `Middlewares/Third_Party/LVGL/lv_conf.h` | LVGL 配置（从模板修改） |
| `Middlewares/Third_Party/LVGL/src/` | LVGL 全部源码（~80 个 .c 文件） |
| `Core/Inc/lv_port_disp.h` | 显示驱动头文件 |
| `Core/Src/lv_port_disp.c` | 显示驱动（DMA2D flush） |
| `Core/Inc/lv_port_indev.h` | 输入驱动头文件 |
| `Core/Src/lv_port_indev.c` | 输入驱动（4 按键映射） |
| `Debug/Middlewares/Third_Party/LVGL/Source/subdir.mk` | LVGL 构建规则 |

### 修改文件

| 文件 | 改动 |
|------|------|
| `Core/Src/freertos.c` | 新增 LVGL 任务 + tick 定时器 + demo |
| `Core/Inc/FreeRTOSConfig.h` | `configTOTAL_HEAP_SIZE` 15KB → 30KB |
| `Debug/Core/Src/subdir.mk` | 添加 lv_port_disp.c / lv_port_indev.c 和 LVGL include path |
| `Debug/sources.mk` | 添加 `Middlewares/Third_Party/LVGL/Source` |
| `Debug/makefile` | 添加 `-include Middlewares/Third_Party/LVGL/Source/subdir.mk` |

### 不修改的文件

- `lcd.c` / `lcd.h` — 保留不动
- `dma2d.c` / `ltdc.c` / `main.c` / `gpio.c` — 无需改动

## 关键配置

### lv_conf.h
- `LV_COLOR_DEPTH = 16`（RGB565）
- `LV_MEM_SIZE = 64KB`（LVGL 内置内存池）
- `LV_TICK_CUSTOM = 1`（外部提供 tick）
- `LV_USE_GPU_STM32_DMA2D = 0`（手动在 flush_cb 中调用 DMA2D）
- 启用 Montserrat 14/24/24 字体 + 所有控件和 extra 组件

### lv_port_disp.c
- 双缓冲：`disp_buf1` / `disp_buf2` 各 480×80 = 38,400 像素，固定地址 0xC1000000 / 0xC1012C00
- flush_cb：DMA2D M2M 拷贝，OutputOffset = LCD_FB_STRIDE - w，~1ms 完成
- 显示注册后调用 `lv_disp_set_rotation(disp, LV_DISP_ROT_90)`

### lv_port_indev.c
- 4 按键映射：
  - WK_UP (PA0) → LV_KEY_UP
  - KEY0 (PH3) → LV_KEY_ENTER
  - KEY1 (PH2) → LV_KEY_LEFT
  - KEY2 (PC12) → LV_KEY_RIGHT
- 简单消抖：连续两次读到相同键值才上报

### FreeRTOS 改动
- 新增 LVGL 任务：stack 8KB，优先级 Normal
- 新增 1ms 周期定时器调用 `lv_tick_inc(1)`
- `StartDefaultTask` 保留 LED 闪烁，移除 LCD 颜色切换（LVGL 接管）

## 实现步骤

1. 下载 LVGL v8.4.0 release 并解压到 `Middlewares/Third_Party/LVGL/`
2. 复制 `lv_conf_template.h` → `lv_conf.h`，修改配置
3. 创建 `lv_port_disp.c/h`（DMA2D flush + 双缓冲 + 旋转）
4. 创建 `lv_port_indev.c/h`（4 按键）
5. 创建 `Debug/Middlewares/Third_Party/LVGL/Source/` 目录结构
6. 创建 `Debug/Middlewares/Third_Party/LVGL/Source/subdir.mk`（~80 个源文件）
7. 修改 `Debug/Core/Src/subdir.mk`（添加 port 文件 + LVGL include path）
8. 修改 `Debug/sources.mk` + `Debug/makefile`
9. 修改 `Core/Src/freertos.c`（LVGL 任务 + 定时器 + demo）
10. 修改 `FreeRTOSConfig.h`（堆大小）
11. 编译验证

## 验证方法

1. 编译应无错误
2. 烧录后串口输出系统时钟和 SDRAM 测试（同之前）
3. LCD 显示 LVGL 自定义 demo（"Hello LVGL v8.4!" + 一个 Button）
4. 按键可导航 LVGL 控件
5. LED (PB0) 持续闪烁

## 注意事项

- **SDRAM 全区域 MPU 配置为 Non-Cacheable**，无需处理 D-Cache 一致性问题
- **DMA2D 模式切换**：flush_cb 和 LCD_Clear 都切换 DMA2D 模式，但 LVGL 接管后 LCD_Clear 不再被调用，无冲突
- **LVGL 缓冲区放在 SDRAM 固定地址**，不占用内部 RAM
- 如果后续需要更好性能，可将 SDRAM 拆分为 Non-Cacheable（framebuffer）+ Cacheable WB（LVGL buffer），flush 前 Clean DCache