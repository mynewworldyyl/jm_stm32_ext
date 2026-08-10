/**
 * @file jm_stm32_interrupt.c
 * @brief STM32 引脚中断模块
 *
 * 实现引脚中断的注册/注销、去抖处理及 EXTI 中断服务函数。
 * 中断事件通过串口上报给 ESP8266 netproxy。
 *
 * @copyright MIT
 */

#include "jm_stm32.h"
#include "jm_stm32_buf.h"

#if !defined(USE_HAL_UART)
#include <stm32f1xx.h>
#endif

#if JM_STM32_INTERRUPT_ENABLE

/** @brief 触发类型：上升沿 */
#define JM_STM32_TRIGGER_RISING  1
/** @brief 触发类型：下降沿 */
#define JM_STM32_TRIGGER_FALLING 2
/** @brief 触发类型：边沿变化 */
#define JM_STM32_TRIGGER_CHANGE  3

/** @brief 最大支持的引脚中断数量 */
#define JM_STM32_MAX_PIN_INTERRUPTS 16

/** @brief 按键去抖延时（毫秒） */
#define JM_STM32_DEBOUNCE_MS    70

/** @brief 引脚中断描述结构 */
typedef struct {
    uint16_t pin;              /**< 引脚编号 */
    uint8_t triggerType;       /**< 触发类型 */
    bool active;               /**< 是否激活 */
    uint32_t last_irq_time;    /**< 上次中断时间（用于去抖） */
} jm_pin_interrupt_t;

jm_pin_interrupt_t g_pinInterrupts[JM_STM32_MAX_PIN_INTERRUPTS] = {0};

static GPIO_TypeDef *jm_stm32_gpioNo_to_port(uint32_t gpioNo) {
    if (gpioNo < 16) return GPIOA;
    else if (gpioNo < 32) return GPIOB;
    return NULL;
}

static uint16_t jm_stm32_gpioNo_to_pin(uint32_t gpioNo) {
    if (gpioNo < 16) return (uint16_t)(1 << gpioNo);
    else if (gpioNo < 32) return (uint16_t)(1 << (gpioNo - 16));
    return 0;
}

bool jm_stm32_registerPinInterrupt(uint16_t gpioNo, uint8_t triggerType) {
    if (gpioNo >= 32) return false;
    GPIO_TypeDef *port = jm_stm32_gpioNo_to_port(gpioNo);
    uint16_t pinMask = jm_stm32_gpioNo_to_pin(gpioNo);
    uint8_t extiLine = (uint8_t)(gpioNo & 0x0F);
    if (!port || !pinMask) return false;

    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;

    if (port == GPIOA) RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    else if (port == GPIOB) RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    else if (port == GPIOC) RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;

    uint32_t portSource = (port == GPIOA) ? 0 :
                          (port == GPIOB) ? 1 :
                          (port == GPIOC) ? 2 : 3;
    uint32_t extiReg = extiLine / 4;
    uint32_t extiShift = (extiLine % 4) * 4;
    AFIO->EXTICR[extiReg] = (AFIO->EXTICR[extiReg] & ~(0xFUL << extiShift)) | (portSource << extiShift);

    EXTI->IMR |= (1UL << extiLine);
    if (triggerType == JM_STM32_TRIGGER_RISING) {
        EXTI->RTSR |= (1UL << extiLine);
        EXTI->FTSR &= ~(1UL << extiLine);
    } else if (triggerType == JM_STM32_TRIGGER_FALLING) {
        EXTI->RTSR &= ~(1UL << extiLine);
        EXTI->FTSR |= (1UL << extiLine);
    } else {
        EXTI->RTSR |= (1UL << extiLine);
        EXTI->FTSR |= (1UL << extiLine);
    }

    IRQn_Type irqn;
    if (extiLine <= 4) {
        irqn = (IRQn_Type)(EXTI0_IRQn + extiLine);
    } else if (extiLine <= 9) {
        irqn = EXTI9_5_IRQn;
    } else {
        irqn = EXTI15_10_IRQn;
    }
    NVIC_EnableIRQ(irqn);

    for (int i = 0; i < JM_STM32_MAX_PIN_INTERRUPTS; i++) {
        if (!g_pinInterrupts[i].active) {
            g_pinInterrupts[i].pin = gpioNo;
            g_pinInterrupts[i].triggerType = triggerType;
            g_pinInterrupts[i].active = true;
            break;
        }
    }

    return true;
}

bool jm_stm32_unregisterPinInterrupt(uint16_t gpioNo) {
    if (gpioNo >= 32) return false;
    uint8_t extiLine = (uint8_t)(gpioNo & 0x0F);

    EXTI->IMR &= ~(1UL << extiLine);
    EXTI->RTSR &= ~(1UL << extiLine);
    EXTI->FTSR &= ~(1UL << extiLine);
    EXTI->PR = (1UL << extiLine);

    IRQn_Type irqn;
    if (extiLine <= 4) {
        irqn = (IRQn_Type)(EXTI0_IRQn + extiLine);
    } else if (extiLine <= 9) {
        irqn = EXTI9_5_IRQn;
    } else {
        irqn = EXTI15_10_IRQn;
    }
    NVIC_DisableIRQ(irqn);

    for (int i = 0; i < JM_STM32_MAX_PIN_INTERRUPTS; i++) {
        if (g_pinInterrupts[i].active && g_pinInterrupts[i].pin == gpioNo) {
            g_pinInterrupts[i].active = false;
            break;
        }
    }

    return true;
}

