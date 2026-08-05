#ifndef JM_STM32_GPIO_H_
#define JM_STM32_GPIO_H_

#include "jm_stm32.h"

#ifdef __cplusplus
extern "C" {
#endif

 void jm_gpio_init(void);


#if JM_STM32_TESTEVENT_ENABLE==1
    void jm_test_event_init(void);
#endif


#if JM_STM32_TESTTCP_ENABLE
void jm_tcp_test_init(const jm_config_t *config);
void jm_tcp_test_loop(void);
void jm_tcp_test_on_event(uint8_t event_type, void *data);
#endif /* JM_STM32_TESTTCP_ENABLE */

#if JM_STM32_TESTUDP_ENABLE
void jm_udp_test_init(const jm_config_t *config);
void jm_udp_test_loop(void);
void jm_udp_test_on_event(uint8_t event_type, jm_buf_t *data);
#endif /* JM_STM32_TESTUDP_ENABLE */

#if JM_MQTT_PROXY_ENABLE
#include "jm_mqtt_proxy.h"
void jm_mqtt_proxy_init(const jm_config_t *config);
void jm_mqtt_proxy_loop(void);
void jm_mqtt_test_init(const jm_config_t *config);
void jm_mqtt_test_loop(void);
#endif

#if JM_MQTT_CLIENT_ENABLE
#include "jm_mqtt_client.h"
int jm_mqtt_client_init(jm_mqtt_client_msg_cb msg_cb,
                            jm_mqtt_client_connect_cb connect_cb,
                            jm_mqtt_client_disconnect_cb disconnect_cb);
void jm_mqtt_client_loop(void);
void jm_mqtt_client_test_init(const jm_config_t *config);
void jm_mqtt_client_test_loop(void);
#endif /* JM_MQTT_CLIENT_ENABLE */

#ifdef __cplusplus
}
#endif

#endif /* JM_STM32_GPIO_H_ */
