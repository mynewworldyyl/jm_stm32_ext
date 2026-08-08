#ifndef JMICRO_JM_PCfG_H_
#define JMICRO_JM_PCfG_H_

//日志模块
#define JM_LOG_DEBUG_ENABLE 1
#define JM_LOG_ERROR_ENABLE 1

//GPIO基本操作接口
#define JM_GPIO_CTRL_ENABLE 1

//启用事件模块
#define JM_STM32_EVENT_ENABLE 1

//启用中断模块
#define JM_STM32_INTERRUPT_ENABLE 1

//启用音调模块
#define JM_TONE_ENABLE 1

//启用AT24CXX EEPROM I2C操作
#define JM_AT24CXX_ENABLE 0

//启用I2C包装器（通用I2C主从操作）
#define JM_I2C_WRAPPER_ENABLE 0

//简化MQTT代理实现
#define JM_MQTT_CLIENT_ENABLE 1

//==========================以下测试模块宏开始==========================
//启用中断模块
#define JM_STM32_TESTEVENT_ENABLE 0

//启用TCP测试模块
#define JM_STM32_TESTTCP_ENABLE 0

//启用UDP测试模块
#define JM_STM32_TESTUDP_ENABLE 0

//启用MQTT 测试模块
#define JM_MQTT_CLIENT_TEST_ENABLE 1

//==========================测试模块宏结束==========================

#endif //JMICRO_JM_PCfG_H_
