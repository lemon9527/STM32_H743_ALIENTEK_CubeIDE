# STM32H743 正点原子开发板 Study 项目

基于正点原子 STM32H743IIT6 开发板的学习项目，使用 FreeRTOS 实时操作系统，基于 CubeIDE 开发，逐步学习各个外设。

## 当前进度

| 外设 | 功能 | 引脚 | 状态 |
|------|------|------|------|
| GPIO | LED1 Green | PB0 | 已启用 |
| GPIO | LED0 Red | PB1 | 已启用 |
| GPIO | LCD Backlight | PB5 | 已启用 |
| USART1 | 调试串口 (printf) | PA9 (TX), PA10 (RX) | 已启用 |
| TIM7 | HAL 系统时基 | 内部 | 已启用 |
| FreeRTOS | CMSIS-RTOS V2 | - | 已启用 |
| FMC SDRAM | W9825G6KH (32MB) | FMC Bank1 | 已启用 |
| LTDC | 4.3" RGB LCD (800x480) + 双 Layer | 28-pin TTL | 已启用 |
| DMA2D | 图形加速 (R2M/M2M) | 内部 | 已启用 |
| QSPI | W25Q256 (32MB) + Memory-Mapped | PB2/PB6/PF6-PF9 | 已启用 |

## 时钟树

```
HSE (25MHz)
  └── PLL1: /5 ×160 /2 = 400MHz (SYSCLK)
       ├── SYSCLK: /1 = 400MHz
       ├── HCLK:   /2 = 200MHz
       ├── APB1:   /2 = 100MHz
       ├── APB2:   /2 = 100MHz
       ├── APB3:   /2 = 100MHz
       └── APB4:   /2 = 100MHz
```

## 内存布局

STM32H743IIT6 片上内存：

| 区域 | 大小 | 起始地址 | 用途 |
|------|------|----------|------|
| ITCMRAM | 64KB | 0x00000000 | 指令紧耦合内存 |
| FLASH | 2048KB | 0x08000000 | 程序存储 |
| DTCMRAM | 128KB | 0x20000000 | 数据紧耦合内存 |
| RAM_D1 | 512KB | 0x24000000 | 域1 SRAM (data/bss/heap/stack) |
| RAM_D2 | 288KB | 0x30000000 | 域2 SRAM |
| RAM_D3 | 64KB | 0x38000000 | 域3 SRAM |

当前链接脚本使用 RAM_D1 作为数据段、BSS 段、堆和栈的存放位置。

## 项目结构

```
STM32_H743_ALIENTEK_CubeIDE/
├── Core/
│   ├── Inc/                        # 头文件
│   │   ├── main.h                  # 主头文件
│   │   ├── gpio.h                  # GPIO 配置
│   │   ├── usart.h                 # USART 配置
│   │   ├── lcd.h                   # LCD 驱动接口
│   │   ├── animation.h             # 动画引擎接口
│   │   ├── lv_port_disp.h          # LVGL 显示驱动接口
│   │   ├── lv_port_indev.h         # LVGL 输入驱动接口
│   │   ├── qspi_flash.h            # QSPI Flash 芯片操作接口
│   │   ├── qspi_video.h            # QSPI 视频帧读取接口
│   │   ├── jpeg_decoder.h          # 硬件 JPEG 解码接口
│   │   ├── stm32h7xx_hal_conf.h    # HAL 配置
│   │   └── stm32h7xx_it.h          # 中断服务声明
│   ├── Src/
│   │   ├── main.c                  # 主程序
│   │   ├── gpio.c                  # GPIO 初始化
│   │   ├── usart.c                 # USART 初始化
│   │   ├── fmc.c                   # FMC SDRAM 初始化
│   │   ├── ltdc.c                  # LTDC 初始化
│   │   ├── dma2d.c                 # DMA2D 初始化
│   │   ├── lcd.c                   # LCD 驱动 (DMA2D 加速)
│   │   ├── animation.c             # 320x320 动画引擎 (raw RGB565, LTDC双Layer)
│   │   ├── qspi_flash.c            # QSPI Flash 芯片操作 (读写)
│   │   ├── qspi_video.c            # QSPI 视频帧读取 (Memory-Mapped)
│   │   ├── jpeg_decoder.c          # 硬件 JPEG 解码 (YCbCr→RGB565)
│   │   ├── lv_port_disp.c          # LVGL 显示驱动 (DMA2D flush)
│   │   ├── lv_port_indev.c         # LVGL 输入驱动 (KEY0)
│   │   ├── freertos.c              # FreeRTOS 任务
│   │   ├── stm32h7xx_hal_msp.c     # HAL MSP 配置
│   │   ├── stm32h7xx_hal_timebase_tim.c  # TIM7 时基
│   │   ├── stm32h7xx_it.c          # 中断服务实现
│   │   ├── system_stm32h7xx.c      # 系统初始化
│   │   ├── syscalls.c              # 系统调用 (printf 重定向)
│   │   └── sysmem.c                # 内存管理
│   ├── O2/                          # -O2 优化编译
│   │   └── ycbcr_conv.c            # YCbCr→RGB565 MCU 转换
│   └── Startup/                    # 启动文件
├── Drivers/
│   ├── STM32H7xx_HAL_Driver/       # HAL 驱动库
│   └── CMSIS/                      # CMSIS 核心
├── Middlewares/
│   ├── Third_Party/
│   │   ├── FreeRTOS/               # FreeRTOS 内核
│   │   └── LVGL/                   # LVGL v8.4 图形库 (待集成)
├── STM32_H743_ALIENTEK_CubeIDE.ioc # CubeMX 项目配置
├── STM32H743IITX_FLASH.ld          # FLASH 链接脚本
├── STM32H743IITX_RAM.ld            # RAM 调试链接脚本
├── scripts/
│   ├── export_frames.py            # MP4→RGB565 帧导出工具
│   └── program_frames.py           # 帧数据 UART 烧录到 QSPI Flash
└── README.md
```

