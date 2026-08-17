# 320x320 20fps MJPEG 动画播放计划

## 概述

将两个 1080×1080 30fps MP4 视频转换为 320×320 20fps MJPEG 动画序列，在 STM32H743（W25Q256 32MB QSPI Flash）上利用**硬件 JPEG 解码器**实时解码播放。

**关键决策：RGB565 → MJPEG**，因为 10 秒原始 RGB565 需要 40.96 MB（超出 32 MB Flash），而 MJPEG 仅需 **~3.2 MB**。

| 参数 | 值 |
|------|-----|
| 帧尺寸 | 320×320 像素 |
| 帧率 | 20 fps |
| 每个动画时长 | 5 秒（100 帧） |
| 动画数量 | 2 个（交错播放，共 10 秒） |
| 单帧 JPEG 大小 | ~8 KB（Q=80） |
| 总存储需求 | ~3.2 MB |
| 存储介质 | W25Q256 QSPI Flash（32 MB） |
| 解码方式 | STM32H743 硬件 JPEG Codec |

---

## 当前状态

- LCD 竖屏正常，旋转公式：`phys_x = log_y, phys_y = 479 - log_x`
- SDRAM 32MB 已初始化，帧缓冲在 `0xC0000000`
- DMA2D 可用（M2M_PFC 模式），LCD_Clear 使用 R2M 模式
- FreeRTOS 运行中，defaultTask 闪灯
- LVGL 库保留（`lvgl.h`、`lv_port_disp.h`、`lv_port_indev.h` 引用保留，任务已停用）
- **QSPI Flash 未配置**（`HAL_QSPI_MODULE_ENABLED` 被注释）
- **JPEG Codec 未配置**（`HAL_JPEG_MODULE_ENABLED` 被注释）

---

## Phase 1: PC 端预处理

**目标：** 将 MP4 转为预旋转的 MJPEG 帧序列 + 帧索引表

**脚本：** `tools/convert_video_to_mjpeg.sh`

```bash
#!/bin/bash
# Usage: ./convert_video_to_mjpeg.sh input.mp4 anim_id

INPUT="$1"
ANIM_ID="${2:-1}"
OUTDIR="anim_${ANIM_ID}"
mkdir -p "$OUTDIR"

# Step 1: Extract frames at 20fps, resize to 320x320,
#          rotate 90° CCW (transpose=2), output as JPEG
ffmpeg -i "$INPUT" \
  -vf "fps=20,scale=320:320:flags=lanczos,transpose=2" \
  -q:v 3 \
  -f image2 "${OUTDIR}/frame_%04d.jpg"

# Step 2: Build index file (offset, size per frame) + merge JPEGs
FRAME_COUNT=$(ls "${OUTDIR}"/frame_*.jpg | wc -l)
echo "Frames: $FRAME_COUNT"

# frames.bin: [4-byte index header]
#   uint32_t frame_count
# Then for each frame: [uint32_t offset, uint32_t size]
# Then JPEG data concatenated
python3 << EOF
import os, struct

outdir = "${OUTDIR}"
files = sorted(f for f in os.listdir(outdir) if f.startswith("frame_") and f.endswith(".jpg"))
frame_count = len(files)

# First pass: read all JPEG data
jpeg_data = []
for f in files:
    with open(os.path.join(outdir, f), "rb") as fh:
        jpeg_data.append(fh.read())

# Compute offsets
header_size = 4 + 4 + frame_count * 8  # 4 (count) + 4 (reserved) + count*8 (offset+size)
offset = header_size
index_entries = []
for data in jpeg_data:
    index_entries.append((offset, len(data)))
    offset += len(data)

# Write frames.bin
with open(f"{outdir}/frames.bin", "wb") as fh:
    fh.write(struct.pack("<II", frame_count, 0))  # count + reserved
    for off, sz in index_entries:
        fh.write(struct.pack("<II", off, sz))
    for data in jpeg_data:
        fh.write(data)

total_size = os.path.getsize(f"{outdir}/frames.bin")
print(f"frames.bin: {total_size} bytes ({total_size/1024/1024:.1f} MB)")
EOF

echo "Done: ${OUTDIR}/frames.bin"
```

**输出文件：**
- `anim_1/frames.bin` — 第 1 个动画的 MJPEG 帧包（~1.6 MB）
- `anim_2/frames.bin` — 第 2 个动画的 MJPEG 帧包（~1.6 MB）

**验证：** 每个 `frames.bin` 约 1.6 MB，两个合计 ~3.2 MB

---

## Phase 2: MCU 端测试（5 帧内嵌 RGB565，验证播放管道）

**目标：** 用 5 帧内嵌 RGB565 数据验证 DMA2D 播放管道，确保定时器 → 信号量 → DMA2D 拷贝链路正常

> 为什么不直接用 JPEG 测试？因为 JPEG 硬件解码器需要先配置 CubeMX 才能使用。Phase 2 先用 RGB565 验证 DMA2D 管道，Phase 3 再切换为 JPEG。

