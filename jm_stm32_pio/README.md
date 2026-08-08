# jm_stm32_pio 透明串口联网集成说明书

## 1. 项目概述

`jm_stm32_pio` 是一个基于 PlatformIO 的 STM32 项目，实现了通过 UART 串口与 ESP8266 模块通信，将 ESP8266 的联网能力透明地暴露给 STM32 主控。用户无需在 STM32 端实现复杂的网络协议栈，只需通过串口发送指令即可完成 WiFi 配置、TCP/UDP 通信、设备登录等联网操作。

ESP8266 端运行 `jm_client_esp8266` 的 netproxy 功能，负责解析串口指令、管理网络连接，并将网络数据透传回 STM32。

## 2. 系统架构

```
+----------------+         UART          +----------------+       WiFi/Internet       +----------------+
|   STM32 主控   | <-------------------> |   ESP8266      | <---------------------> |   远程服务器    |
|  (本工程)       |   115200 8N1         |  (netproxy)    |   TCP/UDP/HTTP          |   (云平台)      |
|                |                      |                |                         |                |
| jm_stm32 库    |                      | jm_client      |                         |                |
| + 用户业务代码 |                      | _esp8266       |                         |                |
+----------------+                      +----------------+                         +----------------+
```

- **STM32 端**：负责业务逻辑，通过 UART 与 ESP8266 交互
- **ESP8266 端**：负责 WiFi 连接、网络代理、数据转发
- **通信方式**：UART 串口，波特率 115200，8 数据位，无校验，1 停止位

## 3. 硬件接线

### 3.1 STM32 与 ESP8266 串口连接

| STM32 引脚 | ESP8266 引脚 | 说明 |
|-----------|-------------|------|
| PA9 / PA10 (USART1) | TXD / RXD | 主串口通信 |
| GND | GND | 共地 |
| 3.3V | 3.3V (或 VCC) | 供电（如果 ESP8266 独立供电则不需要） |

> **注意**：
> 1. ESP8266 的 IO 电平为 3.3V，STM32 的 IO 电平也为 3.3V，可直接相连。
> 2. 如果 ESP8266 需要从 STM32 取电，请确认 STM32 板子 3.3V 输出电流足够（建议大于 300mA）。
> 3. 如果使用其他串口（如 USART2/3），需要修改代码中的 UART 初始化及收发函数。

### 3.2 STM32 烧录接口

| ST-LINK 引脚 | STM32 引脚 |
|-------------|-----------|
| SWDIO | PA13 |
| SWCLK | PA14 |
| GND | GND |
| VCC | 3.3V（可选） |

## 4. 环境配置

### 4.1 PlatformIO 配置

项目已包含 `platformio.ini`，主要配置：

- **平台**：`ststm32`
- **框架**：`cmsis`（寄存器直驱模式，无需 HAL 库）
- **烧录协议**：`stlink`
- **监视器波特率**：115200
- **本地库路径**：`../jm_stm32`（上游 jm_stm32 库）

支持以下开发板（通过不同的 `env` 切换）：

| 环境 | 板子 | 芯片 |
|------|------|------|
| `stm32f1_blackpill` | Blue Pill C8 | STM32F103C8T6 |
| `stm32f1_bluepill` | Blue Pill C6 | STM32F103C6 |
| `stm32f103_nucleo64` | Nucleo64 | STM32F103RB |
| `stm32f411_nucleo64` | Nucleo64 | STM32F411RE |

### 4.2 编译模式选择

项目默认使用**寄存器直驱模式**（CMSIS），无需 HAL 库，体积小、速度快。

如需使用 HAL 库模式，修改 `platformio.ini`：

```ini
[env:stm32f1_blackpill]
board = bluepill_f103c8
framework = stm32cube
build_flags =
    -DUSE_HAL_UART
    -DSTM32F103xB
```

并在 `src/main.c` 中确保 HAL 初始化代码正确（已有条件编译支持）。

## 5. 代码修改与集成

### 5.1 核心文件

用户主要需要修改的文件是 `src/main.c`。该文件实现了三个核心回调和一个主循环：

#### 5.1.1 系统时间函数

```c
static uint32_t get_sys_time(void)
```

返回毫秒级系统时间，用于协议超时、心跳间隔等计时。

- **寄存器直驱模式（默认）**：基于 `SysTick_Handler` 累加 `sys_tick_ms`
- **HAL 模式（可选）**：使用 `HAL_GetTick()`

用户如果需要更高精度或不同时间源，可修改此函数。

#### 5.1.2 UART 发送函数

