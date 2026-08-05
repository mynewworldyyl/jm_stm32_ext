#include "jm_mqtt_client.h"
#include "jm_stm32.h"
#include <string.h>
#include <stdio.h>

#if JM_MQTT_CLIENT_ENABLE

static jm_mqtt_client_msg_cb g_msg_cb = NULL;
static jm_mqtt_client_connect_cb g_connect_cb = NULL;
static jm_mqtt_client_disconnect_cb g_disconnect_cb = NULL;

static bool g_initialized = false;
static bool g_connected = false;
static bool g_connecting = false;

static char g_broker_host[JM_MQTT_CLIENT_MAX_BROKER_LEN] = {0};
static uint16_t g_broker_port = 1883;
static char g_client_id[JM_MQTT_CLIENT_MAX_CLIENT_ID] = {0};
static uint16_t g_keepalive = 60;

static uint32_t g_connect_start_time = 0;
static uint8_t g_rx_buf[JM_MQTT_CLIENT_RX_BUF_SIZE] = {0};
static uint16_t g_rx_len = 0;

static uint8_t jm_mqtt_client_next_req_id(void) {
    return jm_stm32_next_req_id();
}

static void jm_mqtt_sendHead(uint16_t total_len)
{
    uint8_t len_header[] = {PCK_HEANDER, (total_len >> 8) & 0xFF, total_len & 0xFF, jm_mqtt_client_next_req_id()};
    jm_stm32_uart_send(len_header, sizeof(len_header));
}


static int jm_mqtt_client_send_cmd(uint8_t cmd, const uint8_t *data, uint16_t data_len)
{
    if (!g_initialized) return JM_ERR_NOT_READY;

    uint8_t hdata[] = {0,0,JM_SDADA_CHECK_NUM,JM_SERIALNET_TYPE_MQTT,cmd};

    uint16_t total_len = data_len + sizeof(hdata);
    jm_mqtt_sendHead(total_len);
    jm_stm32_uart_send(hdata,sizeof(hdata));

    if(data_len > 0 && data)
        jm_stm32_uart_send(data, data_len);

    return JM_SUCCESS;
}

void jm_mqtt_client_on_serial_data(const uint8_t *data, uint16_t len)
{
    if (!g_initialized || len < 2) return;

    uint8_t cmd = data[0];
    const uint8_t *payload = data + 1;
    uint16_t payload_len = len - 1;

    switch (cmd) {
        case JM_MQTT_CLIENT_RSP_CONNECT:
            if (payload_len >= 1 && payload[0] == 0) {
                g_connected = true;
                g_connecting = false;
                JM_LOG_D("mqtt client connected");
                if (g_connect_cb) g_connect_cb();
            } else {
                g_connecting = false;
                JM_LOG_E("mqtt client connect failed");
            }
            break;

        case JM_MQTT_CLIENT_RSP_PUBLISH:
            JM_LOG_D("mqtt client publish rsp");
            break;

        case JM_MQTT_CLIENT_RSP_SUBSCRIBE:
            JM_LOG_D("mqtt client subscribe rsp");
            break;

        case JM_MQTT_CLIENT_EVT_MESSAGE:
            if (payload_len >= 4) {
                jm_buf_t *buf = jm_buf_wrap_array(payload, payload_len);
                if (!buf) break;

                int8_t flag = 0;
                char *topic = jm_buf_read_string(buf, &flag);
                if (!topic || flag <= 0) {
                    jm_buf_release(buf);
                    break;
                }

                uint16_t msg_payload_len = 0;
                if (!jm_buf_get_u16(buf, &msg_payload_len)) {
                    free(topic);
                    jm_buf_release(buf);
                    break;
                }

                if (msg_payload_len > jm_buf_readable_len(buf)) {
                    free(topic);
                    jm_buf_release(buf);
                    break;
                }

                if (g_msg_cb) {
                    const uint8_t *msg_payload = jm_buf_read_buf(buf);
                    g_msg_cb(topic, msg_payload, msg_payload_len);
                }

                free(topic);
                jm_buf_release(buf);
            }
            break;

        case JM_MQTT_CLIENT_EVT_DISCONNECTED:
            g_connected = false;
            g_connecting = false;
            JM_LOG_D("mqtt client disconnected");
            if (g_disconnect_cb) g_disconnect_cb();
            break;

        default:
            break;
    }
}

int jm_mqtt_client_init(jm_mqtt_client_msg_cb msg_cb,
                         jm_mqtt_client_connect_cb connect_cb,
                         jm_mqtt_client_disconnect_cb disconnect_cb)
{
    g_msg_cb = msg_cb;
    g_connect_cb = connect_cb;
    g_disconnect_cb = disconnect_cb;
    g_initialized = true;
    g_connected = false;
    g_connecting = false;
    g_rx_len = 0;

    JM_LOG_D("mqtt client init done");
    return JM_SUCCESS;
}

