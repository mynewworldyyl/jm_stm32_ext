#include "jm_stm32.h"
#include <string.h>

#if !defined(USE_HAL_UART)
#include "stm32f1xx.h"
#endif

#if JM_STM32_TESTTCP_ENABLE

#define TCP_TEST_HOST      "192.168.3.10"
#define TCP_TEST_PORT      8888
#define TCP_TEST_BTN_PIN   0
#define BTN_DEBOUNCE_MS    70

typedef struct {
    jm_tcp_conn_info_t conn;
    uint8_t connected;
    uint32_t btn_last_time;
    uint8_t btn_last_state;
    uint8_t btn_triggered;
    uint32_t (*get_time_ms)(void);
} tcp_test_ctx_t;

static tcp_test_ctx_t g_tcp_test;

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

static uint8_t button_read(void)
{
#if defined(USE_HAL_UART)
    return HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) ? 1 : 0;
#else
    return (GPIOA->IDR & (1 << 0)) ? 1 : 0;
#endif
}

void jm_tcp_test_on_event(uint8_t event_type, void *data)
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

void jm_tcp_test_init(const jm_config_t *config)
{
    memset(&g_tcp_test, 0, sizeof(g_tcp_test));
    g_tcp_test.get_time_ms = config->get_sys_time_ms;
    g_tcp_test.btn_last_state = 1;

    button_init();

    JM_LOG_D("TCP_TEST: init done");
}

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