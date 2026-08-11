# EEPROM/存储 API (OpCode 54-59)

## 概述

本模块提供 AT24CXX 系列 EEPROM（如 AT24C02、AT24C04、AT24C32 等）的读写功能。

通过 I2C 总线与 EEPROM 通信，支持字节读写、页读写等操作。

## 集成步骤

### 1. 启用模块宏

在 `jm_pcfg.h` 中开启：

```c
#define JM_AT24CXX_ENABLE 1
```

### 2. 硬件连接

| 引脚 | 功能 | 说明 |
|------|------|------|
| PA11 | SDA | I2C 数据线 (开漏输出) |
| PA12 | SCL | I2C 时钟线 (开漏输出) |

> 需要外接上拉电阻 (4.7kΩ) 到 3.3V。EEPROM 设备地址默认为 0x50。

### 3. JS 调用示例

```javascript
// 初始化 EEPROM I2C 接口 (SDA=PA11=11, SCL=PA12=12)
// OpCode=54, d=11(SDA), c=12(SCL)
{
    "funName": 0,
    "op": 54,
    "d": 11,
    "c": 12
}

// 读取地址 10 的字节
// OpCode=55, a=10
{
    "funName": 0,
    "op": 55,
    "a": 10
}

// 向地址 10 写入字节 0xAB
// OpCode=56, a=10, d=171
{
    "funName": 0,
    "op": 56,
    "a": 10,
    "d": 171
}

// 读取地址 10 的 8 个字节
// OpCode=57, a=10, s=8
{
    "funName": 0,
    "op": 57,
    "a": 10,
    "s": 8
}

// 写字节到地址 10（带成功/失败判断）
// OpCode=58, a=10, d=171
{
    "funName": 0,
    "op": 58,
    "a": 10,
    "d": 171
}

// 扫描 I2C 总线 (地址 8-119)
// OpCode=59, 无参数
{
    "funName": 0,
    "op": 59
}
```

## OpCode 说明

| OpCode | 功能 | 参数 | 说明 |
|--------|------|------|------|
| 54 | 初始化 EEPROM | `d`(SDA pin), `c`(SCL pin) | 初始化 I2C 接口 |
| 55 | 读取字节 | `a`(addr) | 读取指定地址的一个字节 |
| 56 | 写入字节 | `a`(addr), `d`(data) | 向指定地址写入一个字节 |
| 57 | 读取多字节 | `a`(addr), `s`(len) | 读取指定地址开始的多个字节 |
| 58 | 写入字节（带 ACK） | `a`(addr), `d`(data) | 写入并返回操作结果 |
| 59 | 扫描设备 | 无 | 扫描 I2C 总线 8-119 地址 |

## 参数说明

### OpCode 54-58

| 参数 | 字段 | 类型 | 说明 |
|------|------|------|------|
| addr | `a` | int | EEPROM 地址 (0-65535) |
| data | `d` | int | 数据字节 (0-255) |
| sda | `d` | int | SDA 引脚编号 (OpCode 54) |
| scl | `c` | int | SCL 引脚编号 (OpCode 54) |
| length | `s` | int | 读取长度 (1-64) (OpCode 57) |

## 响应格式

### OpCode 55/56/58: 读取/写入单个字节

```json
{
    "code": 0,
    "v": 171,    // 返回值（读取字节值或写入结果）
    "status": 1
}
```

### OpCode 57: 读取多字节

```json
{
    "code": 0,
    "v": [171, 87, 200, ...],  // 读取到的多个字节
    "status": 1
}
```

### OpCode 59: 扫描设备

```json
{
    "code": 0,
    "status": 1
}
```

## 使用说明

- **设备地址**：固定为 0x50（AT24CXX 默认地址）
- **写延时**：写入后自动延时 10ms，等待 EEPROM 写完成
- **地址范围**：支持 16 位地址（适用于 AT24C32 等大容量 EEPROM）
- **读取长度**：一次最多读取 64 字节
- **I2C 时钟**：默认 100kHz

## API 参考

| 函数 | 说明 |
|------|------|
| `bool eeprom_i2c_init(uint8_t sda, uint8_t scl)` | 初始化 I2C 接口 |
| `bool eeprom_write_byte(uint16_t addr, uint8_t data)` | 写入一个字节 |
| `uint8_t eeprom_read_byte(uint16_t addr)` | 读取一个字节 |
| `bool eeprom_read_bytes(uint16_t addr, uint8_t *buf, uint8_t len)` | 读取多个字节 |
| `void eeprom_scan_devices(void)` | 扫描 I2C 总线 |
| `jm_emap_t* jm_stm32_at24cxx_call(jm_emap_t *ps)` | EEPROM 控制命令处理 |

## 相关文件

- [返回 JS API 主页](README_JS_API.md)
- [返回 I2C 通用接口 API](README_I2C.md)
- [返回 demo 目录 README](demo/README.md)
- [返回 jm_stm32 库 README](../README.md)
