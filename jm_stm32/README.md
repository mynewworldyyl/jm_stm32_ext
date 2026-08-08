# jm_stm32

Minimal STM32 serial proxy client for ESP8266 network connectivity.

## Overview

This library enables STM32F103C8T6 (and similar) to access WiFi networks through an ESP8266 acting as a network proxy, communicating over UART. It uses the **register-direct (CMSIS) API** as the default development mode, with optional HAL library support.

The library is lightweight and fits within the 64KB Flash constraint of C8T6.

## Features

- UART-based communication with ESP8266 network proxy
- Register-direct (CMSIS) mode as default — no HAL dependency required
- Optional HAL library mode (define `USE_HAL_UART` to enable)
- Packet fragmentation/reassembly support
- Callback-based event handling
- Minimal memory footprint
- Compatible with Keil/STM32 standard library development

## Supported Development Modes

### Register-direct Mode (Default)

Uses CMSIS register definitions directly. Framework: `cmsis`. No HAL library needed.

- UART communication via direct register access
- System clock configured via RCC registers
- SysTick for timing
- Interrupt-driven UART RX (`USART1_IRQHandler`)

### HAL Library Mode (Optional)

Define `USE_HAL_UART` in build flags to use STM32Cube HAL. Framework: `stm32cube`.

- Uses `HAL_UART_Transmit()` for sending
- Uses `HAL_GetTick()` for timing
- Uses `jm_serial_read(&huart1)` for polling-based UART reads

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

### Initialization (Register-direct Mode — Default)

```c
#include "jm_stm32.h"
#include "stm32f1xx.h"

static volatile uint32_t sys_tick_ms = 0;

void SysTick_Handler(void)
{
    sys_tick_ms++;
}

static uint32_t get_sys_time(void)
{
    return sys_tick_ms;
}

static void uart_send(const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        while (!(USART1->SR & USART_SR_TXE));
        USART1->DR = data[i];
    }
    while (!(USART1->SR & USART_SR_TC));
}

static void uart_send_log(const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        while (!(USART2->SR & USART_SR_TXE));
        USART2->DR = data[i];
    }
}

static void on_event(uint8_t event_type, uint16_t sub_type, void *data, void *user_data)
{
    // ... handle events ...
}

void USART1_IRQHandler(void)
{
    if (USART1->SR & USART_SR_RXNE) {
        uint8_t byte = (uint8_t)(USART1->DR & 0xFF);
        jm_stm32_uart_push_byte(byte);
    }
}

int main(void)
{
    SystemInit();
    // Configure system clock (register access)
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));
    // ... PLL config ...

    SysTick_Config(SystemCoreClock / 1000);  // 1ms tick

    // UART1 init (for ESP8266 communication)
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN | RCC_APB2ENR_IOPAEN;
    // PA9=TX, PA10=RX
    GPIOA->CRH = (GPIOA->CRH & ~((0xF << 4) | (0xF << 8))) | (0xB << 4) | (0x4 << 8);
    USART1->BRR = SystemCoreClock / 115200;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;
    NVIC_EnableIRQ(USART1_IRQn);

    jm_config_t config = {
        .get_sys_time_ms = get_sys_time,
        .uart_send      = uart_send,
        .uart_send_log  = uart_send_log,
        .event_cb       = on_event,
        .user_data      = NULL,
    };

    int ret = jm_stm32_init(&config);
    if (ret != JM_SUCCESS) {
        while (1);
    }

    while (1) {
        jm_stm32_loop();
        // Your application code
    }
}
```

### Initialization (HAL Library Mode — Optional)

To use HAL mode, define `USE_HAL_UART` in build flags:

```c
#include "jm_stm32.h"
#include "stm32f1xx_hal.h"

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

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
        jm_serial_read(&huart1);  // Polling UART read
        jm_stm32_loop();
        // Your application code
    }
}
```

### 轮询读接口 (HAL Only)

HAL 模式下可使用轮询接口读取 UART 数据：

```c
int ret = jm_serial_read(&huart1);
if (ret == 0) {
    // Data was processed
}
```

### UART RX ISR (Register-direct Mode)

Register-direct mode uses interrupt-driven UART RX. The ISR directly calls `jm_stm32_uart_push_byte()`:

```c
void USART1_IRQHandler(void)
{
    if (USART1->SR & USART_SR_RXNE) {
        uint8_t byte = (uint8_t)(USART1->DR & 0xFF);
        jm_stm32_uart_push_byte(byte);
    }
}
```

## Sending Commands

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
- Default mode: register-direct (CMSIS), no HAL dependency
- Optional: HAL library mode (define `USE_HAL_UART`)

## License

MIT
