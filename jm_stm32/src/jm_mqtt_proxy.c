#include "jm_mqtt_proxy.h"
#include "jm_stm32.h"
#include "jm_stm32_com.h"
#include <stdio.h>
#include <string.h>


#if JM_MQTT_PROXY_ENABLE

#define MQTTCONNECT    0x10
#define MQTTCONNACK    0x20
#define MQTTPUBLISH    0x30
#define MQTTPUBACK     0x40
#define MQTTPUBREC     0x50
#define MQTTPUBREL     0x62
#define MQTTPUBCOMP    0x70
#define MQTTSUBSCRIBE  0x82
#define MQTTSUBACK     0x90
#define MQTTUNSUBSCRIBE 0xA2
#define MQTTUNSUBACK   0xB0
#define MQTTPINGREQ    0xC0
#define MQTTPINGRESP   0xD0
#define MQTTDISCONNECT 0xE0

#define MQTT_QOS0 0
#define MQTT_QOS1 1
#define MQTT_QOS2 2


static jm_mqtt_client_t g_mqtt_client;
static bool g_mqtt_initialized = false;
static const jm_config_t *g_mqtt_config = NULL;

static uint16_t jm_mqtt_next_msg_id(jm_mqtt_client_t *client) {
    client->msg_id++;
    if (client->msg_id == 0) client->msg_id = 1;
    return client->msg_id;
}

static uint32_t jm_mqtt_get_time_ms(void) {
    if (g_mqtt_config && g_mqtt_config->get_sys_time_ms) {
        return g_mqtt_config->get_sys_time_ms();
    }
    return 0;
}

static int jm_mqtt_send_packet(jm_mqtt_client_t *client, const uint8_t *data, uint16_t len) {
    if (!g_mqtt_initialized || !g_mqtt_config || !g_mqtt_config->uart_send) {
        return JM_ERR_NOT_READY;
    }
    if (client->sock < 0) {
        return JM_ERR_NOT_READY;
    }

    jm_buf_t *buf = jm_buf_create(24);
    if (!buf) return JM_ERR_MEMORY;

    jm_buf_put_u16(buf, 0);
    jm_buf_put_u8(buf, JM_SDADA_CHECK_NUM);
    jm_buf_put_u8(buf, JM_SERIALNET_TYPE_TCP);
    jm_buf_put_s8(buf, client->sock);

    uint16_t head_len = jm_buf_readable_len(buf);
    uint16_t total_len = head_len + len;

    uint8_t byte0 = (total_len >> 8) & 0xFF;
    uint8_t byte1 = total_len & 0xFF;
    uint8_t reqId = jm_stm32_next_req_id();

    uint8_t header[3] = {byte0, byte1, reqId};
    g_mqtt_config->uart_send(header, sizeof(header));
    g_mqtt_config->uart_send(jm_buf_read_buf(buf), head_len);
    g_mqtt_config->uart_send(data, len);

    jm_buf_release(buf);

    client->last_activity = jm_mqtt_get_time_ms();
    return JM_SUCCESS;
}