int jm_stm32_pinInterrupt(const uint16_t pin) {
    jm_buf_t *hbuf = jm_buf_create(5);
    if(hbuf == NULL) {
        JM_LOG_E("InteMN");
        return JM_ERR_NOT_READY;
    }
    jm_buf_put_u16(hbuf, pin);

    jm_send_serial_packet(JM_TASK_APP_PROXY_NETCARD_INTERRUPT, 0, (uint8_t*) hbuf->data, jm_buf_readable_len(hbuf));

    jm_buf_release(hbuf);

    JM_LOG_D("transInte E");
    return true;
}

void EXTI0_IRQHandler(void) {
    if (EXTI->PR & (1UL << 0)) {
        EXTI->PR = (1UL << 0);
        uint32_t now = jm_stm32_get_time();
        for (int i = 0; i < JM_STM32_MAX_PIN_INTERRUPTS; i++) {
            if (g_pinInterrupts[i].active && (g_pinInterrupts[i].pin & 0x0F) == 0) {
                if (now - g_pinInterrupts[i].last_irq_time < JM_STM32_DEBOUNCE_MS) continue;
                g_pinInterrupts[i].last_irq_time = now;
                jm_stm32_pinInterrupt(g_pinInterrupts[i].pin);
            }
        }
    }
}

void EXTI1_IRQHandler(void) {
    if (EXTI->PR & (1UL << 1)) {
        EXTI->PR = (1UL << 1);
        uint32_t now = jm_stm32_get_time();
        for (int i = 0; i < JM_STM32_MAX_PIN_INTERRUPTS; i++) {
            if (g_pinInterrupts[i].active && (g_pinInterrupts[i].pin & 0x0F) == 1) {
                if (now - g_pinInterrupts[i].last_irq_time < JM_STM32_DEBOUNCE_MS) continue;
                g_pinInterrupts[i].last_irq_time = now;
                jm_stm32_pinInterrupt(g_pinInterrupts[i].pin);
            }
        }
    }
}

void EXTI2_IRQHandler(void) {
    if (EXTI->PR & (1UL << 2)) {
        EXTI->PR = (1UL << 2);
        uint32_t now = jm_stm32_get_time();
        for (int i = 0; i < JM_STM32_MAX_PIN_INTERRUPTS; i++) {
            if (g_pinInterrupts[i].active && (g_pinInterrupts[i].pin & 0x0F) == 2) {
                if (now - g_pinInterrupts[i].last_irq_time < JM_STM32_DEBOUNCE_MS) continue;
                g_pinInterrupts[i].last_irq_time = now;
                jm_stm32_pinInterrupt(g_pinInterrupts[i].pin);
            }
        }
    }
}

void EXTI3_IRQHandler(void) {
    if (EXTI->PR & (1UL << 3)) {
        EXTI->PR = (1UL << 3);
        uint32_t now = jm_stm32_get_time();
        for (int i = 0; i < JM_STM32_MAX_PIN_INTERRUPTS; i++) {
            if (g_pinInterrupts[i].active && (g_pinInterrupts[i].pin & 0x0F) == 3) {
                if (now - g_pinInterrupts[i].last_irq_time < JM_STM32_DEBOUNCE_MS) continue;
                g_pinInterrupts[i].last_irq_time = now;
                jm_stm32_pinInterrupt(g_pinInterrupts[i].pin);
            }
        }
    }
}

void EXTI4_IRQHandler(void) {
    if (EXTI->PR & (1UL << 4)) {
        EXTI->PR = (1UL << 4);
        uint32_t now = jm_stm32_get_time();
        for (int i = 0; i < JM_STM32_MAX_PIN_INTERRUPTS; i++) {
            if (g_pinInterrupts[i].active && (g_pinInterrupts[i].pin & 0x0F) == 4) {
                if (now - g_pinInterrupts[i].last_irq_time < JM_STM32_DEBOUNCE_MS) continue;
                g_pinInterrupts[i].last_irq_time = now;
                jm_stm32_pinInterrupt(g_pinInterrupts[i].pin);
            }
        }
    }
}

void EXTI9_5_IRQHandler(void) {
    uint32_t now = jm_stm32_get_time();
    for (int line = 5; line <= 9; line++) {
        if (EXTI->PR & (1UL << line)) {
            EXTI->PR = (1UL << line);
            for (int i = 0; i < JM_STM32_MAX_PIN_INTERRUPTS; i++) {
                if (g_pinInterrupts[i].active && (g_pinInterrupts[i].pin & 0x0F) == line) {
                    if (now - g_pinInterrupts[i].last_irq_time < JM_STM32_DEBOUNCE_MS) continue;
                    g_pinInterrupts[i].last_irq_time = now;
                    jm_stm32_pinInterrupt(g_pinInterrupts[i].pin);
                }
            }
        }
    }
}

void EXTI15_10_IRQHandler(void) {
    uint32_t now = jm_stm32_get_time();
    for (int line = 10; line <= 15; line++) {
        if (EXTI->PR & (1UL << line)) {
            EXTI->PR = (1UL << line);
            for (int i = 0; i < JM_STM32_MAX_PIN_INTERRUPTS; i++) {
                if (g_pinInterrupts[i].active && (g_pinInterrupts[i].pin & 0x0F) == line) {
                    if (now - g_pinInterrupts[i].last_irq_time < JM_STM32_DEBOUNCE_MS) continue;
                    g_pinInterrupts[i].last_irq_time = now;
                    jm_stm32_pinInterrupt(g_pinInterrupts[i].pin);
                }
            }
        }
    }
}

#endif //JM_STM32_INTERRUPT_ENABLE
