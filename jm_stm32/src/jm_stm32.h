/**
 * @file jm_stm32.h
 * @brief STM32 串口透传客户端 — 通过 ESP8266 netproxy 实现 TCP/UDP/MQTT 网络连接
 *
 * @mainpage jm_stm32 库
 *
 * jm_stm32 是一个轻量级 STM32 串口代理客户端，通过 UART 与 ESP8266(netproxy)
 * 通信，实现 STM32 与云服务器之间的透传。
 *
 * - 默认使用寄存器直驱（CMSIS）模式，无需 HAL 库
 * - 可选 HAL 库模式（定义 `USE_HAL_UART` 启用）
 * - 适用于 STM32F103C8T6 等 64KB Flash 芯片
 *
 * @section 初始化
 * 1. 实现 `get_sys_time_ms`, `uart_send`, `uart_send_log` 回调
 * 2. 实现 `on_event` 事件处理回调
 * 3. 调用 `jm_stm32_init()` 初始化
 * 4. 在主循环中调用 `jm_stm32_loop()`
 * 5. 寄存器直驱模式下，通过 `USARTx_IRQHandler` + `jm_stm32_uart_push_byte()` 接收数据
 *    HAL 模式下，可选使用 `jm_serial_read()` 轮询接收
 *
 * @copyright MIT
 */

#ifndef JM_STM32_H_
#define JM_STM32_H_

#include "jm_pcfg.h"
#include "jm_sensor.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 库版本字符串 */
#define JM_STM32_VERSION "1.0.0"

/* ===================== 串口协议常量 ===================== */

/** @brief 数据包头字节，用于同步包边界 */
#define PCK_HEANDER 0xAA

/** @brief 数据包校验字节，固定为 0x55 */
#define JM_SDADA_CHECK_NUM       0x55

/** @brief 串口数据包头部大小（Length 高字节 + Length 低字节 + Check Num） */
#define JM_SDATA_HEADER_SIZE     3

/** @brief 单个串口数据块最大大小（超过时自动分片） */
#define JM_MAX_SERIAL_BLOCK_SIZE 1024

/** @brief 串口数据包最大总大小 */
#define JM_SERIAL_MAX_PCK_SIZE   4096

/**
 * @brief 串口数据包类型
 * @note ESP8266 netproxy 与 STM32 之间的透传数据包类型标识
 */
#define JM_SERIALNET_TYPE_UDP      1  /**< ESP→STM32：JM 平台 UDP 代理数据 */
#define JM_SERIALNET_TYPE_TCP      2  /**< ESP→STM32：JM 平台 TCP 代理数据 */
#define JM_SERIALNET_TYPE_SERIAL   3  /**< 双向：串口配置/控制命令 */
#define JM_SERIALNET_TYPE_UDP_COM  4  /**< ESP→STM32：非 JM 平台 UDP 透传 */
#define JM_SERIALNET_TYPE_SYS      5  /**< 双向：系统底层命令 */
#define JM_SERIALNET_TYPE_MQTT     6  /**< MQTT 代理数据 */
#define JM_SERIALNET_TYPE_HTTP     7  /**< HTTP 代理数据 */

/* ===================== 控制命令子类型 ===================== */

/** @brief TCP 代理事件子类型：连接成功 */
#define JM_TASK_APP_PROXY_TCP_CONNECTED (1)
/** @brief TCP 代理事件子类型：连接断开 */
#define JM_TASK_APP_PROXY_TCP_DISCONNECTED (2)
/** @brief TCP 发送数据结果 */
#define JM_TASK_APP_PROXY_TCP_SEND 3
/** @brief TCP 连接结果 */
#define JM_TASK_APP_PROXY_TCP_CONN 4
/** @brief TCP 错误 */
#define JM_TASK_APP_PROXY_TCP_ERR 5

