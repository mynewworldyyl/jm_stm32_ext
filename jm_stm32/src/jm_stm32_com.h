#ifndef JM_STM32_GPIO_H_
#define JM_STM32_GPIO_H_

#include "jm_stm32.h"

#ifdef __cplusplus
extern "C" {
#endif

 void jm_gpio_init(void);


#if JM_STM32_TESTEVENT_ENABLE==1
    void jm_test_event_init(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* JM_STM32_GPIO_H_ */
