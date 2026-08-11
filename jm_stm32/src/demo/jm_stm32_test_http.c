/**
 * @file jm_stm32_http_client_test.c
 * @brief HTTP 客户端测试/示例模块
 *
 * 本模块演示如何使用 jm_http_client API 通过 ESP8266 netproxy 发起 HTTP 请求。
 * STM32 本身不运行 HTTP 协议栈，所有 HTTP 操作通过串口命令发送给
 * ESP8266，由 ESP8266 使用 ESP8266HTTPClient 完成实际的 HTTP 请求处理。
 *
 * @section 集成步骤
 * 1. 在 `jm_pcfg.h` 中启用 HTTP 客户端及本测试模块：
 *    @code
 *    #define JM_HTTP_CLIENT_ENABLE 1
 *    #define JM_HTTP_CLIENT_TEST_ENABLE 1
 *    @endcode
 *
 * 2. 将本文件添加到工程源文件列表。
 *
 * 3. 由 `jm_comp_init()` / `jm_comp_loop()` 自动调用本模块的 init/loop。
 *
 * @section 使用说明
 * - 按下 GPIO 按键 (PA0) 发起一次 HTTP GET 请求
 * - 请求地址：https://jmicro.cn/firmw/updater.info
 * - 响应文本通过日志串口输出
 */

#include "jm_stm32.h"
#include "jm_stm32_com.h"
#include "jm_http_client.h"
#include <string.h>
#include <stdio.h>

#if !defined(USE_HAL_UART)
#include "stm32f1xx.h"
#endif

#if JM_HTTP_CLIENT_TEST_ENABLE && JM_HTTP_CLIENT_ENABLE

//#define HTTP_TEST_URL         "https://47.107.141.158/firmw/updater.info"
//#define HTTP_TEST_URL         "https://jmicro.cn/firmw/test.txt"
#define HTTP_TEST_URL           "https://jmicro.cn/_http_/testHttpSrv?a=123&t=tssfsa"

//#define HTTP_TEST_URL           "https://jmicro.cn/"
#define HTTP_TEST_BTN_PIN     0               /**< 按键引脚编号 (PA0)http://192.168.3.10:8888/update/test.txt */
#define BTN_DEBOUNCE_MS       100              /**< 按键去抖时间（ms） */

/* ===================== 全局状态 ===================== */

static uint32_t g_btn_last_time = 0;   /**< 上次按键触发时间 */
static uint8_t g_btn_last_state = 1;   /**< 上次按键状态 */
static uint8_t g_btn_triggered = 0;    /**< 按键触发标志 */

/* ===================== 按键初始化 ===================== */

/** @brief 初始化按键引脚 (PA0 为上拉输入) */
static void button_init(void)
{
#if defined(USE_HAL_UART)
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
#else
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    GPIOA->CRL &= ~(0xF << 0);
    GPIOA->CRL |= (0x8 << 0);
    GPIOA->ODR |= (1 << 0);
#endif
}

/**
 * @brief 读取按键状态
 * @return 1=按下，0=释放
 */
static uint8_t button_read(void)
{
#if defined(USE_HAL_UART)
    return HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) ? 1 : 0;
#else
    return (GPIOA->IDR & (1 << 0)) ? 1 : 0;
#endif
}

/* ===================== HTTP 回调函数 ===================== */

/**
 * @brief HTTP 错误回调
 */
static void http_client_error_callback(int error_code, const char *error_msg) {
    JM_LOG_E("HTTP err: code=%d msg=%s", error_code, error_msg ? error_msg : "null");
}

/**
 * @brief 分片长度包回调 — 直接日志输出，不缓存
 */
static void http_client_length_callback(uint16_t status_code, uint32_t total_body_len) {
    JM_LOG_D("HTTP LENGTH: status=%u total_len=%u", status_code, total_body_len);
}

/**
 * @brief 分片数据包回调 — 直接日志输出，不缓存
 * @param seq 分片序号
 * @param chunk_len 本分片数据长度
 * @param data 分片数据指针
 */
static void http_client_data_callback(uint8_t seq, uint8_t chunk_len, const uint8_t *data) {
    //JM_LOG_D("HTTP DATA: seq=%u chunk=%u", seq, chunk_len);
    if (chunk_len > 0 && data) {
        const uint16_t LINE = 64;
        uint16_t offset = 0;
        while (offset < chunk_len) {
            uint16_t part = (chunk_len - offset > LINE) ? LINE : (chunk_len - offset);
           // JM_LOG_D("  text%u: %.*s", offset, part, (const char*)data + offset);
             JM_LOG_D(" %.*s",(const char*)data + offset);
            offset += part;
        }
    }
}

/**
 * @brief 分片结束包回调 — 直接日志输出，不缓存
 */
static void http_client_end_callback(uint16_t status_code) {
    JM_LOG_D("HTTP END: status=%u", status_code);
}

/* ===================== 公共 API ===================== */

/**
 * @brief HTTP 客户端测试模块初始化
 *
 * 初始化 HTTP 客户端，注册回调函数。
 * 由 `jm_comp_init()` 在启用时自动调用。
 *
 * @param config jm_stm32 配置结构
 */
void jm_http_client_test_init(const jm_config_t *config) {
    (void)config;

    jm_http_client_init(http_client_data_callback,
                        http_client_error_callback,
                        http_client_length_callback,
                        http_client_end_callback);

    button_init();

    JM_LOG_D("HTTP_TEST: init done, url=%s", HTTP_TEST_URL);
}

/**
 * @brief HTTP 客户端测试模块轮询
 *
 * 检测按键状态，按下时发起 HTTP GET 请求。
 * 应在主循环中周期性调用。
 */
void jm_http_client_test_loop(void) {
    uint32_t now = jm_stm32_get_time();
    uint8_t state = button_read();

    if (g_btn_last_state == 1 && state == 0) {
        if (now - g_btn_last_time > BTN_DEBOUNCE_MS && !g_btn_triggered) {
            g_btn_last_time = now;
            g_btn_triggered = 1;

            JM_LOG_D("HTTP_TEST: button pressed, sending GET %s", HTTP_TEST_URL);
            int rc = jm_http_client_get(HTTP_TEST_URL, "Accept: text/plain");
            if (rc != JM_SUCCESS) {
                JM_LOG_E("HTTP_TEST: send failed rc=%d", rc);
            }
        }
    }
    if (state == 1) {
        g_btn_triggered = 0;
    }
    g_btn_last_state = state;
}

#endif //#if JM_HTTP_CLIENT_TEST_ENABLE && JM_HTTP_CLIENT_ENABLE
