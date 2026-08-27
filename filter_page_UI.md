# Filter Page UI Design

## 1. Overview

Filter Page 根据 Control PCB 通过 UART 协议帧传回的数据，动态显示滤网类型、布局（单/双滤网）及剩余寿命百分比。

- **显示区域**: 320 × 480 矩形框内
- **标题**: 左上角 "Filter"（inter_bold_42，距左侧 24，距顶部 24）
- **布局模式**: 根据 `model_type` 决定单滤网或双滤网布局

---

## 2. Data Source

来自 UART 协议帧的 `sensor_data_t` 结构体（详见 `uart_protocol.md`）：

| 结构体字段 | 类型 | 说明 |
|-----------|------|------|
| `model_type` | `uint8_t` | 设备型号，决定单/双滤网布局 |
| `left_filter_type` | `uint8_t` | 左滤网类型 |
| `right_filter_type` | `uint8_t` | 右滤网类型 |
| `left_hepa_life` | `uint8_t` | 左 HEPA 滤网剩余寿命 (%) |
| `left_carbon_life` | `uint8_t` | 左活性炭滤网剩余寿命 (%) |
| `right_hepa_life` | `uint8_t` | 右 HEPA 滤网剩余寿命 (%) |
| `right_carbon_life` | `uint8_t` | 右活性炭滤网剩余寿命 (%) |

---

## 3. Model Type — 单/双滤网判定

| `model_type` 值 | 含义 | 布局 |
|:---:|------|------|
| 3 | 单滤网机型 | 单滤网布局 |
| 4 | 双滤网机型 | 双滤网布局 |

> 其他值视为未知型号，默认按单滤网布局处理。

---

## 4. Filter Type — Icon 映射

### 4.1 Icon 资源索引

所有 icon 资源位于 `Core/Src/icon_resource/filters/`，已转换为 LVGL 图像描述符（`filter_icons.h`，`LV_IMG_CF_RAW_ALPHA` 格式）。

| 滤网类型 | 大 icon (单Filter) | 中 icon (双Filter) | 小 icon (24x24) |
|---------|-------------------|-------------------|----------------|
| **Hybrid** | `icon_large_HYBRID_Filter_AMIII_123_179` | `icon_medium_HYBRID_Filter_AMIII_93_135` | `icon_mini_Hybrid_24_24` |
| **HEPA** | `icon_large_HEPA_Filter_AMIII_160_184` | `icon_medium_HEPA_Filter_AMIII_122_141` | `icon_mini_HEPA_24_24` |
| **Carbon** | `icon_large_Carbon_Filter_AMIII_124_182` | `icon_medium_Carbon_Filter_AMIII_94_138` | `icon_mini_Carbon_24_24` |
| **IAQP** | `icon_large_AM3_4_IAQP_image_lowrez_130_195` | `icon_medium_AM3_4_IAQP_image_lowrez_100_150` | `icon_mini_IAQP_24_24` |
| **missing** | - | `icon_filter_missing_72_72` | — |

### 4.2 Filter Type 枚举值 → Icon 映射关系

| `left_filter_type` / `right_filter_type` 值 | 滤网类型 | 对应 Icon |
|:---:|---------|-----------|
| 1 | Standard (HEPA) | 对应 HEPA 系列 icon |
| 2 | Carbon | 对应 Carbon 系列 icon |
| 3 | Hybrid | 对应 Hybrid 系列 icon |
| 4 | IAQP | 对应 IAQP 系列 icon |
| 0 | filter missing | `icon_filter_missing` |
| 其他 | 未知/缺失 | `icon_filter_missing` |

---

## 5. Layout Design

### 5.1 单 Filter 布局

适用于单滤网机型。

```
┌──────────────────────────────────—┐  ← 320 × 480 矩形框
│  Filter          -----            │  ← 标题: inter_bold_42, 距左 24, 距顶 24
│                  小icon  filter类型│
│                  -----            │
│                                  │
│             ┌────────┐           │
│             │  ICON  │           │  ← 大 icon (如 Hybrid/HEPA/Carbon/IAQP)
│             │ (大尺寸)│           │     居中显示
│             └────────┘           │
│                                  │
│                                  │
│              ┌──────┐            │
│              │ 75%  │            │  ← 剩余寿命百分比 (inter_bold_50)
│              └──────┘            │
│           ████████░░             │  ← lv_bar 进度条，填充长度 = 寿命 %
│                                  │     距百分比数字底部若干 px
│                                  │     底部居中
└──────────────────────────────────┘
```