static int jm_mqtt_send_connect(jm_mqtt_client_t *client, const char *user, const char *pass, const char *will_topic, const char *will_msg, uint8_t will_qos, bool will_retain, bool clean_session) {
    uint8_t buf[256];
    uint16_t pos = 0;

    buf[pos++] = 0x00;
    buf[pos++] = 0x04;
    buf[pos++] = 'M';
    buf[pos++] = 'Q';
    buf[pos++] = 'T';
    buf[pos++] = 'T';
    buf[pos++] = JM_MQTT_PROTOCOL_VERSION;
    buf[pos++] = 0x02;
    if (clean_session) buf[pos - 1] |= 0x02;
    if (user) buf[pos - 1] |= 0x80;
    if (pass) buf[pos - 1] |= 0x40;
    if (will_topic) buf[pos - 1] |= 0x04 | (will_qos << 3) | (will_retain ? 0x20 : 0x00);

    uint16_t keepalive = client->keepalive;
    buf[pos++] = (keepalive >> 8) & 0xFF;
    buf[pos++] = keepalive & 0xFF;

    uint16_t cid_len = strlen(client->client_id);
    buf[pos++] = (cid_len >> 8) & 0xFF;
    buf[pos++] = cid_len & 0xFF;
    memcpy(&buf[pos], client->client_id, cid_len);
    pos += cid_len;

    if (will_topic && will_msg) {
        uint16_t wt_len = strlen(will_topic);
        buf[pos++] = (wt_len >> 8) & 0xFF;
        buf[pos++] = wt_len & 0xFF;
        memcpy(&buf[pos], will_topic, wt_len);
        pos += wt_len;

        uint16_t wm_len = strlen(will_msg);
        buf[pos++] = (wm_len >> 8) & 0xFF;
        buf[pos++] = wm_len & 0xFF;
        memcpy(&buf[pos], will_msg, wm_len);
        pos += wm_len;
    }

    if (user) {
        uint16_t u_len = strlen(user);
        buf[pos++] = (u_len >> 8) & 0xFF;
        buf[pos++] = u_len & 0xFF;
        memcpy(&buf[pos], user, u_len);
        pos += u_len;
    }

    if (pass) {
        uint16_t p_len = strlen(pass);
        buf[pos++] = (p_len >> 8) & 0xFF;
        buf[pos++] = p_len & 0xFF;
        memcpy(&buf[pos], pass, p_len);
        pos += p_len;
    }

    uint16_t remaining_len = pos;
    uint8_t encoded_len[4];
    uint8_t elen = 0;
    uint32_t tmp = remaining_len;
    do {
        encoded_len[elen++] = (tmp % 128) | (tmp >= 128 ? 0x80 : 0x00);
        tmp /= 128;
    } while (tmp > 0 && elen < 4);

    uint8_t final_buf[256];
    uint16_t fpos = 0;
    final_buf[fpos++] = MQTTCONNECT;
    for (int i = elen - 1; i >= 0; i--) {
        final_buf[fpos++] = encoded_len[i];
    }
    memcpy(&final_buf[fpos], buf, remaining_len);
    fpos += remaining_len;

    return jm_mqtt_send_packet(client, final_buf, fpos);
}

static int jm_mqtt_send_publish(jm_mqtt_client_t *client, const char *topic, const uint8_t *payload, uint16_t len, uint8_t qos, bool retained) {
    uint8_t buf[256];
    uint16_t pos = 0;

    uint16_t topic_len = strlen(topic);
    buf[pos++] = (topic_len >> 8) & 0xFF;
    buf[pos++] = topic_len & 0xFF;
    memcpy(&buf[pos], topic, topic_len);
    pos += topic_len;

    if (qos > 0) {
        uint16_t msg_id = jm_mqtt_next_msg_id(client);
        buf[pos++] = (msg_id >> 8) & 0xFF;
        buf[pos++] = msg_id & 0xFF;
    }

    memcpy(&buf[pos], payload, len);
    pos += len;

    uint8_t header = MQTTPUBLISH | (qos << 1) | (retained ? 0x01 : 0x00);

    uint16_t remaining_len = pos;
    uint8_t encoded_len[4];
    uint8_t elen = 0;
    uint32_t tmp = remaining_len;
    do {
        encoded_len[elen++] = (tmp % 128) | (tmp >= 128 ? 0x80 : 0x00);
        tmp /= 128;
    } while (tmp > 0 && elen < 4);

    uint8_t final_buf[256];
    uint16_t fpos = 0;
    final_buf[fpos++] = header;
    for (int i = elen - 1; i >= 0; i--) {
        final_buf[fpos++] = encoded_len[i];
    }
    memcpy(&final_buf[fpos], buf, remaining_len);
    fpos += remaining_len;

    return jm_mqtt_send_packet(client, final_buf, fpos);
}

