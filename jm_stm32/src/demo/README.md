# demo 示例模块

本目录提供 `jm_stm32` 库的功能演示示例，每个示例对应一个功能模块。

## 目录结构

```
demo/
├── README.md                    # 本文件
├── README_TCP.md                # TCP 连接测试模块
├── README_UDP.md                # UDP 通信测试模块
├── README_EVENT.md              # 异步事件系统测试模块
├── README_MQTT_CLIENT.md        # MQTT 客户端测试模块
├── jm_stm32_tcp_test.c          # TCP 测试源文件
├── jm_stm32_udp_test.c          # UDP 测试源文件
├── jm_stm32_testEvent.c         # 事件测试源文件
└── jm_stm32_mqtt_client_test.c  # MQTT 客户端测试源文件
```

## 可用模块

| 模块 | 源文件 | 说明 | 配置文件宏 |
|------|--------|------|------------|
| [TCP 连接测试](README_TCP.md) | `jm_stm32_tcp_test.c` | TCP 连接、发送、接收 | `JM_STM32_TESTTCP_ENABLE` |
| [UDP 通信测试](README_UDP.md) | `jm_stm32_udp_test.c` | UDP 数据发送、接收 | `JM_STM32_TESTUDP_ENABLE` |
| [事件系统测试](README_EVENT.md) | `jm_stm32_testEvent.c` | 异步事件监听与分发 | `JM_STM32_TESTEVENT_ENABLE` |
| [MQTT 客户端测试](README_MQTT_CLIENT.md) | `jm_stm32_mqtt_client_test.c` | MQTT 连接、发布、订阅 | `JM_MQTT_CLIENT_TEST_ENABLE` |

## 快速开始

### 1. 配置模块开关

在 `jm_pcfg.h` 中启用所需模块：

```c
#define JM_STM32_EVENT_ENABLE 1     // 事件系统（推荐启用）
#define JM_STM32_TESTTCP_ENABLE 1   // 启用 TCP 测试
#define JM_STM32_TESTUDP_ENABLE 1   // 启用 UDP 测试
#define JM_MQTT_CLIENT_ENABLE 1     // 启用 MQTT 客户端
#define JM_MQTT_CLIENT_TEST_ENABLE 1 // 启用 MQTT 测试
```

### 2. 将测试模块添加到工程

将选中的测试文件添加到 `platformio.ini` 的源文件列表：

```ini
[platformio]
src_dir = src
build_src_filter =
    +<*.c>
    +<src/demo/jm_stm32_tcp_test.c>
    +<src/demo/jm_stm32_udp_test.c>
    +<src/demo/jm_stm32_testEvent.c>
    +<src/demo/jm_stm32_mqtt_client_test.c>
```

### 3. 在 `main.c` 中注册事件回调

在 `on_event()` 回调函数中分发事件到对应测试模块：

```c
static void on_event(uint8_t event_type, uint16_t sub_type, void *data, void *user_data)
{
    switch (event_type) {
    // TCP 模块事件
    case JM_EVENT_TCP_CONNECTED:
    case JM_EVENT_TCP_DISCONNECTED:
    case JM_EVENT_TCP_SEND_RESULT:
    case JM_EVENT_TCP_ERROR:
    case JM_EVENT_TCP_DATA:
        jm_tcp_test_on_event(event_type, data);
        break;

    // UDP 模块事件
    case JM_EVENT_UDP_DATA:
        jm_udp_test_on_event(event_type, data);
        break;

    // 其他事件...
    }
}
```

### 4. 运行

编译并烧录到 STM32 开发板，打开串口监控工具（日志波特率 115200）即可查看测试结果。

## 详细文档

| 模块 | 整合指南 |
|------|----------|
| [TCP 连接测试](README_TCP.md) | 启用步骤、配置参数、API 参考 |
| [UDP 通信测试](README_UDP.md) | 启用步骤、配置参数、API 参考 |
| [事件系统测试](README_EVENT.md) | 事件类型、监听器注册、自定义事件 |
| [MQTT 客户端测试](README_MQTT_CLIENT.md) | 连接配置、回调函数、发布订阅示例 |

## JS API 接口

jm_stm32 还支持通过 JS 环境（H5/微信小程序）调用 STM32 硬件接口：

| 模块 | OpCode | 说明 |
|------|--------|------|
| [Tone 音乐](../README_TONE.md) | 11, 12 | 音调播放/停止 |
| [Pulse 脉冲检测](../README_PULSE.md) | 13, 14 | 脉冲宽度测量 |
| [Shift 移位寄存器](../README_SHIFT.md) | 15, 16 | 移位数据读写 |
| [Interrupt 中断控制](../README_INTERRUPT.md) | 17, 18 | 全局中断使能/关闭 |
| [I2C 通用接口](../README_I2C.md) | 45-53 | I2C 主从通信 |
| [EEPROM 存储](../README_EEPROM.md) | 54-59 | AT24CXX EEPROM 读写 |

详见 [JS API 开发指南](../README_JS_API.md)。

## 相关链接

- [返回 jm_stm32 库主 README](../README.md)
- [返回 jm_stm32_pio 示例工程 README](../../../jm_stm32_pio/README.md)
