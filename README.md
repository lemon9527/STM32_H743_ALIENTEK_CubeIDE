# STM32H743 正点原子开发板 Study 项目

基于正点原子 STM32H743IIT6 开发板的学习项目，使用 FreeRTOS 实时操作系统，基于 CubeIDE 开发，逐步学习各个外设。

## 当前进度

| 外设 | 功能 | 引脚 | 状态 |
|------|------|------|------|
| GPIO | LED1 Green | PB0 | 已启用 |
| GPIO | LED0 Red | PB1 | 已启用 |
| USART1 | 调试串口 (printf) | PA9 (TX), PA10 (RX) | 已启用 |
| TIM7 | HAL 系统时基 | 内部 | 已启用 |
| FreeRTOS | CMSIS-RTOS V2 | - | 已启用 |
| FMC SDRAM | W9825G6KH (32MB) | FMC Bank1 | 已启用 |

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
│   │   ├── stm32h7xx_hal_conf.h    # HAL 配置
│   │   └── stm32h7xx_it.h          # 中断服务声明
│   ├── Src/
│   │   ├── main.c                  # 主程序
│   │   ├── gpio.c                  # GPIO 初始化
│   │   ├── usart.c                 # USART 初始化
│   │   ├── stm32h7xx_hal_msp.c     # HAL MSP 配置
│   │   ├── stm32h7xx_hal_timebase_tim.c  # TIM7 时基
│   │   ├── stm32h7xx_it.c          # 中断服务实现
│   │   ├── system_stm32h7xx.c      # 系统初始化
│   │   ├── syscalls.c              # 系统调用
│   │   └── sysmem.c                # 内存管理
│   └── Startup/                    # 启动文件
├── Drivers/
│   ├── STM32H7xx_HAL_Driver/       # HAL 驱动库
│   └── CMSIS/                      # CMSIS 核心
├── STM32_H743_ALIENTEK_CubeIDE.ioc # CubeMX 项目配置
├── STM32H743IITX_FLASH.ld          # FLASH 链接脚本
├── STM32H743IITX_RAM.ld            # RAM 调试链接脚本
└── README.md
```

## 初始化流程

```
main()
  ├── MPU_Config()            # MPU 配置 (背景区域 4GB, 禁止访问)
  ├── SCB_EnableICache()      # 使能 I-Cache
  ├── SCB_EnableDCache()      # 使能 D-Cache
  ├── HAL_Init()              # HAL 库初始化
  ├── SystemClock_Config()    # 系统时钟配置 (400MHz)
  ├── MX_GPIO_Init()          # GPIO 初始化 (PB0, PB1)
  ├── MX_USART1_UART_Init()   # 调试串口初始化 (PA9, PA10)
  ├── MX_FMC_Init()           # FMC / SDRAM 初始化 (W9825G6KH)
  ├── osKernelInitialize()    # FreeRTOS 内核初始化
  ├── MX_FREERTOS_Init()      # 创建 defaultTask 线程
  └── osKernelStart()         # 启动调度器
       └── StartDefaultTask()  # 打印系统时钟信息 → 进入主循环
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

### 方式二：stm32flash 串口烧录 (macOS)
1. 编译生成 hex 文件（CubeIDE 会自动生成到 `Debug/` 目录）
2. 开发板 USB-232 口连接到 Mac
3. BOOT0 跳线帽接 **3.3V**，按 RESET 进入 ISP 模式
4. 执行烧录：
   ```bash
   stm32flash -w Debug/STM32_H743_ALIENTEK_CubeIDE.hex -v -g 0 -b 115200 /dev/tty.usbserial-2110
   ```
5. 烧录完成后 BOOT0 接回 GND，程序自动运行

## 学习路线 (待完成)

- [x] GPIO 输出 (PB0/PB1 LED)
- [x] USART 调试串口 + printf 重定向
- [x] FreeRTOS (CMSIS-RTOS V2)
- [x] SDRAM 驱动 (W9825G6KH)
- [ ] RGB LCD 显示 (LTDC)
- [ ] DMA2D 图形加速
- [ ] QSPI Flash
- [ ] ...