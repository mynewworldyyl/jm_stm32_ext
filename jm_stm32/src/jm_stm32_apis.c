/**
 * @file jm_stm32_tone.c
 * @brief 蜂鸣器/音调、脉冲检测、移位寄存器、AT24CXX EEPROM、I2C 接口实现
 *
 * 使用寄存器直驱方式，无需 HAL 库。
 * 受 @ref JM_APIS_ENABLE@ref JM_AT24CXX_ENABLE、
 * @ref JM_I2C_WRAPPER_ENABLE 宏控制编译。
 */

#include "jm_stm32.h"
#include "jm_stm32_com.h"

#if defined(USE_HAL_UART)
#include "stm32f1xx_hal.h"
#else
#include <stm32f1xx.h>
#endif

/** @brief 系统时钟频率（Hz） */
#define SYSTEM_CLOCK 72000000

/**
 * @brief 将引脚编号映射到 GPIO 端口
 * @param pin 引脚编号
 * @return 端口指针，无效返回 NULL
 */
static GPIO_TypeDef *pin_to_port(uint32_t pin) {
    if (pin < 16) return GPIOA;
    else if (pin < 32) return GPIOB;
    return NULL;
}

/**
 * @brief 将引脚编号映射到引脚掩码
 * @param pin 引脚编号
 * @return 引脚掩码，无效返回 0
 */
static uint16_t pin_to_mask(uint32_t pin) {
    if (pin < 16) return (uint16_t)(1 << pin);
    else if (pin < 32) return (uint16_t)(1 << (pin - 16));
    return 0;
}

#if JM_APIS_ENABLE==1

/**
 * @brief 音调/PWM 信息结构
 *
 * 记录每个音调引脚对应的定时器和通道。
 */
typedef struct {
    uint32_t pin;            /**< 引脚编号 */
    GPIO_TypeDef *port;      /**< 所属端口 */
    uint16_t pinMask;        /**< 引脚掩码 */
    TIM_TypeDef *tim;        /**< 定时器 */
    uint8_t channel;         /**< 定时器通道 */
} tone_info_t;

static tone_info_t g_tone_infos[4] = {0};
static uint8_t g_tone_info_count = 0;

/**
 * @brief 查找指定引脚的音调信息
 * @param pin 引脚编号
 * @return 音调信息指针，未找到返回 NULL
 */
static tone_info_t *tone_find_info(uint32_t pin) {
    for (int i = 0; i < g_tone_info_count; i++) {
        if (g_tone_infos[i].pin == pin) {
            return &g_tone_infos[i];
        }
    }
    return NULL;
}

/**
 * @brief 分配一个音调信息结构
 * @param pin      引脚编号
 * @param port     端口指针
 * @param pinMask  引脚掩码
 * @param tim      定时器指针
 * @param channel  定时器通道
 * @return 音调信息指针，失败返回 NULL
 */
static tone_info_t *tone_alloc_info(uint32_t pin, GPIO_TypeDef *port, uint16_t pinMask, TIM_TypeDef *tim, uint8_t channel) {
    if (g_tone_info_count >= 4) return NULL;
    tone_info_t *info = &g_tone_infos[g_tone_info_count++];
    info->pin = pin;
    info->port = port;
    info->pinMask = pinMask;
    info->tim = tim;
    info->channel = channel;
    return info;
}



/**
 * @brief 将引脚映射到定时器和通道
 * @param pin    引脚编号
 * @param tim    输出：定时器指针
 * @param channel 输出：定时器通道
 */
static void pin_to_timer(uint32_t pin, TIM_TypeDef **tim, uint8_t *channel) {
    *tim = NULL;
    *channel = 0;
    switch (pin) {
        case 0: *tim = TIM2; *channel = 1; break;
        case 1: *tim = TIM2; *channel = 2; break;
        case 2: *tim = TIM2; *channel = 3; break;
        case 3: *tim = TIM2; *channel = 4; break;
        case 6: *tim = TIM3; *channel = 1; break;
        case 7: *tim = TIM3; *channel = 2; break;
        case 8: *tim = TIM3; *channel = 3; break;
        case 9: *tim = TIM3; *channel = 4; break;
        default: break;
    }
}

/* ===================== 音调/PWM/移位/脉冲 ===================== */

/**
 * @brief 设置引脚为复用推挽输出（用于 PWM/音调）
 * @param pin 引脚编号
 */
static void tone_set_pin_af_pp(uint32_t pin) {
    GPIO_TypeDef *port = pin_to_port(pin);
    uint16_t pinMask = pin_to_mask(pin);
    if (!port || !pinMask) return;

    if (port == GPIOA) RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    else if (port == GPIOB) RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;

    uint8_t pin_no = 0;
    for (int i = 0; i < 16; i++) {
        if (pinMask & (1 << i)) { pin_no = i; break; }
    }

    if (pin_no < 8) {
        uint32_t cr_reg = port->CRL;
        cr_reg &= ~(0xF << (pin_no * 4));
        cr_reg |= (0xB << (pin_no * 4));
        port->CRL = cr_reg;
    } else {
        uint32_t cr_reg = port->CRH;
        cr_reg &= ~(0xF << ((pin_no - 8) * 4));
        cr_reg |= (0xB << ((pin_no - 8) * 4));
        port->CRH = cr_reg;
    }
}

