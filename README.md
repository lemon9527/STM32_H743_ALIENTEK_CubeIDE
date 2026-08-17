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
| LTDC | 4.3" RGB LCD (800x480) | 28-pin TTL | 已启用 |
| DMA2D | 图形加速 (LCD_Clear) | 内部 | 已启用 |
| QSPI | W25Q256 (32MB) | PB2/PB6/PF6-PF9 | 已启用 |

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
│   │   ├── animation.c             # 320x320 动画引擎 (20fps)
│   │   ├── lv_port_disp.c          # LVGL 显示驱动 (DMA2D flush)
│   │   ├── lv_port_indev.c         # LVGL 输入驱动 (KEY0)
│   │   ├── freertos.c              # FreeRTOS 任务
│   │   ├── stm32h7xx_hal_msp.c     # HAL MSP 配置
│   │   ├── stm32h7xx_hal_timebase_tim.c  # TIM7 时基
│   │   ├── stm32h7xx_it.c          # 中断服务实现
│   │   ├── system_stm32h7xx.c      # 系统初始化
│   │   ├── syscalls.c              # 系统调用 (printf 重定向)
│   │   └── sysmem.c                # 内存管理
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
  ├── MX_LTDC_Init()          # LTDC 初始化 (800x480, 帧缓冲@0xC0000000)
  ├── MX_QUADSPI_Init()       # QSPI 初始化 (W25Q256, 32MB)
  ├── osKernelInitialize()    # FreeRTOS 内核初始化
  ├── MX_FREERTOS_Init()      # 创建 defaultTask + animTask 线程
  └── osKernelStart()         # 启动调度器
       ├── StartDefaultTask()  # 打印时钟 → SDRAM 测试 → LED 闪烁
       └── StartAnimationTask() # 清屏 → 320x320 20fps 动画循环播放
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
- 帧缓冲位于 SDRAM 起始地址 `0xC0000000`
- 帧缓冲步幅（ImageWidth）为 1600 像素，有效显示区域 800 像素
- DMA2D 用于硬件加速填充（`LCD_Clear` / `LCD_Fill`）

**重要**：STM32H7 的 `HAL_SDRAM_Init` 不会自动发送 SDRAM 初始化命令序列，需在 `MX_FMC_Init` 中手动调用：
- `HAL_SDRAM_SendCommand` (CLK_ENABLE → PALL → AUTOREFRESH ×8 → LOAD_MODE)
- `HAL_SDRAM_ProgramRefreshRate` (设置刷新计数器)

上电后 LCD 显示黑色背景，屏幕中央循环播放 320×320 20fps 动画。

```c
// 基本用法
LCD_Init();                    // 打开背光
LCD_Clear(LCD_COLOR_BLACK);   // 清除为黑色
LCD_Fill(0, 0, 100, 100, LCD_COLOR_RED);  // 填充红色矩形
LCD_DrawPixel(50, 50, LCD_COLOR_WHITE);  // 画一个白点
```
	
## QSPI Flash (W25Q256)

- 容量：32MB (256Mbit)，4 字节地址模式
- 引脚：PB2 (CLK), PB6 (NCS), PF6-PF9 (IO1-IO4)
- 时钟：100MHz (DDR 模式，200MHz 等效)
- 检测：上电自动读取 JEDEC ID，输出 `EF 40 19`

## 动画引擎

- 320×320 RGB565 帧，20fps 循环播放
- 帧数据存储在内部 Flash（测试阶段 5 帧，后续迁移至 QSPI Flash）
- 使用 FreeRTOS 50ms 定时器 + 二值信号量触发帧更新
- 帧拷贝：CPU memcpy 逐行拷贝（DMA2D 待优化）

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
- [x] RGB LCD 显示 (LTDC + DMA2D)
- [x] QSPI Flash (W25Q256, 32MB)
- [x] 320x320 20fps 动画播放
- [ ] MJPEG 硬件解码 (JPEG Codec)
- [ ] LVGL UI 集成
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