/** @brief WiFi 配置（SSID + PWD） */
#define JM_TASK_APP_PROXY_WIFI_CFG         6
/** @brief 查询 WiFi 状态 */
#define JM_TASK_APP_PROXY_WIFI_CONNECTED   7
/** @brief 查询互联网可用性 */
#define JM_TASK_APP_PROXY_INTERNET_ENABLE  10
/** @brief 查询 WiFi 是否启用 */
#define JM_TASK_APP_PROXY_WIFI_IS_ENABLE   12
/** @brief 心跳/状态上报 */
#define JM_TASK_APP_PROXY_HB               13
/** @brief 关闭 TCP 连接 */
#define JM_TASK_APP_PROXY_TCP_CLOSE        14
/** @brief 设置设备 ID */
#define JM_TASK_APP_PROXY_UID              15
/** @brief 登录结果 */
#define JM_TASK_APP_PROXY_LOGIN_RESULT     17
/** @brief 请求登录 */
#define JM_TASK_APP_PROXY_LOGIN            19
/** @brief 系统配置查询 */
#define JM_TASK_APP_PROXY_SYS_CFG          20
/** @brief 透传控制命令 */
#define JM_TASK_APP_PROXY_TRANS_CMD        21
/** @brief 语音播放 */
#define JM_TASK_APP_PROXY_AUDIO_PLAY       22
/** @brief 控制事件 */
#define JM_TASK_APP_PROXY_CTRL_EVENT       27
/** @brief 控制命令 */
#define JM_TASK_APP_PROXY_CTRL_CMD         29
/** @brief 控制重置响应 */
#define JM_TASK_APP_PROXY_CTRL_RST         30

/** @brief 下发事件命令到主机或上行事件到网卡 */
#define JM_TASK_APP_PROXY_NETCARD_EVENT 31
/** @brief 引脚中断事件 */
#define JM_TASK_APP_PROXY_NETCARD_INTERRUPT (32)

/* ===================== 错误码 ===================== */

/** @brief 操作成功 */
#define JM_SUCCESS                  0
/** @brief 内存分配失败 */
#define JM_ERR_MEMORY              -1
/** @brief 无效数据包 */
#define JM_ERR_INVALID_PACKET      -2
/** @brief 校验失败 */
#define JM_ERR_CHECKSUM            -3
/** @brief 操作超时 */
#define JM_ERR_TIMEOUT             -4
/** @brief 未就绪 */
#define JM_ERR_NOT_READY           -5

/* ===================== 事件类型 ===================== */

/** @brief WiFi 状态变化事件 */
#define JM_EVENT_WIFI_STATUS       1
/** @brief 互联网可用性事件 */
#define JM_EVENT_INTERNET_STATUS   2
/** @brief 设备登录结果事件 */
#define JM_EVENT_LOGIN_RESULT      3
/** @brief TCP 连接建立事件 */
#define JM_EVENT_TCP_CONNECTED     4
/** @brief TCP 连接断开事件 */
#define JM_EVENT_TCP_DISCONNECTED  5
/** @brief TCP 发送结果事件 */
#define JM_EVENT_TCP_SEND_RESULT   6
/** @brief TCP 错误事件 */
#define JM_EVENT_TCP_ERROR         7
/** @brief 设备 ID 响应事件 */
#define JM_EVENT_UID_RESPONSE      8
/** @brief 系统配置事件 */
#define JM_EVENT_SYS_CFG           9
/** @brief 透传命令事件 */
#define JM_EVENT_TRANS_CMD         10
/** @brief 控制事件 */
#define JM_EVENT_CTRL_EVENT        11
/** @brief 控制命令事件 */
#define JM_EVENT_CTRL_CMD          14
/** @brief TCP 接收数据事件 */
#define JM_EVENT_TCP_DATA          12
/** @brief UDP 接收数据事件 */
#define JM_EVENT_UDP_DATA          13
/** @brief HTTP 响应事件 */
#define JM_EVENT_HTTP_RESPONSE     14
/** @brief HTTP 错误事件 */
#define JM_EVENT_HTTP_ERROR        15

/**
 * @brief 序列化前缀类型标识
 * @note 用于 emap 序列化/反序列化时标识字段数据类型
 */
#define PREFIX_TYPE_NULL            -128  /**< 空值类型 */
#define PREFIX_TYPE_MAP             -122  /**< map（键值对）类型 */
#define PREFIX_TYPE_BYTE            -121  /**< 字节（int8）类型 */
#define PREFIX_TYPE_SHORTT_TYPE     -120  /**< 短整型（int16）类型 */
#define PREFIX_TYPE_INT             -119  /**< 整型（int32）类型 */
#define PREFIX_TYPE_LONG            -118  /**< 长整型类型 */
#define PREFIX_TYPE_STRINGG         -113  /**< 字符串类型 */

