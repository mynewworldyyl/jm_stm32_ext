/**
 * @file jm_stm32_udp_test.c
 * @brief UDP 收发测试/示例模块
 * UdpServer.java是本样例的服务端实现，仅用于测试，完成具体功能需要根据需求修改
 * 
 * 本模块演示如何通过 jm_stm32 库向 ESP8266 netproxy 发送 UDP 数据，
 * 以及接收 UDP 数据上行事件。
 *
 * @section 集成步骤
 * 1. 在 `jm_pcfg.h` 中启用本模块：
 *    @code
 *    #define JM_STM32_TESTUDP_ENABLE 1
 *    @endcode
 *
 * 2. 将本文件添加到工程源文件列表。
 *
 * 3. 在 `main.c` 的 `on_event()` 回调中添加 UDP 事件分发：
 *    @code
 *    case JM_EVENT_UDP_DATA:
 *        jm_udp_test_on_event(event_type, data);
 *        break;
 *    @endcode
 *
 * 4. 在 `main.c` 的主循环中调用轮询函数：
 *    @code
 *    jm_udp_test_loop();
 *    @endcode
 *    或通过 `@ref jm_comp_loop` 自动调用。
 *
 * @section 使用说明
 * - 按下 GPIO 按键 (PA0) 向 @ref UDP_TEST_HOST:@ref UDP_TEST_PORT 发送 UDP 数据包
 * - 通过 ESP8266 netproxy 透传，目标服务器可接收到 STM32 发送的 UDP 数据
 * - 收到 UDP 数据通过 @ref JM_EVENT_UDP_DATA 事件上报
 * - 接收到的数据通过日志串口打印输出
 */

#include "jm_stm32.h"
#include <string.h>

#if !defined(USE_HAL_UART)
#include "stm32f1xx.h"
#endif

#if JM_STM32_TESTUDP_ENABLE

#define UDP_TEST_HOST      "192.168.3.10"  /**< UDP 目标服务器地址 */
#define UDP_TEST_PORT      9999            /**< UDP 目标服务器端口 */
#define UDP_TEST_BTN_PIN   0               /**< 按键引脚编号 (PA0) */
#define BTN_DEBOUNCE_MS    70              /**< 按键去抵抗时间（ms） */

/** @brief UDP 测试上下文 */
typedef struct {
    uint32_t btn_last_time;       /**< 上次按键触发时间 */
    uint8_t btn_last_state;       /**< 上次按键状态 */
    uint8_t btn_triggered;        /**< 按键触发标志 */
    uint32_t (*get_time_ms)(void); /**< 获取系统时间回调 */
} udp_test_ctx_t;

static udp_test_ctx_t g_udp_test;

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

/**
 * @brief UDP 事件处理回调
 *
 * 处理 UDP 数据接收事件。
 * 应在 `on_event()` 中调用此函数分发 UDP 相关事件。
 *
 * @param event_type 事件类型（@ref JM_EVENT_UDP_DATA）
 * @param data       事件数据 (jm_buf_t*)
 */
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

/**
 * @brief UDP 测试模块初始化
 * @param config jm_stm32 配置结构
 */
void jm_udp_test_init(const jm_config_t *config)
{
    memset(&g_udp_test, 0, sizeof(g_udp_test));
    g_udp_test.get_time_ms = config->get_sys_time_ms;
    g_udp_test.btn_last_state = 1;

    button_init();

    JM_LOG_D("UDP_TEST: init done");
}

/**
 * @brief UDP 测试模块轮询
 *
 * 检测按键状态，触发 UDP 数据发送。
 * 应在主循环中周期性调用。
 */
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