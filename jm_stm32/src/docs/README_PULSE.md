# Pulse/脉冲检测 API (OpCode 13, 14)

## 概述

本模块提供脉冲信号宽度测量功能，对应 Arduino 的 `pulseIn()` 和 `pulseInLong()` API。

通过 SysTick 定时器精确测量 GPIO 引脚上脉冲信号的持续时间。

## 集成步骤

### 1. 启用模块宏

在 `jm_pcfg.h` 中开启：

```c
#define JM_APIS_ENABLE 1
```

### 2. JS 调用示例

```javascript
// 测量 PA0 引脚上高电平脉冲宽度（超时 1秒）
// OpCode=13, p=0(PA0), s=1(高电平), t=1000000(1秒)
{
    "funName": 0,
    "op": 13,
    "p": 0,
    "s": 1,
    "t": 1000000
}

// 测量 PA1 引脚上低电平脉冲宽度（超时 500ms）
// OpCode=14 (pulseInLong), p=1(PA1), s=0(低电平), t=500000
{
    "funName": 0,
    "op": 14,
    "p": 1,
    "s": 0,
    "t": 500000
}
```

### 3. Arduino API 映射

| Arduino API | OpCode | 说明 |
|-------------|--------|------|
| `pulseIn(pin, state, timeout)` | 13 | 测量脉冲宽度（微秒） |
| `pulseInLong(pin, state, timeout)` | 14 | 测量脉冲宽度（长超时，完全相同的实现） |

## 参数说明

### OpCode 13: pulseIn

### OpCode 14: pulseInLong

| 参数 | 字段 | 类型 | 默认值 | 说明 |
|------|------|------|--------|------|
| pin | `p` | int | 0 | 引脚编号 (0-15=PA0-PA15, 16-31=PB0-PB15) |
| state | `s` | int | 0 | 等待的电平状态 (0=低电平, 1=高电平) |
| timeout | `t` | int | 1000000 | 超时时间（微秒） |

## 响应格式

```json
{
    "code": 0,
    "v": 1500,    // 脉冲宽度（微秒）
    "status": 1
}
```

| 字段 | 说明 |
|------|------|
| `v` | 测量到的脉冲宽度（微秒），超时时为 0 |
| `status` | 1=成功 |

## API 参考

| 函数 | 说明 |
|------|------|
| `uint32_t tone_pulseIn(uint32_t pin, uint8_t state, uint32_t timeout_us)` | 测量脉冲宽度 |
| `uint32_t tone_pulseInLong(uint32_t pin, uint8_t state, uint32_t timeout_us)` | 测量脉冲宽度（长超时） |

## 使用说明

1. 测量前需要先设置引脚为输入模式
2. 测量过程中阻塞，期间无法处理其他任务
3. SysTick 定时器会被临时 reconfiguration，测量完成后恢复
4. 精确度：1us（72MHz 系统时钟）

## 相关文件

- [返回 JS API 主页](README_JS_API.md)
- [返回 Tone/音乐 API](README_TONE.md)
- [返回 demo 目录 README](demo/README.md)
- [返回 jm_stm32 库 README](../README.md)
