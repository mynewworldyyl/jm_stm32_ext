/**
 * @file jm_stm32_testEvent.c
 * @brief 异步事件系统测试/示例模块
 *
 * 本模块演示如何使用 jm_stm32 的异步事件系统。
 * 通过注册事件监听器，可以在事件发生时被动接收通知，
 * 而无需在主循环中轮询所有事件来源。
 *
 * @section 集成步骤
 * 1. 在 `jm_pcfg.h` 中启用必要的模块：
 *    @code
 *    #define JM_STM32_EVENT_ENABLE 1   // 必须启用事件系统
 *    #define JM_STM32_TESTEVENT_ENABLE 1  // 启用本测试模块
 *    @endcode
 *
 * 2. 将本文件添加到工程源文件列表。
 *
 * 3. 系统初始化后自动注册事件监听器。
 *    通过 `@ref jm_stm32_regEventListener` 可以注册更多监听器。
 *
 * 4. 在 `main.c` 的主循环中调用 `@ref jm_stm32_loop`，
 *    事件队列会自动在循环中处理。
 *
 * @section 事件类型
 * 系统支持的事件类型见 `@ref JM_EVENT_*` 常量：
 * - @ref JM_EVENT_WIFI_STATUS — WiFi 连接状态变化
 * - @ref JM_EVENT_INTERNET_STATUS — 互联网可用性变化
 * - @ref JM_EVENT_LOGIN_RESULT — 登录结果
 * - @ref JM_EVENT_TCP_CONNECTED — TCP 连接建立
 * - @ref JM_EVENT_TCP_DISCONNECTED — TCP 连接断开
 * - @ref JM_EVENT_TCP_SEND_RESULT — TCP 发送结果
 * - @ref JM_EVENT_TCP_ERROR — TCP 错误
 * - @ref JM_EVENT_TCP_DATA — TCP 数据到达
 * - @ref JM_EVENT_UDP_DATA — UDP 数据到达
 * - @ref JM_EVENT_UID_RESPONSE — 设备 ID 响应
 * - @ref JM_EVENT_SYS_CFG — 系统配置
 * - @ref JM_EVENT_TRANS_CMD — 透传命令
 * - @ref JM_EVENT_CTRL_EVENT — 控制事件
 * - @ref JM_EVENT_CTRL_CMD — 控制命令
 *
 * @section 自定义事件
 * 可以通过 `@ref jm_stm32_postEvent` 投递自定义事件（eventType=40），
 * 在监听器中接收并处理。
 *
 * @attention 事件从 ESP8266 网卡下发时，不会自动转发回网卡。
 *          若需上行事件到网卡，请在监听器中调用 @ref jm_stm32_transEventToCard。
 */

#include "jm_stm32.h"
#include "jm_stm32_com.h"

#if JM_STM32_TESTEVENT_ENABLE==1

/** @brief 事件监听器注册的事件类型（40=自定义事件） */
#define JM_TEST_EVENT_TYPE 40

/**
 * @brief 事件监听回调函数
 * @param evt 指向事件结构体
 */
static void _jm_test_event_lis(jm_event_t *evt) {
    JM_LOG_D("_jm_test_event_lis t=%d st=%d f=%d", evt->type, evt->subType, evt->flag);
}

/**
 * @brief 事件测试模块初始化
 *
 * 注册事件监听器，监听类型 40 的自定义事件。
 * 由 `jm_comp_init()` 在启用时自动调用。
 */
void jm_test_event_init(void) {
    jm_stm32_regEventListener(JM_TEST_EVENT_TYPE, _jm_test_event_lis);
}

#endif // JM_STM32_TESTEVENT_ENABLE==1
