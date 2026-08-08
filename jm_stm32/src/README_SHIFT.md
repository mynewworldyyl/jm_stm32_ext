# Shift/移位寄存器 API (OpCode 15, 16)

## 概述

本模块提供移位寄存器数据读写功能，对应 Arduino 的 `shiftIn()` 和 `shiftOut()` API。

通过 GPIO 模拟时钟和数据信号，支持 74HC595 等移位寄存器的驱动。

## 集成步骤

### 1. 启用模块宏

在 `jm_pcfg.h` 中开启：

```c
#define JM_TONE_ENABLE 1
```

### 2. JS 调用示例

```javascript
// 从移位寄存器读取一个字节
// OpCode=15, p=0(数据引脚 PA0), c=1(时钟引脚 PA1), b=1(MSB first)
{
    "funName": 0,
    "op": 15,
    "p": 0,
    "c": 1,
    "b": 1
}

// 向移位寄存器写入一个字节 (值=0xAB)
// OpCode=16, p=0(数据引脚 PA0), c=1(时钟引脚 PA1), b=1(MSB first), v=0xAB
{
    "funName": 0,
    "op": 16,
    "p": 0,
    "c": 1,
    "b": 1,
    "v": 171
}
```

### 3. Arduino API 映射

| Arduino API | OpCode | 说明 |
|-------------|--------|------|
| `shiftIn(dataPin, clockPin, bitOrder)` | 15 | 从移位寄存器读取 1 字节 |
| `shiftOut(dataPin, clockPin, bitOrder, val)` | 16 | 向移位寄存器写入 1 字节 |

## 参数说明

### OpCode 15: shiftIn

| 参数 | 字段 | 类型 | 默认值 | 说明 |
|------|------|------|--------|------|
| dataPin | `p` | int | 0 | 数据引脚编号 |
| clockPin | `c` | int | 0 | 时钟引脚编号 |
| bitOrder | `b` | int | 1 | 位序 (0=LSB-first, 1=MSB-first) |

### OpCode 16: shiftOut

| 参数 | 字段 | 类型 | 默认值 | 说明 |
|------|------|------|--------|------|
| dataPin | `p` | int | 0 | 数据引脚编号 |
| clockPin | `c` | int | 0 | 时钟引脚编号 |
| bitOrder | `b` | int | 1 | 位序 (0=LSB-first, 1=MSB-first) |
| val | `v` | int | 0 | 待写入的值 (0-255) |

## 响应格式

```json
{
    "code": 0,
    "v": 171,    // shiftIn 返回值（读取的字节）
    "status": 1
}
```

| 字段 | 说明 |
|------|------|
| `v` | 读取到的数据（仅 shiftIn 返回） |
| `status` | 1=成功 |

## API 参考

| 函数 | 说明 |
|------|------|
| `uint8_t tone_shiftIn(uint32_t dataPin, uint32_t clockPin, uint8_t bitOrder)` | 读取 1 字节 |
| `void tone_shiftOut(uint32_t dataPin, uint32_t clockPin, uint8_t bitOrder, uint8_t val)` | 写入 1 字节 |

## 使用说明

- 时钟引脚使用推挽输出，数据引脚根据操作类型设置为输入或输出
- 支持任意 GPIO 引脚 (PA0-PA15, PB0-PB15)
- 时钟空闲时为低电平，上升沿有效
- 移位顺序可通过 `bitOrder` 参数控制

## 相关文件

- [返回 JS API 主页](README_JS_API.md)
- [返回 Tone/音乐 API](README_TONE.md)
- [返回 demo 目录 README](demo/README.md)
- [返回 jm_stm32 库 README](../README.md)
