/**
 * @file jm_mqtt_client.h
 * @brief 轻量级 MQTT 客户端实现
 *
 * 通过 ESP8266 netproxy 的 MQTT 代理功能，实现 STM32 端的 MQTT 通信。
 * 所有 MQTT 操作通过串口发送到 ESP8266，由 ESP8266 完成实际的 MQTT 协议处理。
 */

#ifndef JM_MQTT_CLIENT_H_
#define JM_MQTT_CLIENT_H_

#include "jm_stm32.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#if JM_MQTT_CLIENT_ENABLE

#ifdef __cplusplus
extern "C" {
#endif

/* ===================== 配置常量 ===================== */

/** @brief 最大主题长度 */
#define JM_MQTT_CLIENT_MAX_TOPIC_LEN    64
/** @brief 最大负载长度 */
#define JM_MQTT_CLIENT_MAX_PAYLOAD_LEN  256
/** @brief 最大客户端 ID 长度 */
#define JM_MQTT_CLIENT_MAX_CLIENT_ID    32
/** @brief 最大服务器地址长度 */
#define JM_MQTT_CLIENT_MAX_BROKER_LEN   32
/** @brief 接收缓冲区大小 */
#define JM_MQTT_CLIENT_RX_BUF_SIZE      512

/* ===================== 命令类型 ===================== */

/** @brief 连接命令 */
#define JM_MQTT_CLIENT_CMD_CONNECT      1
/** @brief 发布消息命令 */
#define JM_MQTT_CLIENT_CMD_PUBLISH      2
/** @brief 订阅主题命令 */
#define JM_MQTT_CLIENT_CMD_SUBSCRIBE    3
/** @brief 取消订阅命令 */
#define JM_MQTT_CLIENT_CMD_UNSUBSCRIBE  4
/** @brief 断开连接命令 */
#define JM_MQTT_CLIENT_CMD_DISCONNECT   5

/* ===================== 响应/事件类型 ===================== */

/** @brief 连接响应 */
#define JM_MQTT_CLIENT_RSP_CONNECT      1
/** @brief 发布响应 */
#define JM_MQTT_CLIENT_RSP_PUBLISH      2
/** @brief 订阅响应 */
#define JM_MQTT_CLIENT_RSP_SUBSCRIBE    3
/** @brief 消息到达事件 */
#define JM_MQTT_CLIENT_EVT_MESSAGE      4
/** @brief 断开连接事件 */
#define JM_MQTT_CLIENT_EVT_DISCONNECTED 5

/* ===================== 回调函数类型 ===================== */

/**
 * @brief 消息到达回调
 * @param topic   消息主题
 * @param payload 消息负载
 * @param len     负载长度
 */
typedef void (*jm_mqtt_client_msg_cb)(const char *topic, const uint8_t *payload, uint16_t len);

/**
 * @brief 连接成功回调
 */
typedef void (*jm_mqtt_client_connect_cb)(void);

/**
 * @brief 断开连接回调
 */
typedef void (*jm_mqtt_client_disconnect_cb)(void);

/* ===================== API ===================== */

/**
 * @brief 初始化 MQTT 客户端
 * @param msg_cb         消息到达回调
 * @param connect_cb     连接成功回调
 * @param disconnect_cb  断开连接回调
 * @return @ref JM_SUCCESS 成功
 */
int jm_mqtt_client_init(jm_mqtt_client_msg_cb msg_cb,
                        jm_mqtt_client_connect_cb connect_cb,
                        jm_mqtt_client_disconnect_cb disconnect_cb);

/**
 * @brief 连接到 MQTT 服务器
 * @param broker_host 服务器地址
 * @param broker_port 服务器端口
 * @param client_id   客户端 ID
 * @param keepalive   心跳间隔（秒）
 * @param username    用户名（可为 NULL）
 * @param password    密码（可为 NULL）
 * @return @ref JM_SUCCESS 成功，其他 错误码
 */
int jm_mqtt_client_connect(const char *broker_host, uint16_t broker_port,
                           const char *client_id, uint16_t keepalive,
                           const char *username, const char *password);

/**
 * @brief 发布消息
 * @param topic   主题
 * @param payload 负载数据
 * @param len     负载长度
 * @param qos     QoS 等级（0/1/2）
 * @param retained 是否保留消息
 * @return @ref JM_SUCCESS 成功
 */
int jm_mqtt_client_publish(const char *topic, const uint8_t *payload, uint16_t len,
                           uint8_t qos, bool retained);

/**
 * @brief 订阅主题
 * @param topic 主题
 * @param qos   QoS 等级（0/1）
 * @return @ref JM_SUCCESS 成功
 */
int jm_mqtt_client_subscribe(const char *topic, uint8_t qos);

/**
 * @brief 取消订阅主题
 * @param topic 主题
 * @return @ref JM_SUCCESS 成功
 */
int jm_mqtt_client_unsubscribe(const char *topic);

/**
 * @brief 断开 MQTT 连接
 * @return @ref JM_SUCCESS 成功
 */
int jm_mqtt_client_disconnect(void);

/**
 * @brief MQTT 客户端轮询（处理心跳超时等）
 */
void jm_mqtt_client_loop(void);

/**
 * @brief 检查是否已连接到服务器
 * @return true 已连接
 */
bool jm_mqtt_client_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif //#if JM_MQTT_CLIENT_ENABLE

#endif /* JM_MQTT_CLIENT_H_ */