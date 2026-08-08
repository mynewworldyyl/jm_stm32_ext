/**
 * @file jm_stm32_mqtt_client_test.c
 * @brief MQTT 客户端测试/示例模块
 *
 * 本模块演示如何使用 jm_mqtt_client API 与 ESP8266 netproxy 进行 MQTT 通信。
 * STM32 本身不运行 MQTT 协议栈，所有 MQTT 操作通过串口 AT 命令发送给
 * ESP8266，由 ESP8266 完成实际的 MQTT 协议处理。
 *
 * @section 集成步骤
 * 1. 在 `jm_pcfg.h` 中启用 MQTT 客户端：
 *    @code
 *    #define JM_MQTT_CLIENT_ENABLE 1      // 启用 MQTT 客户端框架
 *    #define JM_MQTT_CLIENT_TEST_ENABLE 1  // 启用本测试模块
 *    @endcode
 *
 * 2. 确保 `src/jm_mqtt_client.c` 和 `src/jm_mqtt_client.h` 已加入工程。
 *
 * 3. 在 `main.c` 中初始化 MQTT 客户端（由 `jm_comp_init` 自动调用）：
 *    @code
 *    jm_mqtt_client_init(msg_cb, connect_cb, disconnect_cb);
 *    @endcode
 *    或通过 `@ref jm_mqtt_client_test_init` 自动初始化。
 *
 * 4. 在 `main.c` 的主循环中调用轮询函数：
 *    @code
 *    jm_mqtt_client_loop();
 *    @endcode
 *    或通过 `@ref jm_comp_loop` 自动调用。
 *
 * @section 使用示例
 * 本示例模块的功能：
 * - 自动连接到 @ref MQTT_BROKER_HOST:MQTT_BROKER_PORT
 * - 使用 @ref MQTT_CLIENT_ID 作为客户端 ID，60 秒心跳
 * - 按下按键时发布消息到 @ref MQTT_PUB_TOPIC
 * - 自动订阅 @ref MQTT_SUB_TOPIC 主题
 * - 接收到消息时在日志中打印主题和负载内容
 *
 * @section 自定义使用
 * 替换以下宏定义以适配你的场景：
 * | 宏 | 说明 |
 * |-----|------|
 * | @ref MQTT_BROKER_HOST | MQTT 代理服务器地址 |
 * | @ref MQTT_BROKER_PORT | MQTT 代理服务器端口 |
 * | @ref MQTT_CLIENT_ID | 客户端 ID（需全局唯一） |
 * | @ref MQTT_KEEPALIVE | 心跳间隔（秒） |
 * | @ref MQTT_SUB_TOPIC | 订阅主题 |
 * | @ref MQTT_PUB_TOPIC | 发布主题 |
 * | @ref MQTT_PUB_MSG | 发布消息内容 |
 *
 * @note 连接凭据（用户名/密码）在本示例中硬编码为 ("jmicro", "jmicro123")，
 *       请根据实际 MQTT 服务器配置修改。
 */

#include "jm_stm32.h"
#include "jm_stm32_com.h"
#include "jm_mqtt_client.h"
#include <string.h>

#if !defined(USE_HAL_UART)
#include "stm32f1xx.h"
#endif

#if JM_MQTT_CLIENT_TEST_ENABLE && JM_MQTT_CLIENT_ENABLE

#define MQTT_BROKER_HOST     "192.168.3.10"   /**< MQTT 代理服务器地址 */
#define MQTT_BROKER_PORT     1883             /**< MQTT 代理服务器端口 */
#define MQTT_CLIENT_ID       "stm32_mqtt_client_test" /**< 客户端 ID（需唯一） */
#define MQTT_KEEPALIVE       60               /**< 心跳间隔（秒） */
#define MQTT_SUB_TOPIC       "stm32/test"     /**< 订阅主题 */
#define MQTT_PUB_TOPIC       "stm32/test"     /**< 发布主题 */
#define MQTT_PUB_MSG         "Hello from STM32 MQTT client" /**< 默认发布消息 */
#define MQTT_TEST_BTN_PIN    0               /**< 按键引脚 (PA0) */
#define BTN_DEBOUNCE_MS      70              /**< 按键去抵抗时间（ms） */

/* ===================== 全局状态 ===================== */

static uint8_t g_mqtt_connected = 0;    /**< MQTT 连接状态 */
static uint32_t g_last_pub_time = 0;    /**< 上次发布时间 */
static uint32_t g_last_sub_time = 0;   /**< 上次订阅时间 */
static uint8_t g_subscribed = 0;       /**< 订阅状态 */
static uint32_t g_btn_last_time = 0;   /**< 上次按键触发时间 */
static uint8_t g_btn_last_state = 1;   /**< 上次按键状态 */
static uint8_t g_btn_triggered = 0;    /**< 按键触发标志 */

static bool inited = false;            /**< 初始化标志 */

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

/* ===================== MQTT 回调函数 ===================== */

