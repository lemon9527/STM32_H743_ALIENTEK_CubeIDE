# Raw RGB565 QSPI 直读动画播放方案

## Context

当前动画系统使用 JPEG 压缩帧（4:4:4 子采样），每帧需硬件 JPEG 解码耗时 ~65-70ms，远超 20fps 所需的 50ms 预算，导致卡顿。

改为将原始 RGB565 帧（无压缩）直接存入 QSPI Flash，播放时只需 DMA2D 从 QSPI 内存映射区拷贝到帧缓冲，每帧 ~6-8ms，远低于 50ms。

**核心约束**：100 帧 × 320×320×2 = 20 MB，远超 STM32H743 内部 Flash（2 MB），无法像之前那样嵌入 ELF。需要通过 UART 将帧数据写入 QSPI Flash。

## 文件变更

### 1. `scripts/export_frames.py` — 输出原始 RGB565

**改动**：
- 移除 JPEG 压缩，改为输出原始 RGB565 像素数据
- 使用 numpy 加速 RGB→RGB565 转换（`(r>>3)<<11 | (g>>2)<<5 | (b>>3)`）
- 输出版本号 7
- 输出两个文件：
  - `frames_hdr.bin`：仅 header（8B）+ 帧表（N×8B），约 808 字节，嵌入 ELF
  - `frames_data.bin`：原始 RGB565 帧数据，20 MB，通过 UART 发送

### 2. `scripts/program_frames.py` — 新建：串口发送帧数据

**UART 协议**：
```
MCU 发送: "READY\r\n"
PC 发送:  [total_size:4B LE]
MCU 擦除 QSPI 扇区...
MCU 发送: "ERASE_OK\r\n"
PC 发送:  [data chunks: 256B each]
MCU 逐页写入 QSPI Flash...
MCU 发送: "DONE\r\n"
```

### 3. `Core/Inc/video_frames.h` — 更新常量

- 符号名 `_binary_test_frames_bin_start` → `_binary_frames_hdr_bin_start`（仅含 header + 表）
- 新增 `#define QSPI_DATA_OFFSET`（帧数据起始地址）
- 新增 `#define FRAME_RAW_SIZE 204800`（每帧固定大小）

### 4. `Core/Src/qspi_video.c` — 新增 UART 编程 + 地址查询

- `QSPI_Video_WriteToFlash()`：仅写入 header + 帧表（小数据，嵌入 ELF）
- `QSPI_Video_ProgramFrameData()`：**新增**，通过 UART 接收 20 MB 数据并写入 QSPI Flash
- `QSPI_Video_GetFrameAddr(frame_index)`：**新增**，返回 `0x90000000 + offset`
- 移除 `QSPI_Video_ReadFrame()`（不再需要缓冲读取）

### 5. `Core/Src/animation.c` — 简化为纯 DMA2D 管线

**移除**：
- `#include "jpeg_decoder.h"`
- `decode_buf[ANIM_FRAME_PIXELS]`（200 KB 解码缓冲）
- `jpeg_buf[JPEG_BUF_SIZE]`（32 KB JPEG 缓冲）
- JPEG 解码逻辑、D-Cache flush

**新动画循环**（伪代码）：
```c
for (;;) {
    osSemaphoreAcquire(anim_sem_id, osWaitForever);
    
    // 获取 QSPI 内存映射地址
    uint32_t src_addr = QSPI_Video_GetFrameAddr(frame_index);
    
    // DMA2D 直接从 QSPI 拷贝到帧缓冲
    CopyFrame((const uint16_t *)src_addr, dst);
    
    // 帧号叠加
    DrawFrameNumber(...);
    
    // 交换帧缓冲
    SwapFramebuffer(fb_dst);
}
```

**无需 Cache Flush**：SDRAM 为 non-cacheable（MPU 配置），DMA2D 绕过 Cache 直接读写 AHB 总线。

### 6. `STM32H743IITX_FLASH.ld` — 无需修改

`.rodata.video` 段已存在，用于存放嵌入的 header + 帧表（~808 字节）。

### 7. 编译流程

```bash
# 1. 导出帧
python3 scripts/export_frames.py ~/OneDrive/STM32/video/input1.mp4

# 2. 生成 test_frames.o（仅 header + 表）
cd Core/Src
cp ../../scripts/frames_hdr.bin test_frames.bin
arm-none-eabi-objcopy -I binary -O elf32-littlearm \
  --rename-section .data=.rodata.video test_frames.bin test_frames.o

# 3. 编译
cd Debug && make all -j8

# 4. 烧录（CubeProgrammer）

# 5. 发送帧数据（UART）
python3 scripts/program_frames.py /dev/tty.usbserial-2120 scripts/frames_data.bin
```

## 预期性能

| 阶段 | 耗时 |
|------|------|
| DMA2D QSPI→SDRAM (200KB) | ~5-7ms |
| DrawFrameNumber (CPU) | <0.1ms |
| SwapFramebuffer (LTDC) | <0.1ms |
| **每帧总计** | **~6-8ms** |

对比当前 JPEG 方案：~68ms → 6-8ms，提升约 10 倍。

## 验证步骤

1. 烧录后串口应输出 `QSPI: Video data needs programming`
2. 运行 `program_frames.py`，观察进度条，约 30-60 秒完成
3. 完成后动画自动开始播放，串口输出 `T:0 QSPI=XXXus DMA2D=XXXus`
4. 确认帧号显示正常，画面无卡顿
5. 断电重启，确认不再需要重新编程（header 匹配）