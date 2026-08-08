# Interrupt/中断控制 API (OpCode 17, 18)

## 概述

本模块提供全局中断的使能和关闭功能，对应 Arduino 的 `noInterrupts()` 和 `interrupts()` API。

通过操作 CM3 内核的 `PRIMASK` 寄存器，控制全局中断的开启和关闭。

## 集成步骤

### 1. 启用模块宏

在 `jm_pcfg.h` 中开启：

```c
#define JM_STM32_INTERRUPT_ENABLE 1
```

### 2. JS 调用示例

```javascript
// 关闭全局中断
// OpCode=17, 无参数
{
    "funName": 0,
    "op": 17
}

// 开启全局中断
// OpCode=18, 无参数
{
    "funName": 0,
    "op": 18
}
```

### 3. Arduino API 映射

| Arduino API | OpCode | 说明 |
|-------------|--------|------|
| `noInterrupts()` | 17 | 关闭全局中断 |
| `interrupts()` | 18 | 开启全局中断 |

## 参数说明

### OpCode 17: noInterrupts

无参数。

### OpCode 18: interrupts

无参数。

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
| (内部) `__disable_irq()` | 关闭全局中断 (CM3 内核函数) |
| (内部) `__enable_irq()` | 开启全局中断 (CM3 内核函数) |

## 使用说明

- **临界区保护**：在修改共享资源时，先调用 `noInterrupts()`，修改完成后调用 `interrupts()`
- **嵌套**：`__disable_irq()`/`__enable_irq()` 支持嵌套，每次 `disable` 后必须配对 `enable`
- **注意**：关闭中断会阻塞 UART 接收，避免长时间关闭中断

## 相关文件

- [返回 JS API 主页](README_JS_API.md)
- [返回 demo 目录 README](demo/README.md)
- [返回 jm_stm32 库 README](../README.md)
