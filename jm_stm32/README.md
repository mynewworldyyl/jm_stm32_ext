# jm_stm32

Minimal STM32 serial proxy client for ESP8266 network connectivity.

## Overview

This library enables STM32F103C8T6 (and similar) to access WiFi networks through an ESP8266 acting as a network proxy, communicating over UART. It replaces the heavy `jm_client_stm32` library with a lightweight implementation that fits within the 64KB Flash constraint of C8T6.

## Features

- UART-based communication with ESP8266 network proxy
- Packet fragmentation/reassembly support
- Callback-based event handling
- Minimal memory footprint
- No dependency on jm_client, jm_libs, or Arduino core
- Compatible with Keil/STM32 standard library development

## Protocol

The library implements the same serial protocol as the ESP8266 `jm_proxyserial` component.

### RX Format (ESP8266 -> STM32)

```
[length_hi length_lo] [0x55] [type] [payload...]
```

For `JM_SERIALNET_TYPE_SERIAL (3)`:
```
[length_hi length_lo] [0x55] [3] [subtype_hi subtype_lo] [reqId_hi reqId_lo] [payload...]
```

### TX Format (STM32 -> ESP8266)

```
[length_hi length_lo] [req_id] [0] [0] [0x55] [type] [payload...]
```

For serial commands (`JM_SERIALNET_TYPE_SERIAL`), the payload is automatically wrapped.

## Usage

### 编译选项

在使用轮询读接口前，请在工程中定义 `USE_HAL_UART`（例如在编译器预定义宏里添加），否则 `jm_serial_read()` 将不可用。

### Initialization (HAL)

```c
#include "jm_stm32.h"

static uint32_t get_sys_time(void) {
    return HAL_GetTick();
}

static void uart_send(const uint8_t *data, uint16_t len) {
    HAL_UART_Transmit(&huart1, (uint8_t *)data, len, 100);
}

static void on_event(uint8_t event_type, uint16_t sub_type, void *data, void *user_data) {
    // ... handle events ...
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_USART1_UART_Init();

    jm_config_t config = {
        .get_sys_time_ms = get_sys_time,
        .uart_send      = uart_send,
        .event_cb       = on_event,
        .user_data      = NULL,
    };

    jm_stm32_init(&config);

    while (1) {
        jm_serial_read(&huart1);  // 轮询读取 UART 字节并喂给协议解析
        jm_stm32_loop();          // 处理超时
        // Your application code
    }
}
```

### 轮询读接口

```c
// 在 main 循环中调用，自动读取 UART 硬件 FIFO 中所有已接收字节
int ret = jm_serial_read(&huart1);
if (ret == 0) {
    // 有新数据被处理
}
```

### 原始串口发送

```c
uint8_t raw[] = {0x01, 0x02, 0x03};
jm_serial_write(raw, sizeof(raw));
```

### UART RX ISR（中断模式，可选）

如果使用中断接收，依然可以保留 ISR 方式：

```c
void USART1_IRQHandler(void) {
    uint8_t byte;
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE) != RESET) {
        byte = (uint8_t)(huart1.Instance->DR & 0xFF);
        jm_stm32_uart_rx_byte(byte);
    }
}
```

### Sending Commands

```c
// Send WiFi credentials
jm_stm32_send_wifi_cfg("SSID", "PASSWORD");

// Request WiFi status
jm_stm32_send_wifi_status_req();

// Request internet status
jm_stm32_send_internet_status_req();

// Send login request
jm_stm32_send_login();

// Connect TCP
jm_stm32_send_tcp_connect("example.com", 8080);

// Close TCP
jm_stm32_send_tcp_close(sock);

// Send TCP data
jm_stm32_send_tcp_data(sock, data, len);

// Play audio text
jm_stm32_send_audio_play("Hello World");
```

## API Reference

See `jm_stm32.h` for complete API documentation.

### Events

| Event Type | Description | Data Type |
|------------|-------------|-----------|
| `JM_EVENT_WIFI_STATUS` | WiFi connection status changed | `jm_wifi_status_t *` |
| `JM_EVENT_INTERNET_STATUS` | Internet connectivity status | `jm_wifi_status_t *` |
| `JM_EVENT_LOGIN_RESULT` | Login result from JM platform | `jm_login_result_t *` |
| `JM_EVENT_TCP_CONNECTED` | TCP connection established | `jm_tcp_conn_info_t *` |
| `JM_EVENT_TCP_DISCONNECTED` | TCP connection closed | `jm_tcp_conn_info_t *` |
| `JM_EVENT_TCP_SEND_RESULT` | TCP send result | `jm_tcp_conn_info_t *` |
| `JM_EVENT_TCP_ERROR` | TCP error occurred | `jm_tcp_conn_info_t *` |
| `JM_EVENT_UID_RESPONSE` | Request device UID from STM32 | `NULL` |
| `JM_EVENT_SYS_CFG` | System configuration received | `jm_sys_cfg_t *` |
| `JM_EVENT_TRANS_CMD` | Control command received | `uint8_t *` data |
| `JM_EVENT_CTRL_EVENT` | Control event received | `uint8_t *` data |

## Compatibility

- STM32F103C8T6 (and other STM32F1 series)
- Works with `jm_client_esp8266` netproxy component
- No changes required on ESP8266 side

## License

MIT
