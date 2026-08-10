/**
 * @file jm_stm32_gpio.c
 * @brief GPIO/ADC/PWM/I2C 控制实现
 *
 * 实现通过串口控制命令对 GPIO 进行读写、PWM 输出、ADC 读取、
 * I2C 通信等操作。使用寄存器直驱方式，无需 HAL 库。
 */

#include "jm_stm32.h"
#include "jm_stm32_com.h"

#if defined(USE_HAL_UART)
#include "stm32f1xx_hal.h"
#else
#include <stm32f1xx.h>
#endif

#if JM_GPIO_CTRL_ENABLE==1

#define GPIO_MAX_PIN 32

/** @brief GPIO 状态跟踪结构 */
typedef struct {
    uint32_t output_enabled;   /**< 输出已初始化的引脚掩码 */
    uint32_t adc_initialized;    /**< ADC 初始化标志 */
} gpio_state_t;

static gpio_state_t g_gpio_state = {0};

/**
 * @brief 将引脚编号映射到 GPIO 端口
 * @param gpioNo 引脚编号 (0-15=PA0-PA15, 16-31=PB0-PB15)
 * @return 端口指针，无效返回 NULL
 */
static GPIO_TypeDef *gpio_no_to_port(uint32_t gpioNo) {
    if (gpioNo < 16) {
        return GPIOA;
    } else if (gpioNo < 32) {
        return GPIOB;
    }
    return NULL;
}

/**
 * @brief 将引脚编号映射到引脚掩码
 * @param gpioNo 引脚编号
 * @return 引脚掩码，无效返回 0
 */
static uint16_t gpio_no_to_pin(uint32_t gpioNo) {
    if (gpioNo < 16) {
        return (uint16_t)(1 << gpioNo);
    } else if (gpioNo < 32) {
        return (uint16_t)(1 << (gpioNo - 16));
    }
    return 0;
}

/**
 * @brief 使能 GPIO 端口时钟
 * @param port 端口指针
 */
static void gpio_enable_clock(GPIO_TypeDef *port) {
    if (port == GPIOA) {
        RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    } else if (port == GPIOB) {
        RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    }
}

/**
 * @brief 设置 GPIO 为输出模式
 * @param gpioNo 引脚编号
 */
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

/**
 * @brief 设置 GPIO 为输入模式
 * @param gpioNo 引脚编号
 */
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

/**
 * @brief 一次性初始化 ADC1
 */
static void adc_init_once(void) {
    if (g_gpio_state.adc_initialized) return;

    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_ADCPRE) | (2 << 14);

    ADC1->CR2 = 0;
    for (volatile int i = 0; i < 1000; i++);
    ADC1->CR1 = 0;
    ADC1->SQR1 = 0;
    ADC1->SMPR2 = 0;

    ADC1->CR2 |= ADC_CR2_ADON;
    for (volatile int i = 0; i < 72000; i++);

    ADC1->CR2 |= ADC_CR2_RSTCAL;
    for (volatile int i = 0; i < 10000 && (ADC1->CR2 & ADC_CR2_RSTCAL); i++);

    ADC1->CR2 |= ADC_CR2_CAL;
    for (volatile int i = 0; i < 10000 && (ADC1->CR2 & ADC_CR2_CAL); i++);

    ADC1->CR2 |= ADC_CR2_ADON;
    for (volatile int i = 0; i < 72000; i++);

    g_gpio_state.adc_initialized = 1;
}

/**
 * @brief 读取 ADC 单个通道的值
 * @param channel ADC 通道（0-7）
 * @return 12 位 ADC 值（0-4095）
 */
static uint16_t adc_read_channel(uint8_t channel) {
    if (channel > 7) return 0;

    adc_init_once();

    ADC1->SR = 0;
    ADC1->SQR3 = channel;
    ADC1->CR2 |= ADC_CR2_SWSTART;

    int timeout = 50000;
    while (!(ADC1->SR & ADC_SR_EOC)) {
        if (--timeout <= 0) break;
    }
    return (uint16_t)(ADC1->DR & 0xFFF);
}

/**
 * @brief 初始化 PWM 输出引脚（设置为复用推挽输出）
 * @param gpioNo 引脚编号
 */
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

/**
 * @brief 设置 PWM 占空比
 * @param gpioNo 引脚编号
 * @param value  占空比值（0-255）
 */