/**
 * @brief MQTT 消息到达回调
 * @param topic   消息主题
 * @param payload 消息负载
 * @param len     负载长度
 */
static void mqtt_client_message_callback(const char *topic, const uint8_t *payload, uint16_t len) {
    JM_LOG_D("MQTT client RX: topic=%s len=%u", topic, len);
    //*(payload+len) = '\0';
    JM_LOG_D("Pl=%s", (char*)payload);
}

/**
 * @brief MQTT 连接成功回调
 */
static void mqtt_client_connect_callback(void) {
    JM_LOG_D("MQTT client: connected to broker");
    g_mqtt_connected = 1;
    g_subscribed = 0;
    g_last_sub_time = 0;
}

/**
 * @brief MQTT 断开连接回调
 */
static void mqtt_client_disconnect_callback(void) {
    JM_LOG_D("MQTT client: disconnected from broker");
    g_mqtt_connected = 0;
    g_subscribed = 0;
}

/**
 * @brief MQTT 客户端测试模块初始化
 *
 * 初始化 MQTT 客户端，注册回调函数。
 * 由 `jm_comp_init()` 在启用时自动调用。
 *
 * @param config jm_stm32 配置结构
 */
void jm_mqtt_client_test_init(const jm_config_t *config) {
    jm_mqtt_client_init(mqtt_client_message_callback,
                        mqtt_client_connect_callback,
                        mqtt_client_disconnect_callback);
}

/**
 * @brief MQTT 客户端测试模块轮询
 *
 * 首次调用时自动连接到 MQTT 服务器并订阅主题。
 * 之后检测按键，按下时发布消息。
 * 周期性检查订阅状态。
 *
 * 应在主循环中周期性调用。
 */
void jm_mqtt_client_test_loop(void) {

    if(!inited) {
        button_init();
        JM_LOG_D("MQTT inito %s:%d", MQTT_BROKER_HOST, MQTT_BROKER_PORT);

        int rc = jm_mqtt_client_connect(MQTT_BROKER_HOST, MQTT_BROKER_PORT,
                                        MQTT_CLIENT_ID, MQTT_KEEPALIVE,
                                        "jmicro", "jmicro123");

        JM_LOG_D("mqcR");

        if (rc != JM_SUCCESS) {
            JM_LOG_E("MQTT client connect failed rc=%d", rc);
        }
        inited = true;
        return;
    }


    if (!g_mqtt_connected) {
        // JM_LOG_E("C");
        return;
    }

    //JM_LOG_E("test_loop");
    uint32_t now = jm_stm32_get_time();
    uint8_t state = button_read();

    if (g_btn_last_state == 1 && state == 0) {
        if (now - g_btn_last_time > BTN_DEBOUNCE_MS && !g_btn_triggered) {
            g_btn_last_time = now;
            g_btn_triggered = 1;

            int rc = jm_mqtt_client_publish(MQTT_SUB_TOPIC,
                                            (const uint8_t *)MQTT_PUB_MSG,
                                            strlen(MQTT_PUB_MSG)+1, //包手字符串结尾字符
                                            0, false);
            if (rc == JM_SUCCESS) {
                JM_LOG_D("MQTT client: button publish to %s", MQTT_SUB_TOPIC);
            } else {
                JM_LOG_E("MQTT client: button publish failed rc=%d", rc);
            }

            /*
            JM_LOG_D("subtop");
            g_last_sub_time = now;
            int rc = jm_mqtt_client_subscribe(MQTT_SUB_TOPIC, 0);
            if (rc == JM_SUCCESS) {
                JM_LOG_D("MQTT client: subscribed to %s", MQTT_SUB_TOPIC);
                g_subscribed = 1;
            }*/
            
        }
    }
    if (state == 1) {
        g_btn_triggered = 0;
    }
    g_btn_last_state = state;


    if (!g_subscribed && (now - g_last_sub_time > 1000)) {
        JM_LOG_D("subtop");
        g_last_sub_time = now;
        int rc = jm_mqtt_client_subscribe(MQTT_SUB_TOPIC, 0);
        if (rc == JM_SUCCESS) {
            JM_LOG_D("MQTT client: subscribed to %s", MQTT_SUB_TOPIC);
            g_subscribed = 1;
        }
    }

    //JM_LOG_E("ptier %u %u %u",now, g_last_pub_time, now - g_last_pub_time);
/*
    if (now - g_last_pub_time > 10000) {
        g_last_pub_time = now;
        JM_LOG_D("_publiAuto");
        int rc = jm_mqtt_client_publish(MQTT_PUB_TOPIC,
                                        (const uint8_t *)MQTT_PUB_MSG,
                                        strlen(MQTT_PUB_MSG), 0, false);
        if (rc == JM_SUCCESS) {
            JM_LOG_D("MQTT client: published to %s", MQTT_PUB_TOPIC);
        }
    }
*/
    jm_mqtt_client_loop();
    
}

#endif //#if JM_MQTT_CLIENT_TEST_ENABLE && JM_MQTT_CLIENT_ENABLE