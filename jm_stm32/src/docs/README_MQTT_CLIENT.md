# MQTT 客户端测试模块 (jm_stm32_mqtt_client_test)

## 概述

本模块演示如何使用 `jm_mqtt_client` API 与 ESP8266 netproxy 进行 MQTT 通信。
STM32 本身不运行完整的 MQTT 协议栈，所有 MQTT 操作通过串口协议发送给 ESP8266，
由 ESP8266 完成实际的 MQTT 协议处理。

- **自动连接**：首次轮询时自动连接到指定 MQTT 代理服务器
- **按键发布**：按下按键时发布消息到指定主题
- **自动订阅**：连接成功后自动订阅指定主题
- **消息接收**：订阅主题的消息通过回调函数接收

## 集成步骤

### 1. 启用模块宏

在 `jm_pcfg.h` 中开启：

```c
#define JM_MQTT_CLIENT_ENABLE 1      // 启用 MQTT 客户端框架
#define JM_MQTT_CLIENT_TEST_ENABLE 1  // 启用本测试模块
```

### 2. 添加源文件

将以下文件添加到工程源文件列表：

```
src/jm_mqtt_client.c          // MQTT 客户端核心实现
src/demo/jm_stm32_mqtt_client_test.c  // 测试模块
```

### 3. 初始化

通过 `jm_comp_init()` 自动调用 `jm_mqtt_client_test_init()`，
在初始化时注册消息回调、连接回调、断开回调。

### 4. 轮询

在主循环中调用 `jm_comp_loop()` 自动轮询：

```c
int main(void)
{
    // ... 系统初始化
    jm_stm32_init(&cfg);

    while (1) {
        jm_stm32_loop();    // 自动调用 jm_comp_loop()
    }
}
```

## 配置参数

在 `jm_stm32_mqtt_client_test.c` 中修改以下宏：

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `MQTT_BROKER_HOST` | `"192.168.3.10"` | MQTT 代理服务器地址 |
| `MQTT_BROKER_PORT` | `1883` | MQTT 代理服务器端口 |
| `MQTT_CLIENT_ID` | `"stm32_mqtt_client_test"` | 客户端 ID（需全局唯一） |
| `MQTT_KEEPALIVE` | `60` | 心跳间隔（秒） |
| `MQTT_SUB_TOPIC` | `"stm32/test"` | 订阅主题 |
| `MQTT_PUB_TOPIC` | `"stm32/test"` | 发布主题 |
| `MQTT_PUB_MSG` | `"Hello from STM32 MQTT client"` | 默认发布消息 |

> **注意**：连接凭据（用户名/密码）在本示例中硬编码为 `("jmicro", "jmicro123")`，
> 请根据实际 MQTT 服务器配置修改 `jm_mqtt_client_connect()` 调用中的参数。

## 使用方法

1. 上电运行 → 自动连接到 MQTT 代理服务器
2. 连接成功后 → 自动订阅 `MQTT_SUB_TOPIC`
3. 按下 PA0 按键 → 发布 `MQTT_PUB_MSG` 到 `MQTT_PUB_TOPIC`
4. 订阅主题收到消息 → 通过 `mqtt_client_message_callback` 输出到日志

## 自定义使用

### 自定义回调函数

```c
#include "jm_mqtt_client.h"

// 消息接收回调
static void on_mqtt_message(const char *topic, const uint8_t *payload, uint16_t len)
{
    // 处理接收到的消息
}

// 连接成功回调
static void on_mqtt_connect(void)
{
    // 订阅主题
    jm_mqtt_client_subscribe("my/topic", 0);
}

// 断开连接回调
static void on_mqtt_disconnect(void)
{
    // 重新连接逻辑
}

void my_mqtt_init(void)
{
    jm_mqtt_client_init(on_mqtt_message, on_mqtt_connect, on_mqtt_disconnect);
}
```

### 发布消息

```c
const char *msg = "Hello MQTT";
jm_mqtt_client_publish("my/topic", (const uint8_t *)msg, strlen(msg), 0, false);
```

### 订阅/取消订阅

```c
jm_mqtt_client_subscribe("my/topic", 0);       // 订阅
jm_mqtt_client_unsubscribe("my/topic");        // 取消订阅
```

## API 参考

| 函数 | 说明 |
|------|------|
| `int jm_mqtt_client_init(msg_cb, connect_cb, disconnect_cb)` | 初始化 MQTT 客户端 |
| `int jm_mqtt_client_connect(host, port, id, keepalive, user, pass)` | 连接到服务器 |
| `int jm_mqtt_client_publish(topic, payload, len, qos, retained)` | 发布消息 |
| `int jm_mqtt_client_subscribe(topic, qos)` | 订阅主题 |
| `int jm_mqtt_client_unsubscribe(topic)` | 取消订阅 |
| `int jm_mqtt_client_disconnect(void)` | 断开连接 |
| `void jm_mqtt_client_loop(void)` | 轮询（处理心跳超时） |
| `bool jm_mqtt_client_is_connected(void)` | 检查连接状态 |

## 回调函数类型

| 类型 | 说明 |
|------|------|
| `jm_mqtt_client_msg_cb` | 消息到达回调 |
| `jm_mqtt_client_connect_cb` | 连接成功回调 |
| `jm_mqtt_client_disconnect_cb` | 断开连接回调 |

## 依赖

- `jm_stm32.h` — 核心库
- `jm_mqtt_client.h` / `jm_mqtt_client.c` — MQTT 客户端实现
- `JM_MQTT_CLIENT_ENABLE` 宏必须为 1
- ESP8266 netproxy 必须在 MQTT 代理模式下运行

## 相关文件

- [返回 demo 目录 README](./README.md)
- [返回 jm_stm32 库 README](../../README.md)
- [事件测试模块](../README_EVENT.md)
