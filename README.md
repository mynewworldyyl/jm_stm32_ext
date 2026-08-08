# jm_stm32

STM32 通过 ESP8266 串口实现 TCP/UDP/MQTT 网络连接的解决方案。

通过 UART 与 ESP8266 通信，调用 ESP8266 的 netproxy 功能，实现 STM32 与云服务器之间的透传通信。用户无需在 STM32 端实现复杂的网络协议栈。

**默认使用寄存器直驱（CMSIS）模式**，无需 HAL 库，适合 64KB Flash 的 STM32F103C8T6 等资源受限的芯片。同时支持可选的 HAL 库模式。

## 项目结构

```
jm_stm32/
├── jm_stm32/          # 核心协议库（Arduino 库格式）
└── jm_stm32_pio/      # 基于 PlatformIO 的示例工程
```

| 目录 | 说明 |
|------|------|
| [`jm_stm32/`](jm_stm32/README.md) | 核心协议库，提供串口协议封装与事件回调 |
| [`jm_stm32_pio/`](jm_stm32_pio/README.md) | PlatformIO 示例工程，包含完整的硬件接线、配置和使用说明 |

## 快速开始

**使用 PlatformIO (寄存器直驱模式)：**

```
cd jm_stm32_pio
pio run -t download
pio monitor
```

**在 Arduino 环境中：**

将 `jm_stm32/` 目录复制到 Arduino `libraries` 目录下，使用库例程即可。

## 相关文档

- [jm_stm32 库文档](jm_stm32/README.md) — 协议格式、API 接口、事件说明
- [jm_stm32_pio 示例说明](jm_stm32_pio/README.md) — 硬件接线、环境配置、工程集成、调试方法

## License

MIT