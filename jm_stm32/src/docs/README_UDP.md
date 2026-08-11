# UDP 通信测试模块 (jm_stm32_udp_test)

## 概述

本模块演示如何通过 `jm_stm32` 库向 ESP8266 netproxy 发送 UDP 数据，以及接收 UDP 上行数据。

- 按键触发向指定 UDP 服务器发送数据包
- 通过 ESP8266 netproxy 透传，服务器可接收 STM32 发送的 UDP 数据
- 收到 UDP 数据通过 `JM_EVENT_UDP_DATA` 事件上报

## 集成步骤

### 1. 启用模块宏

在 `jm_pcfg.h` 中开启：

```c
#define JM_STM32_TESTUDP_ENABLE 1
```

### 2. 添加源文件

将以下文件添加到工程源文件列表：

```
src/demo/jm_stm32_udp_test.c
```

### 3. 注册事件回调

在 `main.c` 的 `on_event()` 回调中添加 UDP 事件分发：

```c
static void on_event(uint8_t event_type, uint16_t sub_type, void *data, void *user_data)
{
    switch (event_type) {
    case JM_EVENT_UDP_DATA:
        jm_udp_test_on_event(event_type, data);
        break;
    // ... 其他事件
    }
}
```

### 4. 初始化与轮询

通过 `jm_comp_init()` 自动初始化；在主循环中调用 `jm_comp_loop()` 自动轮询：

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

在 `jm_stm32_udp_test.c` 中修改以下宏：

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `UDP_TEST_HOST` | `"192.168.3.10"` | UDP 目标服务器地址 |
| `UDP_TEST_PORT` | `9999` | UDP 目标服务器端口 |
| `UDP_TEST_BTN_PIN` | `0` | 按键引脚编号 (PA0) |
| `BTN_DEBOUNCE_MS` | `70` | 按键去抵抗时间（ms） |

## 使用方法

1. 上电运行，按下 PA0 按键 → 向 UDP 服务器发送 `"Hello from STM32 UDP\n"`
2. 服务器通过 ESP8266 netproxy 向 STM32 发送 UDP 数据
3. 接收到 UDP 数据 → 通过日志串口打印输出

## API 参考

| 函数 | 说明 |
|------|------|
| `void jm_udp_test_init(const jm_config_t *config)` | 初始化 UDP 测试模块 |
| `void jm_udp_test_loop(void)` | 轮询函数（检测按键、触发发送） |
| `void jm_udp_test_on_event(uint8_t event_type, jm_buf_t *data)` | UDP 事件处理回调 |

## 依赖

- `jm_stm32.h` — 核心库
- `JM_STM32_TESTUDP_ENABLE` 宏必须为 1
- 需要实现 `get_sys_time_ms` 和 `uart_send` 回调

## 相关文件

- [返回 demo 目录 README](./README.md)
- [返回 jm_stm32 库 README](../../README.md)
