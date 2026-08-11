# Tone/音乐 API (OpCode 11, 12)

## 概述

本模块提供蜂鸣器/扬声器音调播放功能，对应 Arduino 的 `tone()` 和 `noTone()` API。

通过 PWM 定时器输出指定频率的方波，支持多引脚独立控制。

## 集成步骤

### 1. 启用模块宏

在 `jm_pcfg.h` 中开启：

```c
#define JM_APIS_ENABLE 1
```

### 2. 硬件连接

连接蜂鸣器/扬声器到以下引脚：

| 引脚 | 定时器 | PWM 通道 |
|------|--------|----------|
| PA0 | TIM2 | CH1 |
| PA1 | TIM2 | CH2 |
| PA2 | TIM2 | CH3 |
| PA3 | TIM2 | CH4 |
| PA6 | TIM3 | CH1 |
| PA7 | TIM3 | CH2 |
| PA8 | TIM3 | CH3 |
| PA9 | TIM3 | CH4 |

> PA9 同时用于 USART1 TX，播放音调时请避免使用此引脚。

### 3. JS 调用示例

```javascript
// 播放 440Hz 音调，持续 1000ms
// OpCode=11, p=0(PA0), f=440, d=1000
{
    "funName": 0,
    "op": 11,
    "p": 0,
    "f": 440,
    "d": 1000
}

// 停止播放
// OpCode=12, p=0(PA0)
{
    "funName": 0,
    "op": 12,
    "p": 0
}
```

### 4. Arduino API 映射

| Arduino API | OpCode | 说明 |
|-------------|--------|------|
| `tone(pin, freq, duration)` | 11 | 播放指定频率音调 |
| `noTone(pin)` | 12 | 停止播放 |

## 参数说明

### OpCode 11: tone

| 参数 | 字段 | 类型 | 默认值 | 说明 |
|------|------|------|--------|------|
| pin | `p` | int | 0 | 引脚编号 (0-3, 6-7) |
| freq | `f` | int | 0 | 频率（Hz） |
| duration | `d` | int | 0 | 持续时间（ms），0=持续播放 |

### OpCode 12: noTone

| 参数 | 字段 | 类型 | 默认值 | 说明 |
|------|------|------|--------|------|
| pin | `p` | int | 0 | 引脚编号 |

## 响应格式

```json
{
    "code": 0,
    "status": 1
}
```

## API 参考

| 函数 | 说明 |
|------|------|
| `void tone(uint32_t pin, uint32_t freq, uint32_t duration)` | 播放音调 |
| `void noTone(uint32_t pin)` | 停止播放 |

## 限制

- 支持引脚：PA0-PA3, PA6-PA7 (TIM2 CH1-CH4, TIM3 CH1-CH2)
- PWM 频率范围：1 ~ 2MHz
- 占空比默认 50%

## 相关文件

- [返回 JS API 主页](README_JS_API.md)
- [返回 demo 目录 README](demo/README.md)
- [返回 jm_stm32 库 README](../README.md)
