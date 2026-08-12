/**
 * @file main.c
 * @brief STM32 与 ESP8266 串口透传示例工程
 *
 * 通过 UART 与 ESP8266(netproxy)通信，实现 WiFi 连接、TCP/UDP/MQTT 等网络功能。
 *
 * 默认使用寄存器直驱（CMSIS）模式，无需 HAL 库。
 * 如需使用 HAL 库模式，添加 `-DUSE_HAL_UART` 编译宏即可。
 *
 * 硬件连接：
 * - PA9 (TX) / PA10 (RX) -> ESP8266 RXD / TXD  (USART1, 115200)
 * - PA2 (TX) -> USB-TTL RXD  (USART2，用于日志输出)
 * - GND 共地
 */

#include "jm_stm32.h"
#include "jm_stm32_com.h"

#if defined(USE_HAL_UART)
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal.h"
#include <stddef.h>
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
#else
#include <stm32f1xx.h>
#endif

static volatile uint32_t sys_tick_ms = 0;  /**< 系统毫秒计数器（寄存器直驱模式） */

/* ===================== 延时函数 ===================== */

/**
 * @brief 微秒级软件延时（基于 SysTick）
 *
 * 使用 SysTick 定时器进行精确延时，延时期间阻塞。
 * 注意：调用前需确保系统时钟为 72MHz。
 *
 * @param xus 延时时长（微秒），范围：0~233015
 */
void jm_delay_us(uint32_t xus)
{
	SysTick->LOAD = 72 * xus;				//设置定时器重装值
	SysTick->VAL = 0x00;					//清空当前计数值
	SysTick->CTRL = 0x00000005;				//设置时钟源为HCLK，启动定时器
	while(!(SysTick->CTRL & 0x00010000));	//等待计数到0
	SysTick->CTRL = 0x00000004;				//关闭定时器
}

/**
 * @brief 毫秒级软件延时（基于 SysTick）
 *
 * 通过多次调用 @ref jm_delay_us 实现毫秒延时。
 * @param xms 延时时长（毫秒），范围：0~4294967295
 */
void jm_delay_ms(uint32_t xms)
{
	while(xms--)
	{
		jm_delay_us(1000);
	}
}

/**
 * @brief HAL 模式下的系统时钟配置
 * @note 寄存器直驱模式下由 SystemClock_Config 实现
 */
#if defined(USE_HAL_UART)
static uint32_t get_sys_time(void)
{
    return HAL_GetTick();
}

/**
 * @brief HAL 模式下的 SysTick 中断处理
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/**
 * @brief HAL 模式下通过 UART 发送数据
 */
static void uart_send(const uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)data, len, 100);
}

/**
 * @brief HAL 模式下通过 USART2 发送日志
 */
static void uart_send_log(const uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)data, len, 100);
}
#else
#include "stm32f1xx.h"

/* ===================== 寄存器直驱模式 ===================== */

/**
 * @brief 寄存器直驱模式的 SysTick 中断，每 1ms 递增
 */
void SysTick_Handler(void)
{
    sys_tick_ms++;
}

/**
 * @brief 寄存器直驱模式下获取系统毫秒时间
 */
static uint32_t get_sys_time(void)
{
    return sys_tick_ms;
}

/**
 * @brief 寄存器直驱模式下通过 USART1 发送数据
 */
static void uart_send(const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        while (!(USART1->SR & USART_SR_TXE));
        USART1->DR = data[i];
    }
    while (!(USART1->SR & USART_SR_TC));
}

/**
 * @brief 寄存器直驱模式下通过 USART2 发送日志
 */
static void uart_send_log(const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        while (!(USART2->SR & USART_SR_TXE));
        USART2->DR = data[i];
    }
    //while (!(USART2->SR & USART_SR_TC));
}

/**
 * @brief 寄存器直驱模式下 USART2（日志）初始化
 *
 * 配置 PA2 为 TX（复用推挽输出），波特率 115200，仅 TX。
 */
static void log_uart_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    GPIOA->CRL = (GPIOA->CRL & ~(0xF << 8)) | (0xB << 8);
    USART2->BRR = 36000000 / 115200;
    USART2->CR1 = USART_CR1_TE | USART_CR1_UE;
}

/**
 * @brief 寄存器直驱模式的系统时钟配置
 *
 * 配置 HSE 晶振为时钟源，PLL 倍频 9 倍，系统时钟 72MHz。
 * APB1 预分频 2（36MHz），APB2 1 分频（72MHz）。
 */
static void SystemClock_Config(void)
{
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY)) {}
    RCC->CFGR = RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PPRE2_DIV1 | RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9;
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)) {}
    FLASH->ACR = FLASH_ACR_LATENCY_2;
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {}
}
#endif

/**
 * @brief 事件回调函数
 *
 * 处理从 ESP8266 网卡下发的各种事件，包括 WiFi 状态、登录结果、
 * TCP/UDP 数据、MQTT 消息等。
 *
 * @param event_type 事件类型（@ref JM_EVENT_*）
 * @param sub_type   子类型
 * @param data       事件数据
 * @param user_data  用户自定义数据
 */
