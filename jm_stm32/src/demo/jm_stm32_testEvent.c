#include "jm_stm32.h"
#include "jm_stm32_com.h"

//接收事件测试模块，使用JM_STM32_TESTEVENT_ENABLE做开关
#if JM_STM32_TESTEVENT_ENABLE==1

static void _jm_test_event_lis(jm_event_t *evt) {
   JM_LOG_D("_jm_test_event_lis t=%d st=%d f=%d",evt->type, evt->subType, evt->flag);
}

void jm_test_event_init(void) {
   jm_stm32_regEventListener(40, _jm_test_event_lis);
}

#endif // JM_STM32_TESTEVENT_ENABLE==1