## 初始化流程

```
main()
  ├── MPU_Config()            # MPU 配置 (背景区域 4GB + SDRAM 32MB)
  ├── SCB_EnableICache()      # 使能 I-Cache
  ├── SCB_EnableDCache()      # 使能 D-Cache
  ├── HAL_Init()              # HAL 库初始化
  ├── SystemClock_Config()    # 系统时钟配置 (400MHz)
  ├── MX_GPIO_Init()          # GPIO 初始化 (PB0, PB1, PB5)
  ├── MX_USART1_UART_Init()   # 调试串口初始化 (PA9, PA10)
  ├── MX_FMC_Init()           # FMC / SDRAM 初始化 (W9825G6KH)
  ├── MX_DMA2D_Init()         # DMA2D 初始化
  ├── MX_LTDC_Init()          # LTDC 初始化 (双 Layer: 动画 + UI)
  ├── MX_QUADSPI_Init()       # QSPI 初始化 (W25Q256, 32MB)
  ├── osKernelInitialize()    # FreeRTOS 内核初始化
  ├── MX_FREERTOS_Init()      # 创建 defaultTask + animTask 线程
  └── osKernelStart()         # 启动调度器
       ├── StartDefaultTask()  # 打印时钟 → SDRAM 测试 → LED 闪烁
       └── StartAnimationTask() # 清屏 → QSPI Flash 编程/验证 → 循环播放动画
```

## printf 重定向

通过实现 `__io_putchar()` 将 printf 输出重定向到 USART1 (115200-8-N-1)：

```c
int __io_putchar(int ch)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}
```

上电后 `StartDefaultTask` 自动输出系统时钟信息：

```
========================================
  STM32H743 System Clock Info
========================================
  SYSCLK: 400 MHz
  HCLK:   200 MHz
  APB1:   100 MHz
  APB2:   100 MHz
========================================
```

## LCD 驱动 (LTDC + DMA2D)

- 分辨率：800x480 RGB565，4.3" TFT-LCD
- 帧缓冲位于 SDRAM，双缓冲用于动画层 (Layer 1)
- LTDC 配置为双 Layer 模式：
  - **Layer 1**：RGB565 动画帧，双缓冲 (0xC0000000 / 0xC0180000)
  - **Layer 2**：ARGB1555 UI 叠加层 (0xC0300000)，1-bit alpha 硬件混合
- DMA2D 用于硬件加速填充（`LCD_Clear` / `LCD_Fill`）

**重要**：STM32H7 的 `HAL_SDRAM_Init` 不会自动发送 SDRAM 初始化命令序列，需在 `MX_FMC_Init` 中手动调用：
- `HAL_SDRAM_SendCommand` (CLK_ENABLE → PALL → AUTOREFRESH ×8 → LOAD_MODE)
- `HAL_SDRAM_ProgramRefreshRate` (设置刷新计数器)
	