/**
 * @brief 设置引脚为输入模式
 * @param pin 引脚编号
 */
static void tone_set_pin_input(uint32_t pin) {
    GPIO_TypeDef *port = pin_to_port(pin);
    uint16_t pinMask = pin_to_mask(pin);
    if (!port || !pinMask) return;

    uint8_t pin_no = 0;
    for (int i = 0; i < 16; i++) {
        if (pinMask & (1 << i)) { pin_no = i; break; }
    }

    if (pin_no < 8) {
        uint32_t cr_reg = port->CRL;
        cr_reg &= ~(0xF << (pin_no * 4));
        cr_reg |= (0x1 << (pin_no * 4));
        port->CRL = cr_reg;
    } else {
        uint32_t cr_reg = port->CRH;
        cr_reg &= ~(0xF << ((pin_no - 8) * 4));
        cr_reg |= (0x1 << ((pin_no - 8) * 4));
        port->CRH = cr_reg;
    }
}

/**
 * @brief 使能定时器时钟
 * @param tim 定时器指针
 */
static void tone_enable_clock(TIM_TypeDef *tim) {
    if (tim == TIM2) RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    else if (tim == TIM3) RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
}

static void noTone(uint32_t pin);

/**
 * @brief 播放音调
 * @param pin     引脚编号
 * @param freq    频率（Hz）
 * @param duration 持续时间（ms），0 表示持续播放
 */
static void tone(uint32_t pin, uint32_t freq, uint32_t duration) {
    if (freq == 0) return;

    GPIO_TypeDef *port = pin_to_port(pin);
    uint16_t pinMask = pin_to_mask(pin);
    if (!port || !pinMask) return;

    TIM_TypeDef *tim;
    uint8_t channel;
    pin_to_timer(pin, &tim, &channel);
    if (!tim) return;

    tone_info_t *info = tone_find_info(pin);
    if (info) {
        info->tim->CR1 &= ~TIM_CR1_CEN;
    }

    tone_set_pin_af_pp(pin);
    tone_enable_clock(tim);

    uint32_t period = SYSTEM_CLOCK / freq;
    uint32_t psc = 0;
    uint32_t arr = period - 1;

    if (arr > 65535) {
        psc = period / 65536;
        if (psc == 0) psc = 1;
        arr = period / psc;
        if (arr > 65535) arr = 65535;
        if (arr == 0) arr = 1;
    }

    tim->CR1 = 0;
    tim->PSC = psc;
    tim->ARR = arr;
    tim->CCMR1 = 0;
    tim->CCMR2 = 0;
    tim->CCER = 0;

    uint32_t ccr = arr / 2;
    switch (channel) {
        case 1:
            tim->CCMR1 |= (0x6 << 4) | (1 << 3);
            tim->CCR1 = ccr;
            tim->CCER |= TIM_CCER_CC1E;
            break;
        case 2:
            tim->CCMR1 |= (0x6 << 12) | (1 << 11);
            tim->CCR2 = ccr;
            tim->CCER |= TIM_CCER_CC2E;
            break;
        case 3:
            tim->CCMR2 |= (0x6 << 4) | (1 << 3);
            tim->CCR3 = ccr;
            tim->CCER |= TIM_CCER_CC3E;
            break;
        case 4:
            tim->CCMR2 |= (0x6 << 12) | (1 << 11);
            tim->CCR4 = ccr;
            tim->CCER |= TIM_CCER_CC4E;
            break;
        default:
            return;
    }

    tim->CR1 |= TIM_CR1_CEN;

    if (info == NULL) {
        info = tone_alloc_info(pin, port, pinMask, tim, channel);
    }

    if (duration > 0) {
        jm_delay_ms(duration);
        noTone(pin);
    }
}

/**
 * @brief 停止播放音调
 * @param pin 引脚编号
 */
static void noTone(uint32_t pin) {
    tone_info_t *info = tone_find_info(pin);
    if (info == NULL) return;

    info->tim->CR1 &= ~TIM_CR1_CEN;
    tone_set_pin_input(pin);
}

/**
 * @brief 设置引脚为普通输出模式
 * @param pin 引脚编号
 */
static void tone_set_pin_output(uint32_t pin) {
    GPIO_TypeDef *port = pin_to_port(pin);
    uint16_t pinMask = pin_to_mask(pin);
    if (!port || !pinMask) return;

    if (port == GPIOA) RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    else if (port == GPIOB) RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;

    uint8_t pin_no = 0;
    for (int i = 0; i < 16; i++) {
        if (pinMask & (1 << i)) { pin_no = i; break; }
    }

    if (pin_no < 8) {
        uint32_t cr_reg = port->CRL;
        cr_reg &= ~(0xF << (pin_no * 4));
        cr_reg |= (0x3 << (pin_no * 4));
        port->CRL = cr_reg;
    } else {
        uint32_t cr_reg = port->CRH;
        cr_reg &= ~(0xF << ((pin_no - 8) * 4));
        cr_reg |= (0x3 << ((pin_no - 8) * 4));
        port->CRH = cr_reg;
    }
}

/**
 * @brief 读取脉冲信号持续时间（微秒）
 * @param pin        引脚编号
 * @param state      等待的电平状态
 * @param timeout_us 超时时间（微秒）
 * @return 脉冲持续时间（微秒），超时返回 0
 */
