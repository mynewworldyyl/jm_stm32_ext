/**
 * @file jm_http_client.c
 * @brief 轻量级 HTTP 客户端实现
 *
 * 通过 ESP8266 netproxy 的 HTTP 代理功能，实现 STM32 端的 HTTP 通信。
 * 所有 HTTP 操作通过串口发送到 ESP8266，由 ESP8266 完成实际的 HTTP 请求处理。
 */

#include "jm_http_client.h"
#include "jm_stm32.h"
#include <string.h>
#include <stdio.h>

#if JM_HTTP_CLIENT_ENABLE

/* ===================== 全局状态 ===================== */

static jm_http_client_error_cb g_error_cb = NULL;
static jm_http_client_length_cb g_length_cb = NULL;
static jm_http_client_data_cb g_data_cb = NULL;
static jm_http_client_end_cb g_end_cb = NULL;

static bool g_initialized = false;
static bool g_waiting_response = false;
static uint32_t g_response_start_time = 0;

static uint16_t g_rx_len = 0;

/* ===================== 内部辅助函数 ===================== */

static uint8_t jm_http_client_next_req_id(void) {
    return jm_stm32_next_req_id();
}

static void jm_http_send_head(uint16_t total_len)
{
    uint8_t len_header[] = {PCK_HEANDER, (total_len >> 8) & 0xFF, total_len & 0xFF, jm_http_client_next_req_id()};
    jm_stm32_uart_send(len_header, sizeof(len_header));
}

static int jm_http_client_send_cmd(uint8_t cmd, const uint8_t *data, uint16_t data_len)
{
    if (!g_initialized) return JM_ERR_NOT_READY;

    uint8_t hdata[] = {0, 0, JM_SDADA_CHECK_NUM, JM_SERIALNET_TYPE_HTTP, cmd};

    uint16_t total_len = data_len + sizeof(hdata);
    jm_http_send_head(total_len);
    jm_stm32_uart_send(hdata, sizeof(hdata));

    if(data_len > 0 && data)
        jm_stm32_uart_send(data, data_len);

    return JM_SUCCESS;
}

static int jm_http_client_request(uint8_t cmd, const char *url, const char *headers, const uint8_t *body, uint16_t body_len)
{
    if (!g_initialized) return JM_ERR_NOT_READY;
    if (!url || strlen(url) >= JM_HTTP_CLIENT_MAX_URL_LEN) return JM_ERR_INVALID_PACKET;

    uint16_t url_len = (uint16_t)strlen(url);
    uint16_t headers_len = headers ? (uint16_t)strlen(headers) : 0;

    jm_buf_t *buf = jm_buf_create(4 + url_len + 2 + headers_len + 2 + body_len);
    if (!buf) return JM_ERR_MEMORY;

    jm_buf_write_string(buf, url, url_len);
    jm_buf_put_u16(buf, headers_len);
    if (headers_len > 0) {
        jm_buf_put_bytes(buf, (const uint8_t*)headers, headers_len);
    }
    jm_buf_put_u16(buf, body_len);
    if (body_len > 0 && body) {
        jm_buf_put_bytes(buf, body, body_len);
    }

    uint16_t payload_len = jm_buf_readable_len(buf);
    const uint8_t *payload = jm_buf_read_buf(buf);
    int rc = jm_http_client_send_cmd(cmd, payload, payload_len);
    jm_buf_release(buf);

    if (rc == JM_SUCCESS) {
        g_waiting_response = true;
        g_response_start_time = jm_stm32_get_time();
        g_rx_len = 0;
    }

    return rc;
}

/* ===================== 公共 API ===================== */

int jm_http_client_init(jm_http_client_data_cb data_cb,
                        jm_http_client_error_cb error_cb,
                        jm_http_client_length_cb length_cb,
                        jm_http_client_end_cb end_cb)
{
    g_error_cb = error_cb;
    g_length_cb = length_cb;
    g_data_cb = data_cb;
    g_end_cb = end_cb;
    g_initialized = true;
    g_waiting_response = false;
    g_rx_len = 0;

    return JM_SUCCESS;
}

