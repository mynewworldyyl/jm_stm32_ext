#include "jm_stm32.h"
#include "jm_stm32_com.h"

#if defined(USE_HAL_UART)
#include "stm32f1xx_hal.h"
#else
#include <stm32f1xx.h>
#endif

#if JM_GPIO_CTRL_ENABLE==1

#define GPIO_MAX_PIN 32

typedef struct {
    uint32_t output_enabled;
    uint32_t adc_initialized;
} gpio_state_t;

static gpio_state_t g_gpio_state = {0};

static GPIO_TypeDef *gpio_no_to_port(uint32_t gpioNo) {
    if (gpioNo < 16) {
        return GPIOA;
    } else if (gpioNo < 32) {
        return GPIOB;
    }
    return NULL;
}

static uint16_t gpio_no_to_pin(uint32_t gpioNo) {
    if (gpioNo < 16) {
        return (uint16_t)(1 << gpioNo);
    } else if (gpioNo < 32) {
        return (uint16_t)(1 << (gpioNo - 16));
    }
    return 0;
}

static void gpio_enable_clock(GPIO_TypeDef *port) {
    if (port == GPIOA) {
        RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    } else if (port == GPIOB) {
        RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    }
}

static void gpio_set_output(uint32_t gpioNo) {
    GPIO_TypeDef *port = gpio_no_to_port(gpioNo);
    uint16_t pin = gpio_no_to_pin(gpioNo);
    if (!port || !pin) return;

    gpio_enable_clock(port);

    uint8_t pin_no = 0;
    for (int i = 0; i < 16; i++) {
        if (pin & (1 << i)) {
            pin_no = i;
            break;
        }
    }

    uint32_t cr_reg, shift;
    if (pin_no < 8) {
        cr_reg = port->CRL;
        shift = pin_no * 4;
    } else {
        cr_reg = port->CRH;
        shift = (pin_no - 8) * 4;
    }

    cr_reg &= ~(0xF << shift);
    cr_reg |= (0x2 << shift);  // MODE=10 (2MHz), CNF=00 (push-pull)
    if (pin_no < 8) {
        port->CRL = cr_reg;
    } else {
        port->CRH = cr_reg;
    }

    g_gpio_state.output_enabled |= pin;
}

static void gpio_set_input(uint32_t gpioNo) {
    GPIO_TypeDef *port = gpio_no_to_port(gpioNo);
    uint16_t pin = gpio_no_to_pin(gpioNo);
    if (!port || !pin) return;

    gpio_enable_clock(port);

    uint8_t pin_no = 0;
    for (int i = 0; i < 16; i++) {
        if (pin & (1 << i)) {
            pin_no = i;
            break;
        }
    }

    uint32_t cr_reg;
    if (pin_no < 8) {
        cr_reg = port->CRL;
        cr_reg &= ~(0xF << (pin_no * 4));
        cr_reg |= (0x1 << (pin_no * 4));  // MODE=00 (input), CNF=01 (floating)
        port->CRL = cr_reg;
    } else {
        cr_reg = port->CRH;
        cr_reg &= ~(0xF << ((pin_no - 8) * 4));
        cr_reg |= (0x1 << ((pin_no - 8) * 4));
        port->CRH = cr_reg;
    }
}

static void adc_init_once(void) {
    if (g_gpio_state.adc_initialized) return;

    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    RCC->CFGR &= ~RCC_CFGR_ADCPRE;  // ADC prescaler = 2 (72MHz/6 = 12MHz, but max is 14MHz)

    ADC1->CR2 = 0;
    ADC1->CR1 = 0;
    ADC1->SQR1 = 0;  // 1 conversion
    ADC1->SMPR2 = 0;  // 1.5 cycles sample time (fastest)

    ADC1->CR2 |= ADC_CR2_ADON;
    for (volatile int i = 0; i < 1000; i++);

    g_gpio_state.adc_initialized = 1;
}

static uint16_t adc_read_channel(uint8_t channel) {
    if (channel > 7) return 0;

    adc_init_once();

    ADC1->SQR3 = channel;
    ADC1->CR2 |= ADC_CR2_SWSTART;
    while (!(ADC1->SR & ADC_SR_EOC));
    return (uint16_t)(ADC1->DR & 0xFFF);
}

static void pwm_init_pin(uint32_t gpioNo) {
    GPIO_TypeDef *port = gpio_no_to_port(gpioNo);
    if (!port) return;

    gpio_enable_clock(port);

    uint8_t pin_no = 0;
    for (int i = 0; i < 16; i++) {
        if (gpio_no_to_pin(gpioNo) & (1 << i)) {
            pin_no = i;
            break;
        }
    }

    if (pin_no < 8) {
        uint32_t cr_reg = port->CRL;
        cr_reg &= ~(0xF << (pin_no * 4));
        cr_reg |= (0xB << (pin_no * 4));  // MODE=11 (50MHz), CNF=11 (AF push-pull)
        port->CRL = cr_reg;
    } else {
        uint32_t cr_reg = port->CRH;
        cr_reg &= ~(0xF << ((pin_no - 8) * 4));
        cr_reg |= (0xB << ((pin_no - 8) * 4));
        port->CRH = cr_reg;
    }
}