### 2.1 提取 5 帧测试数据

```bash
# 从 Phase 1 生成的 frames.bin 中提取 5 帧 RGB565 作为测试
ffmpeg -i input.mp4 \
  -vf "fps=20,scale=320:320:flags=lanczos,transpose=2" \
  -f rawvideo -pix_fmt rgb565 \
  -vframes 5 frames_test_5.bin
```

生成链接器目标文件（放入 `.rodata` 即 FLASH）：
```bash
arm-none-eabi-objcopy -I binary -O elf32-littlearm -B arm \
  --rename-section .data=.rodata,contents,alloc,load,readonly,data \
  frames_test_5.bin test_frames.o
```

### 2.2 新建动画模块

**新建 `Core/Inc/animation.h`：**
```c
#define ANIM_FRAME_WIDTH    320
#define ANIM_FRAME_HEIGHT   320
#define ANIM_FRAME_PIXELS   (320 * 320)
#define ANIM_FRAME_BYTES    (ANIM_FRAME_PIXELS * 2)   // 204,800
#define ANIM_FPS            20
#define ANIM_PERIOD_MS      50
#define ANIM_DST_X          240                          // (800-320)/2
#define ANIM_DST_Y          80                           // (480-320)/2
#define ANIM_TEST_FRAMES    5

void AnimTimerCallback(void *argument);
void StartAnimationTask(void *argument);
```

**新建 `Core/Src/animation.c`：**
- `DMA2D_CopyFrame(src, dst)`：M2M_PFC 矩形拷贝，OutputOffset = 1600 - 320 = 1280
- `AnimTimerCallback`：50ms 定时器回调，递增 `frame_index`，释放信号量
- `StartAnimationTask`：LCD_Clear(BLACK) → 创建信号量 → 阻塞等待 → DMA2D 拷贝当前帧
- 帧数据通过 linker symbol 引用（`test_frames.o` 嵌入的 `.rodata`）

### 2.3 修改 freertos.c

- 移除 LVGL 任务和 timer（保留 `#include "lvgl.h"` 等头文件，后续可恢复）
- 添加 `anim_timer_id`（50ms 周期）、`animTaskHandle`
- 保留 LVGL 库文件不删除

### 2.4 编译验证

- 将 `test_frames.o` 加入链接（CubeIDE → Properties → C/C++ Build → Settings → MCU GCC Linker → Miscellaneous → Other objects → 添加 `Core/Src/test_frames.o`）
- 编译烧录，预期：黑底 + 居中 320×320 动画，5 帧循环
- 串口输出 "Animation started: 5 test frames..."

---

## Phase 3: 硬件 JPEG 解码 + QSPI Flash 配置

**目标：** 启用 JPEG Codec 和 QSPI Flash，实现单帧 JPEG → RGB565 解码

### 3.1 CubeMX 配置

**QUADSPI 引脚（正点原子 H743 开发板）：**
| 引脚 | 功能 |
|------|------|
| PF6 | QUADSPI_BK1_IO3 |
| PF7 | QUADSPI_BK1_IO2 |
| PF8 | QUADSPI_BK1_IO0 |
| PF9 | QUADSPI_BK1_IO1 |
| PB2 | QUADSPI_CLK |
| PB6 | QUADSPI_BK1_NCS |

- Clock Prescaler: 1（QSPI 时钟 = 100 MHz）
- FIFO Threshold: 4
- Flash Size: 26 (32 MB)

**JPEG Codec：**
- 启用 JPEG（`HAL_JPEG_MODULE_ENABLED`）
- 无需额外引脚（内部外设）

### 3.2 新建 QSPI 驱动

**新建 `Core/Inc/qspi_flash.h` + `Core/Src/qspi_flash.c`：**
- `QSPI_Flash_Init()`：复位 → 读取 JEDEC ID（预期 0xEF4019）→ 验证
- `QSPI_Flash_Read(dst, addr, size)`：普通读取模式
- `QSPI_Flash_EnterMemoryMapped()`：进入内存映射模式（`0x90000000`）

### 3.3 新建 JPEG 解码模块

**新建 `Core/Inc/jpeg_dec.h` + `Core/Src/jpeg_dec.c`：**
- `JPEG_Decode_RGB565(jpeg_data, jpeg_size, output_buf)`：
  1. `HAL_JPEG_Decode(&hjpeg, jpeg_data, jpeg_size, output_buf, ANIM_FRAME_BYTES, HAL_MAX_DELAY)`
  2. 输出为 RGB565 格式，直接可被 DMA2D 使用
- 解码缓冲区放在 SDRAM（`0xC2000000`），避免占用 D1 RAM

### 3.4 验证

- 编译烧录，串口输出 "QSPI Flash JEDEC ID: EF 40 19"
- 单帧 JPEG 解码测试通过

---

## Phase 4: 完整双动画播放（10 秒）