static uint32_t tone_pulseIn(uint32_t pin, uint8_t state, uint32_t timeout_us) {
    GPIO_TypeDef *port = pin_to_port(pin);
    uint16_t pinMask = pin_to_mask(pin);
    if (!port || !pinMask) return 0;

    tone_set_pin_input(pin);

    uint32_t systick_ctrl = SysTick->CTRL;
    uint32_t systick_load = SysTick->LOAD;

    SysTick->LOAD = 0xFFFFFF;
    SysTick->VAL = 0;
    SysTick->CTRL = 5;

    uint32_t start = SysTick->VAL;
    while (((port->IDR & pinMask) ? 1 : 0) != state) {
        if ((start - SysTick->VAL) / 72 > timeout_us) {
            SysTick->CTRL = 0;
            SysTick->LOAD = systick_load;
            SysTick->VAL = 0;
            SysTick->CTRL = systick_ctrl;
            return 0;
        }
    }

    uint32_t pulse_start = SysTick->VAL;
    while (((port->IDR & pinMask) ? 1 : 0) == state) {
        if ((pulse_start - SysTick->VAL) / 72 > timeout_us) {
            SysTick->CTRL = 0;
            SysTick->LOAD = systick_load;
            SysTick->VAL = 0;
            SysTick->CTRL = systick_ctrl;
            return 0;
        }
    }

    uint32_t pulse_end = SysTick->VAL;

    SysTick->CTRL = 0;
    SysTick->LOAD = systick_load;
    SysTick->VAL = 0;
    SysTick->CTRL = systick_ctrl;

    uint32_t elapsed = (pulse_start >= pulse_end) ? (pulse_start - pulse_end) : (0x1000000 - pulse_end + pulse_start);
    return elapsed / 72;
}

/**
 * @brief 读取脉冲信号持续时间（长超时版本）
 * @param pin        引脚编号
 * @param state      等待的电平状态
 * @param timeout_us 超时时间（微秒）
 * @return 脉冲持续时间（微秒），超时返回 0
 */
static uint32_t tone_pulseInLong(uint32_t pin, uint8_t state, uint32_t timeout_us) {
    return tone_pulseIn(pin, state, timeout_us);
}

/**
 * @brief 从移位寄存器读取一个字节
 * @param dataPin   数据引脚编号
 * @param clockPin  时钟引脚编号
 * @param bitOrder  位序（0=LSB 优先，1=MSB 优先）
 * @return 读取的字节
 */
static uint8_t tone_shiftIn(uint32_t dataPin, uint32_t clockPin, uint8_t bitOrder) {
    uint8_t value = 0;
    uint8_t i;

    GPIO_TypeDef *clkPort = pin_to_port(clockPin);
    uint16_t clkMask = pin_to_mask(clockPin);
    GPIO_TypeDef *dataPort = pin_to_port(dataPin);
    uint16_t dataMask = pin_to_mask(dataPin);

    if (!clkPort || !dataPort) return 0;

    if (clkPort == GPIOA) RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    else if (clkPort == GPIOB) RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    if (dataPort == GPIOA) RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    else if (dataPort == GPIOB) RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;

    tone_set_pin_output(clockPin);
    tone_set_pin_input(dataPin);

    for (i = 0; i < 8; i++) {
        clkPort->BSRR = clkMask;

        if (dataPort->IDR & dataMask) {
            if (bitOrder == 0) {
                value |= (1 << i);
            } else {
                value |= (1 << (7 - i));
            }
        }

        clkPort->BSRR = ((uint32_t)clkMask << 16);
    }

    return value;
}

/**
 * @brief 向移位寄存器写入一个字节
 * @param dataPin   数据引脚编号
 * @param clockPin  时钟引脚编号
 * @param bitOrder  位序（0=LSB 优先，1=MSB 优先）
 * @param val       待写入的字节
 */
static void tone_shiftOut(uint32_t dataPin, uint32_t clockPin, uint8_t bitOrder, uint8_t val) {
    GPIO_TypeDef *clkPort = pin_to_port(clockPin);
    uint16_t clkMask = pin_to_mask(clockPin);
    GPIO_TypeDef *dataPort = pin_to_port(dataPin);
    uint16_t dataMask = pin_to_mask(dataPin);

    if (!clkPort || !dataPort) return;

    if (clkPort == GPIOA) RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    else if (clkPort == GPIOB) RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    if (dataPort == GPIOA) RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    else if (dataPort == GPIOB) RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;

    tone_set_pin_output(clockPin);
    tone_set_pin_output(dataPin);

    clkPort->BSRR = ((uint32_t)clkMask << 16);

    for (int i = 0; i < 8; i++) {
        uint8_t bit;
        if (bitOrder == 0) {
            bit = (val >> i) & 1;
        } else {
            bit = (val >> (7 - i)) & 1;
        }

        if (bit) {
            dataPort->BSRR = dataMask;
        } else {
            dataPort->BSRR = ((uint32_t)dataMask << 16);
        }

        clkPort->BSRR = clkMask;
        clkPort->BSRR = ((uint32_t)clkMask << 16);
    }
}
#endif // JM_APIS_ENABLE==1

#if JM_AT24CXX_ENABLE==1

/* ===================== AT24CXX EEPROM (I2C) ===================== */

#define EEPROM_I2C_ADDRESS 0x50  /**< AT24CXX I2C 设备地址 */
#define EEPROM_I2C_TIMEOUT 10000 /**< I2C 超时计数 */