/* ===================== 日志接口 ===================== */

/**
 * @brief 输出单个字符到日志串口
 * @param ch 字符
 */
void jm_log_char(char ch);

/**
 * @brief 格式化输出日志到日志串口
 * @param format 格式化字符串
 * @param ... 可变参数
 */
void jm_log_print(const char *format, ...);

#include <stdarg.h>

/** @brief 输出格式化日志（不自动换行） */
#define JM_LOG(format, ...) \
    do { jm_log_print((const char*)format, ## __VA_ARGS__);} while(0)

/** @brief 输出格式化日志（自动换行） */
#define JM_LOG_LINE(format, ...) \
    do { jm_log_print((const char*)format, ## __VA_ARGS__); jm_log_char('\n'); } while(0)

/** @brief 调试日志（由 @ref JM_LOG_DEBUG_ENABLE 宏控制） */
#if JM_LOG_DEBUG_ENABLE
#define JM_LOG_D(format, ...) JM_LOG_LINE(format, ## __VA_ARGS__);
#else
#define JM_LOG_D(format, ...)
#endif

/** @brief 错误日志（由 @ref JM_LOG_ERROR_ENABLE 宏控制） */
#if JM_LOG_ERROR_ENABLE
#define JM_LOG_E(format, ...) JM_LOG_LINE(format, ## __VA_ARGS__);
#else
#define JM_LOG_E(format, ...)
#endif


#include "jm_stm32_buf.h"

/* ===================== 数据结构 ===================== */

/**
 * @brief TCP 连接信息
 *
 * 包含 TCP 连接的套接字、目标主机、端口以及错误信息。
 * 当 TCP 连接事件（@ref JM_EVENT_TCP_CONNECTED 等）触发时，data 参数指向本结构体。
 */
typedef struct {
    int8_t sock;          /**< 套接字描述符（0~N） */
    char host[64];        /**< 服务器主机名或 IP 地址（以 \0 结尾） */
    uint16_t port;        /**< 服务器端口 */
    int8_t err_code;      /**< 错误码（0 表示无错误） */
    char err_msg[64];     /**< 错误描述字符串 */
} jm_tcp_conn_info_t;

/**
 * @brief WiFi 连接状态
 *
 * 当 @ref JM_EVENT_WIFI_STATUS 或 @ref JM_EVENT_INTERNET_STATUS 事件触发时，
 * data 参数指向本结构体。
 */
typedef struct {
    uint32_t devId;          /**< 设备 ID */
    bool wifi_enabled;       /**< WiFi 是否可用/已启用 */
    bool isLogin;            /**< 是否已登录 JM 平台 */
} jm_wifi_status_t;

/**
 * @brief 登录结果
 *
 * 当 @ref JM_EVENT_LOGIN_RESULT 事件触发时，data 参数指向本结构体。
 */
typedef struct {
    int32_t login_code;     /**< 登录结果码（0 表示成功） */
    uint32_t dev_uid;        /**< 设备唯一 ID */
    int32_t act_id;          /**< 活动 ID */
    int32_t client_id;       /**< 客户端 ID */
    int8_t grp_id;           /**< 分组 ID */
    char login_key[64];      /**< 登录密钥 */
} jm_login_result_t;

/**
 * @brief UID 响应结果
 *
 * 当 @ref JM_EVENT_UID_RESPONSE 事件触发时，data 参数指向本结构体。
 */
typedef struct {
    int8_t result_code;     /**< 响应结果码 */
} jm_uid_response_t;

/**
 * @brief 系统配置
 *
 * 当 @ref JM_EVENT_SYS_CFG 事件触发时，data 参数指向本结构体。
 */
typedef struct {
    uint8_t data[128];      /**< 原始配置数据 */
    uint16_t len;           /**< 数据有效长度 */
} jm_sys_cfg_t;

/**
 * @brief 事件回调函数类型
 *
 * 用户在 @ref jm_config_t 中注册此回调，用于处理来自 ESP8266 的各种事件。
 *
 * @param event_type 事件类型，见 @ref JM_EVENT_WIFI_STATUS 等
 * @param sub_type   子类型，对应 @ref JM_TASK_APP_PROXY_* 常量
 * @param data       事件数据指针（类型因事件而异）
 * @param user_data  用户自定义数据（来自 @ref jm_config_t::user_data）
 */
typedef void (*jm_event_callback_t)(uint8_t event_type,
                                    uint16_t sub_type,
                                    void *data,
                                    void *user_data);

/**
 * @brief jm_stm32 库初始化配置结构
 *
 * 用户在调用 @ref jm_stm32_init 前填充此结构，并提供必要的回调函数。
 */
typedef struct {
    uint32_t (*get_sys_time_ms)(void);                       /**< 获取系统毫秒时间，必须实现 */
    void (*uart_send)(const uint8_t *data, uint16_t len);     /**< 通过 UART 发送数据到 ESP8266，必须实现 */
    void (*uart_send_log)(const uint8_t *data, uint16_t len); /**< 通过日志 UART 发送数据，可选 */
    jm_event_callback_t event_cb;                              /**< 事件回调函数 */
    void *user_data;                                           /**< 用户自定义数据，原样传递给 event_cb */
} jm_config_t;

/* ===================== emap（键值对容器） ===================== */

/**
 * @brief 键值对链表节点
 *
 * emap 是一个简单的键值对集合，支持整数值和字符串值。
 */
typedef struct jm_emap_node {
    char *key;                    /**< 键名 */
    int32_t ival;                 /**< 整数值（当 is_int 为 true 时有效） */
    char *sval;                   /**< 字符串值（当 is_int 为 false 时有效） */
    bool is_int;                  /**< true 表示整数值，false 表示字符串值 */
    bool copy_key;                /**< 是否需要释放 key 内存 */
    bool copy_val;                /**< 是否需要释放 sval 内存 */
    struct jm_emap_node *next;    /**< 下一个节点 */
} jm_emap_node_t;

/**
 * @brief 键值对容器
 */
typedef struct {
    jm_emap_node_t *head;    /**< 链表头指针 */
    uint8_t type;            /**< 容器类型（保留） */
} jm_emap_t;

/**
 * @brief 控制命令处理函数类型
 *
 * 用户注册的控制命令回调，当 ESP8266 下发控制命令时被调用。
 *
 * @param ps 解析后的键值对参数
 * @return 返回包含响应数据的 emap，调用者需释放
 */
typedef jm_emap_t* (*jm_ctrl_fn_t)(jm_emap_t *ps);

/**
 * @brief 控制命令注册表项
 */
typedef struct {
    int32_t defId;       /**< 命令 ID（对应 funName/_fn 字段） */
    jm_ctrl_fn_t fn;     /**< 命令处理函数 */
} jm_ctrl_item_t;

/* ===================== emap API ===================== */

/**
 * @brief 创建一个新的 emap 容器
 * @param type 容器类型（保留，传 0 即可）
 * @return emap 指针，失败返回 NULL
 */
jm_emap_t *jm_emap_create(uint8_t type);

/**
 * @brief 释放 emap 容器及其所有节点
 * @param map 待释放的 emap 指针
 */
void jm_emap_release(jm_emap_t *map);

/**
 * @brief 添加整数键值对
 * @param map     emap 容器
 * @param key     键名
 * @param val     整数值
 * @param copyKey 是否复制 key 字符串（true 则内部 strdup，false 则直接使用指针）
 * @return true 成功，false 失败
 */
bool jm_emap_putInt(jm_emap_t *map, const char *key, int32_t val, bool copyKey);

/**
 * @brief 添加字符串键值对
 * @param map        emap 容器
 * @param key        键名
 * @param val        字符串值
 * @param needFreeMem 是否在释放 emap 时释放 val 内存
 * @param copyKey    是否复制 key 字符串
 * @return true 成功，false 失败
 */
bool jm_emap_putStr(jm_emap_t *map, const char *key, const char *val, bool needFreeMem, bool copyKey);

/**
 * @brief 添加字节（int8）键值对
 * @param map     emap 容器
 * @param key     键名
 * @param val     字节值
 * @param copyKey 是否复制 key 字符串
 * @return true 成功，false 失败
 */
bool jm_emap_putByte(jm_emap_t *map, const char *key, int8_t val, bool copyKey);

/**
 * @brief 获取整数值
 * @param map  emap 容器
 * @param key  键名
 * @param def  默认值（键不存在时返回）
 * @return 整数值
 */
int32_t jm_emap_getInt(jm_emap_t *map, const char *key, int32_t def);

/**
 * @brief 获取字节值
 * @param map  emap 容器
 * @param key  键名
 * @param def  默认值
 * @return 字节值
 */
int8_t jm_emap_getByte(jm_emap_t *map, const char *key, int8_t def);

/**
 * @brief 获取字符串值
 * @param map  emap 容器
 * @param key  键名
 * @return 字符串指针（不应手动释放），键不存在返回 NULL
 */
char *jm_emap_getStr(jm_emap_t *map, const char *key);

/**
 * @brief 检查键是否存在
 * @param map  emap 容器
 * @param key  键名
 * @return true 存在，false 不存在
 */
bool jm_emap_exist(jm_emap_t *map, const char *key);

/**
 * @brief 将 emap 序列化到缓冲区
 * @param map  emap 容器
 * @param buf  目标缓冲区
 * @return true 成功，false 失败
 */
bool jm_emap_encode(const jm_emap_t *map, jm_buf_t *buf);

/**
 * @brief 从字节数据反序列化为 emap
 * @param data 序列化后的数据
 * @param len  数据长度
 * @return emap 指针（调用者需释放），失败返回 NULL
 */
jm_emap_t *jm_emap_decode(const uint8_t *data, uint16_t len);

/* ===================== 控制命令 ===================== */

/**
 * @brief 注册控制命令处理函数
 * @param fn    处理函数
 * @param defId 命令 ID
 * @return true 成功，false 失败
 */
bool jm_ctrl_registFun(jm_ctrl_fn_t fn, int32_t defId);

/**
 * @brief 调用控制命令处理函数
 * @param ps 包含 funName/_fn 字段的 emap 参数
 * @return 处理结果 emap，调用者需释放
 */
jm_emap_t *jm_ctrl_invokeFunc(jm_emap_t *ps);

/**
 * @brief 默认控制命令处理（tone、pin 中断、I2C、at24cxx 等）
 * @param ps emap 参数
 * @return 结果 emap，调用者需释放
 */
jm_emap_t *jm_stm32_ctrl_def(jm_emap_t *ps);

/* ===================== 核心 API ===================== */

/**
 * @brief 初始化 jm_stm32 库
 *
 * 必须在使用其他 API 之前调用一次。
 *
 * @param config 初始化配置结构体，见 @ref jm_config_t
 * @return @ref JM_SUCCESS 成功，其他 错误码
 */
int jm_stm32_init(const jm_config_t *config);

/**
 * @brief 协议状态机轮询函数
 *
 * 应在主循环中周期性调用，处理超时、事件派发等。
 */
void jm_stm32_loop(void);

/**
 * @brief 将接收到的 UART 字节推入接收环形缓冲区
 *
 * 寄存器直驱模式下，UART 中断服务函数调用本函数。
 *
 * @param byte 收到的字节
 * @return true 推入成功，false 缓冲区满
 */
bool jm_stm32_uart_push_byte(uint8_t byte);

/**
 * @brief 直接处理一个 UART 字节（不经过环形缓冲区）
 *
 * @param byte 收到的字节
 */
void jm_stm32_uart_rx_byte(uint8_t byte);

/**
 * @brief 获取系统运行时间（毫秒）
 * @return 系统毫秒时间
 */
uint32_t jm_stm32_get_time(void);

/**
 * @brief HAL 模式下的轮询读取 UART 数据
 *
 * @note 仅在定义了 `USE_HAL_UART` 时可用
 * @param huart HAL UART 句柄指针
 * @return 0 成功，-1 失败
 */
int jm_serial_read(void *huart);

/* ===================== 发送命令 API ===================== */

/**
 * @brief 发送设备 UID 到 ESP8266
 * @return @ref JM_SUCCESS 成功，其他 错误码
 */
int jm_stm32_send_uid();

/**
 * @brief 配置 WiFi 凭据（SSID + 密码）
 * @param ssid WiFi 名称
 * @param pwd  WiFi 密码
 * @return @ref JM_SUCCESS 成功，其他 错误码
 */
int jm_stm32_send_wifi_cfg(const char *ssid, const char *pwd);

/**
 * @brief 请求查询 WiFi 状态
 * @return @ref JM_SUCCESS 成功
 */
int jm_stm32_send_wifi_status_req(void);

/**
 * @brief 请求查询互联网连接状态
 * @return @ref JM_SUCCESS 成功
 */
int jm_stm32_send_internet_status_req(void);

/**
 * @brief 发起登录请求到 JM 平台
 * @return @ref JM_SUCCESS 成功
 */
int jm_stm32_send_login(void);

/**
 * @brief 发起 TCP 连接
 * @param host 目标主机名或 IP
 * @param port 目标端口
 * @return @ref JM_SUCCESS 成功
 */
int jm_stm32_send_tcp_connect(const char *host, uint16_t port);

/**
 * @brief 关闭 TCP 连接
 * @param sock 套接字描述符
 * @return @ref JM_SUCCESS 成功
 */
int jm_stm32_send_tcp_close(int8_t sock);

/**
 * @brief 通过已建立的 TCP 连接发送数据
 * @param sock  套接字描述符
 * @param data  数据指针
 * @param len   数据长度
 * @return @ref JM_SUCCESS 成功
 */
int jm_stm32_send_tcp_data(int8_t sock, const uint8_t *data, uint16_t len);

/**
 * @brief 通过 UDP 发送数据
 * @param host  目标主机名或 IP
 * @param port  目标端口
 * @param data  数据指针
 * @param len   数据长度
 * @return @ref JM_SUCCESS 成功
 */
int jm_stm32_send_udp_data(const char *host, uint16_t port, const uint8_t *data, uint16_t len);

/**
 * @brief 发送语音播放文本到 ESP8266
 * @param text 待播放的文本内容
 * @return @ref JM_SUCCESS 成功
 */
int jm_stm32_send_audio_play(const char *text);

/**
 * @brief 发送控制事件到 ESP8266
 * @param data 事件数据
 * @param len  数据长度
 * @return @ref JM_SUCCESS 成功
 */
int jm_stm32_send_ctrl_event(const uint8_t *data, uint16_t len);

/**
 * @brief 回复透传命令
 * @param req_id 请求 ID
 * @param data   响应数据
 * @param len    数据长度
 * @return @ref JM_SUCCESS 成功
 */
int jm_stm32_send_trans_cmd_response(uint16_t req_id, const uint8_t *data, uint16_t len);

/**
 * @brief 发送控制命令响应
 * @param req_id 请求 ID
 * @param rst    响应 emap 容器
 * @return @ref JM_SUCCESS 成功
 */
int jm_stm32_send_ctrl_rst(uint16_t req_id, jm_emap_t *rst);

/**
 * @brief 获取下一个请求 ID（自动递增，0 和 1 保留）
 * @return 请求 ID
 */
uint8_t jm_stm32_next_req_id(void);

/**
 * @brief 通过 uart_send 直接发送原始字节数据
 * @param data 数据指针
 * @param len  数据长度
 * @return @ref JM_SUCCESS 成功
 */
int jm_stm32_uart_send(const uint8_t *data, uint16_t len);

/**
 * @brief 处理来自 ESP8266 的 MQTT 串口数据
 * @param data 数据指针
 * @param len  数据长度
 */
void jm_mqtt_client_on_serial_data(const uint8_t *data, uint16_t len);

/**
 * @brief 处理来自 ESP8266 的 HTTP 串口数据
 * @param data 数据指针
 * @param len  数据长度
 */
void jm_http_client_on_serial_data(const uint8_t *data, uint16_t len);

/**
 * @brief 发送串口数据包（内部使用，也可用于自定义协议）
 * @param subtype     子类型
 * @param msg_id      消息 ID
 * @param payload     数据负载
 * @param payload_len 负载长度
 * @return @ref JM_SUCCESS 成功，@ref JM_ERR_NOT_READY 未初始化
 */
int jm_send_serial_packet(uint16_t subtype, uint16_t msg_id, const uint8_t *payload, uint16_t payload_len);

/* ===================== 引脚中断 API ===================== */

/**
 * @brief 注册引脚中断
 * @param gpioNo      引脚编号（0~31）
 * @param triggerType 触发类型：1=上升沿，2=下降沿，3=边沿变化
 * @return true 成功，false 失败
 */
bool jm_stm32_registerPinInterrupt(uint16_t gpioNo, uint8_t triggerType);

/**
 * @brief 注销引脚中断
 * @param gpioNo 引脚编号（0~31）
 * @return true 成功，false 失败
 */
bool jm_stm32_unregisterPinInterrupt(uint16_t gpioNo);

/**
 * @brief 上报引脚中断事件到 ESP8266
 * @param pin 引脚编号
 * @return true 成功
 */
int jm_stm32_pinInterrupt(const uint16_t pin);

/* ===================== 组件管理 ===================== */

/**
 * @brief 初始化所有组件（GPIO、测试模块、MQTT 等）
 * @param config jm_stm32 配置结构
 */
void jm_comp_init(const jm_config_t *config);

/**
 * @brief 组件轮询函数，处理各组件的状态机
 */
void jm_comp_loop(void);

/**
 * @brief 微秒级延时（基于 SysTick）
 * @param xus 延时时长，范围：0~233015
 */
void jm_delay_us(uint32_t xus);

/**
 * @brief 毫秒级延时（基于 SysTick）
 * @param xms 延时时长，范围：0~4294967295
 */
void jm_delay_ms(uint32_t xms);

/* ===================== Event system ===================== */

/** @brief 默认事件标志（无特殊处理） */
#define JM_EVENT_FLAG_DEFAULT     0
/** @brief 释放 data 指针内存 */
#define JM_EVENT_FLAG_FREE_DATA   0x01
/** @brief 释放 emap 数据内存 */
#define JM_EVENT_FLAG_FREE_EMAP   0x02
/** @brief 释放 elist 内存 */
#define JM_EVENT_FLAG_FREE_ELIST  0x04
/** @brief 释放字符串内存 */
#define JM_EVENT_FLAG_FREE_STR    0x08
/** @brief 释放消息内存 */
#define JM_EVENT_FLAG_FREE_MSG    0x10
/** @brief 事件来源网卡（不转发） */
#define JM_EVENT_FLAG_FROM_NETCARD 0x20

#if JM_STM32_EVENT_ENABLE

/**
 * @brief 异步事件结构
 */
typedef struct {
    uint8_t type;       /**< 事件类型 */
    uint16_t subType;   /**< 子类型 */
    void *data;         /**< 事件数据 */
    uint8_t flag;       /**< 内存管理标志，见 @ref JM_EVENT_FLAG_DEFAULT 等 */
} jm_event_t;

/**
 * @brief 事件监听回调函数类型
 * @param event 指向事件结构体
 */
typedef void (*jm_event_listener_fn)(jm_event_t *event);

/**
 * @brief 投递一个异步事件到事件队列
 * @param eventType 事件类型
 * @param subType   子类型
 * @param data      事件数据
 * @param flag      内存管理标志
 * @return true 成功入队，false 队列满
 */
bool jm_stm32_postEvent(uint8_t eventType, uint16_t subType, void *data, uint8_t flag);

/**
 * @brief 注册事件监听器
 * @param eventType 监听的事件类型
 * @param callback  回调函数
 * @return true 成功，false 失败
 */
bool jm_stm32_regEventListener(uint8_t eventType, jm_event_listener_fn callback);

/**
 * @brief 注销事件监听器
 * @param eventType 事件类型
 * @param callback  回调函数
 * @return true 成功，false 失败
 */
bool jm_stm32_unregEventListener(uint8_t eventType, jm_event_listener_fn callback);

#endif //#if JM_STM32_EVENT_ENABLE

#ifdef __cplusplus
}
#endif

#endif
