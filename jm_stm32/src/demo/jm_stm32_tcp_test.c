/**
 * @file jm_stm32_tcp_test.c
 * @brief TCP 连接测试/示例模块
 *
 * TcpServer.java是本样例的服务端实现，仅用于测试，完成具体功能需要根据需求修改
 * 
 * 本模块演示如何通过 jm_stm32 库与 ESP8266 netproxy 建立 TCP 连接，
 * 并实现 TCP 数据收发。
 *
 * @section 集成步骤
 * 1. 在 `jm_pcfg.h` 中启用本模块：
 *    @code
 *    #define JM_STM32_TESTTCP_ENABLE 1
 *    @endcode
 *
 * 2. 将本文件添加到工程源文件列表中。
 *
 * 3. 在 `main.c` 的 `on_event()` 回调中添加 TCP 事件分发：
 *    @code
 *    case JM_EVENT_TCP_CONNECTED:
 *    case JM_EVENT_TCP_DISCONNECTED:
 *    case JM_EVENT_TCP_SEND_RESULT:
 *    case JM_EVENT_TCP_ERROR:
 *    case JM_EVENT_TCP_DATA:
 *        jm_tcp_test_on_event(event_type, data);
 *        break;
 *    @endcode
 *
 * 4. 在 `main.c` 的主循环中调用轮询函数：
 *    @code
 *    jm_tcp_test_loop();
 *    @endcode
 *    或通过 `@ref jm_comp_loop` 自动调用。
 *
 * @section 使用说明
 * - 按下 GPIO 按键 (PA0) 触发 TCP 连接到 @ref TCP_TEST_HOST:@ref TCP_TEST_PORT
 * - 按键第二次按下（已连接时）发送 "@ref TCP_TEST_HOST" 数据
 * - 收到 TCP 数据时通过 @ref JM_EVENT_TCP_DATA 事件上报
 * - 所有日志通过 `JM_LOG_D` 输出到 USART2
 *
 * @note 连接参数（主机、端口）通过 @ref TCP_TEST_HOST 和 @ref TCP_TEST_PORT 修改
 */

#include "jm_stm32.h"
#include <string.h>

#if !defined(USE_HAL_UART)
#include "stm32f1xx.h"
#endif

#if JM_STM32_TESTTCP_ENABLE

#define TCP_TEST_HOST      "192.168.3.10"  /**< TCP 服务器地址 */
#define TCP_TEST_PORT      8888             /**< TCP 服务器端口 */
#define TCP_TEST_BTN_PIN   0                /**< 按键引脚编号 (PA0) */
#define BTN_DEBOUNCE_MS    70               /**< 按键去抵抖时间（ms） */

/**
 * @brief TCP 测试上下文
 */
typedef struct {
    jm_tcp_conn_info_t conn;      /**< TCP 连接信息 */
    uint8_t connected;            /**< 连接状态标志 */
    uint32_t btn_last_time;       /**< 上次按键触发时间 */
    uint8_t btn_last_state;       /**< 上次按键状态 */
    uint8_t btn_triggered;        /**< 按键触发标志 */
    uint32_t (*get_time_ms)(void);/**< 获取系统时间回调 */
} tcp_test_ctx_t;

static tcp_test_ctx_t g_tcp_test;

/* ===================== 按键初始化 ===================== */

/**
 * @brief 初始化按键引脚 (PA0 为上拉输入)
 */
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
 * @return 1=按下（高电平），0=释放
 */
static uint8_t button_read(void)
{
#if defined(USE_HAL_UART)
    return HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) ? 1 : 0;
#else
    return (GPIOA->IDR & (1 << 0)) ? 1 : 0;
#endif
}

/**
 * @brief TCP 事件处理回调
 *
 * 处理 TCP 连接、断开、发送结果、错误和数据接收事件。
 * 应在 `on_event()` 中调用此函数分发 TCP 相关事件。
 *
 * @param event_type 事件类型（@ref JM_EVENT_TCP_*）
 * @param data       事件数据（jm_tcp_conn_info_t 或 jm_buf_t）
 */