static void pwm_set_duty(uint32_t gpioNo, uint32_t value) {
    if ((gpioNo >= 4 && gpioNo <= 5) || gpioNo >= 8) return;

    uint32_t ccr = (value * 1000) / 255;
    if (ccr > 1000) ccr = 1000;

    if (gpioNo <= 3) {
        static int tim2_initialized = 0;
        if (!tim2_initialized) {
            RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
            TIM2->CR1 = 0;
            TIM2->PSC = 71;
            TIM2->ARR = 1000;
            TIM2->CCMR1 = 0;
            TIM2->CCMR2 = 0;
            TIM2->CCER = 0;
            TIM2->CR1 |= TIM_CR1_CEN;
            tim2_initialized = 1;
        }
        switch (gpioNo) {
            case 0:
                TIM2->CCR1 = ccr;
                TIM2->CCMR1 |= (0x6 << 4) | (1 << 3);
                TIM2->CCER |= TIM_CCER_CC1E;
                break;
            case 1:
                TIM2->CCR2 = ccr;
                TIM2->CCMR1 |= (0x6 << 12) | (1 << 11);
                TIM2->CCER |= TIM_CCER_CC2E;
                break;
            case 2:
                TIM2->CCR3 = ccr;
                TIM2->CCMR2 |= (0x6 << 4) | (1 << 3);
                TIM2->CCER |= TIM_CCER_CC3E;
                break;
            case 3:
                TIM2->CCR4 = ccr;
                TIM2->CCMR2 |= (0x6 << 12) | (1 << 11);
                TIM2->CCER |= TIM_CCER_CC4E;
                break;
        }
    } else {
        static int tim3_initialized = 0;
        if (!tim3_initialized) {
            RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
            TIM3->CR1 = 0;
            TIM3->PSC = 71;
            TIM3->ARR = 1000;
            TIM3->CCMR1 = 0;
            TIM3->CCER = 0;
            TIM3->CR1 |= TIM_CR1_CEN;
            tim3_initialized = 1;
        }
        switch (gpioNo) {
            case 6:
                TIM3->CCR1 = ccr;
                TIM3->CCMR1 |= (0x6 << 4) | (1 << 3);
                TIM3->CCER |= TIM_CCER_CC1E;
                break;
            case 7:
                TIM3->CCR2 = ccr;
                TIM3->CCMR1 |= (0x6 << 12) | (1 << 11);
                TIM3->CCER |= TIM_CCER_CC2E;
                break;
        }
    }
}

/**
 * @brief GPIO 控制处理函数
 *
 * 处理从 ESP8266 下发的 GPIO 操作命令，支持输入读取、输出设置、
 * PWM 输出、ADC 读取等操作。
 *
 * @param ps 包含命令参数的 emap (op, gpioNo, v 等)
 * @return 包含响应数据的 emap，调用者需释放
 */
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

     JM_LOG_D("ctrlGpio op=%d gpioNo=%d",op,gpioNo);

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
            JM_LOG_D("3 gpioNo=%d",gpioNo);
            gpio_set_output(gpioNo);
            port->ODR ^= pin;
            break;
        }
        case 8: {
            JM_LOG_D("gpioNo=%d",gpioNo);
            if (gpioNo >= 8) {
                 JM_LOG_D("tooB8=%d",gpioNo);
                jm_emap_putInt(h, "code", 3, false);
                jm_emap_putStr(h, "msg", "ADC only PA0-PA7", false, false);
                break;
            }
            gpio_set_input(gpioNo);
            uint8_t pin_no = 0;
            for (int i = 0; i < 16; i++) {
                if (gpio_no_to_pin(gpioNo) & (1 << i)) {
                    pin_no = i;
                    break;
                }
            }
            if (pin_no < 8) {
                port->CRL &= ~(0xF << (pin_no * 4));
            } else {
                port->CRH &= ~(0xF << ((pin_no - 8) * 4));
            }
            if (gpioNo <= 3) {
                switch (gpioNo) {
                    case 0: TIM2->CCER &= ~TIM_CCER_CC1E; break;
                    case 1: TIM2->CCER &= ~TIM_CCER_CC2E; break;
                    case 2: TIM2->CCER &= ~TIM_CCER_CC3E; break;
                    case 3: TIM2->CCER &= ~TIM_CCER_CC4E; break;
                }
            } else {
                switch (gpioNo) {
                    case 6: TIM3->CCER &= ~TIM_CCER_CC1E; break;
                    case 7: TIM3->CCER &= ~TIM_CCER_CC2E; break;
                }
            }
            JM_LOG_D("RB=%d",gpioNo);
            uint16_t v = adc_read_channel((uint8_t)gpioNo);
             JM_LOG_D("RR=%d",v);
            jm_emap_putInt(h, "v", v, false);
            JM_LOG_D("RRE");
            break;
        }
        case 9: {
            if ((gpioNo >= 4 && gpioNo <= 5) || gpioNo >= 8) {
                jm_emap_putInt(h, "code", 3, false);
                jm_emap_putStr(h, "msg", "PWM only PA0-PA3, PA6-PA7", false, false);
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

         case 100: {
            uint32_t scl = jm_emap_getInt(ps, "scl", 0);
            uint32_t sda = jm_emap_getInt(ps, "sda", 0);
            uint32_t rot = jm_emap_getInt(ps, "rot", 0);
            JM_LOG_D("scl=%d sda=%d rot=%d", scl, sda, rot);


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

/**
 * @brief GPIO 控制模块初始化
 * 注册 GPIO 控制命令处理函数（defId=53）。
 */
void jm_gpio_init(void) {
    jm_ctrl_registFun(ctrl_remote_ctrlGpio, 53);
}

#endif // JM_GPIO_CTRL_ENABLE==1
