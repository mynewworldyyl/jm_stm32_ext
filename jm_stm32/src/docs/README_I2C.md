# I2C 通用接口 API (OpCode 45-53)

## 概述

本模块提供 Arduino Wire 类风格的 I2C 通信 API，对应 Arduino 的 `Wire.begin()`、
`Wire.requestFrom()`、`Wire.beginTransmission()`、`Wire.endTransmission()`、`Wire.write()`、
`Wire.read()`、`Wire.available()` 等方法。

通过 STM32 的 I2C1 硬件外设实现，支持通用 I2C 主设备通信。

## 集成步骤

### 1. 启用模块宏

在 `jm_pcfg.h` 中开启：

```c
#define JM_I2C_WRAPPER_ENABLE 1
```

### 2. 硬件连接

| 引脚 | 功能 | 说明 |
|------|------|------|
| PA11 | SDA | I2C1 数据线 (开漏输出) |
| PA12 | SCL | I2C1 时钟线 (开漏输出) |

> **注意**：需要外接上拉电阻 (4.7kΩ) 到 3.3V。

### 3. wireId 编码

I2C 引脚通过 `wireId` 编码为 `(SCL引脚 << 8) | SDA引脚`：

| wireId 十进制 | wireId 十六进制 | SDA | SCL |
|---------------|------------------|------|------|
| 2818 | 0x0B0A | PA10 | PA11 |
| 3172 | 0x0C64 | PA68 | PA69 |
| ... | ... | ... | ... |

> 示例：SDA=PA11, SCL=PA12 → wireId = (12<<8\|11) = 3083

### 4. JS 调用示例

```javascript
// 初始化 I2C: SDA=PA11(11), SCL=PA12(12), device addr=0x50
// OpCode=45, i=(12<<8|11)=3083, a=0x50
{
    "funName": 0,
    "op": 45,
    "i": 3083,
    "a": 80
}

// 设置 I2C 时钟为 100kHz
// OpCode=46, i=3083, c=100000
{
    "funName": 0,
    "op": 46,
    "i": 3083,
    "c": 100000
}

// 开始发送 (设备地址 0x50)
// OpCode=47, i=3083, a=80
{
    "funName": 0,
    "op": 47,
    "i": 3083,
    "a": 80
}

// 发送一个字节 0xAB
// OpCode=50, i=3083, d=171
{
    "funName": 0,
    "op": 50,
    "i": 3083,
    "d": 171
}

// 结束发送事务 (释放总线)
// OpCode=48, i=3083, r=1
{
    "funName": 0,
    "op": 48,
    "i": 3083,
    "r": 1
}

// 请求从设备读取 4 字节
// OpCode=49, i=3083, a=80, s=4, p=1
{
    "funName": 0,
    "op": 49,
    "i": 3083,
    "a": 80,
    "s": 4,
    "p": 1
}

// 查询可读字节数
// OpCode=52, i=3083
{
    "funName": 0,
    "op": 52,
    "i": 3083
}

// 读取 4 字节
// OpCode=53, i=3083, s=4
{
    "funName": 0,
    "op": 53,
    "i": 3083,
    "s": 4
}
```

### 5. Arduino API 映射

| Arduino API | OpCode | 说明 |
|-------------|--------|------|
| `Wire.begin()` | 45 | 初始化 I2C 总线 |
| `Wire.setClock()` | 46 | 设置 I2C 时钟频率 |
| `Wire.beginTransmission(addr)` | 47 | 开始发送事务 |
| `Wire.endTransmission()` | 48 | 结束发送事务 |
| `Wire.requestFrom(addr, size)` | 49 | 请求读取 |
| `Wire.write(data)` | 50 | 写入一个字节 |
| `Wire.available()` | 52 | 查询可读字节数 |
| `Wire.read()` | 53 | 读取数据 |

## 参数说明

### OpCode 45: begin