```c
static void uart_send(const uint8_t *data, uint16_t len)
```

将数据通过 UART 发送给 ESP8266。

- **寄存器直驱模式（默认）**：轮询 `USART1->SR` 的 TXE 标志
- **HAL 模式（可选）**：使用 `HAL_UART_Transmit()`

如果用户更换了 UART 端口或使用了 DMA，需要修改此函数。

#### 5.1.3 事件回调函数

```c
static void on_event(uint8_t event_type, uint16_t sub_type, void *data, void *user_data)
```

这是用户业务逻辑的核心入口。ESP8266 通过网络事件或串口指令触发的事件会通过此回调通知 STM32 应用。

#### 5.1.4 主循环

**寄存器直驱模式（默认）**：UART 接收通过中断完成，`main` 循环中无需轮询读取：

```c
int main(void)
{
    SystemClock_Config();          // 寄存器直驱时钟配置
    SysTick_Config(72000);         // 1ms 系统时钟
    uart_init();                   // USART1 初始化（使能 RX 中断）

    jm_config_t cfg = {
        .get_sys_time_ms = get_sys_time,
        .uart_send      = uart_send,
        .uart_send_log  = uart_send_log,
        .event_cb       = on_event,
        .user_data      = NULL,
    };
    jm_stm32_init(&cfg);

    while (1) {
        jm_stm32_loop();          // 处理协议状态机（超时、事件派发）
        // 用户业务代码 ...
    }
}
```

UART 接收中断处理函数：

```c
void USART1_IRQHandler(void)
{
    if (USART1->SR & USART_SR_RXNE) {
        uint8_t byte = (uint8_t)(USART1->DR & 0xFF);
        jm_stm32_uart_push_byte(byte);
    }
}
```

- `UART 中断`：通过 `USART1_IRQHandler` 将收到的字节推给 `jm_stm32_uart_push_byte()`
- `jm_stm32_loop()`：驱动协议状态机，处理超时、事件派发等
- 用户业务代码放在主循环中，或使用定时器/中断

**HAL 模式（可选）**：UART 接收通过 `jm_serial_read()` 轮询读取：

```c
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART1_UART_Init();

    jm_config_t cfg = {
        .get_sys_time_ms = get_sys_time,
        .uart_send      = uart_send,
        .event_cb       = on_event,
        .user_data      = NULL,
    };
    jm_stm32_init(&cfg);

    while (1) {
        jm_serial_read(&huart1);  // 轮询读取 UART 数据并喂给 jm_stm32 库
        jm_stm32_loop();          // 处理协议状态机
        // 用户业务代码 ...
    }
}
```

- `jm_serial_read()`：从 UART 读取字节并交给协议解析（仅 HAL 模式可用）
- `jm_stm32_loop()`：驱动协议状态机，处理超时、事件派发等

### 5.2 集成到用户项目

用户将本工程集成到自己的项目时，步骤如下：

**步骤 1：复制核心文件**
- `src/main.c` — 主程序框架
- `platformio.ini` — 项目配置模板
- `lib/jm_stm32` 或 `lib_extra_dirs` 引用的 jm_stm32 库
- `../jm_stm32/jm_stm32_boards/board001/jm_pcfg.h` — 库配置文件（如需自定义）

**步骤 2：修改 `jm_config_t` 配置**
```c
jm_config_t cfg = {
    .get_sys_time_ms = get_sys_time,  // 必须实现
    .uart_send      = uart_send,      // 必须实现
    .uart_send_log  = uart_send_log,   // 日志输出（寄存器直驱模式下使用 USART2）
    .event_cb       = on_event,       // 用户实现业务逻辑
    .user_data      = NULL,           // 用户自定义数据
};
jm_stm32_init(&cfg);
```

**步骤 3：实现业务事件处理**
在 `on_event` 中处理以下事件类型：

