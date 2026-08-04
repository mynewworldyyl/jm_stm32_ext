#include "jm_stm32.h"

#if defined(USE_HAL_UART)
#include "stm32f1xx_hal.h"
#include <stddef.h>
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
#endif

static volatile uint32_t sys_tick_ms = 0;

#if defined(USE_HAL_UART)
static uint32_t get_sys_time(void)
{
    return HAL_GetTick();
}

static void uart_send(const uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)data, len, 100);
}

static void uart_send_log(const uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)data, len, 100);
}
#else
#include "stm32f1xx.h"

void SysTick_Handler(void)
{
    sys_tick_ms++;
}

static uint32_t get_sys_time(void)
{
    return sys_tick_ms;
}

static void uart_send(const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        while (!(USART1->SR & USART_SR_TXE));
        USART1->DR = data[i];
    }
    while (!(USART1->SR & USART_SR_TC));
    
}

static void uart_send_log(const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        while (!(USART2->SR & USART_SR_TXE));
        USART2->DR = data[i];
    }
    //while (!(USART2->SR & USART_SR_TC));
}

static void log_uart_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    GPIOA->CRL = (GPIOA->CRL & ~(0xF << 8)) | (0xB << 8);
    USART2->BRR = 36000000 / 115200;
    USART2->CR1 = USART_CR1_TE | USART_CR1_UE;
}

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
    case JM_EVENT_TCP_CONNECTED:
    case JM_EVENT_TCP_DISCONNECTED: {
        jm_tcp_conn_info_t *c = (jm_tcp_conn_info_t *)data;
        JM_LOG_D("EVT: tcp %s sock=%d %s:%d",
            event_type == JM_EVENT_TCP_CONNECTED ? "connected" : "disconnected",
            c->sock, c->host, c->port);
        break;
    }
    case JM_EVENT_TCP_DATA:
    case JM_EVENT_UDP_DATA: {
        jm_buf_t *buf = (jm_buf_t *)data;
        uint16_t n = jm_buf_readable_len(buf);
        JM_LOG_D("EVT: %s data len=%u", event_type == JM_EVENT_TCP_DATA ? "tcp" : "udp", n);
        break;
    }
    default:
        JM_LOG_D("NSPE: event_type=%d sub_type=%d",event_type, sub_type);
        break;
    }
}

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

    //uint32_t last_log = get_sys_time();
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
static void uart_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN | RCC_APB2ENR_IOPAEN;
    GPIOA->CRH = (GPIOA->CRH & ~((0xF << 4) | (0xF << 8))) | (0xB << 4) | (0x4 << 8);
    USART1->BRR = 72000000 / 115200;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;
    NVIC_EnableIRQ(USART1_IRQn);
}

int main(void)
{
    SystemClock_Config();
    SysTick_Config(72000);
    uart_init();

#if JM_LOG_DEBUG_ENABLE || JM_LOG_ERROR_ENABLE
    log_uart_init();
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
        JM_LOG_LINE("jm_stm32 init failed=%d", ret);
        while (1);
    }

    JM_LOG_LINE("jm_stm32 start");

   // uint32_t last_log = get_sys_time();
    while (1) {
        //uint32_t now = get_sys_time();
        //if (now - last_log >= 2000) {
         //   last_log = now;
           //JM_LOG_LINE("test log: %lu ms", now);
       // }
        jm_stm32_loop();
    }
}

void USART1_IRQHandler(void)
{
    if (USART1->SR & USART_SR_RXNE) {
        uint8_t b = (uint8_t)(USART1->DR & 0xFF);
        jm_stm32_uart_push_byte(b);
    }
}
#endif