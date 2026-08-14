/**
 * @file jm_stm32_oled_test.c
 * @brief OLED 显示测试/示例模块
 *
 * 本模块演示如何使用 jm_stm32 库驱动 SSD1306 OLED 显示屏，
 * 在屏幕上显示 "JMicro" 字符串并绘制图形。
 *
 * @section 集成步骤
 * 1. 在 `jm_pcfg.h` 中启用必要的模块：
 *    @code
 *    #define JM_OLED_ENABLE 1             // 必须启用 OLED 模块
 *    #define JM_STM32_TESTOLED_ENABLE 1   // 启用本测试模块
 *    @endcode
 *
 * 2. 将本文件添加到工程源文件列表。
 *
 * 3. 确保 `platformio.ini` 中包含 OLED 头文件路径：
 *    @code
 *    build_flags =
 *        -I../jm_stm32/src/oled
 *    @endcode
 *
 * 4. 系统初始化后自动调用 `jm_oled_test_init()`，
 *    在主循环中调用 `jm_oled_test_loop()` 轮询。
 *
 * @section 硬件连接
 * SSD1306 OLED   | STM32F103
 * VCC           | 3.3V
 * GND           | GND
 * SCL           | PB6 (I2C1_SCL)
 * SDA           | PB7 (I2C1_SDA)
 *
 * @section 使用说明
 * - 程序启动后 OLED 初始化并显示 "JMicro" 字样
 * - 屏幕上同时显示当前的系统运行毫秒计数
 * - 每秒刷新一次显示内容
 */

#include "jm_stm32.h"

#if JM_STM32_TESTOLED_ENABLE==1

#include "ssd1306.h"
#include "fonts.h"
#include <stdio.h>

/** @brief 刷新显示的间隔（毫秒） */
#define OLED_TEST_REFRESH_MS 1000

/** @brief 静态变量标记OLED是否已初始化 */
static uint8_t oled_initialized = 0;

/**
 * @brief OLED 测试模块初始化
 *
 * 初始化 SSD1306 OLED，清屏并在屏幕上显示 "JMicro"。
 * 由 `jm_comp_init()` 在启用时自动调用。
 *
 * @param config jm_stm32 配置结构（用于获取系统时间回调）
 */
void jm_oled_test_init(const jm_config_t *config)
{
    (void)config;

    /* 初始化 OLED */
    uint8_t ret = SSD1306_Init();
    if (ret == 0) {
        JM_LOG_E("OLED init failed!");
        return;
    }

    oled_initialized = 1;

    /* 清屏 */
    SSD1306_Fill(SSD1306_COLOR_BLACK);
    SSD1306_UpdateScreen();

    /* 显示标题 */
    SSD1306_GotoXY(0, 0);
    SSD1306_Puts("JMicro", &Font_11x18, SSD1306_COLOR_WHITE);

    /* 显示副标题 */
    SSD1306_GotoXY(0, 20);
    SSD1306_Puts("OLED Test", &Font_7x10, SSD1306_COLOR_WHITE);

    SSD1306_UpdateScreen();

    JM_LOG_D("OLED test init done");
}

/**
 * @brief OLED 测试模块轮询
 *
 * 每隔 OLED_TEST_REFRESH_MS 毫秒刷新一次屏幕上的系统时间。
 * 应在主循环中周期性调用。
 */
void jm_oled_test_loop(void)
{
    /*
    if (!oled_initialized) return;

    static uint32_t last_ms = 0;
    uint32_t now = jm_stm32_get_time();

    if (now - last_ms >= OLED_TEST_REFRESH_MS) {
        last_ms = now;

      
        char buf[32];
        snprintf(buf, sizeof(buf), "Time: %lu ms", (unsigned long)now);

        SSD1306_GotoXY(0, 40);
        SSD1306_Puts("                ", &Font_7x10, SSD1306_COLOR_WHITE);
        SSD1306_GotoXY(0, 40);
        SSD1306_Puts(buf, &Font_7x10, SSD1306_COLOR_WHITE);
        SSD1306_UpdateScreen();

        JM_LOG_D("OLED display updated: %s", buf);
    }
    */
}

#endif // JM_STM32_TESTOLED_ENABLE==1