int jm_http_client_get(const char *url, const char *headers)
{
    return jm_http_client_request(JM_HTTP_CLIENT_CMD_GET, url, headers, NULL, 0);
}

int jm_http_client_post(const char *url, const char *headers, const uint8_t *body, uint16_t body_len)
{
    return jm_http_client_request(JM_HTTP_CLIENT_CMD_POST, url, headers, body, body_len);
}

int jm_http_client_put(const char *url, const char *headers, const uint8_t *body, uint16_t body_len)
{
    return jm_http_client_request(JM_HTTP_CLIENT_CMD_PUT, url, headers, body, body_len);
}

int jm_http_client_delete(const char *url, const char *headers)
{
    return jm_http_client_request(JM_HTTP_CLIENT_CMD_DELETE, url, headers, NULL, 0);
}

int jm_http_client_head(const char *url, const char *headers)
{
    return jm_http_client_request(JM_HTTP_CLIENT_CMD_HEAD, url, headers, NULL, 0);
}

void jm_http_client_loop(void)
{
    if (!g_initialized) return;

    if (g_waiting_response) {
        uint32_t now = jm_stm32_get_time();
        if (now - g_response_start_time > 30000) {
            g_waiting_response = false;
            if (g_error_cb) {
                g_error_cb(-4, "timeout");
            }
        }
    }
}

/* ===================== 串口数据处理 ===================== */

void jm_http_client_on_serial_data(const uint8_t *data, uint16_t len)
{
    if (!g_initialized || len < 2) return;

    g_response_start_time = jm_stm32_get_time();

    uint8_t cmd = data[0];
    const uint8_t *payload = data + 1;
    uint16_t payload_len = len - 1;

    switch (cmd) {
        case JM_HTTP_CLIENT_RSP_LENGTH: {
            if (payload_len < 6) break;
            uint16_t status_code = (payload[0] << 8) | payload[1];
            uint32_t total_body_len = ((uint32_t)payload[2] << 24) | ((uint32_t)payload[3] << 16) | ((uint32_t)payload[4] << 8) | payload[5];
            //JM_LOG_D("HTTP length: status=%u total_len=%u", status_code, total_body_len);
            if (g_length_cb) {
                g_length_cb(status_code, total_body_len);
            }
            break;
        }

        case JM_HTTP_CLIENT_RSP_DATA: {
            if (payload_len < 2) break;
            uint8_t seq = payload[0];
            uint8_t chunk_len = payload[1];
            if (chunk_len == 0 || chunk_len > payload_len - 2) break;
            //JM_LOG_D("HTTP data: seq=%u chunk=%u", seq, chunk_len);
            if (g_data_cb) {
                g_data_cb(seq, chunk_len, payload + 2);
            }
            break;
        }

        case JM_HTTP_CLIENT_RSP_END: {
            if (payload_len < 2) break;
            uint16_t status_code = (payload[0] << 8) | payload[1];
            g_waiting_response = false;
            //JM_LOG_D("HTTP end: status=%u", status_code);
            if (g_end_cb) {
                g_end_cb(status_code);
            }
            break;
        }

        case JM_HTTP_CLIENT_EVT_ERROR: {
            if (payload_len < 1) break;
            int error_code = payload[0];
            uint16_t msg_len = 0;
            if (payload_len >= 3) {
                msg_len = (payload[1] << 8) | payload[2];
            }
            const char *error_msg = (payload_len >= 3 + msg_len) ? (const char*)(payload + 3) : "";

            g_waiting_response = false;

            if (g_error_cb) {
                g_error_cb(error_code, error_msg);
            }
            break;
        }

        default:
            break;
    }
}

#endif //#if JM_HTTP_CLIENT_ENABLE
