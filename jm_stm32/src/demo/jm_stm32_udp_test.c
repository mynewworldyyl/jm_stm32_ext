#include "jm_stm32.h"
#include <string.h>

#if !defined(USE_HAL_UART)
#include "stm32f1xx.h"
#endif

#if JM_STM32_TESTUDP_ENABLE

#define UDP_TEST_HOST      "192.168.3.10"
#define UDP_TEST_PORT      9999
#define UDP_TEST_BTN_PIN   0
#define BTN_DEBOUNCE_MS    70

typedef struct {
    uint32_t btn_last_time;
    uint8_t btn_last_state;
    uint8_t btn_triggered;
    uint32_t (*get_time_ms)(void);
} udp_test_ctx_t;

static udp_test_ctx_t g_udp_test;

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

void jm_udp_test_on_event(uint8_t event_type, jm_buf_t *data)
{
    switch (event_type) {
    case JM_EVENT_UDP_DATA: {
        jm_buf_t *buf = (jm_buf_t *)data;
        uint16_t n = jm_buf_readable_len(buf);
        JM_LOG_D("UDP_TEST: data len=%u", n);
        for (int i = 0; i < n; i++) {
            jm_log_char((char)buf->data[buf->rpos + i]);
        }
        break;
    }
    default:
        break;
    }
}

void jm_udp_test_init(const jm_config_t *config)
{
    memset(&g_udp_test, 0, sizeof(g_udp_test));
    g_udp_test.get_time_ms = config->get_sys_time_ms;
    g_udp_test.btn_last_state = 1;

    button_init();

    JM_LOG_D("UDP_TEST: init done");
}

void jm_udp_test_loop(void)
{
    if (!g_udp_test.get_time_ms) return;

    uint32_t now = g_udp_test.get_time_ms();
    uint8_t state = button_read();

    if (g_udp_test.btn_last_state == 1 && state == 0) {
        if (now - g_udp_test.btn_last_time > BTN_DEBOUNCE_MS && !g_udp_test.btn_triggered) {
            g_udp_test.btn_last_time = now;
            g_udp_test.btn_triggered = 1;

            JM_LOG_D("UDP_TEST: sending data to %s:%d", UDP_TEST_HOST, UDP_TEST_PORT);
            const char *msg = "Hello from STM32 UDP\n";
            jm_stm32_send_udp_data(UDP_TEST_HOST, UDP_TEST_PORT, (const uint8_t *)msg, strlen(msg));
        }
    }
    if (state == 1) {
        g_udp_test.btn_triggered = 0;
    }
    g_udp_test.btn_last_state = state;
}

#endif //#if JM_STM32_TESTUDP_ENABLE