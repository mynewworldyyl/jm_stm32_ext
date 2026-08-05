#include "jm_stm32.h"
#include "jm_stm32_com.h"

void jm_comp_init(const jm_config_t *config) {
    JM_LOG_D("jm_comp_initB");
#if JM_GPIO_CTRL_ENABLE==1
    jm_gpio_init();
#endif

#if JM_STM32_TESTEVENT_ENABLE==1
    jm_test_event_init();
#endif

#if JM_STM32_TESTTCP_ENABLE
    jm_tcp_test_init(config);
#endif

#if JM_STM32_TESTUDP_ENABLE
    jm_udp_test_init(config);
#endif

#if JM_MQTT_PROXY_ENABLE
    jm_mqtt_proxy_init(config);
    jm_mqtt_test_init(config);
#endif

#if JM_MQTT_CLIENT_ENABLE
    jm_mqtt_client_test_init(config);
#endif

}

void jm_comp_loop(void) {
#if JM_GPIO_CTRL_ENABLE==1
    //jm_gpio_loop();
#endif

#if JM_STM32_TESTTCP_ENABLE
        jm_tcp_test_loop();
#endif

#if JM_STM32_TESTUDP_ENABLE
        jm_udp_test_loop();
#endif

#if JM_MQTT_PROXY_ENABLE
    jm_mqtt_proxy_loop();
    jm_mqtt_test_loop();
#endif

#if JM_MQTT_CLIENT_ENABLE
    //jm_mqtt_client_loop();
    jm_mqtt_client_test_loop();
#endif

}