static I2C_TypeDef *eeprom_i2c = I2C1;
static bool eeprom_i2c_inited = false;

static bool i2c_wait_sr1(uint32_t flag) {
    int timeout = EEPROM_I2C_TIMEOUT;
    while (!(eeprom_i2c->SR1 & flag)) {
        if (--timeout <= 0) return false;
    }
    return true;
}

static bool i2c_start(void) {
    eeprom_i2c->CR1 |= I2C_CR1_START;
    return i2c_wait_sr1(I2C_SR1_SB);
}

static void i2c_stop(void) {
    eeprom_i2c->CR1 |= I2C_CR1_STOP;
}

static bool i2c_send_addr(uint8_t addr, bool read) {
    eeprom_i2c->DR = (addr << 1) | (read ? 1 : 0);
    return i2c_wait_sr1(I2C_SR1_ADDR);
}

static bool i2c_write_byte(uint8_t data) {
    eeprom_i2c->DR = data;
    return i2c_wait_sr1(I2C_SR1_TXE);
}

static uint8_t i2c_read_byte(bool ack) {
    if (!ack) eeprom_i2c->CR1 &= ~I2C_CR1_ACK;
    while (!(eeprom_i2c->SR1 & I2C_SR1_RXNE));
    return eeprom_i2c->DR;
}

/**
 * @brief 初始化 EEPROM 的 I2C 接口
 * @param sda SDA 引脚编号
 * @param scl SCL 引脚编号
 * @return true 成功
 */
static bool eeprom_i2c_init(uint8_t sda, uint8_t scl) {
    GPIO_TypeDef *sda_port = pin_to_port(sda);
    GPIO_TypeDef *scl_port = pin_to_port(scl);
    uint16_t sda_mask = pin_to_mask(sda);
    uint16_t scl_mask = pin_to_mask(scl);
    if (!sda_port || !scl_port) return false;

    if (sda_port == GPIOA) RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    else if (sda_port == GPIOB) RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    if (scl_port == GPIOA) RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    else if (scl_port == GPIOB) RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;

    uint8_t sda_pin_no = 0;
    for (int i = 0; i < 16; i++) {
        if (sda_mask & (1 << i)) { sda_pin_no = i; break; }
    }
    uint8_t scl_pin_no = 0;
    for (int i = 0; i < 16; i++) {
        if (scl_mask & (1 << i)) { scl_pin_no = i; break; }
    }

    if (sda_pin_no < 8) {
        uint32_t cr_reg = sda_port->CRL;
        cr_reg &= ~(0xF << (sda_pin_no * 4));
        cr_reg |= (0xE << (sda_pin_no * 4));
        sda_port->CRL = cr_reg;
    } else {
        uint32_t cr_reg = sda_port->CRH;
        cr_reg &= ~(0xF << ((sda_pin_no - 8) * 4));
        cr_reg |= (0xE << ((sda_pin_no - 8) * 4));
        sda_port->CRH = cr_reg;
    }

    if (scl_pin_no < 8) {
        uint32_t cr_reg = scl_port->CRL;
        cr_reg &= ~(0xF << (scl_pin_no * 4));
        cr_reg |= (0xE << (scl_pin_no * 4));
        scl_port->CRL = cr_reg;
    } else {
        uint32_t cr_reg = scl_port->CRH;
        cr_reg &= ~(0xF << ((scl_pin_no - 8) * 4));
        cr_reg |= (0xE << ((scl_pin_no - 8) * 4));
        scl_port->CRH = cr_reg;
    }

    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    RCC->APB1RSTR |= RCC_APB1RSTR_I2C1RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;

    eeprom_i2c->CR1 = 0;
    eeprom_i2c->CR2 = 72;
    eeprom_i2c->CCR = 360;
    eeprom_i2c->TRISE = 73;
    eeprom_i2c->CR1 |= I2C_CR1_PE;

    eeprom_i2c_inited = true;
    return true;
}

/**
 * @brief 向 EEPROM 指定地址写入一个字节
 * @param addr 地址
 * @param data 数据
 * @return true 成功
 */
static bool eeprom_write_byte(uint16_t addr, uint8_t data) {
    if (!i2c_start()) return false;
    if (!i2c_send_addr(EEPROM_I2C_ADDRESS, false)) return false;
    if (!i2c_write_byte(addr >> 8)) return false;
    if (!i2c_write_byte(addr & 0xFF)) return false;
    if (!i2c_write_byte(data)) return false;
    i2c_stop();
    jm_delay_ms(10);
    return true;
}

/**
 * @brief 从 EEPROM 指定地址读取一个字节
 * @param addr 地址
 * @return 读取到的数据
 */
static uint8_t eeprom_read_byte(uint16_t addr) {
    if (!i2c_start()) return 0;
    if (!i2c_send_addr(EEPROM_I2C_ADDRESS, false)) return 0;
    if (!i2c_write_byte(addr >> 8)) return 0;
    if (!i2c_write_byte(addr & 0xFF)) return 0;

    i2c_start();
    if (!i2c_send_addr(EEPROM_I2C_ADDRESS, true)) return 0;

    uint8_t data = i2c_read_byte(false);
    i2c_stop();
    return data;
}

/**
 * @brief 从 EEPROM 指定地址读取多个字节
 * @param addr 起始地址
 * @param buf  输出缓冲区
 * @param len  读取长度
 * @return true 成功
 */