int jm_mqtt_client_connect(const char *broker_host, uint16_t broker_port,
                            const char *client_id, uint16_t keepalive)
{
    if (!g_initialized) return JM_ERR_NOT_READY;
    if (!broker_host || strlen(broker_host) >= JM_MQTT_CLIENT_MAX_BROKER_LEN) {
        return JM_ERR_INVALID_PACKET;
    }

    strncpy(g_broker_host, broker_host, JM_MQTT_CLIENT_MAX_BROKER_LEN - 1);
    g_broker_host[JM_MQTT_CLIENT_MAX_BROKER_LEN - 1] = '\0';
    g_broker_port = broker_port;
    g_keepalive = keepalive ? keepalive : 60;

    if (client_id && strlen(client_id) < JM_MQTT_CLIENT_MAX_CLIENT_ID) {
        strncpy(g_client_id, client_id, JM_MQTT_CLIENT_MAX_CLIENT_ID - 1);
        g_client_id[JM_MQTT_CLIENT_MAX_CLIENT_ID - 1] = '\0';
    } else {
        snprintf(g_client_id, JM_MQTT_CLIENT_MAX_CLIENT_ID, "stm32_mqtt");
    }

    jm_buf_t *buf = jm_buf_create(64);
    if (!buf) return JM_ERR_MEMORY;

    jm_buf_write_string(buf, g_broker_host, (uint16_t)strlen(g_broker_host));
    jm_buf_put_u16(buf, g_broker_port);
    jm_buf_write_string(buf, g_client_id, (uint16_t)strlen(g_client_id));
   
    jm_buf_put_u16(buf, g_keepalive);

    uint16_t payload_len = jm_buf_readable_len(buf);
    const uint8_t *payload = jm_buf_read_buf(buf);

    g_connecting = true;
    g_connect_start_time = jm_stm32_get_time();
    int rc = jm_mqtt_client_send_cmd(JM_MQTT_CLIENT_CMD_CONNECT, payload, payload_len);

    jm_buf_release(buf);
    if (rc != JM_SUCCESS) {
        g_connecting = false;
    }

    return rc;
}

int jm_mqtt_client_publish(const char *topic, const uint8_t *payload, uint16_t len,
                            uint8_t qos, bool retained)
{
    if (!g_initialized || !g_connected) return JM_ERR_NOT_READY;
    if (!topic || strlen(topic) >= JM_MQTT_CLIENT_MAX_TOPIC_LEN) return JM_ERR_INVALID_PACKET;
    if (len > JM_MQTT_CLIENT_MAX_PAYLOAD_LEN) return JM_ERR_INVALID_PACKET;
    if (qos > 2) qos = 0;

    jm_buf_t *buf = jm_buf_create(strlen(topic) + len + 4);
    if (!buf) return JM_ERR_MEMORY;

    jm_buf_write_string(buf, topic, (uint16_t)strlen(topic));
    jm_buf_put_u8(buf, qos);
    jm_buf_put_u8(buf, retained ? 1 : 0);
    jm_buf_put_u16(buf, len);
    jm_buf_put_bytes(buf, payload, len);

    uint16_t payload_len = jm_buf_readable_len(buf);
    const uint8_t *data = jm_buf_read_buf(buf);
    int rc = jm_mqtt_client_send_cmd(JM_MQTT_CLIENT_CMD_PUBLISH, data, payload_len);
    jm_buf_release(buf);
    return rc;
}

int jm_mqtt_client_subscribe(const char *topic, uint8_t qos)
{
    if (!g_initialized || !g_connected) return JM_ERR_NOT_READY;
    if (!topic || strlen(topic) >= JM_MQTT_CLIENT_MAX_TOPIC_LEN) return JM_ERR_INVALID_PACKET;
    if (qos > 1) qos = 0;

    jm_buf_t *buf = jm_buf_create(JM_MQTT_CLIENT_MAX_TOPIC_LEN + 4);
    if (!buf) return JM_ERR_MEMORY;

    jm_buf_write_string(buf, topic, (uint16_t)strlen(topic));
    jm_buf_put_u8(buf, qos);

    uint16_t payload_len = jm_buf_readable_len(buf);
    const uint8_t *data = jm_buf_read_buf(buf);
    int rc = jm_mqtt_client_send_cmd(JM_MQTT_CLIENT_CMD_SUBSCRIBE, data, payload_len);
    jm_buf_release(buf);
    return rc;
}

int jm_mqtt_client_unsubscribe(const char *topic)
{
    if (!g_initialized || !g_connected) return JM_ERR_NOT_READY;
    if (!topic || strlen(topic) >= JM_MQTT_CLIENT_MAX_TOPIC_LEN) return JM_ERR_INVALID_PACKET;

    jm_buf_t *buf = jm_buf_create(JM_MQTT_CLIENT_MAX_TOPIC_LEN + 2);
    if (!buf) return JM_ERR_MEMORY;

    jm_buf_write_string(buf, topic, (uint16_t)strlen(topic));

    uint16_t payload_len = jm_buf_readable_len(buf);
    const uint8_t *data = jm_buf_read_buf(buf);
    int rc = jm_mqtt_client_send_cmd(JM_MQTT_CLIENT_CMD_UNSUBSCRIBE, data, payload_len);
    jm_buf_release(buf);
    return rc;
}

int jm_mqtt_client_disconnect(void)
{
    if (!g_initialized) return JM_ERR_NOT_READY;

    g_connected = false;
    g_connecting = false;
    return jm_mqtt_client_send_cmd(JM_MQTT_CLIENT_CMD_DISCONNECT, NULL, 0);
}

void jm_mqtt_client_loop(void)
{
    if (!g_initialized) return;

    if (g_connecting) {
        uint32_t now = jm_stm32_get_time();
        if (now - g_connect_start_time > 60000) {
            g_connecting = false;
            g_connected = false;
            JM_LOG_E("mqtt client connect timeout");
        }
    }
}

bool jm_mqtt_client_is_connected(void)
{
    return g_connected;
}

#endif //#if JM_MQTT_CLIENT_ENABLE