**关键实现逻辑**:
- 根据 `left_filter_type` 选择对应的大 icon
- 剩余寿命百分比 = `(left_hepa_life + left_carbon_life) / 2` 或显示两者中较低的值（待确认）
- 使用 `lv_img_set_src()` 设置 icon
- 使用 `lv_bar_create()` 创建进度条，`lv_bar_set_value()` 设置进度值
- 进度条样式：自定义颜色（如绿色 → 黄色 → 红色随百分比变化），圆角epa_life` 和 `right_carbon_life` 计算
- 左右各创建一个 `lv_bar`，分别对应左右滤网的剩余寿命

### 5.2 双 Filter 布局

适用于双滤网机型。

```
┌──────────────────────────────────┐  ← 320 × 480 矩形框
│  Filter                          │  ← 标题: inter_bold_42, 距左 24, 距顶 24
│                                  │
│                                  │
│       ┌──────┐     ┌──────┐      │
│       │ ICON │     │ ICON │      │  ← 左右各一个中 icon
│       │ (左) │     │ (右)  │      │     左: 根据 left_filter_type 选择
│       └──────┘     └──────┘      │     右: 根据 right_filter_type 选择
│                                  │
│    ┌──────┐          ┌──────┐    │
│    │ 75%  │          │ 80%  │    │  ← 左右剩余寿命百分比
│    └──────┘          └──────┘    │
│   ████░░░░          ██████░░░    │  ← 左右各一个 lv_bar 进度条
│                                  │     左: 对应左滤网剩余寿命
│                                  │     右: 对应右滤网剩余寿命
└──────────────────────────────────┘
```

**关键实现逻辑**:
- 左 icon 根据 `left_filter_type` 选择中 icon
- 右 icon 根据 `right_filter_type` 选择中 icon
- 左剩余寿命根据 `left_hepa_life` 和 `left_carbon_life` 计算
- 右剩余寿命根据 `right_hepa_life` 和 `right_carbon_life` 计算
- 左右各创建一个 `lv_bar`，分别对应左右滤网的剩余寿命

---

## 6. 剩余寿命计算规则

| 场景 | 计算方式 | 说明 |
|------|---------|------|
| 单滤网机型 | `(hepa_life + carbon_life) / 2` | 取 HEPA 和 Carbon 寿命的平均值 |
| 或 | `MIN(hepa_life, carbon_life)` | 取两者中较低值（更保守） |
| 双滤网机型 | 同上，左右分别计算 | 左: 取 left_* 字段; 右: 取 right_* 字段 |

> **TODO**: 确认采用平均值还是取较低值。

---

## 7. 更新机制

### 7.1 创建函数

```c
void lvgl_filter_create(lv_obj_t *scr, const sensor_data_t *data);
```

- 创建背景矩形框、标题
- 根据 `data->model_type` 判断单/双滤网布局
- 根据 `data->left_filter_type` / `data->right_filter_type` 选择对应 icon
- 创建剩余寿命百分比标签
- 创建 `lv_bar` 进度条，设置初始值

### 7.2 更新函数

```c
void lvgl_filter_update(const sensor_data_t *data);
```

- 更新剩余寿命百分比文本
- 调用 `lv_bar_set_value(bar, life_percent, LV_ANIM_ON)` 更新进度条
- 根据剩余寿命高低动态调整进度条颜色（如：>60% 绿色，30%~60% 黄色，<30% 红色）
- 滤网类型和布局在运行中不变化（由硬件型号决定），无需更新

### 7.3 调用时机

在 `freertos.c` 主循环中，每 500ms：

```c
if (uart_protocol_has_data()) {
    uart_protocol_get_data(&sensor_data);
    if (current_page == PAGE_FILTER) {
        lvgl_filter_update(&sensor_data);
    }
}
```

---

## 8. 所用字体

| 用途 | 字体 / 控件 |
|------|-------------|
| 标题 "Filter" | `inter_bold_42` |
| 剩余寿命百分比数字 | `inter_bold_50` (暂定，或根据实际效果调整) |
| 进度条 | `lv_bar` 控件，自定义配色 |
| 进度条内部文字（可选） | 若需在条内显示百分比，可使用 `inter_bold_40` 或 `inter_regular_18` |

---

## 9. 实现文件清单

| 文件 | 说明 |
|------|------|
| `Core/Src/icon_resource/filter_icons.h` | Filter icon 的 LVGL 图像描述符（已生成） |
| `Core/Src/lv_demo.c` | Filter Page 的创建和更新函数，含 `lv_bar` 创建和更新逻辑（待实现） |
| `Core/Inc/lv_demo.h` | 函数声明（待添加） |
| `Core/Src/freertos.c` | 主循环定时调用更新函数（待修改） |

---

## 10. 待确认事项

- [ ] 剩余寿命计算采用平均值还是取较低值
- [ ] 剩余寿命百分比字体大小和样式
- [ ] 寿命百分比数字与底部边距的精确间距
- [ ] 双滤网布局中左右 icon 的精确间距
- [ ] `lv_bar` 进度条的尺寸（宽度、高度）
- [ ] `lv_bar` 进度条的颜色分段规则（绿色/黄色/红色的阈值）
- [ ] 进度条与百分比数字的间距
- [ ] 进度条是否显示百分比文字在条内或条外