| 参数 | 字段 | 类型 | 说明 |
|------|------|------|------|
| wireId | `i` | int | 总线 ID (编码: (SCL<<8\|SDA)) |
| address | `a` | int | 设备地址 |

### OpCode 46: setClock

| 参数 | 字段 | 类型 | 默认值 | 说明 |
|------|------|------|--------|------|
| wireId | `i` | int | - | 总线 ID |
| clock | `c` | int | 100000 | 时钟频率（Hz） |

### OpCode 47: beginTransmission

| 参数 | 字段 | 类型 | 说明 |
|------|------|------|------|
| wireId | `i` | int | 总线 ID |
| address | `a` | int | 设备地址 |

### OpCode 48: endTransmission

| 参数 | 字段 | 类型 | 默认值 | 说明 |
|------|------|------|--------|------|
| wireId | `i` | int | - | 总线 ID |
| releaseBus | `r` | int | 1 | 1=释放总线, 0=保持 |

### OpCode 49: requestFrom

| 参数 | 字段 | 类型 | 默认值 | 说明 |
|------|------|------|--------|------|
| wireId | `i` | int | - | 总线 ID |
| address | `a` | int | - | 设备地址 |
| size | `s` | int | - | 请求读取字节数 |
| stop | `p` | int | 1 | 1=发送停止信号 |

### OpCode 50: write byte

| 参数 | 字段 | 类型 | 说明 |
|------|------|------|------|
| wireId | `i` | int | 总线 ID |
| data | `d` | int | 数据字节 |

### OpCode 51: get write buffer size

| 参数 | 字段 | 类型 | 说明 |
|------|------|------|------|
| size | `s` | int | 缓冲区大小 |

### OpCode 52: available

| 参数 | 字段 | 类型 | 说明 |
|------|------|------|------|
| wireId | `i` | int | 总线 ID |

### OpCode 53: read

| 参数 | 字段 | 类型 | 说明 |
|------|------|------|------|
| wireId | `i` | int | 总线 ID |
| size | `s` | int | 读取字节数 |

## 响应格式

```json
{
    "code": 0,
    "v": [1, 2, 3, 4],  // 读取到的数据 (OpCode 53)
    "status": 1
}
```

## API 参考

| 函数 | 说明 |
|------|------|
| `int8_t jm_i2c_begin(uint16_t wireId, uint8_t address)` | 初始化 I2C |
| `int8_t jm_i2c_set_clock(uint16_t wireId, uint32_t clock)` | 设置时钟 |
| `int8_t jm_i2c_begin_transmission(uint16_t wireId, uint8_t address)` | 开始发送 |
| `int8_t jm_i2c_end_transmission(uint16_t wireId, bool releaseBus)` | 结束发送 |
| `int8_t jm_i2c_request_from(uint16_t wireId, uint8_t address, uint16_t size, bool stop)` | 请求读取 |
| `int8_t jm_i2c_write_byte(uint16_t wireId, uint8_t data)` | 写入字节 |
| `int8_t jm_i2c_write_buffer(uint16_t wireId, const uint8_t *data, uint16_t size)` | 写入缓冲区 |
| `int8_t jm_i2c_available(uint16_t wireId)` | 查询可读字节数 |
| `int8_t jm_i2c_read(uint16_t wireId, uint8_t *buffer, uint16_t size)` | 读取数据 |

## 使用说明

- 当前仅支持 I2C1 (PA11=SDA, PA12=SCL)
- 最大支持 4 个 I2C 总线实例
- I2C 时钟默认 100kHz，可通过 OpCode 46 调整
- 读取数据时，OpCode 53 返回多个 `v` 字段（每个字节一个）

## 相关文件

- [返回 JS API 主页](README_JS_API.md)
- [返回 EEPROM/存储 API](README_EEPROM.md)
- [返回 demo 目录 README](demo/README.md)
- [返回 jm_stm32 库 README](../README.md)