## QSPI Flash (W25Q256)

- 容量：32MB (256Mbit)，4 字节地址模式
- 引脚：PB2 (CLK), PB6 (NCS), PF6-PF9 (IO1-IO4)
- 时钟：100MHz (DDR 模式，200MHz 等效)
- 检测：上电自动读取 JEDEC ID，输出 `EF 40 19`

## 动画引擎 (Phase 6: Raw RGB565 + LTDC 双 Layer)

### 概述

320×320 20fps raw RGB565 动画播放，帧数据存储在 QSPI Flash 上。

**架构**：LTDC 双 Layer 硬件叠加，彻底消除闪烁。
- **Layer 1（底层）**：RGB565 动画帧，双缓冲（前/后帧缓冲），每帧 CPU 旋转 90° CW 适配竖屏
- **Layer 2（上层）**：ARGB1555 UI 叠加层（底部栏、clean_text、帧号、标题文字、蓝色边框），硬件自动混合

### 帧数据准备（PC 端）

MP4 视频 → 逐帧 raw RGB565 → 单一二进制文件 → 通过 UART 烧录到 QSPI Flash：

```
mp4 (input1.mp4)
  │ ffmpeg: 提取帧 (320x320, 20fps), 缩放, 转 RGB565
  ▼
RGB565 帧序列 (raw, 无压缩)
  │ Python struct.pack 打包 (帧数 + 首尾校验 + 帧数据)
  ▼
input1_data.bin → 通过 UART 115200 烧录到 QSPI Flash
```

### 内存布局

```
0xC0000000 ┌──────────────────────────┐
           │  Front Framebuffer       │  800×480×2 = 1.5MB
0xC0180000 ├──────────────────────────┤
           │  Back Framebuffer        │  800×480×2 = 1.5MB
0xC0300000 ├──────────────────────────┤
           │  UI Layer 2 Buffer       │  800×480×2 = 768KB (ARGB1555)
0xC03C0000 ├──────────────────────────┤
           │  (unused)                │
0xC1FFFFFF └──────────────────────────┘
```

- LTDC Layer 1 (动画): 0xC0000000 / 0xC0180000 双缓冲，RGB565
- LTDC Layer 2 (UI): 0xC0300000，ARGB1555，800×480，连续缓冲
- 动画帧数据直接从 QSPI Memory-Mapped 区域 (0x90000000) 读取，无需 SDRAM 中转

### 上电初始化

```
MX_QUADSPI_Init()      → QSPI 外设时钟 + GPIO
QSPI_Flash_Init()      → 读取 JEDEC ID (EF 40 19)，4字节地址模式
QSPI_Video_Init()      → 检查 QSPI Flash 中已有帧数据
                       → 无数据则进入 UART 编程模式等待 PC 传输
                       → 开启 Memory-Mapped 模式 (0x90000000)
QSPI_Video_GetFrameCount() → 读取帧数
```

### 每帧播放流程 (50ms, 20fps)

FreeRTOS 50ms 定时器触发 → 二值信号量 → `StartAnimationTask`：

```
① 获取帧地址 (~0μs)
   QSPI_Video_GetFrameAddr(frame_index) → 返回 QSPI Memory-Mapped 地址

② CPU 旋转 + 拷贝 (~2-3ms) ← 瓶颈
   CopyFrame(): 320×320 pixel, 90° CW 旋转 + 写入非活跃帧缓冲
   for sy in 0..320:
     for sx in 0..320:
       px = ANIM_DST_X + (319 - sy)
       py = ANIM_DST_Y + sx
       dst[py * 1600 + px] = src[sy * 320 + sx]

③ 绘制帧号 (~0.1ms)
   DrawFrameNumberUI(): 将帧号数字写入 Layer 2 UI 缓冲 (ARGB1555)

④ 换帧 (~0μs)
   LTDC_Layer1->CFBAR = 新帧缓冲地址 (立即生效，无撕裂)
```

### 竖屏适配

物理面板 800×480 横向安装，逆时针旋转 90°。逻辑坐标系 480×800：