static bool eeprom_read_bytes(uint16_t addr, uint8_t *buf, uint8_t len) {
    if (!i2c_start()) return false;
    if (!i2c_send_addr(EEPROM_I2C_ADDRESS, false)) return false;
    if (!i2c_write_byte(addr >> 8)) return false;
    if (!i2c_write_byte(addr & 0xFF)) return false;

    i2c_start();
    if (!i2c_send_addr(EEPROM_I2C_ADDRESS, true)) return false;

    for (int i = 0; i < len; i++) {
        bool last = (i == len - 1);
        buf[i] = i2c_read_byte(!last);
    }
    i2c_stop();
    return true;
}

/**
 * @brief 扫描 I2C 总线上可能存在的 EEPROM 设备
 */
static void eeprom_scan_devices(void) {
    for (uint8_t addr = 8; addr < 120; addr++) {
        if (!i2c_start()) continue;
        i2c_send_addr(addr, false);
        i2c_stop();
    }
}

/**
 * @brief AT24CXX EEPROM 控制命令处理
 * @param ps emap 参数（op, addr, data 等）
 * @return 结果 emap，调用者需释放
 */
jm_emap_t* jm_stm32_at24cxx_call(jm_emap_t *ps) {
    jm_emap_t *rst = jm_emap_create(PREFIX_TYPE_STRINGG);
    jm_emap_putByte(rst, (void*)"code", 0, false);

    uint8_t op = (uint8_t)jm_emap_getInt(ps, (void*)"op", 0);

    switch(op) {
        case 54: {
            uint8_t sda = (uint8_t)jm_emap_getInt(ps, (void*)"d", 0);
            uint8_t scl = (uint8_t)jm_emap_getInt(ps, (void*)"c", 0);
            if(!eeprom_i2c_init(sda, scl)) {
                jm_emap_putByte(rst, (void*)"code", 1, false);
            }
        }
        break;

        case 55: {
            uint16_t addr = (uint16_t)jm_emap_getInt(ps, (void*)"a", 0);
            uint8_t v = eeprom_read_byte(addr);
            jm_emap_putByte(rst, (void*)"v", (int8_t)v, false);
        }
        break;

        case 56: {
            uint16_t addra = (uint16_t)jm_emap_getInt(ps, (void*)"a", 0);
            uint8_t d = (uint8_t)jm_emap_getByte(ps, (void*)"d", 0);
            if(!eeprom_write_byte(addra, d)) {
                jm_emap_putByte(rst, (void*)"code", 1, false);
            }
        }
        break;

        case 57: {
            uint16_t addrb = (uint16_t)jm_emap_getInt(ps, (void*)"a", 0);
            uint8_t len = (uint8_t)jm_emap_getInt(ps, (void*)"s", 0);
            if(len <= 0 || len > 64) {
                jm_emap_putByte(rst, (void*)"code", 1, false);
            } else {
                uint8_t buf[64];
                if(!eeprom_read_bytes(addrb, buf, len)){
                    jm_emap_putByte(rst, (void*)"code", 3, false);
                } else {
                    for (int i = 0; i < len; i++) {
                        jm_emap_putByte(rst, (void*)"v", (int8_t)buf[i], false);
                    }
                }
            }
        }
        break;

        case 58: {
            uint16_t addrc = (uint16_t)jm_emap_getInt(ps, (void*)"a", 0);
            uint8_t d = (uint8_t)jm_emap_getByte(ps, (void*)"d", 0);
            bool suc = eeprom_write_byte(addrc, d);
            jm_emap_putByte(rst, (void*)"v", suc ? 1 : 0, false);
        }
        break;

        case 59: {
            eeprom_scan_devices();
        }
        break;

        default:
            jm_emap_putByte(rst, (void*)"code", 100, false);
    }

    return rst;
}

#endif // JM_AT24CXX_ENABLE==1

#if JM_I2C_WRAPPER_ENABLE==1

/* ===================== I2C Wrapper (Wire-like API) ===================== */

#define I2C_MAX_WIRES 4

typedef struct {
    uint16_t wireId;
    GPIO_TypeDef *sda_port;
    GPIO_TypeDef *scl_port;
    uint16_t sda_mask;
    uint16_t scl_mask;
    bool initialized;
    bool transmitting;
    uint8_t address;
    uint8_t *rx_buf;
    uint16_t rx_len;
    uint16_t rx_index;
} i2c_wire_t;

static i2c_wire_t i2c_wires[I2C_MAX_WIRES] = {0};
static uint8_t i2c_wire_count = 0;

static i2c_wire_t *i2c_find_wire(uint16_t wireId) {
    for (int i = 0; i < i2c_wire_count; i++) {
        if (i2c_wires[i].wireId == wireId) {
            return &i2c_wires[i];
        }
    }
    return NULL;
}

static i2c_wire_t *i2c_alloc_wire(uint16_t wireId) {
    if (i2c_wire_count >= I2C_MAX_WIRES) return NULL;
    i2c_wire_t *wire = &i2c_wires[i2c_wire_count++];
    wire->wireId = wireId;
    wire->initialized = false;
    wire->transmitting = false;
    wire->rx_buf = NULL;
    wire->rx_len = 0;
    wire->rx_index = 0;
    return wire;
}

