# JS API 调用 STM32 接口开发指南

## 架构概述

目前支持的最新的JS API项目路径
https://github.com/mynewworldyyl/jmicro-devModule

jm_stm32 库通过**串口代理协议**实现 JS 环境（如 H5 浏览器、微信小程序等）与 STM32 硬件的通信。

```
JS 环境 (H5/小程序)
    │
    │ HTTP/WebSocket 或串口
    ▼
ESP8266 netproxy (jm_proxyserial)
    │
    │ 串口 UART (USART1, 115200)
    ▼
STM32 + jm_stm32 库
    │
    │ jm_stm32_ctrl_def / ctrl_remote_ctrlGpio
    ▼
STM32 硬件外设 (GPIO/TIM/ADC/I2C...)
```

### 通信机制

1. **JS 端**：通过 ESP8266 netproxy 发送控制命令
2. **ESP8266**：解析命令，转发为串口协议包发送给 STM32
3. **STM32**：接收串口数据 → 解析为 `jm_emap_t` 键值对 → 分发到 `jm_ctrl_invokeFunc` → 调用对应函数
4. **STM32**：执行硬件操作 → 打包结果到 `jm_emap_t` → 通过串口发送回 ESP8266
5. **ESP8266**：转发响应到 JS 环境

### 控制命令路由

STM32 端通过 `jm_ctrl_registFun(fn, defId)` 注册控制函数。目前有两个注册点：

| defId | 注册函数 | 说明 |
|-------|----------|------|
| 53 | `ctrl_remote_ctrlGpio` (in `jm_stm32_gpio.c`) | GPIO/ADC/PWM/I2C 控制 |
| - | `jm_stm32_ctrl_def` (in `jm_stm32_tone.c`) | 默认控制处理（tone/pulse/shift/interrupt/I2C/EEPROM） |

JS 环境通过 `funName`（对应 defId）字段选择调用哪个 STM32 函数。

## OpCode 映射表

所有控制命令通过 `op` 字段标识功能，参数通过 `jm_emap_t` 传递：

| OpCode | 功能 | 参数字段 | 配置宏 |
|--------|------|----------|--------|
| [11](README_TONE.md) | 播放音调 | `p`(pin), `f`(freq), `d`(duration) | `JM_APIS_ENABLE` |
| [12](README_TONE.md) | 停止音调 | `p`(pin) | `JM_APIS_ENABLE` |
| [13](README_PULSE.md) | 脉冲检测 (pulseIn) | `p`(pin), `s`(state), `t`(timeout) | `JM_APIS_ENABLE` |
| [14](README_PULSE.md) | 脉冲检测 (pulseInLong) | `p`(pin), `s`(state), `t`(timeout) | `JM_APIS_ENABLE` |
| [15](README_SHIFT.md) | 移位输入 (shiftIn) | `p`(dataPin), `c`(clockPin), `b`(bitOrder) | `JM_APIS_ENABLE` |
| [16](README_SHIFT.md) | 移位输出 (shiftOut) | `p`(dataPin), `c`(clockPin), `b`(bitOrder), `v`(val) | `JM_APIS_ENABLE` |
| [17](README_INTERRUPT.md) | 关闭中断 (noInterrupts) | 无 | `JM_STM32_INTERRUPT_ENABLE` |
| [18](README_INTERRUPT.md) | 开启中断 (interrupts) | 无 | `JM_STM32_INTERRUPT_ENABLE` |
| [45](README_I2C.md) | I2C 初始化 | `i`(wireId), `a`(address) | `JM_I2C_WRAPPER_ENABLE` |
| [46](README_I2C.md) | 设置 I2C 时钟 | `i`(wireId), `c`(clock) | `JM_I2C_WRAPPER_ENABLE` |
| [47](README_I2C.md) | 开始发送事务 | `i`(wireId), `a`(address) | `JM_I2C_WRAPPER_ENABLE` |
| [48](README_I2C.md) | 结束发送事务 | `i`(wireId), `r`(releaseBus) | `JM_I2C_WRAPPER_ENABLE` |
| [49](README_I2C.md) | 请求读取 | `i`(wireId), `a`(address), `s`(size), `p`(stop) | `JM_I2C_WRAPPER_ENABLE` |
| [50](README_I2C.md) | 写入字节 | `i`(wireId), `d`(data) | `JM_I2C_WRAPPER_ENABLE` |
| [51](README_I2C.md) | 获取缓冲区大小 | `i`(wireId), `s`(size) | `JM_I2C_WRAPPER_ENABLE` |
| [52](README_I2C.md) | 查询可读字节数 | `i`(wireId) | `JM_I2C_WRAPPER_ENABLE` |
| [53](README_I2C.md) | 读取数据 | `i`(wireId), `s`(size) | `JM_I2C_WRAPPER_ENABLE` |
| [54](README_EEPROM.md) | EEPROM 初始化 | `d`(sda), `c`(scl) | `JM_AT24CXX_ENABLE` |
| [55](README_EEPROM.md) | 读取字节 | `a`(addr) | `JM_AT24CXX_ENABLE` |
| [56](README_EEPROM.md) | 写入字节 | `a`(addr), `d`(data) | `JM_AT24CXX_ENABLE` |
| [57](README_EEPROM.md) | 读取多字节 | `a`(addr), `s`(len) | `JM_AT24CXX_ENABLE` |
| [58](README_EEPROM.md) | 写入字节（带 ACK） | `a`(addr), `d`(data) | `JM_AT24CXX_ENABLE` |
| [59](README_EEPROM.md) | 扫描 I2C 设备 | 无 | `JM_AT24CXX_ENABLE` |

