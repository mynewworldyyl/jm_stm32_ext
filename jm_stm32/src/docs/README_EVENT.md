# 异步事件系统测试模块 (jm_stm32_testEvent)

## 概述

本模块演示如何使用 `jm_stm32` 的异步事件系统。通过注册事件监听器，可以在事件发生时被动接收通知，
而无需在主循环中轮询所有事件来源。

- **事件队列**：固定大小 10 的环形队列，先进先出
- **监听器**：最多注册 8 个监听器，按事件类型匹配分发
- **事件来源**：来自 ESP8266 netproxy 的 WiFi、TCP、UDP、MQTT 等事件
- **转发机制**：非网卡上行事件自动转发回网卡

## 集成步骤

### 1. 启用模块宏

在 `jm_pcfg.h` 中开启：

```c
#define JM_STM32_EVENT_ENABLE 1      // 必须启用事件系统
#define JM_STM32_TESTEVENT_ENABLE 1  // 启用本测试模块
```

### 2. 添加源文件

将以下文件添加到工程源文件列表：

```
src/demo/jm_stm32_testEvent.c
```

### 3. 初始化

通过 `jm_comp_init()` 自动调用 `jm_test_event_init()`，在初始化时注册事件监听器。

### 4. 轮询

在主循环中调用 `jm_stm32_loop()`，事件队列会自动处理：

```c
int main(void)
{
    // ... 系统初始化
    jm_stm32_init(&cfg);

    while (1) {
        jm_stm32_loop();    // 自动处理事件队列
    }
}
```

## 支持的事件类型

| 事件常量 | 值 | 说明 |
|----------|-----|------|
| `JM_EVENT_WIFI_STATUS` | 1 | WiFi 连接状态变化 |
| `JM_EVENT_INTERNET_STATUS` | 2 | 互联网可用性变化 |
| `JM_EVENT_LOGIN_RESULT` | 3 | 登录结果 |
| `JM_EVENT_TCP_CONNECTED` | 4 | TCP 连接建立 |
| `JM_EVENT_TCP_DISCONNECTED` | 5 | TCP 连接断开 |
| `JM_EVENT_TCP_SEND_RESULT` | 6 | TCP 发送结果 |
| `JM_EVENT_TCP_ERROR` | 7 | TCP 错误 |
| `JM_EVENT_UID_RESPONSE` | 8 | 设备 ID 响应 |
| `JM_EVENT_SYS_CFG` | 9 | 系统配置 |
| `JM_EVENT_TRANS_CMD` | 10 | 透传命令 |
| `JM_EVENT_CTRL_EVENT` | 11 | 控制事件 |
| `JM_EVENT_TCP_DATA` | 12 | TCP 数据到达 |
| `JM_EVENT_UDP_DATA` | 13 | UDP 数据到达 |
| `JM_EVENT_CTRL_CMD` | 14 | 控制命令 |

## 自定义事件

### 注册监听器

```c
#include "jm_stm32.h"

// 监听自定义事件类型 40
static void my_event_handler(jm_event_t *event)
{
    // 处理事件
    // event->type, event->subType, event->data, event->flag
}

void my_init(void)
{
    jm_stm32_regEventListener(40, my_event_handler);
}
```

### 投递自定义事件

```c
// 投递自定义事件到队列
jm_event_t *my_data = create_my_data();
jm_stm32_postEvent(40, 0, my_data, JM_EVENT_FLAG_DEFAULT);
```

## 事件标志

| 标志 | 值 | 说明 |
|------|-----|------|
| `JM_EVENT_FLAG_DEFAULT` | 0 | 无特殊处理 |
| `JM_EVENT_FLAG_FREE_DATA` | 0x01 | 释放 `data` 指针内存 |
| `JM_EVENT_FLAG_FREE_EMAP` | 0x02 | 释放 emap 数据内存 |
| `JM_EVENT_FLAG_FREE_ELIST` | 0x04 | 释放 elist 内存 |
| `JM_EVENT_FLAG_FREE_STR` | 0x08 | 释放字符串内存 |
| `JM_EVENT_FLAG_FREE_MSG` | 0x10 | 释放消息内存 |
| `JM_EVENT_FLAG_FROM_NETCARD` | 0x20 | 事件来源网卡（不转发） |

## 事件转发

当 STM32 设备收到非网卡上行的事件时，系统会自动通过串口发送给 ESP8266 netproxy，
并由 netproxy 转发到云平台。网卡主动下发的事件（`FROM_NETCARD` 标志）不会转发回网卡。

## API 参考

| 函数 | 说明 |
|------|------|
| `bool jm_stm32_postEvent(uint8_t eventType, uint16_t subType, void *data, uint8_t flag)` | 投递事件到队列 |
| `bool jm_stm32_regEventListener(uint8_t eventType, jm_event_listener_fn callback)` | 注册事件监听器 |
| `bool jm_stm32_unregEventListener(uint8_t eventType, jm_event_listener_fn callback)` | 注销事件监听器 |
| `bool jm_stm32_transEventToCard(jm_event_t *e)` | 将事件上行转发到网卡 |

## 依赖

- `jm_stm32.h` — 核心库
- `JM_STM32_EVENT_ENABLE` 宏必须为 1
- 需要实现 `get_sys_time_ms` 回调（用于事件超时管理）

## 相关文件

- [返回 demo 目录 README](./README.md)
- [返回 jm_stm32 库 README](../../README.md)
- [TCP 测试模块](../README_TCP.md)
- [UDP 测试模块](../README_UDP.md)