static bool i2c_configure_pins(i2c_wire_t *wire) {
    uint8_t sda = wire->wireId & 0xFF;
    uint8_t scl = (wire->wireId >> 8) & 0xFF;

    wire->sda_port = pin_to_port(sda);
    wire->scl_port = pin_to_port(scl);
    wire->sda_mask = pin_to_mask(sda);
    wire->scl_mask = pin_to_mask(scl);
    if (!wire->sda_port || !wire->scl_port) return false;

    if (wire->sda_port == GPIOA) RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    else if (wire->sda_port == GPIOB) RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    if (wire->scl_port == GPIOA) RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    else if (wire->scl_port == GPIOB) RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;

    uint8_t sda_pin_no = 0;
    for (int i = 0; i < 16; i++) {
        if (wire->sda_mask & (1 << i)) { sda_pin_no = i; break; }
    }
    uint8_t scl_pin_no = 0;
    for (int i = 0; i < 16; i++) {
        if (wire->scl_mask & (1 << i)) { scl_pin_no = i; break; }
    }

    if (sda_pin_no < 8) {
        uint32_t cr_reg = wire->sda_port->CRL;
        cr_reg &= ~(0xF << (sda_pin_no * 4));
        cr_reg |= (0xE << (sda_pin_no * 4));
        wire->sda_port->CRL = cr_reg;
    } else {
        uint32_t cr_reg = wire->sda_port->CRH;
        cr_reg &= ~(0xF << ((sda_pin_no - 8) * 4));
        cr_reg |= (0xE << ((sda_pin_no - 8) * 4));
        wire->sda_port->CRH = cr_reg;
    }

    if (scl_pin_no < 8) {
        uint32_t cr_reg = wire->scl_port->CRL;
        cr_reg &= ~(0xF << (scl_pin_no * 4));
        cr_reg |= (0xE << (scl_pin_no * 4));
        wire->scl_port->CRL = cr_reg;
    } else {
        uint32_t cr_reg = wire->scl_port->CRH;
        cr_reg &= ~(0xF << ((scl_pin_no - 8) * 4));
        cr_reg |= (0xE << ((scl_pin_no - 8) * 4));
        wire->scl_port->CRH = cr_reg;
    }

    return true;
}

static bool i2cw_wait_sr1(I2C_TypeDef *i2c, uint32_t flag, int timeout) {
    while (timeout--) {
        if (i2c->SR1 & flag) return true;
    }
    return false;
}

static bool i2cw_start(I2C_TypeDef *i2c) {
    i2c->CR1 |= I2C_CR1_START;
    return i2cw_wait_sr1(i2c, I2C_SR1_SB, 10000);
}

static void i2cw_stop(I2C_TypeDef *i2c) {
    i2c->CR1 |= I2C_CR1_STOP;
}

static bool i2cw_send_addr(I2C_TypeDef *i2c, uint8_t addr, bool read) {
    i2c->DR = (addr << 1) | (read ? 1 : 0);
    return i2cw_wait_sr1(i2c, I2C_SR1_ADDR, 10000);
}

static bool i2cw_write(I2C_TypeDef *i2c, uint8_t data) {
    i2c->DR = data;
    return i2cw_wait_sr1(i2c, I2C_SR1_TXE, 10000);
}

static uint8_t i2cw_read(I2C_TypeDef *i2c, bool ack) {
    if (!ack) i2c->CR1 &= ~I2C_CR1_ACK;
    while (!(i2c->SR1 & I2C_SR1_RXNE));
    return i2c->DR;
}

/**
 * @brief 初始化 I2C 总线
 * @param wireId  总线 ID（编码为 (scl<<8|sda)）
 * @param address 设备地址
 * @return 0 成功，-1 失败
 */
int8_t jm_i2c_begin(uint16_t wireId, uint8_t address) {
    i2c_wire_t *wire = i2c_find_wire(wireId);
    if (!wire) wire = i2c_alloc_wire(wireId);
    if (!wire) return -1;

    if (!i2c_configure_pins(wire)) return -1;

    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    RCC->APB1RSTR |= RCC_APB1RSTR_I2C1RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;

    I2C1->CR1 = 0;
    I2C1->CR2 = 72;
    I2C1->CCR = 360;
    I2C1->TRISE = 73;
    I2C1->CR1 |= I2C_CR1_PE;

    wire->initialized = true;
    wire->address = address;
    wire->transmitting = false;
    return 0;
}

/**
 * @brief 设置 I2C 时钟频率
 * @param wireId 总线 ID
 * @param clock  时钟频率（Hz）
 * @return 0 成功，-1 失败
 */
int8_t jm_i2c_set_clock(uint16_t wireId, uint32_t clock) {
    i2c_wire_t *wire = i2c_find_wire(wireId);
    if (!wire || !wire->initialized) return -1;

    uint32_t ccr = 72000000 / (2 * clock);
    I2C1->CCR = ccr;
    I2C1->TRISE = (72000000 / 1000000) + 1;
    return 0;
}

/**
 * @brief 开始 I2C 发送事务
 * @param wireId  总线 ID
 * @param address 设备地址
 * @return 0 成功，-1 失败
 */
int8_t jm_i2c_begin_transmission(uint16_t wireId, uint8_t address) {
    i2c_wire_t *wire = i2c_find_wire(wireId);
    if (!wire || !wire->initialized) return -1;

    wire->address = address;
    wire->transmitting = true;
    return 0;
}