```
逻辑坐标 (480×800)         物理坐标 (800×480)
┌──────────────┐          ┌──────────────────────┐
│  animation   │    90°   │                      │
│  demo        │   ────►  │  ╱╲  动画区域         │
│  20fps       │  CW 旋转 │  ╲╱  320×320          │
│              │          │                      │
│ ┌──────────┐ │          │  ┌───┐               │
│ │ 动画区域  │ │          │  │   │               │
│ │ 320×320  │ │          │  │   │               │
│ └──────────┘ │          │  └───┘               │
│  bottom_bar  │          │  bottom_bar (旋转后)  │
└──────────────┘          └──────────────────────┘
```

动画帧数据在 CPU 拷贝时进行 90° CW 旋转，抵消面板的 90° CCW 物理旋转。

### LTDC 双 Layer 架构

**Layer 1 (底层动画)**：
- 格式：RGB565
- 帧缓冲：双缓冲 (0xC0000000 / 0xC0180000)
- 每帧更新：CPU 旋转 + 写入非活跃缓冲 → 切换 CFBAR

**Layer 2 (上层 UI)**：
- 格式：ARGB1555 (1-bit alpha: bit15=1 不透明, =0 透明)
- 缓冲地址：0xC0300000 (连续，单缓冲)
- 静态元素：PreRenderUI() 启动时一次性渲染
  - 蓝色 2px 矩形边框 (320×480 居中)
  - bottom_bar (320×134, RGB565→ARGB1555 转换, 90° CW 旋转)
  - clean_text 带黑色背景 (140×116, 直接绘制)
  - 标题文字: "animation demo", "STM32H743IIT6", "20fps" (LVGL Montserrat 24, 抗锯齿)
- 动态元素：DrawFrameNumberUI() 每帧更新帧号 (8×16 位图字体, 2x 缩放)
- 硬件混合：BF1 = PxCA, BF2 = 1-PxCA (每个像素的 alpha 值决定透明度)

### UI 元素逻辑坐标

```
元素                    逻辑坐标 (x, y)        大小
─────────────────────────────────────────────────────
动画区域                (80, 160)-(400, 480)   320×320
蓝色矩形框              (80, 80)-(400, 560)    320×480
标题 "animation demo"   居中, y=70              Montserrat 24
标题 "STM32H743IIT6"    居中, y=102             Montserrat 24
标题 "20fps"            居中, y=134             Montserrat 24
bottom_bar              (80, 506)-(400, 640)   320×134
clean_text              (170, 262)-(310, 378)  140×116
帧号                    (16, 16)               8×16 位图×2
```

### 性能分析 (DWT 计时输出)

每 20 帧通过串口输出：
```
T:NN addr=XXXus CPUrot=XXXus swap=XXXus
```
- `T:NN` — 帧索引 (0-99 循环)
- `addr` — 获取帧地址耗时 (~0μs, 内存映射直接返回)
- `CPUrot` — CPU 90° CW 旋转 + 拷贝耗时 (~2500μs @ 200MHz)
- `swap` — 换帧耗时 (~0μs, 寄存器写入)

## 开发环境

| 工具 | 版本/说明 |
|------|------|
| IDE | STM32CubeIDE (macOS) |
| 工具链 | ARM GCC (arm-none-eabi-) |
| FPU | fpv5-d16 (hard ABI) |
| 调试器 | ST-LINK (SWD: PA13/PA14) |
| 开发板 | 正点原子 STM32H743IIT6 |

## 构建与烧录

### 方式一：STM32CubeIDE (ST-LINK)
1. 使用 STM32CubeIDE 打开项目目录
2. 编译：`Project → Build All`
3. 烧录调试：`Run → Debug`

### 方式二：STM32CubeProgrammer (ST-LINK, macOS)
1. 编译生成 hex 文件（CubeIDE 自动生成到 `Debug/` 目录）
2. 开发板 ST-LINK 口连接到 Mac (USB)
3. 打开 CubeProgrammer，选择 ST-LINK 连接
4. 点击 **Open file** 选择 `Debug/STM32_H743_ALIENTEK_CubeIDE.hex`
5. 点击 **Download** 烧录

### 方式三：stm32flash 串口烧录 (macOS)
1. 编译生成 hex 文件（CubeIDE 会自动生成到 `Debug/` 目录）
2. 开发板 USB-232 口连接到 Mac
3. BOOT0 跳线帽接 **3.3V**，按 RESET 进入 ISP 模式
4. 执行烧录：
   ```bash
   stm32flash -w Debug/STM32_H743_ALIENTEK_CubeIDE.hex -v -g 0 -b 115200 /dev/tty.usbserial-2110
   ```