| 事件类型 | 宏定义 | 说明 | data 指向类型 |
|---------|--------|------|-------------|
| 1 | JM_EVENT_WIFI_STATUS | WiFi 状态变化 | `jm_wifi_status_t *` |
| 2 | JM_EVENT_INTERNET_STATUS | 互联网可用性 | `jm_wifi_status_t *` |
| 3 | JM_EVENT_LOGIN_RESULT | 设备登录结果 | `jm_login_result_t *` |
| 4 | JM_EVENT_TCP_CONNECTED | TCP 连接建立 | `jm_tcp_conn_info_t *` |
| 5 | JM_EVENT_TCP_DISCONNECTED | TCP 连接断开 | `jm_tcp_conn_info_t *` |
| 6 | JM_EVENT_TCP_SEND_RESULT | TCP 发送结果 | `jm_tcp_conn_info_t *` |
| 7 | JM_EVENT_TCP_ERROR | TCP 错误 | `jm_tcp_conn_info_t *` |
| 8 | JM_EVENT_UID_RESPONSE | 设备 ID 响应 | `jm_uid_response_t *` |
| 9 | JM_EVENT_SYS_CFG | 系统配置 | `jm_sys_cfg_t *` |
| 10 | JM_EVENT_TRANS_CMD | 透传命令 | `jm_emap_t *` |
| 11 | JM_EVENT_CTRL_EVENT | 控制事件 | `jm_emap_t *` |
| 12 | JM_EVENT_TCP_DATA | TCP 接收数据 | `jm_buf_t *` |
| 13 | JM_EVENT_UDP_DATA | UDP 接收数据 | `jm_buf_t *` |

**步骤 4：调用主动接口函数**
用户可主动发起以下操作：

```c
// 设置设备 ID
jm_stm32_send_uid(uint32_t uid, uint16_t board_type, const char *device_type_name);

// WiFi 配置
jm_stm32_send_wifi_cfg(const char *ssid, const char *pwd);

// 查询 WiFi 状态
jm_stm32_send_wifi_status_req(void);

// 查询互联网状态
jm_stm32_send_internet_status_req(void);

// 发起登录
jm_stm32_send_login(void);

// TCP 连接
jm_stm32_send_tcp_connect(const char *host, uint16_t port);

// 关闭 TCP
jm_stm32_send_tcp_close(int8_t sock);

// TCP 发送数据
jm_stm32_send_tcp_data(int8_t sock, const uint8_t *data, uint16_t len);

// 语音播放（如使用 ML 功能）
jm_stm32_send_audio_play(const char *text);

// 发送控制事件
jm_stm32_send_ctrl_event(const uint8_t *data, uint16_t len);

// 透传命令响应
jm_stm32_send_trans_cmd_response(uint16_t req_id, const uint8_t *data, uint16_t len);
```

## 6. 串口通信协议

### 6.1 数据包格式（通用）

所有通过 UART 传输的数据包都遵循以下格式：

```
+----------------+----------------+----------------+----------------+------------------+
|  Byte 0        |  Byte 1        |  Byte 2        |  Byte 3        |  Byte 4...       |
+----------------+----------------+----------------+----------------+------------------+
|  Length High   |  Length Low    |  Check Num     |  Type          |  Data...         |
+----------------+----------------+----------------+----------------+------------------+
```

| 字段 | 长度 | 说明 |
|------|------|------|
| Length | 2 字节 | 整个数据包的总长度（包括 Length 字段本身），大端序 |
| Check Num | 1 字节 | 固定值 `0x55`，用于校验包合法性 |
| Type | 1 字节 | 数据包类型，见下表 |
| Data | 变长 | 实际业务数据 |

### 6.2 数据包类型

| Type 值 | 宏定义 | 方向 | 说明 |
|---------|--------|------|------|
| 1 | JM_SERIALNET_TYPE_UDP | ESP→STM32 | JM 平台 UDP 代理数据 |
| 2 | JM_SERIALNET_TYPE_TCP | ESP→STM32 | JM 平台 TCP 代理数据 |
| 3 | JM_SERIALNET_TYPE_SERIAL | 双向 | 串口配置/控制命令 |
| 4 | JM_SERIALNET_TYPE_UDP_COM | ESP→STM32 | 非 JM 平台 UDP 透传 |
| 5 | JM_SERIALNET_TYPE_SYS | 双向 | 系统底层命令 |

### 6.3 长包分片

当数据长度超过 `JM_MAX_SERIAL_BLOCK_SIZE` (1024 字节) 时，会拆分为多个包传输：

- 第一个包：包含 Length、Check Num、Type 和部分数据
- 后续包：仅包含数据部分，无额外头信息
- 接收方根据 Length 字段组装完整数据包
- 包之间超时间隔为 500ms

### 6.4 TCP 数据包格式（ESP → STM32）

TCP 服务器下发的数据包格式：

```
+----------------+----------------+----------------+----------------+----------------+----------------+------------------+
|  Len High      |  Len Low       |  Check(0x55)   |  Type(2)       |  Host Len      |  Host String    |  Port            |  Data...         |
+----------------+----------------+----------------+----------------+----------------+----------------+------------------+
```

- Host：目标主机字符串，以 `\0` 结尾
- Port：2 字节，大端序
- Data：TCP 实际数据

### 6.5 串口控制命令（ESP → STM32，Type=3）

