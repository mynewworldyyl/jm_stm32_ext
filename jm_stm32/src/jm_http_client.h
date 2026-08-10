/**
 * @file jm_http_client.h
 * @brief 轻量级 HTTP 客户端实现
 *
 * 通过 ESP8266 netproxy 的 HTTP 代理功能，STM32 端不需要运行完整的 HTTP 协议栈。
 * 所有 HTTP 操作通过串口发送到 ESP8266，由 ESP8266 使用 ESP8266HTTPClient 完成实际的 HTTP 请求处理。
 */

#ifndef JM_HTTP_CLIENT_H_
#define JM_HTTP_CLIENT_H_

#include "jm_stm32.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#if JM_HTTP_CLIENT_ENABLE

#ifdef __cplusplus
extern "C" {
#endif

/* ===================== 配置常量 ===================== */

/** @brief 最大 URL 长度 */
#define JM_HTTP_CLIENT_MAX_URL_LEN     128
/** @brief 最大头部长度 */
#define JM_HTTP_CLIENT_MAX_HEADERS_LEN 512
/** @brief 最大请求体长度 */
#define JM_HTTP_CLIENT_MAX_BODY_LEN    1024
/** @brief 接收缓冲区大小 */
#define JM_HTTP_CLIENT_RX_BUF_SIZE     2048

/* ===================== 命令类型 ===================== */

/** @brief GET 请求 */
#define JM_HTTP_CLIENT_CMD_GET         1
/** @brief POST 请求 */
#define JM_HTTP_CLIENT_CMD_POST        2
/** @brief PUT 请求 */
#define JM_HTTP_CLIENT_CMD_PUT         3
/** @brief DELETE 请求 */
#define JM_HTTP_CLIENT_CMD_DELETE      4
/** @brief HEAD 请求 */
#define JM_HTTP_CLIENT_CMD_HEAD        5

/* ===================== 响应/事件类型 ===================== */

/** @brief HTTP 响应 */
#define JM_HTTP_CLIENT_RSP_RESPONSE    1
/** @brief HTTP 错误事件 */
#define JM_HTTP_CLIENT_EVT_ERROR       2

/* ===================== 回调函数类型 ===================== */

/**
 * @brief HTTP 响应回调
 * @param status_code HTTP 状态码
 * @param body 响应体数据
 * @param body_len 响应体长度
 */
typedef void (*jm_http_client_response_cb)(uint16_t status_code, const uint8_t *body, uint16_t body_len);

/**
 * @brief HTTP 错误回调
 * @param error_code 错误码
 * @param error_msg 错误描述
 */
typedef void (*jm_http_client_error_cb)(int error_code, const char *error_msg);

/* ===================== API ===================== */

/**
 * @brief 初始化 HTTP 客户端
 * @param response_cb 响应回调
 * @param error_cb 错误回调
 * @return JM_SUCCESS 成功
 */
int jm_http_client_init(jm_http_client_response_cb response_cb, jm_http_client_error_cb error_cb);

/**
 * @brief 发送 GET 请求
 * @param url 请求 URL
 * @param headers 请求头字符串
 * @return JM_SUCCESS 成功
 */
int jm_http_client_get(const char *url, const char *headers);

/**
 * @brief 发送 POST 请求
 * @param url 请求 URL
 * @param headers 请求头字符串
 * @param body 请求体数据
 * @param body_len 请求体长度
 * @return JM_SUCCESS 成功
 */
int jm_http_client_post(const char *url, const char *headers, const uint8_t *body, uint16_t body_len);

/**
 * @brief 发送 PUT 请求
 * @param url 请求 URL
 * @param headers 请求头字符串
 * @param body 请求体数据
 * @param body_len 请求体长度
 * @return JM_SUCCESS 成功
 */
int jm_http_client_put(const char *url, const char *headers, const uint8_t *body, uint16_t body_len);

/**
 * @brief 发送 DELETE 请求
 * @param url 请求 URL
 * @param headers 请求头字符串
 * @return JM_SUCCESS 成功
 */
int jm_http_client_delete(const char *url, const char *headers);

/**
 * @brief 发送 HEAD 请求
 * @param url 请求 URL
 * @param headers 请求头字符串
 * @return JM_SUCCESS 成功
 */
int jm_http_client_head(const char *url, const char *headers);

/**
 * @brief HTTP 客户端轮询
 */
void jm_http_client_loop(void);

#ifdef __cplusplus
}
#endif

#endif //#if JM_HTTP_CLIENT_ENABLE

#endif //JM_HTTP_CLIENT_H_