static int jm_mqtt_send_subscribe(jm_mqtt_client_t *client, const char *topic, uint8_t qos) {
    uint8_t buf[128];
    uint16_t pos = 0;

    uint16_t msg_id = jm_mqtt_next_msg_id(client);
    buf[pos++] = (msg_id >> 8) & 0xFF;
    buf[pos++] = msg_id & 0xFF;

    uint16_t topic_len = strlen(topic);
    buf[pos++] = (topic_len >> 8) & 0xFF;
    buf[pos++] = topic_len & 0xFF;
    memcpy(&buf[pos], topic, topic_len);
    pos += topic_len;
    buf[pos++] = qos;

    if (client->sub_count < JM_MQTT_MAX_SUBSCRIPTIONS) {
        strncpy(client->subs[client->sub_count].topic, topic, JM_MQTT_MAX_TOPIC_LEN - 1);
        client->subs[client->sub_count].topic[JM_MQTT_MAX_TOPIC_LEN - 1] = '\0';
        client->subs[client->sub_count].qos = qos;
        client->subs[client->sub_count].msg_id = msg_id;
        client->sub_count++;
    }

    uint16_t remaining_len = pos;
    uint8_t encoded_len[4];
    uint8_t elen = 0;
    uint32_t tmp = remaining_len;
    do {
        encoded_len[elen++] = (tmp % 128) | (tmp >= 128 ? 0x80 : 0x00);
        tmp /= 128;
    } while (tmp > 0 && elen < 4);

    uint8_t final_buf[256];
    uint16_t fpos = 0;
    final_buf[fpos++] = MQTTSUBSCRIBE | 0x02;
    for (int i = elen - 1; i >= 0; i--) {
        final_buf[fpos++] = encoded_len[i];
    }
    memcpy(&final_buf[fpos], buf, remaining_len);
    fpos += remaining_len;

    return jm_mqtt_send_packet(client, final_buf, fpos);
}

static int jm_mqtt_send_unsubscribe(jm_mqtt_client_t *client, const char *topic) {
    uint8_t buf[128];
    uint16_t pos = 0;

    uint16_t msg_id = jm_mqtt_next_msg_id(client);
    buf[pos++] = (msg_id >> 8) & 0xFF;
    buf[pos++] = msg_id & 0xFF;

    uint16_t topic_len = strlen(topic);
    buf[pos++] = (topic_len >> 8) & 0xFF;
    buf[pos++] = topic_len & 0xFF;
    memcpy(&buf[pos], topic, topic_len);
    pos += topic_len;

    for (int i = 0; i < client->sub_count; i++) {
        if (strcmp(client->subs[i].topic, topic) == 0) {
            for (int j = i; j < client->sub_count - 1; j++) {
                client->subs[j] = client->subs[j + 1];
            }
            client->sub_count--;
            break;
        }
    }

    uint16_t remaining_len = pos;
    uint8_t encoded_len[4];
    uint8_t elen = 0;
    uint32_t tmp = remaining_len;
    do {
        encoded_len[elen++] = (tmp % 128) | (tmp >= 128 ? 0x80 : 0x00);
        tmp /= 128;
    } while (tmp > 0 && elen < 4);

    uint8_t final_buf[256];
    uint16_t fpos = 0;
    final_buf[fpos++] = MQTTUNSUBSCRIBE | 0x02;
    for (int i = elen - 1; i >= 0; i--) {
        final_buf[fpos++] = encoded_len[i];
    }
    memcpy(&final_buf[fpos], buf, remaining_len);
    fpos += remaining_len;

    return jm_mqtt_send_packet(client, final_buf, fpos);
}

static int jm_mqtt_send_ping(jm_mqtt_client_t *client) {
    uint8_t buf[2] = {MQTTPINGREQ, 0x00};
    return jm_mqtt_send_packet(client, buf, 2);
}

static int jm_mqtt_send_disconnect(jm_mqtt_client_t *client) {
    uint8_t buf[2] = {MQTTDISCONNECT, 0x00};
    return jm_mqtt_send_packet(client, buf, 2);
}