/**
 * @brief 结束 I2C 发送事务
 * @param wireId    总线 ID
 * @param releaseBus 是否释放总线
 * @return 0 成功，-1 失败
 */
int8_t jm_i2c_end_transmission(uint16_t wireId, bool releaseBus) {
    i2c_wire_t *wire = i2c_find_wire(wireId);
    if (!wire || !wire->initialized) return -1;

    i2cw_stop(I2C1);
    wire->transmitting = false;
    return 0;
}

/**
 * @brief 请求从 I2C 设备读取数据
 * @param wireId  总线 ID
 * @param address 设备地址
 * @param size    请求读取的字节数
 * @param stop    是否发送停止信号
 * @return 读取的字节数，-1 表示失败
 */
int8_t jm_i2c_request_from(uint16_t wireId, uint8_t address, uint16_t size, bool stop) {
    i2c_wire_t *wire = i2c_find_wire(wireId);
    if (!wire || !wire->initialized) return -1;

    wire->address = address;
    wire->rx_len = size;
    wire->rx_index = 0;

    if (!i2cw_start(I2C1)) return -1;
    if (!i2cw_send_addr(I2C1, address, true)) return -1;

    I2C1->CR1 |= I2C_CR1_ACK;

    if (stop) {
        wire->transmitting = false;
    }

    return size;
}

/**
 * @brief 向 I2C 总线写入一个字节
 * @param wireId 总线 ID
 * @param data   数据
 * @return 0 成功，-1 失败
 */
int8_t jm_i2c_write_byte(uint16_t wireId, uint8_t data) {
    i2c_wire_t *wire = i2c_find_wire(wireId);
    if (!wire || !wire->initialized) return -1;

    if (!i2cw_write(I2C1, data)) return -1;
    return 0;
}

/**
 * @brief 向 I2C 总线写入缓冲区数据
 * @param wireId 总线 ID
 * @param data   数据指针
 * @param size   数据长度
 * @return 写入的字节数，-1 表示失败
 */
int8_t jm_i2c_write_buffer(uint16_t wireId, const uint8_t *data, uint16_t size) {
    i2c_wire_t *wire = i2c_find_wire(wireId);
    if (!wire || !wire->initialized) return -1;

    for (uint16_t i = 0; i < size; i++) {
        if (!i2cw_write(I2C1, data[i])) return -1;
    }
    return size;
}

/**
 * @brief 查询 I2C 总线上可读的字节数
 * @param wireId 总线 ID
 * @return 可读字节数，-1 表示失败
 */
int8_t jm_i2c_available(uint16_t wireId) {
    i2c_wire_t *wire = i2c_find_wire(wireId);
    if (!wire || !wire->initialized) return -1;

    if (wire->rx_index < wire->rx_len) {
        return wire->rx_len - wire->rx_index;
    }
    return 0;
}

/**
 * @brief 从 I2C 总线读取数据
 * @param wireId  总线 ID
 * @param buffer  输出缓冲区
 * @param size    读取字节数
 * @return 读取的字节数，-1 表示失败
 */
int8_t jm_i2c_read(uint16_t wireId, uint8_t *buffer, uint16_t size) {
    i2c_wire_t *wire = i2c_find_wire(wireId);
    if (!wire || !wire->initialized) return -1;

    for (uint16_t i = 0; i < size; i++) {
        bool last = (i == size - 1);
        buffer[i] = i2cw_read(I2C1, !last);
    }

    if (wire->rx_index + size <= wire->rx_len) {
        wire->rx_index += size;
    }

    return (int8_t)size;
}

#endif // JM_I2C_WRAPPER_ENABLE==1

/**
 * @brief 默认控制命令处理
 *
 * 处理音调(tone)、脉冲检测(pulseIn)、移位寄存器(shiftIn/shiftOut)、
 * 中断控制、I2C 操作、EEPROM 操作等控制命令。
 *
 * @param ps emap 参数
 * @return 结果 emap，调用者需释放
 */
