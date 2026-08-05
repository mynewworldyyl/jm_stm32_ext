#ifndef JMICRO_JM_PCfG_H_
#define JMICRO_JM_PCfG_H_

//日志模块
#define JM_LOG_DEBUG_ENABLE 1
#define JM_LOG_ERROR_ENABLE 1

//启用事件模块
#define JM_STM32_EVENT_ENABLE 0

//启用中断模块
#define JM_STM32_INTERRUPT_ENABLE 0

//启用音调模块
#define JM_TONE_ENABLE 0

//启用全局中断控制API（noInterrupts/interrupts）
#define JM_STM32_INTERRUPTS_ENABLE 0

//启用AT24CXX EEPROM I2C操作
#define JM_AT24CXX_ENABLE 0

//启用I2C包装器（通用I2C主从操作）
#define JM_I2C_WRAPPER_ENABLE 0

//==========================以下测试模块宏开始==========================
//启用中断模块
#define JM_STM32_TESTEVENT_ENABLE 0

//启用TCP测试模块
#define JM_STM32_TESTTCP_ENABLE 0

//启用UDP测试模块
#define JM_STM32_TESTUDP_ENABLE 0

//启用MQTT代理模块,通过本地的TCP实现，相比JM_MQTT_CLIENT_ENABLE太费内存不建议使用
#define JM_MQTT_PROXY_ENABLE 0

//简化MQTT代理实现
#define JM_MQTT_CLIENT_ENABLE 1

//==========================测试模块宏结束==========================

#endif //JMICRO_JM_PCfG_H_