所有控制命令的基本格式：

```
+----------------+----------------+----------------+----------------+----------------+
|  Len High      |  Len Low       |  Check(0x55)   |  Type(3)       |  Subtype       |  ReqId High    |  ReqId Low      |  Data...         |
+----------------+----------------+----------------+----------------+----------------+----------------+------------------+
```

| Subtype | 宏定义 | 说明 |
|---------|--------|------|
| 4 | JM_TASK_APP_PROXY_TCP_CONN | 请求 TCP 连接（Host + Port） |
| 14 | JM_TASK_APP_PROXY_TCP_CLOSE | 关闭 TCP 连接（Sock） |
| 6 | JM_TASK_APP_PROXY_WIFI_CFG | WiFi 配置（SSID + PWD） |
| 7 | JM_TASK_APP_PROXY_WIFI_CONNECTED | 查询 WiFi 状态 |
| 10 | JM_TASK_APP_PROXY_INTERNET_ENABLE | 查询互联网 |
| 12 | JM_TASK_APP_PROXY_WIFI_IS_ENABLE | 查询 WiFi 是否启用 |
| 13 | JM_TASK_APP_PROXY_HB | 心跳/状态 |
| 15 | JM_TASK_APP_PROXY_UID | 设置设备 ID |
| 17 | JM_TASK_APP_PROXY_LOGIN_RESULT | 登录结果 |
| 19 | JM_TASK_APP_PROXY_LOGIN | 请求登录 |
| 20 | JM_TASK_APP_PROXY_SYS_CFG | 系统配置查询 |
| 21 | JM_TASK_APP_PROXY_TRANS_CMD | 透传控制命令 |
| 22 | JM_TASK_APP_PROXY_AUDIO_PLAY | 语音播放 |
| 27 | JM_TASK_APP_PROXY_CTRL_EVENT | 控制事件 |

## 7. 数据传输流程

### 7.1 TCP 通信流程

```
STM32                           ESP8266                       服务器
  |                               |                               |
  |-- TCP_CONN(Host, Port) ------>|                               |
  |                               |-- TCP Connect --------------->|
  |                               |<------ ACK -------------------|
  |<-- TCP_CONNECTED(Host,Port,sock)-|                             |
  |                               |                               |
  |-- TCP Data ------------------>|------------------------------>|
  |<-- TCP Data ------------------|------------------------------<|
  |                               |                               |
  |-- TCP_CLOSE(sock) ------------>|-- TCP Disconnect ----------->|
  |<-- TCP_DISCONNECTED -----------|                               |
```

### 7.2 数据透传流程

STM32 发送的非控制类数据，通过以下流程转发到网络：

1. STM32 调用 `jm_stm32_send_tcp_data()` 发送业务数据
2. STM32 协议栈将数据封装为串口包（Type=3, Subtype 由协议内部处理）
3. ESP8266 收到串口包，解析出 TCP 数据
4. ESP8266 通过已建立的 TCP 连接发送到远程服务器
5. 远程服务器数据返回时，ESP8266 封装为 Type=2 的数据包发回 STM32
6. STM32 协议栈解析后，通过 `JM_EVENT_TCP_DATA` 事件通知应用

### 7.3 WiFi 配置流程

```
STM32                           ESP8266
  |-- WIFI_CFG(ssid, pwd) ------>|
  |<-- WIFI_CFG_ACK -------------|
  |                               |
  |     (ESP8266 连接 WiFi)       |
  |<-- WIFI_CONNECTED(ip,mac) ---|
```

## 8. 日志查看与调试

### 8.1 日志输出

STM32 端提供独立的日志输出功能，通过 USART2（仅 TX）输出，波特率 115200，与业务串口 USART1 物理隔离。

#### 8.1.1 开启日志

日志功能通过 `jm_pcfg.h` 宏控制：

```c
#define JM_LOG_DEBUG_ENABLE 1   // 调试日志
#define JM_LOG_ERROR_ENABLE 1   // 错误日志
```

在 PlatformIO 项目中，该配置文件位于 `../jm_stm32/jm_stm32_boards/board001/jm_pcfg.h`。

寄存器直驱模式下，日志通过 USART2（PA2，仅 TX）输出，波特率 115200，与业务串口 USART1 物理隔离。

#### 8.1.2 日志宏

```c
#include "jm_stm32.h"

// 带换行的日志
JM_LOG_LINE("hello %d", 123);

// 不带换行的日志
JM_LOG("no newline");

// 输出单个字符
JM_LOG('c');
```

#### 8.1.3 日志接线

