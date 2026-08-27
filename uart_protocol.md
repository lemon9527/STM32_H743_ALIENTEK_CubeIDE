# UART1 Protocol: ControlPCB -> Display

**链路**: ControlPCB (另一个MCU) → STM32H743ZITx Display 驱动板

## UART1 配置

| 参数 | 值 |
|------|-----|
| Baud Rate | 115200 |
| Data Bits | 8 |
| Parity | None |
| Stop Bits | 1 |
| Flow Control | None |

## 字节序

ControlPCB 发送的所有多字节字段均为 **大端序 (Big-Endian, MSB first)**。

例如 `00 05` 表示 `0x0005 = 5`，`03 84` 表示 `0x0384 = 900`。

## 帧格式

| 字段 | 长度 (byte) | 说明 |
|------|:---:|------|
| Start | 2 | 帧头 `0x44 0x4D` |
| Frame Length | 2 | 后面所有 byte 的个数 (38 = 0x26) |
| PM2.5 | 2 | PM2.5 浓度 (μg/m³) |
| PM10 | 2 | PM10 浓度 (μg/m³) |
| PM0.3 | 2 | PM0.3 浓度 (μg/m³) |
| Model Type | 1 | 设备型号<br>3 = 单滤网<br>4 = 双滤网 |
| Machine Mode | 1 | 运行模式 <br>0 = running mode<br> 1 = sleep mode (0x00)<br>|
| Reserved | 2 | 保留 |
| Left Filter Type | 1 | 左滤网类型<br>0 = filter missing<br>1 = Standard (HEPA)<br>2 = Carbon<br>3 = Hybrid<br>4 = IAQP |
| Right Filter Type | 1 | 右滤网类型<br>0 = filter missing<br>1 = Standard (HEPA)<br>2 = Carbon<br>3 = Hybrid<br>4 = IAQP |
| Left HEPA Life | 1 | 左 HEPA 滤网剩余寿命 |
| Left Carbon Life | 1 | 左活性炭滤网剩余寿命 |
| Right HEPA Life | 1 | 右 HEPA 滤网剩余寿命 |
| Right Carbon Life | 1 | 右活性炭滤网剩余寿命 |
| VOC Level | 1 | VOC 等级 |
| Screen State | 1 | 屏幕状态 |
| Door State | 1 | 门状态 |
| Air Index | 1 | 空气质量指数 |
| CO2 | 2 | CO2 浓度 (ppm) |
| TVOC | 2 | TVOC 浓度 (ppb) |
| Temperature | 2 | 温度 (原始值) |
| Humidity | 2 | 湿度 (原始值) |
| Pressure | 2 | 气压 (原始值) |
| Connection Type | 1 | 连接类型 |
| Lightsensor State | 1 | 光传感器状态 |
| Brightness Mode | 1 | 亮度模式 |
| Lightsensor Level | 1 | 光传感器等级 |
| Reserved | 1 | 保留 |
| Sys Error Code | 1 | 系统错误码 |
| Checksum | 2 | 16-bit 累加和 (取低 16 位) |

**数据字段总计**: 36 bytes | **帧总长**: 42 bytes

## 校验和

```
checksum = sum(所有 checksum 之前的字节) & 0xFFFF
```

覆盖范围: Start + Frame Length + 所有数据字段

Checksum 字段同样为大端序，例如 `02 C6` 表示 `0x02C6`。

## 示例数据 1

```
44 4D 00 26 00 05 00 00 00 00 04 00 00 00 00 00 32 32 32 32 00 00 01 64 02 26 00 09 00 20 00 00 03 84 00 01 00 00 00 00 02 C6
```

### 解析结果

| 字段 | 原始值 | 解析值 |
|------|------|------|
| Start | 44 4D | ✓ |
| Frame Length | 00 26 | 38 |
| PM2.5 | 00 05 | 5 |
| PM10 | 00 00 | 0 |
| PM0.3 | 00 00 | 0 |
| Model Type | 04 | 4 |
| Machine Mode | 00 | 0 |
| Reserved | 00 00 | 0 |
| Left Filter Type | 00 | 0 |
| Right Filter Type | 00 | 0 |
| Left HEPA Life | 32 | 50 |
| Left Carbon Life | 32 | 50 |
| Right HEPA Life | 32 | 50 |
| Right Carbon Life | 32 | 50 |
| VOC Level | 00 | 0 |
| Screen State | 00 | 0 |
| Door State | 01 | 1 |
| Air Index | 64 | 100 |
| CO2 | 02 26 | 550 |
| TVOC | 00 09 | 9 |
| Temperature | 00 20 | 32 |
| Humidity | 00 00 | 0 |
| Pressure | 03 84 | 900 |
| Connection Type | 00 | 0 |
| Lightsensor State | 01 | 1 |
| Brightness Mode | 00 | 0 |
| Lightsensor Level | 00 | 0 |
| Reserved | 00 | 0 |
| Sys Error Code | 00 | 0 |
| Checksum | 02 C6 | ✓ (sum = 0x02C6) |

## 示例数据 2
```
44 4D 00 26 00 2D 00 00 00 00 04 00 00 00 00 00 32 32 32 32 00 00 01 59 01 F3 00 63 00 18 00 37 03 84 00 01 00 00 00 00 04 38
```

### 解析结果

| 字段 | 原始值 | 解析值 |
|------|--------|--------|
| Start | 44 4D | ✓ |
| Frame Length | 00 26 | 38 |
| PM2.5 | 00 2D | 45 |
| PM10 | 00 00 | 0 |
| PM0.3 | 00 00 | 0 |
| Model Type | 04 | 4 |
| Machine Mode | 00 | 0 |
| Reserved | 00 00 | 0 |
| Left Filter Type | 00 | 0 |
| Right Filter Type | 00 | 0 |
| Left HEPA Life | 32 | 50 |
| Left Carbon Life | 32 | 50 |
| Right HEPA Life | 32 | 50 |
| Right Carbon Life | 32 | 50 |
| VOC Level | 00 | 0 |
| Screen State | 00 | 0 |
| Door State | 01 | 1 |
| Air Index | 59 | 89 |
| CO2 | 01 F3 | 499 |
| TVOC | 00 63 | 99 |
| Temperature | 00 18 | 24 |
| Humidity | 00 37 | 55 |
| Pressure | 03 84 | 900 |
| Connection Type | 00 | 0 |
| Lightsensor State | 01 | 1 |
| Brightness Mode | 00 | 0 |
| Lightsensor Level | 00 | 0 |
| Reserved | 00 | 0 |
| Sys Error Code | 00 | 0 |
| Checksum | 04 38 | ✓ (sum = 0x0438)


## 时序

- ControlPCB 每秒发送一帧数据
- 显示驱动板被动接收，不主动请求