static void jm_mqtt_process_packet(jm_mqtt_client_t *client, const uint8_t *data, uint16_t len) {
    if (len < 2) return;

    uint8_t type = data[0] & 0xF0;
    uint8_t qos = (data[0] & 0x06) >> 1;

    uint32_t multiplier = 1;
    uint32_t remaining_len = 0;
    uint16_t elen = 1;
    uint8_t digit;

    do {
        if (elen >= len) return;
        digit = data[elen++];
        remaining_len += (digit & 0x7F) * multiplier;
        multiplier <<= 7;
    } while ((digit & 0x80) != 0);

    uint16_t pos = elen;

    client->last_activity = jm_mqtt_get_time_ms();

    switch (type) {
        case MQTTCONNACK: {
            if (remaining_len < 2) return;
            if (data[pos + 1] == 0) {
                client->state = JM_MQTT_STATE_CONNECTED;
                client->ping_outstanding = false;
                if (client->connected_cb) client->connected_cb();
            }
            break;
        }
        case MQTTPUBLISH: {
            uint16_t topic_len = (data[pos] << 8) | data[pos + 1];
            pos += 2;
            char topic[JM_MQTT_MAX_TOPIC_LEN];
            if (topic_len >= JM_MQTT_MAX_TOPIC_LEN) topic_len = JM_MQTT_MAX_TOPIC_LEN - 1;
            memcpy(topic, &data[pos], topic_len);
            topic[topic_len] = '\0';
            pos += topic_len;

            uint16_t payload_offset = pos;
            uint16_t payload_len = remaining_len - (payload_offset - elen);

            if (qos > 0 && payload_len >= 2) {
                uint16_t msg_id = (data[payload_offset] << 8) | data[payload_offset + 1];
                uint8_t ack[4] = {MQTTPUBACK, 0x02, (msg_id >> 8) & 0xFF, msg_id & 0xFF};
                jm_mqtt_send_packet(client, ack, 4);
            }

            if (client->message_cb) {
                const uint8_t *payload = &data[payload_offset];
                uint16_t plen = payload_len;
                if (qos > 0 && plen >= 2) {
                    payload += 2;
                    plen -= 2;
                }
                client->message_cb(topic, payload, plen);
            }
            break;
        }
        case MQTTPUBACK:
        case MQTTPUBREC:
        case MQTTPUBCOMP:
        case MQTTUNSUBACK:
            break;
        case MQTTSUBACK: {
            pos += 2;
            uint8_t ret_code = data[pos];
            (void)ret_code;
            break;
        }
        case MQTTPINGRESP:
            client->ping_outstanding = false;
            break;
        default:
            break;
    }
}

int jm_mqtt_init(jm_mqtt_client_t *client, const char *client_id, uint16_t keepalive) {
    if (!client || !client_id) return JM_ERR_INVALID_PACKET;

    memset(client, 0, sizeof(jm_mqtt_client_t));
    client->sock = -1;
    client->state = JM_MQTT_STATE_DISCONNECTED;
    client->keepalive = keepalive ? keepalive : JM_MQTT_KEEPALIVE_DEFAULT;

    strncpy(client->client_id, client_id, JM_MQTT_MAX_CLIENT_ID - 1);
    client->client_id[JM_MQTT_MAX_CLIENT_ID - 1] = '\0';

    return JM_SUCCESS;
}

int jm_mqtt_connect(jm_mqtt_client_t *client, const char *host, uint16_t port, const char *user, const char *pass, const char *will_topic, const char *will_msg, uint8_t will_qos, bool will_retain, bool clean_session) {
    if (!client || !host) return JM_ERR_INVALID_PACKET;
    if (client->state == JM_MQTT_STATE_CONNECTING || client->state == JM_MQTT_STATE_CONNECTED) {
        return JM_ERR_NOT_READY;
    }

    client->state = JM_MQTT_STATE_CONNECTING;
    client->sock = -1;
    client->rx_len = 0;
    client->ping_outstanding = false;
    client->sub_count = 0;
    memset(client->subs, 0, sizeof(client->subs));

    jm_stm32_send_tcp_connect(host, port);

    uint32_t start = jm_mqtt_get_time_ms();
    const uint32_t tcp_timeout = 10000;

    while (client->sock < 0) {
        if (jm_mqtt_get_time_ms() - start > tcp_timeout) {
            client->state = JM_MQTT_STATE_DISCONNECTED;
            return JM_ERR_TIMEOUT;
        }
        jm_delay_ms(10);
    }

    int rc = jm_mqtt_send_connect(client, user, pass, will_topic, will_msg, will_qos, will_retain, clean_session);
    if (rc != JM_SUCCESS) {
        client->state = JM_MQTT_STATE_DISCONNECTED;
        return rc;
    }

    start = jm_mqtt_get_time_ms();
    const uint32_t mqtt_timeout = 10000;

    while (client->state == JM_MQTT_STATE_CONNECTING) {
        if (jm_mqtt_get_time_ms() - start > mqtt_timeout) {
            client->state = JM_MQTT_STATE_DISCONNECTED;
            return JM_ERR_TIMEOUT;
        }
        jm_delay_ms(10);
    }

    if (client->state != JM_MQTT_STATE_CONNECTED) {
        return JM_ERR_NOT_READY;
    }

    return JM_SUCCESS;
}

