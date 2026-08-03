#include "jm_stm32.h"
#include "jm_stm32_com.h"

void jm_comp_init(void) {
#if JM_GPIO_CTRL_ENABLE==1
    jm_gpio_init();
#endif
}


void jm_comp_loop(void) {

}