static void on_event(uint8_t event_type, uint16_t sub_type, void *data, void *user_data)
{
    (void)sub_type;
    (void)user_data;

    switch (event_type) {
    case JM_EVENT_WIFI_STATUS: {
        jm_wifi_status_t *st = (jm_wifi_status_t *)data;
        JM_LOG_D("EVT: wifi=%d internet=%d",st->wifi_enabled, st->isLogin);
        break;
    }
    case JM_EVENT_LOGIN_RESULT: {
        jm_login_result_t *r = (jm_login_result_t *)data;
        JM_LOG_D("EVT: login=%ld uid=%u key=%s",
            (long)r->login_code, r->dev_uid, r->login_key);
        break;
    }

#if JM_STM32_TESTTCP_ENABLE   
    case JM_EVENT_TCP_CONNECTED:
    case JM_EVENT_TCP_DISCONNECTED:
    case JM_EVENT_TCP_SEND_RESULT:
    case JM_EVENT_TCP_ERROR:
    case JM_EVENT_TCP_DATA: {
        jm_onTcpEvent(event_type, data);
        break;
    }
#endif


    default:
        JM_LOG_D("NSPE: event_type=%d sub_type=%d",event_type, sub_type);
        break;
    }
}

/**
 * @brief HAL 模式下轮询读取 UART 数据
 *
 * 从 HAL UART 硬件 FIFO 中读取所有已接收的字节并推入环形缓冲区。
 * 仅当定义了 `USE_HAL_UART` 时有效。
 *
 * @param huart HAL UART 句柄指针
 * @return 0 成功，-1 失败
 */
int jm_serial_read(void *huart)
{
#if defined(USE_HAL_UART)
    UART_HandleTypeDef *uart = (UART_HandleTypeDef *)huart;
    uint8_t byte;
    uint32_t flag = __HAL_UART_GET_FLAG(uart, UART_FLAG_RXNE);
    while (flag != RESET) {
        byte = (uint8_t)(uart->Instance->DR & 0xFF);
        jm_stm32_uart_push_byte(byte);
        flag = __HAL_UART_GET_FLAG(uart, UART_FLAG_RXNE);
    }
    return 0;
#else
    (void)huart;
    return -1;
#endif
}

/**
 * @brief HAL 模式下的主函数
 *
 * 使用 HAL 库初始化系统、时钟、UART，注册回调后进入主循环。
 * 在主循环中通过 jm_serial_read 轮询读取 UART 数据。
 */
#if defined(USE_HAL_UART)
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART1_UART_Init();

#if JM_LOG_DEBUG_ENABLE || JM_LOG_ERROR_ENABLE
    MX_USART2_UART_Init();
#endif

    jm_config_t cfg = {
        .get_sys_time_ms = get_sys_time,
        .uart_send      = uart_send,
        .uart_send_log  = uart_send_log,
        .event_cb       = on_event,
        .user_data      = NULL,
    };

    int ret = jm_stm32_init(&cfg);
    if (ret != JM_SUCCESS) {
        JM_LOG_E("jm_stm32 init failed=%d", ret);
        while (1);
    }

    JM_LOG_D("jm_stm32 start");

    /* 寄存器直驱模式主循环：无需轮询 UART，中断驱动接收 */
    while (1) {
        // uint32_t now = get_sys_time();
        // if (now - last_log >= 2000) {
        //     last_log = now;
        //     JM_LOG_D("test log: %lu ms", now);
        // }
#if defined(USE_HAL_UART)
        jm_serial_read(&huart1);
#endif
        jm_stm32_loop();
    }
}
#else
/**
 * @brief 寄存器直驱模式下 USART1 初始化
 *
 * 配置 PA9 为 TX（复用推挽输出），PA10 为 RX（浮空输入），
 * 波特率 115200，使能接收中断。
 */
static void uart_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN | RCC_APB2ENR_IOPAEN;
    GPIOA->CRH = (GPIOA->CRH & ~((0xF << 4) | (0xF << 8))) | (0xB << 4) | (0x4 << 8);
    USART1->BRR = 72000000 / 115200;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;
    NVIC_EnableIRQ(USART1_IRQn);
}

/**
 * @brief 寄存器直驱模式下的主函数
 *
 * 通过直接寄存器操作配置时钟、SysTick、UART，
 * 使用中断接收 UART 数据，主循环仅调用 jm_stm32_loop。
 */
int main(void)
{
    SystemClock_Config();
    SysTick_Config(72000);

    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;  // GPIOB时钟（OLED用）
    
    uart_init();

#if JM_LOG_DEBUG_ENABLE || JM_LOG_ERROR_ENABLE
    log_uart_init();
#endif

    jm_config_t cfg = {
        .get_sys_time_ms = get_sys_time, //毫秒为单位时间
        .uart_send      = uart_send, //与8266网卡通信串口
        .uart_send_log  = uart_send_log, //A2日志输出
        .event_cb       = on_event,
        .user_data      = NULL,
    };

    int ret = jm_stm32_init(&cfg);
    if (ret != JM_SUCCESS) {
        JM_LOG_LINE("jm_stm32 init failed=%d", ret);
        while (1);
    }

    JM_LOG_LINE("jm_stm32 started");

    //uint32_t last_log = get_sys_time();
    while (1) {
        //uint32_t now = get_sys_time();
        //if(!now) JM_LOG_LINE("t %u", now);
        //if (now - last_log >= 2000) {
        //    last_log = now;
         //   JM_LOG_LINE("test log: %lu ms", now);
        //}
        jm_stm32_loop();
    }
}

/**
 * @brief USART1 中断服务函数（寄存器直驱模式）
 *
 * 从 USART1 FIFO 读取收到的字节并推入 jm_stm32 接收缓冲区。
 * ESP8266 通过 USART1 发送数据到 STM32。
 */
void USART1_IRQHandler(void)
{
    if (USART1->SR & USART_SR_RXNE) {
        uint8_t b = (uint8_t)(USART1->DR & 0xFF);
        jm_stm32_uart_push_byte(b);
    }
}
#endif