**目标：** 烧录两个 `frames.bin` 到 QSPI，播放完整 10 秒双动画（交错播放）

### 4.1 烧录 frames.bin 到 QSPI

在 QSPI Flash 中分区：
```
偏移        内容
0x00000000  anim_1/frames.bin  (~1.6 MB)
0x00A00000  anim_2/frames.bin  (~1.6 MB)
```

使用 STM32CubeProgrammer 外部加载器烧录。

### 4.2 修改 animation.c

改为 MJPEG 播放引擎：

```c
// 双动画 QSPI 偏移
#define ANIM1_QSPI_OFFSET  0x00000000
#define ANIM2_QSPI_OFFSET  0x00A00000

// 每帧的播放流程
void PlayFrame(uint32_t anim_offset, int frame_idx)
{
    // 1. 从 QSPI 读取帧索引条目
    uint32_t idx_entry = anim_offset + 8 + frame_idx * 8;
    uint32_t jpeg_offset, jpeg_size;
    QSPI_Flash_Read(&jpeg_offset, idx_entry, 8);  // offset + size (8 bytes)

    // 2. 从 QSPI 读取 JPEG 数据到 SDRAM 缓冲区
    QSPI_Flash_Read(jpeg_buf, anim_offset + jpeg_offset, jpeg_size);

    // 3. 硬件 JPEG 解码 → RGB565 到 SDRAM
    JPEG_Decode_RGB565(jpeg_buf, jpeg_size, decode_buf);

    // 4. DMA2D 拷贝到 LCD 帧缓冲
    DMA2D_CopyFrame(decode_buf, lcd_fb_dst);
}
```

播放逻辑：
- 动画 1 播放 100 帧（5 秒），完成后自动切换到动画 2
- 动画 2 播放 100 帧（5 秒），完成后循环回动画 1
- 帧率由 50ms FreeRTOS 定时器控制

### 4.3 性能预算（每帧 50ms）

| 步骤 | 耗时 |
|------|------|
| QSPI 读取 JPEG（~8KB） | ~0.3ms |
| 硬件 JPEG 解码 | ~2-5ms |
| DMA2D 拷贝到 FB | ~0.5ms |
| **总计** | **~5.8ms** |
| 预算 | 50ms |
| 余量 | **88%** |

DMA2D 拷贝和下一帧的 QSPI 读取可以流水线并行（DMA2D 传输期间 CPU 可发起下一次 QSPI 读取）。

### 4.5 验证

- 动画 1 播放 5 秒 → 动画 2 播放 5 秒 → 循环
- 无撕裂、无闪烁、无掉帧
- 串口输出 "Animation: playing anim 1" / "anim 2"

---

## Phase 5: 清理

- 删除 `Core/Src/test_frames.o`、`frames_test_5.bin`
- 确认 `animation.c` 中无 RGB565 测试代码残留
- 最终代码审查

---

## 文件变更汇总

| 文件 | 操作 | 阶段 |
|------|------|------|
| `tools/convert_video_to_mjpeg.sh` | 新建 | 1 |
| `Core/Inc/animation.h` | 新建 | 2 |
| `Core/Src/animation.c` | 新建 | 2 |
| `Core/Src/test_frames.o` | 新建（Phase 5 删除） | 2 |
| `Core/Src/freertos.c` | 修改（移除 LVGL 任务，添加动画） | 2 |
| `STM32_H743_ALIENTEK_CubeIDE.ioc` | 修改（启用 QUADSPI + JPEG） | 3 |
| `Core/Inc/stm32h7xx_hal_conf.h` | 修改（取消注释 QSPI + JPEG） | 3 |
| `Core/Src/quadspi.c` | 生成（CubeMX） | 3 |
| `Core/Src/jpeg.c` | 生成（CubeMX） | 3 |
| `Core/Inc/qspi_flash.h` | 新建 | 3 |
| `Core/Src/qspi_flash.c` | 新建 | 3 |
| `Core/Inc/jpeg_dec.h` | 新建 | 3 |
| `Core/Src/jpeg_dec.c` | 新建 | 3 |
| `Core/Src/animation.c` | 修改（改为 MJPEG + QSPI 源） | 4 |

---

## 关键技术决策

1. **MJPEG 而非 RGB565**：10 秒 RGB565 = 40.96 MB > 32 MB Flash，JPEG 仅需 ~3.2 MB
2. **硬件 JPEG 解码**：STM32H743 内置 JPEG Codec，解码 320×320 约 2-5ms，CPU 开销极小
3. **预旋转不变**：PC 端 `transpose=2`（90° CCW），MCU 端 DMA2D 直接拷贝
4. **帧索引表**：`frames.bin` 头部包含每帧的 offset 和 size，O(1) 时间定位任意帧
5. **信号量同步**：FreeRTOS 50ms 定时器发信号量，任务等待并处理，避免定时器回调中执行耗时操作
6. **QSPI 内存映射**：JPEG 数据可直接映射到 `0x90000000`，DMA2D 可从此地址读取