## 开发新 API 的步骤

### 步骤 1：确定 OpCode 和参数

1. 选择一个未使用的 OpCode
2. 确定参数字段名（单个字母缩写，如 `p`=pin, `f`=freq）
3. 确定返回值格式

### 步骤 2：在 `jm_stm32_tone.c` 或 `jm_stm32_gpio.c` 中实现

在 `jm_stm32_ctrl_def()` 函数的 `switch(op)` 中添加新 `case`：

```c
case YOUR_OP_CODE: {
    // 1. 从 emap 中提取参数
    uint32_t param1 = jm_emap_getInt(ps, "p", 0);
    uint32_t param2 = jm_emap_getInt(ps, "f", 0);

    // 2. 调用底层函数执行硬件操作
    your_function(param1, param2);

    // 3. 将结果写回 emap
    jm_emap_putInt(h, "v", result_value, false);
    jm_emap_putInt(h, "status", 1, false);
    break;
}
```

### 步骤 3：在 `jm_pcfg.h` 中添加配置宏（可选）

```c
#define YOUR_MODULE_ENABLE 1
```

### 步骤 4：JS 端调用

```javascript
// 通过 ESP8266 netproxy 发送控制命令
// 命令格式: { funName: 0, op: OP_CODE, 参数... }
```

## 参数命名约定

| 字段 | 类型 | 说明 |
|------|------|------|
| `op` | int | 操作码，必填 |
| `p` | int | 引脚编号 (0-15=PA, 16-31=PB) |
| `c` | int | 时钟引脚或配置参数 |
| `d` | int | 数据或持续时间 |
| `f` | int | 频率 |
| `s` | int | 状态或大小或位序 |
| `v` | int | 值 |
| `i` | int | I2C 总线 ID (编码: (scl<<8\|sda)) |
| `a` | int | 地址 |
| `t` | int | 超时 |
| `r` | int | 释放标志 |
| `b` | int | 位序 (0=LSB-first, 1=MSB-first) |

## 响应格式

所有控制命令的响应都使用 `jm_emap_t` 格式：

| 字段 | 说明 |
|------|------|
| `code` | 状态码 (0=成功, 1=参数错误, 2=未找到命令, 3=硬件错误) |
| `msg` | 错误消息字符串 |
| `v` | 返回值（可能多个） |
| `status` | 状态标志 (1=成功) |

## 示例

参见以下模块文档获取完整示例：

| 模块 | 文档 |
|------|------|
| [Tone/音乐](README_TONE.md) | 音调播放 API |
| [Pulse/脉冲检测](README_PULSE.md) | 脉冲宽度测量 API |
| [Shift/移位寄存器](README_SHIFT.md) | 移位数据读写 API |
| [Interrupt/中断控制](README_INTERRUPT.md) | 中断使能/关闭 API |
| [I2C 通用接口](README_I2C.md) | I2C 主从通信 API |
| [EEPROM 存储](README_EEPROM.md) | AT24CXX EEPROM 读写 API |

## 相关文件

- [返回 jm_stm32 库 README](../README.md)
- [返回 demo 模块 README](demo/README.md)
