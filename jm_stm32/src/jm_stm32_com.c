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
 * - HTTP 客户端 (@ref JM_HTTP_CLIENT_ENABLE)
 * - OLED 显示测试 (@ref JM_STM32_TESTOLED_ENABLE)
 */

#include "jm_stm32.h"
#include "jm_stm32_com.h"

#if JM_HTTP_CLIENT_ENABLE
#include "jm_http_client.h"
#endif

#if JM_HTTP_CLIENT_TEST_ENABLE
#include "demo/jm_stm32_http_client_test.h"
#endif

#if JM_OLED_ENABLE
#include "oled/fm_api_oled.h"
#endif


/**
 * @brief 初始化所有启用的组件，根据实际需要在此增加模块的初始化入口
 * @param config jm_stm32 配置结构
 */
void jm_comp_init(const jm_config_t *config) {
    JM_LOG_LINE("comp_init_start");
#if JM_GPIO_CTRL_ENABLE==1
    JM_LOG_LINE("gpio_init");
    jm_gpio_init(); // GPIO操作
#endif

#if JM_STM32_TESTEVENT_ENABLE==1
    JM_LOG_LINE("event_init");
    jm_test_event_init(); //JM事件测试
#endif

#if JM_STM32_TESTTCP_ENABLE
    JM_LOG_LINE("tcp_init");
    jm_tcp_test_init(config);//TCP测试模块
#endif

#if JM_STM32_TESTUDP_ENABLE
    JM_LOG_LINE("udp_init");
    jm_udp_test_init(config);//UDP测试模块
#endif

#if JM_MQTT_CLIENT_ENABLE
    JM_LOG_LINE("mqtt_init");
    jm_mqtt_client_test_init(config);//MQTT模块初始化
#endif

#if JM_HTTP_CLIENT_TEST_ENABLE
    JM_LOG_LINE("http_init");
    jm_http_client_test_init(config);//HTTP测试模块初始化
#endif

#if JM_STM32_TESTOLED_ENABLE
    JM_LOG_LINE("oled_test_init");
    jm_oled_test_init();//OLED测试模块初始化
#endif

#if JM_OLED_ENABLE
    JM_LOG_LINE("oled_ctrl_init");
    jm_oled_ctrl_init();
    JM_LOG_LINE("oled_init");
    fm_api_oled_init();
    JM_LOG_LINE("oled_init_done");
#endif

    JM_LOG_LINE("comp_init_end");
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

#if JM_MQTT_CLIENT_ENABLE
    jm_mqtt_client_test_loop();
    jm_mqtt_client_loop();
#endif

#if JM_HTTP_CLIENT_ENABLE
    jm_http_client_loop();
#endif

#if JM_HTTP_CLIENT_TEST_ENABLE && JM_HTTP_CLIENT_ENABLE
    jm_http_client_test_loop();
#endif

#if JM_STM32_TESTOLED_ENABLE
    jm_oled_test_loop();
#endif

}