static void pwm_set_duty(uint32_t gpioNo, uint32_t value) {
    if (gpioNo >= 4) return;  // Only PA0-PA3 supported (TIM2 CH1-CH4)

    static int tim2_initialized = 0;
    if (!tim2_initialized) {
        RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
        TIM2->CR1 = 0;
        TIM2->PSC = 71;   // 72MHz / 72 = 1MHz
        TIM2->ARR = 1000; // 1kHz PWM frequency
        TIM2->CCMR1 = 0;
        TIM2->CCMR2 = 0;
        TIM2->CCER = 0;
        TIM2->CR1 |= TIM_CR1_CEN;
        tim2_initialized = 1;
    }

    uint32_t ccr = value > 1000 ? 1000 : value;
    switch (gpioNo) {
        case 0: TIM2->CCR1 = ccr; TIM2->CCER |= TIM_CCER_CC1E; break;
        case 1: TIM2->CCR2 = ccr; TIM2->CCER |= TIM_CCER_CC2E; break;
        case 2: TIM2->CCR3 = ccr; TIM2->CCER |= TIM_CCER_CC3E; break;
        case 3: TIM2->CCR4 = ccr; TIM2->CCER |= TIM_CCER_CC4E; break;
        default: break;
    }
}

jm_emap_t *ctrl_remote_ctrlGpio(jm_emap_t *ps) {
    jm_emap_t *h = jm_emap_create(0);
    if (!h) return NULL;

    JM_LOG_D("ctrlGpio");

    jm_emap_putInt(h, "code", 0, false);

    int8_t op = jm_emap_getInt(ps, "op", 0);
    uint32_t gpioNo = jm_emap_getInt(ps, "gpioNo", 0);

    if (gpioNo >= GPIO_MAX_PIN) {
        jm_emap_putInt(h, "code", 3, false);
        jm_emap_putStr(h, "msg", "Invalid gpio NO", false, false);
        return h;
    }

    GPIO_TypeDef *port = gpio_no_to_port(gpioNo);
    uint16_t pin = gpio_no_to_pin(gpioNo);
    if (!port || !pin) {
        jm_emap_putInt(h, "code", 3, false);
        jm_emap_putStr(h, "msg", "Invalid gpio NO", false, false);
        return h;
    }

     JM_LOG_D("ctrlGpio op=%d",op);

    switch (op) {
        case 0: {
            gpio_set_input(gpioNo);
            uint8_t val = (port->IDR & pin) ? 1 : 0;
            jm_emap_putInt(h, "status", val, false);
            break;
        }
        case 1: {
            gpio_set_output(gpioNo);
            port->BSRR = pin;
            break;
        }
        case 2: {
            gpio_set_output(gpioNo);
            port->BSRR = (pin << 16);
            break;
        }
        case 3: {
            gpio_set_output(gpioNo);
            port->ODR ^= pin;
            break;
        }
        case 8: {
            if (gpioNo >= 8) {
                jm_emap_putInt(h, "code", 3, false);
                jm_emap_putStr(h, "msg", "ADC only PA0-PA7", false, false);
                break;
            }
            uint16_t v = adc_read_channel((uint8_t)gpioNo);
            jm_emap_putInt(h, "v", v, false);
            break;
        }
        case 9: {
            if (gpioNo >= 4) {
                jm_emap_putInt(h, "code", 3, false);
                jm_emap_putStr(h, "msg", "PWM only PA0-PA3", false, false);
                break;
            }
            pwm_init_pin(gpioNo);
            uint32_t v = jm_emap_getInt(ps, "v", 0);
            pwm_set_duty(gpioNo, v);
            break;
        }
        case 13: {
            uint32_t v = jm_emap_getInt(ps, "v", 0);
            gpio_set_output(gpioNo);
            if (v) {
                port->BSRR = pin;
            } else {
                port->BSRR = (pin << 16);
            }
            jm_emap_putInt(h, "status", v ? 1 : 0, false);
            break;
        }
        default: {
            jm_emap_putInt(h, "code", 1, false);
            jm_emap_putStr(h, "msg", "Invalid op code", false, false);
            break;
        }
    }

    return h;
}

void jm_gpio_init(void) {
    jm_ctrl_registFun(ctrl_remote_ctrlGpio, 53);
}

#endif // JM_GPIO_CTRL_ENABLE==1