int jm_mqtt_disconnect(jm_mqtt_client_t *client) {
    if (!client || client->state != JM_MQTT_STATE_CONNECTED) {
        return JM_ERR_NOT_READY;
    }

    client->state = JM_MQTT_STATE_DISCONNECTING;
    jm_mqtt_send_disconnect(client);

    if (client->sock >= 0) {
        jm_stm32_send_tcp_close(client->sock);
        client->sock = -1;
    }

    client->state = JM_MQTT_STATE_DISCONNECTED;
    if (client->disconnected_cb) client->disconnected_cb();

    return JM_SUCCESS;
}

int jm_mqtt_publish(jm_mqtt_client_t *client, const char *topic, const uint8_t *payload, uint16_t len, uint8_t qos, bool retained) {
    if (!client || !topic || client->state != JM_MQTT_STATE_CONNECTED) {
        return JM_ERR_NOT_READY;
    }
    if (qos > 2) return JM_ERR_INVALID_PACKET;

    return jm_mqtt_send_publish(client, topic, payload, len, qos, retained);
}

int jm_mqtt_subscribe(jm_mqtt_client_t *client, const char *topic, uint8_t qos) {
    if (!client || !topic || client->state != JM_MQTT_STATE_CONNECTED) {
        return JM_ERR_NOT_READY;
    }
    if (qos > 1) return JM_ERR_INVALID_PACKET;

    return jm_mqtt_send_subscribe(client, topic, qos);
}

int jm_mqtt_unsubscribe(jm_mqtt_client_t *client, const char *topic) {
    if (!client || !topic || client->state != JM_MQTT_STATE_CONNECTED) {
        return JM_ERR_NOT_READY;
    }

    return jm_mqtt_send_unsubscribe(client, topic);
}

int jm_mqtt_loop(jm_mqtt_client_t *client) {
    if (!client || client->state != JM_MQTT_STATE_CONNECTED) {
        return JM_ERR_NOT_READY;
    }

    uint32_t now = jm_mqtt_get_time_ms();

    if (client->keepalive > 0) {
        if (!client->ping_outstanding && (now - client->last_activity >= client->keepalive * 1000)) {
            jm_mqtt_send_ping(client);
            client->ping_outstanding = true;
            client->last_activity = now;
        }

        if (client->ping_outstanding && (now - client->last_activity >= (client->keepalive * 1000 + 5000))) {
            JM_LOG_E("MQTT ping timeout");
            client->state = JM_MQTT_STATE_DISCONNECTED;
            if (client->disconnected_cb) client->disconnected_cb();
            return JM_ERR_TIMEOUT;
        }
    }

    return JM_SUCCESS;
}

void jm_mqtt_on_tcp_data(jm_mqtt_client_t *client, const uint8_t *data, uint16_t len) {
    if (!client || !data || len == 0) return;

    if (client->state != JM_MQTT_STATE_CONNECTED && client->state != JM_MQTT_STATE_CONNECTING) {
        return;
    }

    uint16_t remaining = JM_MQTT_RX_BUF_SIZE - client->rx_len;
    if (remaining == 0) {
        client->rx_len = 0;
        remaining = JM_MQTT_RX_BUF_SIZE;
    }

    uint16_t copy_len = len;
    if (copy_len > remaining) copy_len = remaining;

    memcpy(&client->rx_buf[client->rx_len], data, copy_len);
    client->rx_len += copy_len;

    while (client->rx_len >= 2) {
        uint8_t type = client->rx_buf[0] & 0xF0;
        uint32_t multiplier = 1;
        uint32_t remaining_len = 0;
        uint16_t elen = 1;
        uint8_t digit;

        do {
            if (elen >= client->rx_len) break;
            digit = client->rx_buf[elen++];
            remaining_len += (digit & 0x7F) * multiplier;
            multiplier <<= 7;
        } while ((digit & 0x80) != 0);

        uint32_t total_len = elen + remaining_len;
        if (client->rx_len < total_len) break;

        jm_mqtt_process_packet(client, client->rx_buf, (uint16_t)total_len);

        if (client->rx_len > total_len) {
            memmove(client->rx_buf, &client->rx_buf[total_len], client->rx_len - total_len);
        }
        client->rx_len -= (uint16_t)total_len;
    }
}