5. 烧录完成后 BOOT0 接回 GND，程序自动运行

## 学习路线 (待完成)

- [x] GPIO 输出 (PB0/PB1 LED, PB5 背光)
- [x] USART 调试串口 + printf 重定向
- [x] FreeRTOS (CMSIS-RTOS V2)
- [x] SDRAM 驱动 (W9825G6KH)
- [x] RGB LCD 显示 (LTDC 双 Layer + DMA2D)
- [x] QSPI Flash (W25Q256, 32MB, Memory-Mapped)
- [x] 320x320 20fps 动画播放 (raw RGB565, 无 JPEG)
- [x] LTDC 双 Layer 硬件叠加 (UI + 动画分离, 消除闪烁)
- [x] LVGL 字体渲染 (Montserrat 24, 抗锯齿)
- [ ] LVGL UI 完整集成
- [ ] ...

## 烧录方式四：pyOCD（CMSIS-DAP，交互式 `load`）

1. 列出可用探针：

  ```bash
  pyocd list
  ```

2. 使用交互式 commander 连接（指定 target 和 probe 的 Unique ID）：

  ```bash
  pyocd commander -t stm32h743xx -u 07000001000000000000000000000000a5a5a5a597969908
  ```

3. 在 `pyocd>` 提示符下使用 `load` 写入 HEX/ELF：

  ```text
  pyocd> load Debug/STM32_H743_ALIENTEK_CubeIDE.hex
  ```

4. 常用交互命令：`reset`、`resume`、`halt`、`read32` 等。

注意事项：
- `pyocd list` 中显示的板卡名称（例如 `NUCLEO-F103RB`）是探针/适配器的标识，不一定等同当前目标 MCU 描述。
- 在非交互模式下尝试 `pyocd flash -t ... -u <UID> <file>` 时可能因为 pyOCD 版本、参数或与探针/目标的通信问题而失败；遇到此类错误时可优先使用 `commander` + `load`。
- 如果在连接时看到 `Invalid coresight component`、`Memory transfer fault` 或 `Connected to STM32H743xx [Lockup]` 等警告/错误，尝试：
  - 确认目标供电、复位线以及 SWD 连接正确；
  - 给目标上电重置或断电重连；
  - 尝试短按 Reset，或通过 `halt`/`reset` 在 `pyocd>` 中清除锁死；
  - 必要时使用 OpenOCD（参见本仓库的 `OpenOCD` 任务）作为替代烧录方式。

- 不同 pyOCD 版本支持的命令略有差异；若命令不可识别，请用 `pyocd --version` 和 `pyocd --help` 检查可用子命令。

示例（交互式流程复制粘贴）：

```bash
pyocd list
pyocd commander -t stm32h743xx -u 07000001000000000000000000000000a5a5a5a597969908
# 在 pyocd> 提示符下：
# load Debug/STM32_H743_ALIENTEK_CubeIDE.hex
# reset
```

### 关于 `07000001000000000000000000000000a5a5a5a597969908` 的来源

- 来源：该长串是探针（probe/适配器）的 Unique ID，由探针固件（例如 DAPLink / CMSIS‑DAP）或 USB 设备的序列号生成，用于在主机上区分多个探针。它通常为适配器侧的标识，而不是目标 MCU 的芯片唯一序列号。
- 验证探针来源：

  ```bash
  pyocd list -v
  lsusb -v    # 或使用系统的 USB 信息查看工具，查找对应设备的序列号/厂商字段
  ```

  比对输出即可确认该 UID 对应哪个物理设备（例如开发板或 DAPLink 适配器）。

- 读取目标 MCU 本身的唯一 ID（芯片 UID）：连接后在 `pyocd>` 中读取对应的设备唯一 ID 寄存器地址（不同 STM32 系列地址不同，请以数据手册为准）。示例（仅作参考）：

  ```text
  pyocd> read32 0x1FF1E800 3
  ```

  - 上例读取 3 个 32-bit 单元并输出；请先查阅所用 MCU 的参考手册确认唯一 ID 的基地址。
  - 如果需要，我可以为 `STM32H743IIT6` 查找并写入精确的 UID 地址示例并追加到本节。