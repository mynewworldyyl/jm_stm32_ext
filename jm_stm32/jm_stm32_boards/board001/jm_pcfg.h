#ifndef JMICRO_JM_PCfG_H_
#define JMICRO_JM_PCfG_H_

#ifdef DEBUG
#undef DEBUG
#endif

#define JM_USE_IDLE 0
#define JM_ESP01_RX_FUN 0

//芯片类型码
#define JM_SHIP_TYPE 1
#define JM_SHIP_NAME "ESP8266"
#define JM_ESP8266 1

//固件ID,每个固件可以有多个版本
#define JM_FIRM_ID 27

//构建版本,整数递增

#define JM_FIRM_VER 158

//谁创建的固件
#define JM_FIRM_ACTID 255

//固件价格
#define JM_FIRM_PRICE 0

//账号所属产品
#define JM_PRODUCT_ID 1

//指定账号的固件
#define JM_OWNER_ID 0

//固件类型码
#define BOARD_TYPE 2


// JM_ESPR 在jm_espr_cfg.h中定义
#ifndef JM_KEY
#define JM_KEY -1
#endif
#ifndef JM_IR_RECV_PIN
#define JM_IR_RECV_PIN      -1
#endif
#ifndef JM_IR_SEND_PIN
#define JM_IR_SEND_PIN      -1
#endif
#ifndef JM_SWITCH_SET
#define JM_SWITCH_SET       -1

#endif
#ifndef JM_SWITCH_RESET
#define JM_SWITCH_RESET       -1
#endif


#define JM_NETPROXY 1
#define JM_USE_ARDUINO_HTTP 1
#define JM_HTTP 1

//# 0:非测试设备， 1:测试设备
#define TESTING_DEV 0

#include "jm_sensor.h"

#define JM_BOARD_NAME "ESP8266网卡"

#if JM_KEY > -1
//是否使用定时器讲时按键事件,0不使用，1使用
#define JM_KEY_TIMER 0 
#define JM_ADC_KEY 0
#else
#define JM_ADC_KEY 0
#endif

//按键事件交由jm_key.cpp的_jm_interrupt_cb(void *pin)处理，ADC按键模块只负责调用
#ifndef JM_KEY_SIMPLE_MODE
#define JM_KEY_SIMPLE_MODE 1
#endif


#ifndef JM_ESPR
#define JM_ESPR 1
#endif

#define JM_RSA_ENABLE 0

#ifndef JM_AES_ENABLE
#define JM_AES_ENABLE 1
#endif

#ifndef DEBUG_MEMORY
#define DEBUG_MEMORY 0
#endif

//#定期打印内存信息
#ifndef PRINT_MEM_INFO
#define PRINT_MEM_INFO 1
#endif

#ifndef DEBUG_CACHE_ENABLE
#define DEBUG_CACHE_ENABLE 0
#endif

#ifndef JMICRO_DEBUG_ON
#define JMICRO_DEBUG_ON 0
#endif

#ifndef JM_NEED_LOGIN
#define JM_NEED_LOGIN 1
#endif

#ifndef JM_TCP
#define JM_TCP 0
#endif

#ifndef JM_CLOCK
#define JM_CLOCK 0
#endif

#ifndef JM_U8G2
#define JM_U8G2 0
#endif

#ifndef JM_UDP
#define JM_UDP 1
#endif

#ifndef JM_MASTER_SLAVE
#define JM_MASTER_SLAVE 1
#endif

#ifndef JLOG_ENABLE
#define JLOG_ENABLE 1
#endif

#ifndef JM_OLED
#define JM_OLED 0
#endif

#ifndef SLOG_ENABLE
#define SLOG_ENABLE 1
#endif

#ifndef JMICRO_MEM_DEBUG
#define JMICRO_MEM_DEBUG 1
#endif

#ifndef JM_RPC_ENABLE
#define JM_RPC_ENABLE 1
#endif

#ifndef JM_PS_ENABLE
#define JM_PS_ENABLE 1
#endif

#ifndef JM_HB_ENABLE
//不开启心跳，ESP32心跳由网卡负责
#define JM_HB_ENABLE 1
#endif