void jm_mqtt_on_tcp_connected(jm_mqtt_client_t *client, int8_t sock) {
    if (!client) return;

    if (client->state == JM_MQTT_STATE_CONNECTING) {
        client->sock = sock;
        client->last_activity = jm_mqtt_get_time_ms();
        client->ping_outstanding = false;
        client->rx_len = 0;
        memset(client->rx_buf, 0, sizeof(client->rx_buf));
    }
}

void jm_mqtt_on_tcp_disconnected(jm_mqtt_client_t *client, int8_t sock) {
    if (!client) return;

    if (client->sock == sock) {
        client->sock = -1;
        client->state = JM_MQTT_STATE_DISCONNECTED;
        client->ping_outstanding = false;
        client->rx_len = 0;
        if (client->disconnected_cb) client->disconnected_cb();
    }
}

void jm_mqtt_on_tcp_error(jm_mqtt_client_t *client, int8_t err_code) {
    if (!client) return;

    (void)err_code;

    if (client->state == JM_MQTT_STATE_CONNECTED || client->state == JM_MQTT_STATE_CONNECTING) {
        client->sock = -1;
        client->state = JM_MQTT_STATE_DISCONNECTED;
        client->ping_outstanding = false;
        client->rx_len = 0;
        if (client->disconnected_cb) client->disconnected_cb();
    }
}



static void jm_mqtt_event_listener(jm_event_t *evt) {
    if (!g_mqtt_initialized) return;

    switch (evt->type) {
        case JM_EVENT_TCP_CONNECTED: {
            jm_tcp_conn_info_t *conn = (jm_tcp_conn_info_t *)evt->data;
            if (conn) {
                jm_mqtt_on_tcp_connected(&g_mqtt_client, conn->sock);
            }
            break;
        }
        case JM_EVENT_TCP_DISCONNECTED: {
            jm_tcp_conn_info_t *conn = (jm_tcp_conn_info_t *)evt->data;
            if (conn) {
                jm_mqtt_on_tcp_disconnected(&g_mqtt_client, conn->sock);
            }
            break;
        }
        case JM_EVENT_TCP_ERROR: {
            jm_tcp_conn_info_t *conn = (jm_tcp_conn_info_t *)evt->data;
            if (conn) {
                jm_mqtt_on_tcp_error(&g_mqtt_client, conn->err_code);
            }
            break;
        }
        case JM_EVENT_TCP_DATA: {
            jm_buf_t *buf = (jm_buf_t *)evt->data;
            if (buf) {
                uint16_t n = jm_buf_readable_len(buf);
                const uint8_t *data = jm_buf_read_buf(buf);
                jm_mqtt_on_tcp_data(&g_mqtt_client, data, n);
            }
            break;
        }
        default:
            break;
    }
}

void jm_mqtt_proxy_init(const jm_config_t *config) {
    g_mqtt_config = config;
    jm_mqtt_init(&g_mqtt_client, "stm32_client", JM_MQTT_KEEPALIVE_DEFAULT);
    g_mqtt_initialized = true;

    jm_stm32_regEventListener(JM_EVENT_TCP_CONNECTED, jm_mqtt_event_listener);
    jm_stm32_regEventListener(JM_EVENT_TCP_DISCONNECTED, jm_mqtt_event_listener);
    jm_stm32_regEventListener(JM_EVENT_TCP_ERROR, jm_mqtt_event_listener);
    jm_stm32_regEventListener(JM_EVENT_TCP_DATA, jm_mqtt_event_listener);

    JM_LOG_D("MQTT proxy init done");
}

void jm_mqtt_proxy_loop(void) {
    if (!g_mqtt_initialized) return;
    jm_mqtt_loop(&g_mqtt_client);
}

jm_mqtt_client_t *jm_mqtt_get_client(void) {
    if (g_mqtt_initialized) {
        return &g_mqtt_client;
    }
    return NULL;
}

#endif //#if JM_MQTT_PROXY_ENABLE
