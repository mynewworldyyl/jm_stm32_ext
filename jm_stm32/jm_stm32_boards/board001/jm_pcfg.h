/**
 * @file jm_pcfg.h
 * @brief jm_stm32 库配置文件
 *
 * 定义功能模块开关宏。0 关闭，1 开启。
 * 复制本文件到项目 include 路径下或修改此文件以自定义功能。
 */
#ifndef JMICRO_JM_PCfG_H_
#define JMICRO_JM_PCfG_H_

/** @brief 调试日志输出开关（1=开启 USART2 输出日志） */
#define JM_LOG_DEBUG_ENABLE 1

/** @brief 错误日志输出开关 */
#define JM_LOG_ERROR_ENABLE 1


/* ===================== 基础模块 ===================== */


/** @brief GPIO 控制接口开关 */
#define JM_GPIO_CTRL_ENABLE 1

/** @brief 异步事件模块开关 */
#define JM_STM32_EVENT_ENABLE 0

/** @brief 引脚中断模块开关 */
#define JM_STM32_INTERRUPT_ENABLE 0

/** @brief 音调/PWM/移位/脉冲检测模块开关 */
#define JM_APIS_ENABLE 0

/** @brief AT24CXX EEPROM I2C 操作开关 */
#define JM_AT24CXX_ENABLE 0

/** @brief 通用 I2C 主从操作包装器开关 */
#define JM_I2C_WRAPPER_ENABLE 0

/** @brief 简化 MQTT 客户端开关 */
#define JM_MQTT_CLIENT_ENABLE 0

/** @brief TCP开关 */
#define JM_STM32_TCP_ENABLE 0

/** @brief UDP开关 */
#define JM_STM32_UDP_ENABLE 0

/** @brief 简化 HTTP 客户端开关 */
#define JM_HTTP_CLIENT_ENABLE 1

/** @brief OLED屏幕 */
#define JM_OLED_ENABLE 1

/* ===================== 测试模块 ===================== */

/** @brief 事件测试模块开关 */
#define JM_STM32_TESTEVENT_ENABLE 0

/** @brief TCP 测试模块开关 */
#define JM_STM32_TESTTCP_ENABLE 0

/** @brief UDP 测试模块开关 */
#define JM_STM32_TESTUDP_ENABLE 0

/** @brief MQTT 客户端测试模块开关 */
#define JM_MQTT_CLIENT_TEST_ENABLE 0

/** @brief HTTP 客户端测试模块开关 */
#define JM_HTTP_CLIENT_TEST_ENABLE 1

 // 启用 OLED 测试
#define JM_STM32_TESTOLED_ENABLE 1 

#endif //JMICRO_JM_PCfG_H_