#ifndef JM_LOGIN_ENABLE
//ESP32登录由网卡负责
#define JM_LOGIN_ENABLE 1
#endif

#ifndef JM_ML_PROXY
#if JM_NETPROXY==1
#define JM_ML_PROXY 1
#else
#define JM_ML_PROXY 0
#endif
#endif //JM_ML_PROXY

#ifndef JM_KV_ENABLE
#define JM_KV_ENABLE 1
#endif

#ifndef JM_TIMER_ENABLE
#define JM_TIMER_ENABLE 1
#endif

#ifndef JM_ELIST_ENABLE
#define JM_ELIST_ENABLE 1
#endif

#ifndef JM_EMAP_ENABLE
#define JM_EMAP_ENABLE 1
#endif

#ifndef JM_BUF_ENABLE
#define JM_BUF_ENABLE 1
#endif

#ifndef JM_MSG_ENABLE
#define JM_MSG_ENABLE 1
#endif

#ifndef JM_STD_TIME_ENABLE
#if JM_NETPROXY==1
#define JM_STD_TIME_ENABLE 1
#else
#define JM_STD_TIME_ENABLE 1
#endif //JM_NETPROXY==1
#endif //JM_STD_TIME_ENABLE

#if JM_NETPROXY==1
#define JM_TCP_PROXY_ENABLE 1
#define JM_UDP_PROXY_ENABLE 1
#define JM_SERIAL_ENABLE 1
#else
#define JM_TCP_PROXY_ENABLE 0
#define JM_UDP_PROXY_ENABLE 0
#define JM_SERIAL_ENABLE 0
#endif

#if JM_MSG_ENABLE==1 || JM_EMAP_ENABLE==1 ||JM_ELIST_ENABLE==1
#define JM_MSG_EXTRA_ENABLE 1
#endif

#define JM_ML_DEBUG_ENABLE 0
#define JM_ML_ERROR_ENABLE 0

#define JM_CLI_DEBUG_ENABLE 0
#define JM_CLI_ERROR_ENABLE 1

#define JM_BUF_DEBUG_ENABLE 0
#define JM_BUF_ERROR_ENABLE 1

#define JM_MSG_DEBUG_ENABLE 0
#define JM_MSG_ERROR_ENABLE 0

#define JM_MEM_DEBUG_ENABLE 0
#define JM_MEM_ERROR_ENABLE 1

#define JM_STD_DEBUG_ENABLE 0
#define JM_STD_ERROR_ENABLE 0

#define JM_TCP_DEBUG_ENABLE 0
#define JM_TCP_ERROR_ENABLE 0

#define JM_SERIAL_DEBUG_ENABLE 1
#define JM_SERIAL_ERROR_ENABLE 1

#define JM_UDP_DEBUG_ENABLE 0
#define JM_UDP_ERROR_ENABLE 1
#define JM_UDP_PRO_DEBUG_ENABLE 0

#define JM_CFG_DEBUG_ENABLE 0
#define JM_CFG_ERROR_ENABLE 1

#define JM_CTRL_DEBUG_ENABLE 1
#define JM_CTRL_ERROR_ENABLE 1

#define JM_ESPRVAR_DEBUG_ENABLE 0
#define JM_ESPRVAR_ERROR_ENABLE 0

#define JM_ESPRINT_DEBUG_ENABLE 0
#define JM_ESPRINT_ERROR_ENABLE 0

#define JM_ESPRHW_DEBUG_ENABLE 0
#define JM_ESPRHW_ERROR_ENABLE 0

#define JM_ESPR_DEBUG_ENABLE 0
#define JM_ESPR_ERROR_ENABLE 1

#define JM_FS_DEBUG_ENABLE 0

#define JM_KEY_DEBUG_ENABLE 0

#define JM_UTIL_DEBUG_ENABLE 0

#define JM_SENSOR_DEBUG_ENABLE 0

#define JM_MAIN_DEBUG_ENABLE 0

#define JM_OLED_DEBUG_ENABLE 0
#define JM_OLED_ERROR_ENABLE 0

#define JM_PWM_DEBUG_ENABLE 0

#define JM_GPIO_CTRL_ENABLE 1

#endif //JMICRO_JM_PCfG_H_
