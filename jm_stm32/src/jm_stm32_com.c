/**
 * @file jm_stm32_com.c
 * @brief 组件初始化与轮询实现
 *
 * 根据 jm_pcfg.h 中的宏配置，初始化并轮询各个功能组件：
 * - GPIO 控制 (@ref JM_GPIO_CTRL_ENABLE)
 * - 事件测试 (@ref JM_STM32_TESTEVENT_ENABLE)
 * - TCP 测试 (@ref JM_STM32_TESTTCP_ENABLE)
 * - UDP 测试 (@ref JM_STM32_TESTUDP_ENABLE)
 * - MQTT 代理 (@ref JM_MQTT_PROXY_ENABLE)
 * - MQTT 客户端 (@ref JM_MQTT_CLIENT_ENABLE)
 */

#include "jm_stm32.h"
#include "jm_stm32_com.h"

/**
 * @brief 初始化所有启用的组件，根据实际需要在此增加模块的初始化入口
 * @param config jm_stm32 配置结构
 */
void jm_comp_init(const jm_config_t *config) {
    JM_LOG_D("jm_comp_initB");
#if JM_GPIO_CTRL_ENABLE==1
    jm_gpio_init(); // GPIO操作
#endif

#if JM_STM32_TESTEVENT_ENABLE==1
    jm_test_event_init(); //JM事件测试
#endif

#if JM_STM32_TESTTCP_ENABLE
    jm_tcp_test_init(config);//TCP测试模块
#endif

#if JM_STM32_TESTUDP_ENABLE
    jm_udp_test_init(config);//UDP测试模块
#endif

#if JM_MQTT_CLIENT_ENABLE
    jm_mqtt_client_test_init(config);//MQTT模块初始化
#endif

}

/**
 * @brief 轮询所有启用的组件
 *并不是每个模块都需要在此增加轮询，按需增加即可
 * 在主循环中调用，驱动各组件的状态机。
 */
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

