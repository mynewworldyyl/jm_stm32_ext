/**
 * @file stm32f1xx_hal_msp.c
 * @brief HAL 模式下的 MSP（中断服务/外设支持）初始化
 *
 * 仅在定义了 USE_HAL_UART 时编译。在寄存器直驱模式下不使用。
 */

#ifdef USE_HAL_UART
#include "stm32f1xx_hal.h"

/** @brief USART1 句柄（用于与 ESP8266 通信） */
UART_HandleTypeDef huart1;
/** @brief USART2 句柄（用于日志输出） */
UART_HandleTypeDef huart2;

/**
 * @brief 系统时钟配置
 *
 * HSE 晶振 8MHz，通过 PLL 倍频 9 倍 = 72MHz 系统时钟。
 * AHB 不分频 (72MHz)，APB1 2 分频 (36MHz)，APB2 不分频 (72MHz)。
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

/**
 * @brief HAL MSP 初始化
 *
 * 使能 AFIO、GPIOA、USART1、USART2 时钟。
 */
void HAL_MspInit(void)
{
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();
}

/**
 * @brief USART1 初始化（HAL 模式）
 *
 * 配置为 115200 波特率，8 数据位、1 停止位、无校验、双工模式。
 * 用于与 ESP8266 netproxy 通信。
 */
void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);
}

/**
 * @brief USART2 初始化（HAL 模式）
 *
 * 配置为 115200 波特率，8 数据位、1 停止位、无校验，仅 TX。
 * 用于日志输出，与 ESP8266 通信的 USART1 物理隔离。
 */
void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_ONLY;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
}
#endif