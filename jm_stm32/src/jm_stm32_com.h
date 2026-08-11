/**
 * @file jm_stm32_com.h
 * @brief 组件头文件 — GPIO、测试模块、MQTT 代理与 MQTT 客户端
 *
 * 该头文件声明了各个功能组件的初始化与轮询接口，
 * 这些组件由 @ref jm_comp_init 和 @ref jm_comp_loop 调用。
 */

#ifndef JM_STM32_GPIO_H_
#define JM_STM32_GPIO_H_

#include "jm_stm32.h"

#ifdef __cplusplus
extern "C" {
#endif


/** =========================GPIO开始============================== */
#if JM_GPIO_CTRL_ENABLE == 1

/**
 * @brief GPIO 控制初始化
 * 注册 remote_ctrlGpio 控制命令（defId=53）。
 */
void jm_gpio_init(void);

#endif //#if JM_GPIO_CTRL_ENABLE == 1

#if JM_STM32_TESTEVENT_ENABLE == 1

/**
 * @brief 事件测试模块初始化
 */
void jm_test_event_init(void);

#endif //#if JM_STM32_TESTEVENT_ENABLE == 1
/** =========================GPIO结束============================= */

/** =========================TCP开始============================== */
#if JM_STM32_TESTTCP_ENABLE

/**
 * @brief TCP 测试模块初始化
 * @param config jm_stm32 配置结构
 */
void jm_tcp_test_init(const jm_config_t *config);

/**
 * @brief TCP 测试模块轮询
 */
void jm_tcp_test_loop(void);

/**
 * @brief TCP 测试模块事件处理
 * @param event_type 事件类型
 * @param data       事件数据
 */
void jm_tcp_test_on_event(uint8_t event_type, void *data);

#endif /* JM_STM32_TESTTCP_ENABLE */
/** =========================TCP结束============================== */



/** =========================UDP开始============================== */
#if JM_STM32_TESTUDP_ENABLE

/**
 * @brief UDP 测试模块初始化
 * @param config jm_stm32 配置结构
 */
void jm_udp_test_init(const jm_config_t *config);

/**
 * @brief UDP 测试模块轮询
 */
void jm_udp_test_loop(void);

/**
 * @brief UDP 测试模块事件处理
 * @param event_type 事件类型
 * @param data       缓冲区数据
 */
void jm_udp_test_on_event(uint8_t event_type, jm_buf_t *data);

#endif /* JM_STM32_TESTUDP_ENABLE */
/** =========================UDP结束============================== */


/** =========================HTTP开始============================== */



#if JM_HTTP_CLIENT_TEST_ENABLE && JM_MQTT_CLIENT_ENABLE

/**
 * @brief HTTP 客户端测试模块初始化
 * @param config jm_stm32 配置结构
 */
void jm_http_client_test_init(const jm_config_t *config);

/**
 * @brief HTTP 客户端测试模块轮询
 */
void jm_http_client_test_loop(void);


#endif /* JM_HTTP_CLIENT_TEST_ENABLE && JM_MQTT_CLIENT_ENABLE */

/** ==========================HTTP结束============================== */



/** =========================MQTT开始============================== */

#if JM_MQTT_CLIENT_ENABLE 

#include "jm_mqtt_client.h"

/**
 * @brief 初始化 MQTT 客户端
 * @param msg_cb      消息到达回调
 * @param connect_cb  连接成功回调
 * @param disconnect_cb 断开连接回调
 * @return @ref JM_SUCCESS 成功
 */
int jm_mqtt_client_init(jm_mqtt_client_msg_cb msg_cb,
                        jm_mqtt_client_connect_cb connect_cb,
                        jm_mqtt_client_disconnect_cb disconnect_cb);

/**
 * @brief MQTT 客户端轮询（处理心跳、超时等）
 */
void jm_mqtt_client_loop(void);


//以下两个是测试用的
/**
 * @brief MQTT 客户端测试模块初始化
 * @param config jm_stm32 配置结构
 */
void jm_mqtt_client_test_init(const jm_config_t *config);

/**
 * @brief MQTT 客户端测试模块轮询
 */
void jm_mqtt_client_test_loop(void);

#endif /* JM_MQTT_CLIENT_ENABLE */

/** =========================MQTT结束============================== */

#if JM_STM32_TESTOLED_ENABLE==1 && JM_OLED_ENABLE==1
void jm_oled_test_init(void);
void jm_oled_test_loop(void);
#endif //#if JM_STM32_TESTOLED_ENABLE==1

#if JM_OLED_ENABLE==1
void jm_oled_ctrl_init(void);
#endif //#if JM_OLED_ENABLE==1

#ifdef __cplusplus
}
#endif

#endif /* JM_STM32_GPIO_H_ */