| STM32 引脚 | 说明 |
|-----------|------|
| PA2 (USART2 TX) | 日志输出，连接 USB-TTL 模块的 RXD |
| GND | 共地 |

### 8.2 编译输出日志

使用 `jm.bat` 脚本编译时，输出信息包括：

- 编译器版本
- 板子信息（芯片、RAM、Flash）
- 库依赖列表
- 编译模式（Release/Debug）
- 固件大小

### 8.3 运行时串口日志

日志输出通过 USART2（PA2），需使用 USB-TTL 模块连接 STM32 的 PA2 引脚来查看：

| STM32 引脚 | 说明 |
|-----------|------|
| PA2 (USART2 TX) | 日志输出，连接 USB-TTL 模块的 RXD |
| GND | 共地 |

日志内容包括事件回调、初始化状态、错误信息等，由 `JM_LOG_DEBUG_ENABLE` 和 `JM_LOG_ERROR_ENABLE` 宏控制。

### 8.4 协议调试

如需调试原始协议包，可在 `on_event` 中打印 `event_type` 和 `sub_type`：

```c
static void on_event(uint8_t event_type, uint16_t sub_type, void *data, void *user_data)
{
    JM_LOG_LINE("EVENT: type=%d, subtype=%d", event_type, sub_type);
    switch (event_type) {
        // ...
    }
}
```

## 9. ESP8266 端要求

### 9.1 必备宏定义

ESP8266 端必须启用以下宏：

```c
#define JM_NETPROXY 1
#define JM_TCP_PROXY_ENABLE 1
#define JM_UDP_PROXY_ENABLE 1
#define JM_SERIAL_ENABLE 1
```

### 9.2 UART 接口实现

ESP8266 端需实现以下 UART 底层函数：

```c
// 发送单个字符
void uart_tx_one_char(uint8_t uart_no, uint8_t ch);

// 获取接收缓冲区数据
void jm_uart_getRXBuffer(uint8_t *buf, uint16_t len);

// 清空接收缓冲区
void jm_uart_clearRecvBuf(void);
```

### 9.3 网络初始化

ESP8266 端需在 WiFi 连接成功后初始化 proxy：

```c
jm_netproxy_init();
```

proxy 会自动注册事件监听器，处理串口指令和网络数据转发。

## 10. 注意事项

1. **波特率一致性**：STM32 和 ESP8266 的 UART 波特率必须一致（默认 115200）。
2. **共地**：两模块的 GND 必须连接，否则通信会异常。
3. **供电电流**：ESP8266 在发射时峰值电流可达 200mA+，确保供电充足。
4. **包长度限制**：单包最大 1024 字节，超过会自动分片。
5. **超时机制**：包组装超时 500ms，超过会丢弃未完成的包。
6. **内存限制**：STM32 端内存有限，避免在回调中分配大内存。
7. **事件线程安全**：`on_event` 在主循环中被调用，注意不要阻塞太久。

## 11. 常见问题

**Q: 串口通信失败？**
A: 检查 TX/RX 是否接反，确认波特率一致，确认共地。

**Q: 收不到网络数据？**
A: 确认 ESP8266 已连接 WiFi，检查 ESP8266 端 `jm_netproxy_init()` 是否被调用。

**Q: TCP 连接失败？**
A: 检查 `JM_EVENT_TCP_CONNECTED` 事件中的 `err_code`，确认目标服务器 IP/端口可达。

**Q: 数据分包接收？**
A: 这是正常的。TCP 数据可能分多个包到达，用户需要根据应用层协议自行组包。

**Q: 如何更换 UART 端口？**
A: 修改 `src/main.c` 中的 `uart_init()`（寄存器直驱模式）或 `MX_USARTx_UART_Init()`（HAL 模式），并同步修改 `uart_send()`、`uart_send_log()` 以及 `USART1_IRQHandler` 的端口映射。

## 12. 相关文件

| 文件 | 说明 |
|------|------|
| `src/main.c` | STM32 主程序，实现 UART 初始化、事件处理、主循环 |
| `platformio.ini` | PlatformIO 项目配置 |
| `jm.bat` | 构建/烧录/监视快捷脚本 |
| `jm_manual.md` | jm.bat 使用手册 |
| `../jm_stm32/` | jm_stm32 库源码（协议栈实现） |
| `../jm_stm32/jm_stm32_boards/board001/jm_pcfg.h` | 库配置文件（日志、事件、测试模块宏开关） |
| `E:\arduinoLib\Arduino\libraries\jm_client_esp8266\src\netproxy\` | ESP8266 netproxy 源码 |
