/**
 * @file jm_stm32_oled_test.c
 * @brief OLED 显示屏测试/示例模块
 *
 * 本模块演示如何使用 jm_stm32 的 OLED 显示屏功能。
 * 通过调用 OLED 显示函数，可以在 OLED 屏幕上显示文字、数字等内容。
 *
 * @section 集成步骤
 * 1. 在 `jm_pcfg.h` 中启用 OLED 模块及本测试模块：
 *    @code
 *    #define JM_OLED_ENABLE 1
 *    #define JM_STM32_TESTOLED_ENABLE 1  // 启用本测试模块
 *    @endcode
 *
 * 2. 将本文件添加到工程源文件列表。
 *
 * 3. 在 `main.c` 的主循环中调用 `@ref jm_oled_test_loop`，
 *    以更新 OLED 显示内容。
 *
 * @section 使用说明
 * - 初始化时自动调用 `@ref jm_oled_test_init`，OLED 屏幕显示 "JMicrov"
 * - 主循环中调用 `@ref jm_oled_test_loop` 保持显示更新
 */

#include "jm_stm32.h"
#include "oled/OLED.h"

#if defined(USE_HAL_UART)
#include "stm32f1xx_hal.h"
#endif

#if JM_STM32_TESTOLED_ENABLE==1

/**
 * @brief OLED 测试模块初始化
 *
 * 初始化 OLED 显示屏并显示 "JMicrov"。
 * 由 `jm_comp_init()` 在启用时自动调用。
 */
void jm_oled_test_init(void) {
    JM_LOG_D("jm_oled_test_init")
    OLED_Init();
    OLED_Clear();
    OLED_ShowString(1, 1, "JMicrov");
}

/**
 * @brief OLED 测试模块轮询
 *
 * 在主循环中周期性调用，用于更新 OLED 显示内容。
 * 当前保持显示不变，可按需扩展动态内容。
 */
void jm_oled_test_loop(void) {
   
    static uint32_t cnt = 0;
    if (++cnt >= 500) {
        cnt = 0;
        OLED_ShowString(1, 1, "JMicrov");
        //HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    }
    
}

#endif // JM_STM32_TESTOLED_ENABLE==1
