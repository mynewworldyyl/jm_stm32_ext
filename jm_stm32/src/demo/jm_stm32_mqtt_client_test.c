#include "jm_stm32.h"
#include "jm_stm32_com.h"
#include "jm_mqtt_client.h"
#include <string.h>

#if !defined(USE_HAL_UART)
#include "stm32f1xx.h"
#endif

#if JM_MQTT_CLIENT_ENABLE

#define MQTT_BROKER_HOST     "192.168.3.10"
#define MQTT_BROKER_PORT     1883
#define MQTT_CLIENT_ID       "stm32_mqtt_client_test"
#define MQTT_KEEPALIVE       60
#define MQTT_SUB_TOPIC       "stm32/test"
#define MQTT_PUB_TOPIC       "stm32/test"
#define MQTT_PUB_MSG         "Hello from STM32 MQTT client"
#define MQTT_TEST_BTN_PIN    1
#define BTN_DEBOUNCE_MS      70

static uint8_t g_mqtt_connected = 0;
static uint32_t g_last_pub_time = 0;
static uint32_t g_last_sub_time = 0;
static uint8_t g_subscribed = 0;
static uint32_t g_btn_last_time = 0;
static uint8_t g_btn_last_state = 1;
static uint8_t g_btn_triggered = 0;

static void button_init(void)
{
#if defined(USE_HAL_UART)
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
#else
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    GPIOA->CRL &= ~(0xF << 4);
    GPIOA->CRL |= (0x8 << 4);
    GPIOA->ODR |= (1 << 1);
#endif
}

static uint8_t button_read(void)
{
#if defined(USE_HAL_UART)
    return HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) ? 1 : 0;
#else
    return (GPIOA->IDR & (1 << 1)) ? 1 : 0;
#endif
}

static void mqtt_client_message_callback(const char *topic, const uint8_t *payload, uint16_t len) {
    JM_LOG_D("MQTT client RX: topic=%s len=%u", topic, len);
    for (uint16_t i = 0; i < len; i++) {
        jm_log_char((char)payload[i]);
    }
    jm_log_char('\n');
}

static void mqtt_client_connect_callback(void) {
    JM_LOG_D("MQTT client: connected to broker");
    g_mqtt_connected = 1;
    g_subscribed = 0;
    g_last_sub_time = 0;
}

static void mqtt_client_disconnect_callback(void) {
    JM_LOG_D("MQTT client: disconnected from broker");
    g_mqtt_connected = 0;
    g_subscribed = 0;
}

void jm_mqtt_client_test_init(const jm_config_t *config) {
    jm_mqtt_client_init(mqtt_client_message_callback,
                        mqtt_client_connect_callback,
                        mqtt_client_disconnect_callback);

    button_init();

    JM_LOG_D("MQTT client test init done, connecting to %s:%d", MQTT_BROKER_HOST, MQTT_BROKER_PORT);

    int rc = jm_mqtt_client_connect(MQTT_BROKER_HOST, MQTT_BROKER_PORT,
                                    MQTT_CLIENT_ID, MQTT_KEEPALIVE);
    if (rc != JM_SUCCESS) {
        JM_LOG_E("MQTT client connect failed rc=%d", rc);
    }
}

void jm_mqtt_client_test_loop(void) {
   
    if (!g_mqtt_connected) {
        return;
    }
 JM_LOG_E("test_loop");
    uint32_t now = jm_stm32_get_time();
    uint8_t state = button_read();

    if (g_btn_last_state == 1 && state == 0) {
        if (now - g_btn_last_time > BTN_DEBOUNCE_MS && !g_btn_triggered) {
            g_btn_last_time = now;
            g_btn_triggered = 1;

            int rc = jm_mqtt_client_publish(MQTT_PUB_TOPIC,
                                            (const uint8_t *)MQTT_PUB_MSG,
                                            strlen(MQTT_PUB_MSG), 0, false);
            if (rc == JM_SUCCESS) {
                JM_LOG_D("MQTT client: button publish to %s", MQTT_PUB_TOPIC);
            } else {
                JM_LOG_E("MQTT client: button publish failed rc=%d", rc);
            }
        }
    }
    if (state == 1) {
        g_btn_triggered = 0;
    }
    g_btn_last_state = state;

    if (!g_subscribed && (now - g_last_sub_time > 1000)) {
        g_last_sub_time = now;
        int rc = jm_mqtt_client_subscribe(MQTT_SUB_TOPIC, 0);
        if (rc == JM_SUCCESS) {
            JM_LOG_D("MQTT client: subscribed to %s", MQTT_SUB_TOPIC);
            g_subscribed = 1;
        }
    }

    if (now - g_last_pub_time > 10000) {
        g_last_pub_time = now;
        int rc = jm_mqtt_client_publish(MQTT_PUB_TOPIC,
                                        (const uint8_t *)MQTT_PUB_MSG,
                                        strlen(MQTT_PUB_MSG), 0, false);
        if (rc == JM_SUCCESS) {
            JM_LOG_D("MQTT client: published to %s", MQTT_PUB_TOPIC);
        }
    }

    jm_mqtt_client_loop();
}

#endif