//void jm_tcp_test_on_event(uint8_t event_type, void *data)
void jm_onTcpEvent(uint8_t event_type, void *data)
{
    switch (event_type) {
    case JM_EVENT_TCP_CONNECTED: {
        jm_tcp_conn_info_t *c = (jm_tcp_conn_info_t *)data;
        JM_LOG_D("TCP_TEST: connected sock=%d %s:%d", c->sock, c->host, c->port);
        g_tcp_test.conn = *c;
        g_tcp_test.connected = 1;
        break;
    }
    case JM_EVENT_TCP_DISCONNECTED: {
        jm_tcp_conn_info_t *c = (jm_tcp_conn_info_t *)data;
        JM_LOG_D("TCP_TEST: disconnected sock=%d %s:%d", c->sock, c->host, c->port);
        g_tcp_test.connected = 0;
        memset(&g_tcp_test.conn, 0, sizeof(g_tcp_test.conn));
        break;
    }
    case JM_EVENT_TCP_SEND_RESULT: {
        jm_tcp_conn_info_t *c = (jm_tcp_conn_info_t *)data;
        JM_LOG_D("TCP_TEST: send result sock=%d err=%d", c->sock, c->err_code);
        break;
    }
    case JM_EVENT_TCP_ERROR: {
        jm_tcp_conn_info_t *c = (jm_tcp_conn_info_t *)data;
        JM_LOG_D("TCP_TEST: error sock=%d host=%s err=%d", c->sock, c->host, c->err_code);
        g_tcp_test.connected = 0;
        memset(&g_tcp_test.conn, 0, sizeof(g_tcp_test.conn));
        break;
    }
    case JM_EVENT_TCP_DATA: {
        jm_buf_t *buf = (jm_buf_t *)data;
        uint16_t n = jm_buf_readable_len(buf);
        JM_LOG_D("TCP_TEST: data len=%u", n);
        for (int i = 0; i < n; i++) {
            jm_log_char((char)buf->data[buf->rpos + i]);
        }
        break;
    }
    default:
        break;
    }
}

/**
 * @brief TCP 测试模块初始化
 * @param config jm_stm32 配置结构
 */
void jm_tcp_test_init(const jm_config_t *config)
{
    memset(&g_tcp_test, 0, sizeof(g_tcp_test));
    g_tcp_test.get_time_ms = config->get_sys_time_ms;
    g_tcp_test.btn_last_state = 1;

    button_init();

    JM_LOG_D("TCP_TEST: init done");
}

/**
 * @brief TCP 测试模块轮询
 *
 * 检测按键状态，触发 TCP 连接或发送数据。
 * 应在主循环中周期性调用。
 */
void jm_tcp_test_loop(void)
{
    if (!g_tcp_test.get_time_ms) return;

    uint32_t now = g_tcp_test.get_time_ms();
    uint8_t state = button_read();

    if (g_tcp_test.btn_last_state == 1 && state == 0) {
        if (now - g_tcp_test.btn_last_time > BTN_DEBOUNCE_MS && !g_tcp_test.btn_triggered) {
            g_tcp_test.btn_last_time = now;
            g_tcp_test.btn_triggered = 1;

            if (!g_tcp_test.connected) {
                JM_LOG_D("TCP_TEST: connecting to %s:%d", TCP_TEST_HOST, TCP_TEST_PORT);
                jm_stm32_send_tcp_connect(TCP_TEST_HOST, TCP_TEST_PORT);
            } else {
                JM_LOG_D("TCP_TEST: sending data");
                const char *msg = "Hello from STM32\n";
                jm_stm32_send_tcp_data(g_tcp_test.conn.sock, (const uint8_t *)msg, strlen(msg));
            }
        }
    }
    if (state == 1) {
        g_tcp_test.btn_triggered = 0;
    }
    g_tcp_test.btn_last_state = state;
}

#endif //#if JM_STM32_TESTTCP_ENABLE