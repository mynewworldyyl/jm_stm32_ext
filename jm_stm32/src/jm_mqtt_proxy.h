#ifndef JM_MQTT_PROXY_H_
#define JM_MQTT_PROXY_H_

#include "jm_stm32.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#if JM_MQTT_PROXY_ENABLE

#ifdef __cplusplus
extern "C" {
#endif

#define JM_MQTT_MAX_TOPIC_LEN      128
#define JM_MQTT_MAX_PAYLOAD_LEN    256
#define JM_MQTT_MAX_CLIENT_ID      64
#define JM_MQTT_MAX_USER_LEN       64
#define JM_MQTT_MAX_PASS_LEN       64
#define JM_MQTT_MAX_WILL_LEN       128
#define JM_MQTT_MAX_SUBSCRIPTIONS  8
#define JM_MQTT_RX_BUF_SIZE        512

#define JM_MQTT_PROTOCOL_VERSION   4
#define JM_MQTT_KEEPALIVE_DEFAULT  60

typedef enum {
    JM_MQTT_STATE_DISCONNECTED = 0,
    JM_MQTT_STATE_CONNECTING,
    JM_MQTT_STATE_CONNECTED,
    JM_MQTT_STATE_DISCONNECTING
} jm_mqtt_state_t;

typedef struct {
    int8_t sock;
    jm_mqtt_state_t state;
    uint16_t keepalive;
    uint16_t msg_id;
    uint8_t rx_buf[JM_MQTT_RX_BUF_SIZE];
    uint16_t rx_len;
    uint32_t last_activity;
    bool ping_outstanding;
    char client_id[JM_MQTT_MAX_CLIENT_ID];
    uint8_t sub_count;
    struct {
        char topic[JM_MQTT_MAX_TOPIC_LEN];
        uint8_t qos;
        uint16_t msg_id;
    } subs[JM_MQTT_MAX_SUBSCRIPTIONS];
    void (*message_cb)(const char *topic, const uint8_t *payload, uint16_t len);
    void (*connected_cb)(void);
    void (*disconnected_cb)(void);
} jm_mqtt_client_t;

int jm_mqtt_init(jm_mqtt_client_t *client, const char *client_id, uint16_t keepalive);
int jm_mqtt_connect(jm_mqtt_client_t *client, const char *host, uint16_t port, const char *user, const char *pass, const char *will_topic, const char *will_msg, uint8_t will_qos, bool will_retain, bool clean_session);
int jm_mqtt_disconnect(jm_mqtt_client_t *client);
int jm_mqtt_publish(jm_mqtt_client_t *client, const char *topic, const uint8_t *payload, uint16_t len, uint8_t qos, bool retained);
int jm_mqtt_subscribe(jm_mqtt_client_t *client, const char *topic, uint8_t qos);
int jm_mqtt_unsubscribe(jm_mqtt_client_t *client, const char *topic);
int jm_mqtt_loop(jm_mqtt_client_t *client);
void jm_mqtt_on_tcp_data(jm_mqtt_client_t *client, const uint8_t *data, uint16_t len);
void jm_mqtt_on_tcp_connected(jm_mqtt_client_t *client, int8_t sock);
void jm_mqtt_on_tcp_disconnected(jm_mqtt_client_t *client, int8_t sock);
void jm_mqtt_on_tcp_error(jm_mqtt_client_t *client, int8_t err_code);

#ifdef __cplusplus
}
#endif

#endif

#endif
