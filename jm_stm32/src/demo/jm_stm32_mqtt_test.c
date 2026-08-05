#include "jm_stm32.h"
#include "jm_stm32_com.h"
#include "jm_mqtt_proxy.h"
#include <string.h>

#if JM_MQTT_PROXY_ENABLE

#define MQTT_BROKER_HOST     "broker.emqx.io"
#define MQTT_BROKER_PORT     1883
#define MQTT_CLIENT_ID       "stm32_mqtt_client_001"
#define MQTT_USER            ""
#define MQTT_PASS            ""
#define MQTT_WILL_TOPIC      ""
#define MQTT_WILL_MSG       ""
#define MQTT_WILL_QOS        0
#define MQTT_WILL_RETAIN     false
#define MQTT_CLEAN_SESSION   true
#define MQTT_SUB_TOPIC       "stm32/test"
#define MQTT_PUB_TOPIC       "stm32/test"
#define MQTT_PUB_MSG         "Hello from STM32 via MQTT proxy"
#define MQTT_KEEPALIVE       60

static jm_mqtt_client_t g_mqtt;
static uint8_t g_mqtt_connected = 0;
static uint32_t g_last_pub_time = 0;
static uint32_t g_last_sub_time = 0;
static uint8_t g_subscribed = 0;

static void mqtt_message_callback(const char *topic, const uint8_t *payload, uint16_t len) {
    JM_LOG_D("MQTT RX: topic=%s len=%u", topic, len);
    for (uint16_t i = 0; i < len; i++) {
        jm_log_char((char)payload[i]);
    }
    jm_log_char('\n');
}

static void mqtt_connected_callback(void) {
    JM_LOG_D("MQTT: connected to broker");
    g_mqtt_connected = 1;
    g_subscribed = 0;
    g_last_sub_time = 0;
}

static void mqtt_disconnected_callback(void) {
    JM_LOG_D("MQTT: disconnected from broker");
    g_mqtt_connected = 0;
    g_subscribed = 0;
}

void jm_mqtt_test_init(const jm_config_t *config) {
    memset(&g_mqtt, 0, sizeof(g_mqtt));

    jm_mqtt_init(&g_mqtt, MQTT_CLIENT_ID, MQTT_KEEPALIVE);
    g_mqtt.message_cb = mqtt_message_callback;
    g_mqtt.connected_cb = mqtt_connected_callback;
    g_mqtt.disconnected_cb = mqtt_disconnected_callback;

    JM_LOG_D("MQTT test init done, connecting to %s:%d", MQTT_BROKER_HOST, MQTT_BROKER_PORT);

    int rc = jm_mqtt_connect(&g_mqtt, MQTT_BROKER_HOST, MQTT_BROKER_PORT,
                             MQTT_USER, MQTT_PASS,
                             MQTT_WILL_TOPIC, MQTT_WILL_MSG,
                             MQTT_WILL_QOS, MQTT_WILL_RETAIN,
                             MQTT_CLEAN_SESSION);
    if (rc != JM_SUCCESS) {
        JM_LOG_E("MQTT connect failed rc=%d", rc);
    }
}

void jm_mqtt_test_loop(void) {
    if (!g_mqtt_connected) {
        return;
    }

    uint32_t now = jm_mqtt_get_time_ms();

    if (!g_subscribed && (now - g_last_sub_time > 1000)) {
        g_last_sub_time = now;
        int rc = jm_mqtt_subscribe(&g_mqtt, MQTT_SUB_TOPIC, 0);
        if (rc == JM_SUCCESS) {
            JM_LOG_D("MQTT: subscribed to %s", MQTT_SUB_TOPIC);
            g_subscribed = 1;
        }
    }

    if (now - g_last_pub_time > 10000) {
        g_last_pub_time = now;
        int rc = jm_mqtt_publish(&g_mqtt, MQTT_PUB_TOPIC,
                                 (const uint8_t *)MQTT_PUB_MSG,
                                 strlen(MQTT_PUB_MSG), 0, false);
        if (rc == JM_SUCCESS) {
            JM_LOG_D("MQTT: published to %s", MQTT_PUB_TOPIC);
        }
    }

    jm_mqtt_loop(&g_mqtt);
}

#endif
