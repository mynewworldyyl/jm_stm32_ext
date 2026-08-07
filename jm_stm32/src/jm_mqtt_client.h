#ifndef JM_MQTT_CLIENT_H_
#define JM_MQTT_CLIENT_H_

#include "jm_stm32.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#if JM_MQTT_CLIENT_ENABLE

#ifdef __cplusplus
extern "C" {
#endif

#define JM_MQTT_CLIENT_MAX_TOPIC_LEN    128
#define JM_MQTT_CLIENT_MAX_PAYLOAD_LEN  256
#define JM_MQTT_CLIENT_MAX_CLIENT_ID    64
#define JM_MQTT_CLIENT_MAX_BROKER_LEN   64
#define JM_MQTT_CLIENT_RX_BUF_SIZE      512

#define JM_MQTT_CLIENT_CMD_CONNECT      1
#define JM_MQTT_CLIENT_CMD_PUBLISH      2
#define JM_MQTT_CLIENT_CMD_SUBSCRIBE    3
#define JM_MQTT_CLIENT_CMD_UNSUBSCRIBE  4
#define JM_MQTT_CLIENT_CMD_DISCONNECT   5

#define JM_MQTT_CLIENT_RSP_CONNECT      1
#define JM_MQTT_CLIENT_RSP_PUBLISH      2
#define JM_MQTT_CLIENT_RSP_SUBSCRIBE    3
#define JM_MQTT_CLIENT_EVT_MESSAGE      4
#define JM_MQTT_CLIENT_EVT_DISCONNECTED 5

typedef void (*jm_mqtt_client_msg_cb)(const char *topic, const uint8_t *payload, uint16_t len);
typedef void (*jm_mqtt_client_connect_cb)(void);
typedef void (*jm_mqtt_client_disconnect_cb)(void);

int jm_mqtt_client_init(jm_mqtt_client_msg_cb msg_cb,
                         jm_mqtt_client_connect_cb connect_cb,
                         jm_mqtt_client_disconnect_cb disconnect_cb);
int jm_mqtt_client_connect(const char *broker_host, uint16_t broker_port,
                            const char *client_id, uint16_t keepalive,
                            const char *username, const char *password);
int jm_mqtt_client_publish(const char *topic, const uint8_t *payload, uint16_t len,
                            uint8_t qos, bool retained);
int jm_mqtt_client_subscribe(const char *topic, uint8_t qos);
int jm_mqtt_client_unsubscribe(const char *topic);
int jm_mqtt_client_disconnect(void);
void jm_mqtt_client_loop(void);
bool jm_mqtt_client_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif //#if JM_MQTT_CLIENT_ENABLE

#endif /* JM_MQTT_CLIENT_H_ */