jm_emap_t *jm_stm32_ctrl_def(jm_emap_t *ps) {
    jm_emap_t *h = jm_emap_create(0);
    if (!h) return NULL;

    JM_LOG_D("ctrlTone");

    jm_emap_putInt(h, "code", 0, false);

    int8_t op = jm_emap_getInt(ps, "op", 0);
    
    switch (op) {
#if JM_APIS_ENABLE==1
        case 11: {
            uint32_t pin = jm_emap_getInt(ps, "p", 0);
            uint32_t freq = jm_emap_getInt(ps, "f", 0);
            uint32_t duration = jm_emap_getInt(ps, "d", 0);
            JM_LOG_D("tone pin=%d freq=%d dur=%d", pin, freq, duration);
            tone(pin, freq, duration);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 12: {
            uint32_t pin = jm_emap_getInt(ps, "p", 0);
            JM_LOG_D("noTone pin=%d", pin);
            noTone(pin);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }

        case 13:
        case 14: {
            uint32_t pin = jm_emap_getInt(ps, "p", 0);
            uint32_t state = jm_emap_getInt(ps, "s", 0);
            uint32_t timeout = jm_emap_getInt(ps, "t", 1000000);
            JM_LOG_D("pulseIn pin=%d state=%d timeout=%d", pin, state, timeout);
            uint32_t dur = (op == 13) ? tone_pulseIn(pin, state, timeout) : tone_pulseInLong(pin, state, timeout);
            jm_emap_putInt(h, "v", dur, false);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 15: {
            uint32_t dataPin = jm_emap_getInt(ps, "p", 0);
            uint32_t clockPin = jm_emap_getInt(ps, "c", 0);
            uint32_t bitOrder = jm_emap_getInt(ps, "b", 1);
            JM_LOG_D("shiftIn dataPin=%d clockPin=%d bitOrder=%d", dataPin, clockPin, bitOrder);
            uint8_t val = tone_shiftIn(dataPin, clockPin, (uint8_t)bitOrder);
            jm_emap_putInt(h, "v", val, false);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 16: {
            uint32_t dataPin = jm_emap_getInt(ps, "p", 0);
            uint32_t clockPin = jm_emap_getInt(ps, "c", 0);
            uint32_t bitOrder = jm_emap_getInt(ps, "b", 1);
            uint32_t val = jm_emap_getInt(ps, "v", 0);
            JM_LOG_D("shiftOut dataPin=%d clockPin=%d bitOrder=%d val=%d", dataPin, clockPin, bitOrder, val);
            tone_shiftOut(dataPin, clockPin, (uint8_t)bitOrder, (uint8_t)val);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
#endif // JM_APIS_ENABLE==1

#if JM_STM32_INTERRUPT_ENABLE==1
        case 17: {
            JM_LOG_D("noInterrupts");
            __disable_irq();
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 18: {
            JM_LOG_D("interrupts");
            __enable_irq();
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
#endif // JM_STM32_INTERRUPT_ENABLE==1

#if JM_I2C_WRAPPER_ENABLE==1
        case 45: {
            uint16_t wireId = jm_emap_getInt(ps, "i", 0);
            uint8_t address = (uint8_t)jm_emap_getInt(ps, "a", 0);
            int8_t ret = jm_i2c_begin(wireId, address);
            jm_emap_putInt(h, "v", ret, false);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 46: {
            uint16_t wireId = jm_emap_getInt(ps, "i", 0);
            uint32_t clock = jm_emap_getInt(ps, "c", 100000);
            int8_t ret = jm_i2c_set_clock(wireId, clock);
            jm_emap_putInt(h, "v", ret, false);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 47: {
            uint16_t wireId = jm_emap_getInt(ps, "i", 0);
            uint8_t address = (uint8_t)jm_emap_getInt(ps, "a", 0);
            int8_t ret = jm_i2c_begin_transmission(wireId, address);
            jm_emap_putInt(h, "v", ret, false);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 48: {
            uint16_t wireId = jm_emap_getInt(ps, "i", 0);
            bool releaseBus = jm_emap_getInt(ps, "r", 1) ? true : false;
            int8_t ret = jm_i2c_end_transmission(wireId, releaseBus);
            jm_emap_putInt(h, "v", ret, false);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 49: {
            uint16_t wireId = jm_emap_getInt(ps, "i", 0);
            uint8_t address = (uint8_t)jm_emap_getInt(ps, "a", 0);
            uint16_t size = jm_emap_getInt(ps, "s", 0);
            bool stop = jm_emap_getInt(ps, "p", 1) ? true : false;
            int8_t ret = jm_i2c_request_from(wireId, address, size, stop);
            jm_emap_putInt(h, "v", ret, false);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 50: {
            uint16_t wireId = jm_emap_getInt(ps, "i", 0);
            uint8_t data = (uint8_t)jm_emap_getInt(ps, "d", 0);
            int8_t ret = jm_i2c_write_byte(wireId, data);
            jm_emap_putInt(h, "v", ret, false);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 51: {
            uint16_t size = jm_emap_getInt(ps, "s", 0);
            jm_emap_putInt(h, "v", size, false);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 52: {
            uint16_t wireId = jm_emap_getInt(ps, "i", 0);
            int8_t ret = jm_i2c_available(wireId);
            jm_emap_putInt(h, "v", ret, false);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 53: {
            uint16_t wireId = jm_emap_getInt(ps, "i", 0);
            uint16_t size = jm_emap_getInt(ps, "s", 0);
            uint8_t buf[64];
            int8_t ret = jm_i2c_read(wireId, buf, size);
            if (ret > 0) {
                for (uint16_t i = 0; i < size && i < 64; i++) {
                    jm_emap_putByte(h, "v", (int8_t)buf[i], false);
                }
            }
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
#endif // JM_I2C_WRAPPER_ENABLE==1

#if JM_AT24CXX_ENABLE==1
        case 54:
        case 55:
        case 56:
        case 57:
        case 58:
        case 59:
        {
            jm_emap_t *at24rst = jm_stm32_at24cxx_call(ps);
            if (at24rst) {
                jm_emap_putInt(h, "code", (int32_t)jm_emap_getByte(at24rst, (void*)"code", 0), false);
                if (jm_emap_exist(at24rst, (void*)"v")) {
                    jm_emap_putInt(h, "v", jm_emap_getInt(at24rst, (void*)"v", 0), false);
                }
                jm_emap_release(at24rst);
            }
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
#endif // JM_AT24CXX_ENABLE==1

        default: {
            jm_emap_putInt(h, "code", 1, false);
            jm_emap_putStr(h, "msg", "Invalid op code", false, false);
            break;
        }
    }

    return h;
}

