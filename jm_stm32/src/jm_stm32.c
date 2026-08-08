#include "jm_stm32.h"
#include "jm_stm32_buf.h"

#include <string.h>

#if defined(USE_HAL_UART)
#include "stm32f1xx_hal.h"
#else
#include <stm32f1xx.h>
#endif

#define JM_RX_RING_SIZE 256

#if JM_STM32_EVENT_ENABLE
static void jm_stm32_runEvent(void);
#endif
static int jm_send_serial_packet(uint16_t subtype, uint16_t msg_id, const uint8_t *payload, uint16_t payload_len);

typedef struct {
    uint8_t buf[JM_RX_RING_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} jm_rx_ring_t;

typedef struct {
    uint32_t last_recv_time;
    jm_buf_t *assembling_buf;
    uint16_t data_size;
    uint16_t recv_size;
    uint8_t ds;
    uint8_t req_id;
    uint8_t ack_req_id;
    uint16_t wpos;
    uint8_t cheader;//当前包的头部值
} jm_rx_state_t;

typedef struct {
    const jm_config_t *config;
    jm_rx_state_t rx;
    bool initialized;
} jm_ctx_t;

static jm_ctx_t g_ctx;
static jm_rx_ring_t g_rx_ring;

static uint8_t REQ_ID = 1;


//引脚中断
#if JM_STM32_INTERRUPT_ENABLE

#define JM_STM32_TRIGGER_RISING  1
#define JM_STM32_TRIGGER_FALLING 2
#define JM_STM32_TRIGGER_CHANGE  3

#define JM_STM32_MAX_PIN_INTERRUPTS 16

#define JM_STM32_DEBOUNCE_MS    70

typedef struct {
    uint16_t pin;
    uint8_t triggerType;
    bool active;
    uint32_t last_irq_time;
} jm_pin_interrupt_t;

static jm_pin_interrupt_t g_pinInterrupts[JM_STM32_MAX_PIN_INTERRUPTS] = {0};

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

static bool jm_stm32_registerPinInterrupt(uint16_t gpioNo, uint8_t triggerType) {
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

static bool jm_stm32_unregisterPinInterrupt(uint16_t gpioNo) {
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

//有引脚中断发送，将事件上行到网卡分发出去
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
        uint32_t now = g_ctx.config->get_sys_time_ms();
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
        uint32_t now = g_ctx.config->get_sys_time_ms();
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
        uint32_t now = g_ctx.config->get_sys_time_ms();
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
        uint32_t now = g_ctx.config->get_sys_time_ms();
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
        uint32_t now = g_ctx.config->get_sys_time_ms();
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
    uint32_t now = g_ctx.config->get_sys_time_ms();
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
    uint32_t now = g_ctx.config->get_sys_time_ms();
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

static inline bool jm_rx_ring_push(uint8_t byte)
{
    uint16_t next = (g_rx_ring.head + 1) % JM_RX_RING_SIZE;
    if (next == g_rx_ring.tail) return false;
    g_rx_ring.buf[g_rx_ring.head] = byte;
    g_rx_ring.head = next;
    return true;
}

static inline bool jm_rx_ring_pop(uint8_t *byte)
{
    if (g_rx_ring.head == g_rx_ring.tail) return false;
    *byte = g_rx_ring.buf[g_rx_ring.tail];
    g_rx_ring.tail = (g_rx_ring.tail + 1) % JM_RX_RING_SIZE;
    return true;
}



static void jm_free_string(char *s)
{
    if (s) free(s);
}

static void jm_dispatch_event(uint8_t event_type, uint16_t sub_type, void *data)
{
    if (g_ctx.config && g_ctx.config->event_cb) {
        //JM_LOG_D("event_cb event_type=%d",event_type, sub_type);
        g_ctx.config->event_cb(event_type, sub_type, data, g_ctx.config->user_data);
    }
}

static uint32_t get_unique_id(void) {
    uint32_t a = *(volatile uint32_t *)0x1FFFF7E8;
    uint32_t b = *(volatile uint32_t *)0x1FFFF7EC;
    uint32_t c = *(volatile uint32_t *)0x1FFFF7F0;
    
    uint32_t h = 2166136261UL;
    h ^= a; h *= 16777619UL;
    h ^= b; h *= 16777619UL;
    h ^= c; h *= 16777619UL;
    return h;
}

/* ===================== Minimal map for STM32 control commands ===================== */

#define MAX_CTRL_FUNS 8

static jm_ctrl_item_t g_ctrl_reg[MAX_CTRL_FUNS];
static uint8_t g_ctrl_reg_count = 0;

jm_emap_t *jm_emap_create(uint8_t type) {
    jm_emap_t *map = (jm_emap_t *)malloc(sizeof(jm_emap_t));
    if (map) {
        map->head = NULL;
        map->type = type;
    }
    return map;
}

void jm_emap_release(jm_emap_t *map) {
    if (!map) return;
    jm_emap_node_t *node = map->head;
    while (node) {
        //JM_LOG_D("4");
        jm_emap_node_t *next = node->next;
        //JM_LOG_D("5");
        if (node->copy_key && node->key) free(node->key);
        //JM_LOG_D("6");
        if (!node->is_int && node->copy_val && node->sval) free(node->sval);
        //JM_LOG_D("7");
        free(node);
        //JM_LOG_D("8");
        node = next;
        //JM_LOG_D("9");
    }
    free(map);
    //JM_LOG_D("10");
}

bool jm_emap_putInt(jm_emap_t *map, const char *key, int32_t val, bool copyKey) {
    if (!map || !key) return false;
    jm_emap_node_t *node = map->head;
    while (node) {
        if (strcmp(node->key, key) == 0) {
            node->ival = val;
            return true;
        }
        node = node->next;
    }
    node = (jm_emap_node_t *)malloc(sizeof(jm_emap_node_t));
    if (!node) return false;
    node->key = copyKey ? strdup(key) : (char *)key;
    node->copy_key = copyKey;
    node->ival = val;
    node->is_int = true;
    node->sval = NULL;
    node->copy_val = false;
    node->next = map->head;
    map->head = node;
    return true;
}

bool jm_emap_putStr(jm_emap_t *map, const char *key, const char *val, bool needFreeMem, bool copyKey) {
    if (!map || !key) return false;
    jm_emap_node_t *node = map->head;
    while (node) {
        if (strcmp(node->key, key) == 0) {
            if (!node->is_int) {
                if (node->copy_val && node->sval) free(node->sval);
                node->sval = (char *)val;
                node->copy_val = needFreeMem;
            }
            return true;
        }
        node = node->next;
    }
    node = (jm_emap_node_t *)malloc(sizeof(jm_emap_node_t));
    if (!node) return false;
    node->key = copyKey ? strdup(key) : (char *)key;
    node->copy_key = copyKey;
    node->is_int = false;
    node->sval = (char *)val;
    node->copy_val = needFreeMem;
    node->next = map->head;
    map->head = node;
    return true;
}

bool jm_emap_putByte(jm_emap_t *map, const char *key, int8_t val, bool copyKey) {
    return jm_emap_putInt(map, key, (int32_t)val, copyKey);
}

int8_t jm_emap_getByte(jm_emap_t *map, const char *key, int8_t def) {
    return (int8_t)jm_emap_getInt(map, key, (int32_t)def);
}

int32_t jm_emap_getInt(jm_emap_t *map, const char *key, int32_t def) {
    jm_emap_node_t *node = map->head;
    while (node) {
        if (strcmp(node->key, key) == 0 && node->is_int) {
            return node->ival;
        }
        node = node->next;
    }
    return def;
}

char *jm_emap_getStr(jm_emap_t *map, const char *key) {
    jm_emap_node_t *node = map->head;
    while (node) {
        if (strcmp(node->key, key) == 0 && !node->is_int) {
            return node->sval;
        }
        node = node->next;
    }
    return NULL;
}

bool jm_emap_exist(jm_emap_t *map, const char *key) {
    jm_emap_node_t *node = map->head;
    while (node) {
        if (strcmp(node->key, key) == 0) {
            return true;
        }
        node = node->next;
    }
    return false;
}

bool jm_emap_encode(const jm_emap_t *map, jm_buf_t *buf) {
    if (!map || !buf) return false;
    if (!jm_buf_put_s8(buf, PREFIX_TYPE_MAP)) return false;
    uint8_t count = 0;
    jm_emap_node_t *node = map->head;
    while (node) { count++; node = node->next; }
    if (!jm_buf_put_s16(buf, (int16_t)count)) return false;
    if (!jm_buf_put_s8(buf, PREFIX_TYPE_STRINGG)) return false;
    node = map->head;
    while (node) {
        if (!jm_buf_write_string(buf, node->key, (uint16_t)strlen(node->key))) return false;
        if (node->is_int) {
            if (!jm_buf_put_s8(buf, PREFIX_TYPE_INT)) return false;
            if (!jm_buf_put_s32(buf, node->ival)) return false;
        } else {
            if (!jm_buf_put_s8(buf, PREFIX_TYPE_STRINGG)) return false;
            uint16_t sval_len = node->sval ? (uint16_t)strlen(node->sval) : 0;
            if (!jm_buf_write_string(buf, node->sval ? node->sval : "", sval_len)) return false;
        }
        node = node->next;
    }
    return true;
}

jm_emap_t *jm_emap_decode(const uint8_t *data, uint16_t len) {
    if (!data || len < 4) return NULL;

    jm_buf_t *buf = jm_buf_wrap_array(data, len);
    if (!buf) return NULL;

    int8_t type = 0;
    if (!jm_buf_get_s8(buf, &type)) {
        jm_buf_release(buf);
        return NULL;
    }

    jm_emap_t *map = jm_emap_create(0);
    if (!map) {
        jm_buf_release(buf);
        return NULL;
    }

    int16_t eleLen = 0;
    if (!jm_buf_get_s16(buf, &eleLen)) {
        jm_emap_release(map);
        jm_buf_release(buf);
        return NULL;
    }

    if (eleLen <= 0) {
        jm_buf_release(buf);
        return map;
    }

    int8_t keyType = 0;
    if (!jm_buf_get_s8(buf, &keyType)) {
        jm_emap_release(map);
        jm_buf_release(buf);
        return NULL;
    }

    for (int16_t i = 0; i < eleLen; i++) {
        int8_t flag = 0;
        char *key = jm_buf_read_string(buf, &flag);
        if (!key || flag <= 0) {
            jm_emap_release(map);
            jm_buf_release(buf);
            return NULL;
        }

        int8_t type = 0;
        if (!jm_buf_get_s8(buf, &type)) {
            free(key);
            jm_emap_release(map);
            jm_buf_release(buf);
            return NULL;
        }

        switch (type) {
            case PREFIX_TYPE_INT: {
                int32_t val = 0;
                if (!jm_buf_get_s32(buf, &val)) {
                    free(key);
                    jm_emap_release(map);
                    jm_buf_release(buf);
                    return NULL;
                }
                jm_emap_putInt(map, key, val, true);
                break;
            }
            case PREFIX_TYPE_BYTE: {
                int8_t val = 0;
                if (!jm_buf_get_s8(buf, &val)) {
                    free(key);
                    jm_emap_release(map);
                    jm_buf_release(buf);
                    return NULL;
                }
                jm_emap_putInt(map, key, (int32_t)val, true);
                break;
            }
            case PREFIX_TYPE_STRINGG: {
                int8_t sflag = 0;
                char *sval = jm_buf_read_string(buf, &sflag);
                if (sflag > 0 && sval) {
                    jm_emap_putStr(map, key, sval, true, true);
                } else if (sflag == 0) {
                    jm_emap_putStr(map, key, "", false, true);
                } else {
                    free(key);
                    jm_emap_release(map);
                    jm_buf_release(buf);
                    return NULL;
                }
                break;
            }
            default:
                break;
        }

        free(key);
    }

    jm_buf_release(buf);
    return map;
}

bool jm_ctrl_registFun(jm_ctrl_fn_t fn, int32_t defId) {
    if (!fn || g_ctrl_reg_count >= MAX_CTRL_FUNS) return false;
    for (int i = 0; i < g_ctrl_reg_count; i++) {
        if (g_ctrl_reg[i].defId == defId) return false;
    }
    g_ctrl_reg[g_ctrl_reg_count].defId = defId;
    g_ctrl_reg[g_ctrl_reg_count].fn = fn;
    g_ctrl_reg_count++;
    return true;
}

static jm_emap_t *jm_ctrl_not_found(void) {
    jm_emap_t *rst = jm_emap_create(0);
    if (rst) {
        jm_emap_putInt(rst, "code", 2, false);
        jm_emap_putStr(rst, "msg", "method not found", false, false);
    }
    return rst;
}

jm_emap_t *jm_ctrl_invokeFunc(jm_emap_t *ps) {
    if (!ps) return NULL;

    int32_t defId = jm_emap_getInt(ps, "funName", 0);
    if (!defId) {
        defId = jm_emap_getInt(ps, "_fn", 0);
    }

    JM_LOG_D("defId=%d",defId);
    if (!defId) {
        return jm_stm32_ctrl_def(ps);
    }

    for (int i = 0; i < g_ctrl_reg_count; i++) {
        if (g_ctrl_reg[i].defId == defId) {
            return g_ctrl_reg[i].fn(ps);
        }
    }

    //JM_LOG_D("defP=%d",defId);
    return jm_ctrl_not_found();
}

/* ===================== End map ===================== */

static void jm_parse_serial_packet(const uint8_t *payload, uint16_t payload_len)
{
    if (payload_len < 8) {
        JM_LOG_E("pl8=%d",payload_len);  
        return;
    }

    if (payload[0] != 0 || payload[1] != 0) {
        JM_LOG_E("p[0]=%d 1=%d",payload[0],payload[1]);  
        return;
    }
    if (payload[2] != JM_SDADA_CHECK_NUM) {
        JM_LOG_E("p2NC=%d",payload[2]);  
        return;
    }
    if (payload[3] != JM_SERIALNET_TYPE_SERIAL) {
        JM_LOG_E("p3NS=%d",payload[3]);  
        return;
    }

    uint16_t subtype = ((uint16_t)payload[4] << 8) | payload[5];
    uint16_t req_id = ((uint16_t)payload[6] << 8) | payload[7];
    (void)req_id;
    uint16_t data_len = payload_len - 8;
    const uint8_t *data = &payload[8];

    jm_buf_t *buf = jm_buf_wrap_array(data, data_len);
    if (!buf) {
        JM_LOG_E("BME=%d", data_len);
        return;
    }

    JM_LOG_D("subtype=%d", subtype);
    switch (subtype) {
    case JM_TASK_APP_PROXY_WIFI_CONNECTED: {
        JM_LOG_D("wifiE");
        jm_wifi_status_t status;
        memset(&status, 0, sizeof(status));
        jm_buf_get_u32(buf, &status.devId);
        jm_buf_get_bool(buf, &status.wifi_enabled);
        jm_buf_get_bool(buf, &status.isLogin);
        jm_dispatch_event(JM_EVENT_WIFI_STATUS, subtype, &status);
        break;
    }

    case JM_TASK_APP_PROXY_HB:
    case JM_TASK_APP_PROXY_INTERNET_ENABLE:
    case JM_TASK_APP_PROXY_WIFI_IS_ENABLE: {
        jm_wifi_status_t status;
        memset(&status, 0, sizeof(status));
        jm_buf_get_u32(buf, &status.devId);
        jm_buf_get_bool(buf, &status.wifi_enabled);
        jm_buf_get_bool(buf, &status.isLogin);
        jm_dispatch_event(JM_EVENT_INTERNET_STATUS, subtype, &status);
        //JM_LOG_D("HB IE WIS we=%d lo=%d did=%d", status.wifi_enabled, status.isLogin, status.devId);
       
        //uint16_t subtype, uint16_t msg_id, const uint8_t *payload, uint16_t payload_len
        if(JM_TASK_APP_PROXY_HB == subtype) {
            JM_LOG_D("resp HB");
            jm_send_serial_packet(subtype, 0, NULL, 0);  //给网卡一个同类形的响应，以表示主机还活着
        }
        break;
    }

    case JM_TASK_APP_PROXY_UID: {
        JM_LOG_D("puid");
        jm_stm32_send_uid();
        break;
    }

    case JM_TASK_APP_PROXY_LOGIN_RESULT: {
        JM_LOG_D("login");
        jm_login_result_t result;
        memset(&result, 0, sizeof(result));
        jm_buf_get_s32(buf, &result.login_code);
        jm_buf_get_u32(buf, &result.dev_uid);
        jm_buf_get_s32(buf, &result.act_id);
        jm_buf_get_s32(buf, &result.client_id);
        jm_buf_get_s8(buf, &result.grp_id);
        int8_t flag = 0;
        char *key = jm_buf_read_string(buf, &flag);
        if (key && flag > 0) {
            strncpy(result.login_key, key, sizeof(result.login_key) - 1);
            jm_free_string(key);
        }

        jm_stm32_send_uid();

        jm_dispatch_event(JM_EVENT_LOGIN_RESULT, subtype, &result);
        break;
    }
    case JM_TASK_APP_PROXY_TCP_CONNECTED:
    case JM_TASK_APP_PROXY_TCP_DISCONNECTED:
    case JM_TASK_APP_PROXY_TCP_SEND:
    case JM_TASK_APP_PROXY_TCP_ERR: {
        JM_LOG_D("TCP");
        jm_tcp_conn_info_t conn;
        memset(&conn, 0, sizeof(conn));
        int8_t sflag = 0;
        char *host = jm_buf_read_string(buf, &sflag);
        jm_buf_get_u16(buf, &conn.port);
        jm_buf_get_s8(buf, &conn.sock);
        if (host && sflag > 0) {
            strncpy(conn.host, host, sizeof(conn.host) - 1);
            jm_free_string(host);
        }
        if (subtype == JM_TASK_APP_PROXY_TCP_SEND || subtype == JM_TASK_APP_PROXY_TCP_ERR) {
            jm_buf_get_s8(buf, &conn.err_code);
            if (conn.err_code < 0) {
                char *err = jm_buf_read_string(buf, &sflag);
                if (err && sflag > 0) {
                    strncpy(conn.err_msg, err, sizeof(conn.err_msg) - 1);
                    jm_free_string(err);
                }
            }
        }
        uint8_t evt = 0;
        switch (subtype) {
        case JM_TASK_APP_PROXY_TCP_CONNECTED: evt = JM_EVENT_TCP_CONNECTED; break;
        case JM_TASK_APP_PROXY_TCP_DISCONNECTED: evt = JM_EVENT_TCP_DISCONNECTED; break;
        case JM_TASK_APP_PROXY_TCP_SEND: evt = JM_EVENT_TCP_SEND_RESULT; break;
        case JM_TASK_APP_PROXY_TCP_ERR: evt = JM_EVENT_TCP_ERROR; break;
        }
         JM_LOG_D("TCPE");
        jm_dispatch_event(evt, subtype, &conn);
        break;
    }
    case JM_TASK_APP_PROXY_SYS_CFG: {
        JM_LOG_D("sysCfg");
        jm_sys_cfg_t cfg;
        memset(&cfg, 0, sizeof(cfg));
        uint16_t avail = jm_buf_readable_len(buf);
        if (avail > sizeof(cfg.data)) avail = sizeof(cfg.data);
        jm_buf_get_bytes(buf, cfg.data, avail);
        cfg.len = avail;
        jm_dispatch_event(JM_EVENT_SYS_CFG, subtype, &cfg);
        break;
    }
    case JM_TASK_APP_PROXY_TRANS_CMD: {
         JM_LOG_D("transCmd");
        uint16_t data_len = jm_buf_readable_len(buf);
        if (data_len > 0) {
            uint8_t *data = (uint8_t *)malloc(data_len);
            if (data) {
                jm_buf_get_bytes(buf, data, data_len);
                jm_dispatch_event(JM_EVENT_TRANS_CMD, subtype, data);
                free(data);
            }
        }
        break;
    }
    case JM_TASK_APP_PROXY_CTRL_EVENT: {
        JM_LOG_D("ctrlEvt");
        uint16_t data_len = jm_buf_readable_len(buf);
        if (data_len > 0) {
            uint8_t *data = (uint8_t *)malloc(data_len);
            if (data) {
                jm_buf_get_bytes(buf, data, data_len);
                jm_dispatch_event(JM_EVENT_CTRL_EVENT, subtype, data);
                free(data);
            }
        }
        break;
    }
    case JM_TASK_APP_PROXY_CTRL_CMD: {
        JM_LOG_D("ctrlCmd reqId=%u", req_id);
        uint16_t data_len = jm_buf_readable_len(buf);
         //JM_LOG_D("dl=%u", data_len);
        if (data_len > 0) {
            const uint8_t *data = jm_buf_read_buf(buf);

           // JM_LOG_D("dcb=%p", data);
            jm_emap_t *ps = jm_emap_decode(data, data_len);
            if (ps) {
               // JM_LOG_D("ivf");
                jm_emap_t *rst = jm_ctrl_invokeFunc(ps);
                // JM_LOG_D("ivfR");
                if (rst) {
                    if (req_id > 0) {
                         JM_LOG_D("crst");
                        jm_stm32_send_ctrl_rst(req_id, rst);
                    }
                    //  JM_LOG_D("1");
                    jm_emap_release(rst);
                }
                //  JM_LOG_D("2");
                jm_emap_release(ps);
                 // JM_LOG_D("3");
            }
        }
        JM_LOG_D("ctrlE");
        break;
    }

#if JM_STM32_EVENT_ENABLE
    case JM_TASK_APP_PROXY_NETCARD_EVENT:
		{
			uint8_t flag = 0;
			if(!jm_buf_get_u8(buf, &flag)) {
				JM_LOG_E("evtFErr");
				return;
			}

			uint8_t eventType = 0;
			if(!jm_buf_get_u8(buf, &eventType)) {
				JM_LOG_E("evtTErr");
				return;
			}

			uint8_t evtSubType = 0;
			if(!jm_buf_get_u8(buf, &evtSubType)) {
				JM_LOG_E("evtSTErr");
				return;
			}

			flag = flag|JM_EVENT_FLAG_FROM_NETCARD|JM_EVENT_FLAG_FREE_EMAP;

			JM_LOG_D("hEvent type=%u evtSubType=%u flag=%u ",eventType, evtSubType,flag);

			jm_emap_t *ps = jm_emap_decode(buf->data, jm_buf_readable_len(buf));
			jm_stm32_postEvent(eventType, evtSubType, ps , flag);

		}
		break;
#endif //#if JM_STM32_EVENT_ENABLE

#if JM_STM32_INTERRUPT_ENABLE
    case JM_TASK_APP_PROXY_NETCARD_INTERRUPT:
		{
            //jm_buf_put_u8(hbuf,1);//注册引脚中断
            //jm_buf_put_u16(hbuf,pin);
            // jm_buf_put_u8(hbuf,triggerType);
            uint8_t opType = 1; //1注册引脚中断， 2：取消注册的引脚中断
            uint16_t pin = 0; //要注册中断的引用
			uint8_t triggerType = 0; //引发引脚中断类型与Arduino的引脚中断类型相同，如上升沿，下降沿等

			if(!jm_buf_get_u8(buf, &opType)) {
				JM_LOG_E("inteErr");
				return;
			}

            if(!jm_buf_get_u16(buf, &pin)) {
				JM_LOG_E("inteErr");
				return;
			}

            if(opType == 1 && !jm_buf_get_u8(buf, &triggerType)) {
				JM_LOG_E("inteErr");
				return;
			}

            if(opType == 1) {
                jm_stm32_registerPinInterrupt(pin, triggerType);
            }else if(opType == 2) {
                jm_stm32_unregisterPinInterrupt(pin);
            }
            
		}
		break;
#endif //#if JM_STM32_INTERRUPT_ENABLE

    default:
        break;
    }

    jm_buf_release(buf);

    //JM_LOG_D("SPE");
}

uint32_t jm_stm32_get_time(void) {
    if (g_ctx.initialized && g_ctx.config && g_ctx.config->get_sys_time_ms) {
        return g_ctx.config->get_sys_time_ms();
    }
    return 0;
}

int jm_stm32_init(const jm_config_t *config)
{
    if (!config || !config->get_sys_time_ms || !config->uart_send) {
        JM_LOG_LINE("InvCfg");
        return JM_ERR_NOT_READY;
    }

    memset(&g_ctx, 0, sizeof(g_ctx));
    memset(&g_rx_ring, 0, sizeof(g_rx_ring));
    g_ctx.config = config;
    g_ctx.initialized = true;

    JM_LOG_LINE("init stm32");

    jm_comp_init(config);

    JM_LOG_LINE("init end");

    return JM_SUCCESS;
}

void jm_stm32_loop(void)
{
    if (!g_ctx.initialized) return;

    uint8_t byte;
    while (jm_rx_ring_pop(&byte)) {
        jm_stm32_uart_rx_byte(byte);
    }

    jm_comp_loop();

#if JM_STM32_EVENT_ENABLE
    jm_stm32_runEvent();
#endif

}

bool jm_stm32_uart_push_byte(uint8_t byte)
{
    return jm_rx_ring_push(byte);
}

void jm_stm32_uart_rx_byte(uint8_t byte)
{
    if (!g_ctx.initialized) return;

    uint32_t now = g_ctx.config->get_sys_time_ms();

    if (g_ctx.rx.last_recv_time > 0 && (now - g_ctx.rx.last_recv_time) > 3000) {
        if (g_ctx.rx.assembling_buf) {
            jm_buf_clear(g_ctx.rx.assembling_buf);
        }
        g_ctx.rx.data_size = 0;
        g_ctx.rx.recv_size = 0;
        g_ctx.rx.ds = 0;
        g_ctx.rx.req_id = 0;
        g_ctx.rx.wpos = 0;
        g_ctx.rx.cheader = 0;
         JM_LOG_D("ClsBuf");
    }
    g_ctx.rx.last_recv_time = now;

    if (g_ctx.rx.ds == 0) {
        if(g_ctx.rx.cheader == PCK_HEANDER) {
            g_ctx.rx.data_size = (uint16_t)byte << 8;
            g_ctx.rx.ds = 1;
            g_ctx.rx.cheader =0;
        } else {
            //JM_LOG_D("GPH %x",byte);
            g_ctx.rx.cheader = byte;
        }
        return;
    }

    if (g_ctx.rx.ds == 1) {
        g_ctx.rx.data_size |= byte;
        if (g_ctx.rx.data_size > JM_MAX_SERIAL_BLOCK_SIZE || g_ctx.rx.data_size == 0) {
            g_ctx.rx.last_recv_time = 1;
            return;
        }
        g_ctx.rx.ds = 2;
        return;
    }

    if (g_ctx.rx.ds == 2) {
        g_ctx.rx.req_id = byte;
        g_ctx.rx.ds = 3;
        return;
    }

    if (g_ctx.rx.req_id == 0) {
        if (g_ctx.rx.data_size != 1) {
            g_ctx.rx.last_recv_time = 1;
            JM_LOG_D("InvPck");
            return;
        }

        //JM_LOG_D("Gcpk=%d",byte);
        g_ctx.rx.ack_req_id = byte;
        g_ctx.rx.data_size = 0;
        g_ctx.rx.recv_size = 0;
        g_ctx.rx.ds = 0;
        g_ctx.rx.req_id = 0;
        g_ctx.rx.cheader = 0;
        return;
    }

    if (!g_ctx.rx.assembling_buf) {
        g_ctx.rx.assembling_buf = jm_buf_create(g_ctx.rx.data_size);
        if (!g_ctx.rx.assembling_buf) {
            g_ctx.rx.data_size = 0;
            g_ctx.rx.recv_size = 0;
            g_ctx.rx.ds = 0;
            g_ctx.rx.req_id = 0;
            JM_LOG_E("MER");
            return;
        }
    }

    if (!jm_buf_put_u8(g_ctx.rx.assembling_buf, byte)) {
        jm_buf_release(g_ctx.rx.assembling_buf);
        g_ctx.rx.assembling_buf = NULL;
        g_ctx.rx.last_recv_time = 1;
        return;
    }

    g_ctx.rx.recv_size++;

    if (g_ctx.rx.recv_size == g_ctx.rx.data_size) {
        
        if (g_ctx.rx.req_id > 1) {
           // jm_delay_ms(2);
            uint8_t ack_pkt[] = {PCK_HEANDER, 0, 1, 0, g_ctx.rx.req_id};
            g_ctx.config->uart_send(ack_pkt, sizeof(ack_pkt));
           // g_ctx.config->uart_send(ack_pkt, sizeof(ack_pkt));
            //JM_LOG_D("cfp %d s=%d",g_ctx.rx.req_id,sizeof(ack_pkt) );

            //for (volatile int i = 0; i < 5000; i++);

           // __DSB();  // Data Synchronization Barrier
           // __ISB();  // Instruction Synchronization Barrier

        }else {
            JM_LOG_D("noCfg %d",g_ctx.rx.req_id );
        }

        //JM_LOG_D("oP %d",g_ctx.rx.req_id);
        g_ctx.rx.cheader = 0; //重置包头

        uint16_t payload_len = jm_buf_readable_len(g_ctx.rx.assembling_buf);
        const uint8_t *payload = jm_buf_read_buf(g_ctx.rx.assembling_buf);

         if (payload_len >= 8 && payload[0] == 0 && payload[1] == 0 && payload[2] == JM_SDADA_CHECK_NUM && payload[3] == JM_SERIALNET_TYPE_SERIAL) {
             //JM_LOG_D("JM_SERIALNET_TYPE_SERIAL %d",payload[3]);
             jm_parse_serial_packet(payload, payload_len);
         } else if (payload_len >= 3 && payload[0] == 0 && payload[2] == JM_SDADA_CHECK_NUM) {
             uint8_t type = payload[3];
             uint8_t evt_type = 0;
             if (type == JM_SERIALNET_TYPE_TCP) evt_type = JM_EVENT_TCP_DATA;
             else if (type == JM_SERIALNET_TYPE_UDP || type == JM_SERIALNET_TYPE_UDP_COM) evt_type = JM_EVENT_UDP_DATA;
             else if (type == JM_SERIALNET_TYPE_MQTT) {
                 jm_mqtt_client_on_serial_data(payload + 4, payload_len - 4);
                 g_ctx.rx.assembling_buf = NULL;
                 g_ctx.rx.data_size = 0;
                 g_ctx.rx.recv_size = 0;
                 g_ctx.rx.ds = 0;
                 g_ctx.rx.req_id = 0;
                 g_ctx.rx.wpos = 0;
                 return;
             }

             JM_LOG_D("got type=%d evt_type=%d",type, evt_type);

             if (evt_type != 0) {
                 jm_buf_t *buf = jm_buf_wrap_array(payload, payload_len);
                 if (buf) {
                     JM_LOG_D("dispatch dl=%d evt_type=%d", payload_len, evt_type);
                     jm_dispatch_event(evt_type, 0, buf);
                     jm_buf_release(buf);
                 }
             }
         } else {
            JM_LOG_D("tcp data dl=%d",payload_len);
            jm_buf_t *buf = jm_buf_wrap_array(payload, payload_len);
            if (buf) {
                JM_LOG_D("disp tcp data dl=%d",payload_len);
                jm_dispatch_event(JM_EVENT_TCP_DATA, 0, buf);
                jm_buf_release(buf);
            }
        }

        jm_buf_release(g_ctx.rx.assembling_buf);
        g_ctx.rx.assembling_buf = NULL;
        g_ctx.rx.data_size = 0;
        g_ctx.rx.recv_size = 0;
        g_ctx.rx.ds = 0;
        g_ctx.rx.req_id = 0;
        g_ctx.rx.wpos = 0;

        //JM_LOG_D("POE");
    }
}

 uint8_t jm_stm32_next_req_id(void) {
    uint8_t reqId =  ++REQ_ID;
	if(reqId==0) {
		//确保reqId不等于0或1
		reqId = REQ_ID = 2;
	}
    return reqId;
 }

jm_buf_t* jm_other_buf(uint8_t type, uint16_t size) {

 	jm_buf_t *hbuf = jm_buf_create(size+5);
 	if(hbuf == NULL) {
 		JM_LOG_E("hbuf N");
 		return NULL;
 	}
 	//两个0总长度表示不拆包，也就是本地命令包长度不能大于JM_MAX_SERIAL_BLOCK_SIZE
 	jm_buf_put_u8(hbuf, 0);
 	jm_buf_put_u8(hbuf, 0);
 	jm_buf_put_u8(hbuf, JM_SDADA_CHECK_NUM); // 用于校验数据合法性
 	jm_buf_put_u8(hbuf, type);
 	return hbuf;
}

static int jm_send_other_packet(uint8_t type, const uint8_t *payload, uint16_t payload_len)
{
    if (!g_ctx.initialized || !g_ctx.config->uart_send) {
        JM_LOG_E("SNI")
        return JM_ERR_NOT_READY;
    }

    JM_LOG_D("other_packet type=%u",type); 

    //jm_buf_t* buf = jm_other_buf(type, 0);

    uint16_t len = 4 + payload_len;

   // uint8_t header = PCK_HEANDER;
    //g_ctx.config->uart_send(&header, 1);

    uint8_t byte0 = (len>>8) & 0xFF;
   // g_ctx.config->uart_send(&byte0, 1);//长度高字节

    uint8_t byte1 = len & 0xFF;
    //g_ctx.config->uart_send(&byte1, 1);//长度低字节

    uint8_t reqId =  jm_stm32_next_req_id();
	
    uint8_t data[] = { PCK_HEANDER, byte0, byte1, reqId, 0, 0, JM_SDADA_CHECK_NUM, type};
    g_ctx.config->uart_send(data, sizeof(data)); //协议请求ID,如果reqId为0，则表示是对方返回的确认包

    //JM_LOG_D("sd [%x,%x,%x,%x,%x,%x,%x]", data[0], data[1], data[2], data[3],data[4],data[5],data[6],data[7]); 

   // g_ctx.config->uart_send(data, 4);

    if (payload_len > 0 && payload) {
        g_ctx.config->uart_send(payload, payload_len);
    }

    return JM_SUCCESS;
}

static jm_buf_t* jm_serial_buf(uint16_t subType, uint16_t msgId, uint16_t size) {

    jm_buf_t *hbuf = jm_other_buf(JM_SERIALNET_TYPE_SERIAL, size+5);
    if(hbuf == NULL) {
 		JM_LOG_E("hbuf N");
 		return NULL;
 	}

 	jm_buf_put_u16(hbuf, subType);
 	jm_buf_put_u16(hbuf, msgId);

 	return hbuf;
}

static int jm_send_serial_packet(uint16_t subtype, uint16_t msg_id, const uint8_t *payload, uint16_t payload_len)
{
    if (!g_ctx.initialized || !g_ctx.config->uart_send) {
        JM_LOG_E("SNI")
        return JM_ERR_NOT_READY;
    }

    JM_LOG_D("serial_packet %d msg_id=%u",subtype, msg_id); 

   
    jm_buf_t* buf = jm_serial_buf(subtype, msg_id, 0);
    uint16_t len = jm_buf_readable_len(buf) + payload_len;

    uint8_t byte0 = (len>>8) & 0xFF;
    //g_ctx.config->uart_send(&byte0, 1);//长度高字节

    uint8_t byte1 = len & 0xFF;
    //g_ctx.config->uart_send(&byte1, 1);//长度低字节

    uint8_t reqId =  jm_stm32_next_req_id();

	//g_ctx.config->uart_send(&reqId, 1);//协议请求ID,如果reqId为0，则表示是对方返回的确认包

   // JM_LOG_D("sd [%x,%x,%x,%x,%x,%x,%x]",byte0,byte1, reqId, buf->data[0], buf->data[1], buf->data[2], buf->data[3]); 

    uint8_t data[] = { PCK_HEANDER, byte0, byte1, reqId};
    g_ctx.config->uart_send(data, sizeof(data)); //协议请求ID,如果reqId为0，则表示是对方返回的确认包
    //JM_LOG_D("sd [%x,%x,%x,%x,%x,%x,%x]", data[0], data[1], data[2], data[3],buf->data[0],buf->data[1],buf->data[2]); 

    g_ctx.config->uart_send(buf->data, jm_buf_readable_len(buf));

    if (payload_len > 0 && payload) {
        g_ctx.config->uart_send(payload, payload_len);
    }

    jm_buf_release(buf);

    //JM_LOG_D("roOP"); 

    return JM_SUCCESS;
}

int jm_stm32_send_ctrl_rst(uint16_t req_id, jm_emap_t *rst) {
    if (!rst) return JM_ERR_INVALID_PACKET;
    jm_buf_t *buf = jm_buf_create(128);
    if (!buf) return JM_ERR_MEMORY;
    if (!jm_emap_encode(rst, buf)) {
        jm_buf_release(buf);
        return JM_ERR_MEMORY;
    }
   
    uint16_t rlen = jm_buf_readable_len(buf);
    const uint8_t *rdata = jm_buf_read_buf(buf);

    JM_LOG_D("ctrl_rst %d reqId=%u",rlen, req_id); 
    int ret = jm_send_serial_packet(JM_TASK_APP_PROXY_CTRL_RST, req_id, rdata, rlen);
    jm_buf_release(buf);
    return ret;
}

int jm_stm32_send_uid()
{

    uint32_t uid = get_unique_id();
    uint16_t board_type = 4;
    const char *device_type_name = "STM32";

    uint16_t name_len = device_type_name ? (uint16_t)strlen(device_type_name) : 0;
    uint16_t payload_len = 6 + name_len;
    uint8_t *payload = (uint8_t *)malloc(payload_len);
    if (!payload) return JM_ERR_MEMORY;

    payload[0] = (uint8_t)((uid >> 24) & 0xFF);
    payload[1] = (uint8_t)((uid >> 16) & 0xFF);
    payload[2] = (uint8_t)((uid >> 8) & 0xFF);
    payload[3] = (uint8_t)(uid & 0xFF);

    payload[4] = (uint8_t)((board_type >> 8) & 0xFF);
    payload[5] = (uint8_t)(board_type & 0xFF);

    if (name_len > 0) {
        memcpy(&payload[6], device_type_name, name_len);
    }

    int ret = jm_send_serial_packet(JM_TASK_APP_PROXY_UID, 0, payload, payload_len);
    free(payload);
    return ret;
}

int jm_stm32_send_wifi_cfg(const char *ssid, const char *pwd)
{
    uint16_t ssid_len = ssid ? (uint16_t)strlen(ssid) : 0;
    uint16_t pwd_len = pwd ? (uint16_t)strlen(pwd) : 0;
    uint16_t payload_len = ssid_len + pwd_len;
    uint8_t *payload = (uint8_t *)malloc(payload_len);
    if (!payload) return JM_ERR_MEMORY;

    if (ssid_len > 0) memcpy(payload, ssid, ssid_len);
    if (pwd_len > 0) memcpy(payload + ssid_len, pwd, pwd_len);

    int ret = jm_send_serial_packet(JM_TASK_APP_PROXY_WIFI_CFG, 0, payload, payload_len);
    free(payload);
    return ret;
}

int jm_stm32_send_wifi_status_req(void)
{
    return jm_send_serial_packet(JM_TASK_APP_PROXY_WIFI_CONNECTED, 0, NULL, 0);
}

int jm_stm32_send_internet_status_req(void)
{
    return jm_send_serial_packet(JM_TASK_APP_PROXY_INTERNET_ENABLE, 0, NULL, 0);
}

int jm_stm32_send_login(void)
{
    return jm_send_serial_packet(JM_TASK_APP_PROXY_LOGIN, 0, NULL, 0);
}

int jm_stm32_send_tcp_connect(const char *host, uint16_t port)
{

    jm_buf_t *buf = jm_buf_create(32);
    if (!buf) return JM_ERR_MEMORY;

    jm_buf_write_string(buf, host, strlen(host));
    jm_buf_put_u16(buf,port);
   
    uint16_t rlen = jm_buf_readable_len(buf);
    const uint8_t *rdata = jm_buf_read_buf(buf);

    JM_LOG_D("tcp_connect %s:%u dl=%d",host, port, rlen); 
    int ret = jm_send_serial_packet(JM_TASK_APP_PROXY_TCP_CONN, 0, rdata, rlen);
    jm_buf_release(buf);
    return ret;

}

int jm_stm32_send_tcp_close(int8_t sock)
{
    uint8_t payload[1];
    payload[0] = (uint8_t)sock;
    return jm_send_serial_packet(JM_TASK_APP_PROXY_TCP_CLOSE, 0, payload, 1);
}

int jm_stm32_send_tcp_data(int8_t sock, const uint8_t *payload, uint16_t plen)
{
    if (!g_ctx.initialized || !g_ctx.config->uart_send) {
        JM_LOG_E("SNI")
        return JM_ERR_NOT_READY;
    }

    uint16_t len = 5 + plen;

    uint8_t byte0 = (len>>8) & 0xFF;
    uint8_t byte1 = len & 0xFF;

    uint8_t reqId = jm_stm32_next_req_id();

    uint8_t data[] = {
        PCK_HEANDER,
        byte0, ////长度高字节
        byte1, //长度低字节
        reqId , //数据包ID
        0, 0, //组包类型，0，0不需要重组包
        JM_SDADA_CHECK_NUM, //校验和
        JM_SERIALNET_TYPE_TCP, //TCB数据包
        sock //socket连接标识
    };

    //JM_LOG_D("sd [%x,%x,%x,%x,%x,%x,%x]",data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]); 

    g_ctx.config->uart_send(data, sizeof(data));//协议请求ID,如果reqId为0，则表示是对方返回的确认包

    if (plen > 0 && payload) {
        g_ctx.config->uart_send(payload, plen);
    }

    return JM_SUCCESS;
}

int jm_stm32_send_udp_data(const char *host, uint16_t port, const uint8_t *payload, uint16_t plen)
{
    if (!g_ctx.initialized || !g_ctx.config->uart_send) {
        JM_LOG_E("SNI")
        return JM_ERR_NOT_READY;
    }

    jm_buf_t *buf = jm_buf_create(24);
    if (!buf) return JM_ERR_MEMORY;

    jm_buf_put_u16(buf, 0);//两个字节的头部长度，都是0给不拆包
    jm_buf_put_u8(buf, JM_SDADA_CHECK_NUM);
    jm_buf_put_u8(buf, JM_SERIALNET_TYPE_UDP_COM);

    jm_buf_write_string(buf,host, strlen(host));
    jm_buf_put_u16(buf, port);


    uint16_t len = jm_buf_readable_len(buf) + plen;

    uint8_t byte0 = (len>>8) & 0xFF;
    uint8_t byte1 = len & 0xFF;

    uint8_t reqId = jm_stm32_next_req_id();

    uint8_t data[3] = { PCK_HEANDER, byte0, byte1,reqId};

    g_ctx.config->uart_send(data, sizeof(data));

    g_ctx.config->uart_send(jm_buf_read_buf(buf), jm_buf_readable_len(buf));

    if (plen > 0 && payload) {
        g_ctx.config->uart_send(payload, plen);
    }

    return JM_SUCCESS;
}

int jm_stm32_send_audio_play(const char *text)
{
    uint16_t len = text ? (uint16_t)strlen(text) : 0;
    return jm_send_serial_packet(JM_TASK_APP_PROXY_AUDIO_PLAY, 0, (const uint8_t *)text, len);
}

int jm_stm32_send_ctrl_event(const uint8_t *data, uint16_t len)
{
    if (!data || len == 0) return JM_ERR_INVALID_PACKET;
    return jm_send_serial_packet(JM_TASK_APP_PROXY_CTRL_EVENT, 0, data, len);
}

int jm_serial_read(void *huart)
{
#if defined(USE_HAL_UART)
    if (!huart || !g_ctx.initialized) return -1;

    UART_HandleTypeDef *uart = (UART_HandleTypeDef *)huart;
    uint8_t byte;
    uint32_t flag = __HAL_UART_GET_FLAG(uart, UART_FLAG_RXNE);
    while (flag != RESET) {
        byte = (uint8_t)(uart->Instance->DR & 0xFF);
        jm_rx_ring_push(byte);
        flag = __HAL_UART_GET_FLAG(uart, UART_FLAG_RXNE);
    }
    return 0;
#else
    (void)huart;
    return -1;
#endif
}


#if JM_LOG_DEBUG_ENABLE || JM_LOG_ERROR_ENABLE
#include <stdio.h>

void jm_log_char(char ch)
{
    if (!g_ctx.initialized || !g_ctx.config->uart_send_log) return;
    const uint8_t c = (uint8_t)ch;
    if (g_ctx.config->uart_send_log) {
        g_ctx.config->uart_send_log(&c, 1);
    }
    
    /*else if (g_ctx.config->uart_send) {
        g_ctx.config->uart_send(&c, 1);
    }*/
}

void jm_log_print(const char *format, ...)
{
    if (!g_ctx.initialized  || !g_ctx.config->uart_send_log) return;

    char buf[128];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    if (len <= 0 || len >= (int)sizeof(buf)) return;

    if (g_ctx.config->uart_send_log) {
        g_ctx.config->uart_send_log((const uint8_t *)buf, (uint16_t)len);
    }/* else if (g_ctx.config->uart_send) {
        g_ctx.config->uart_send((const uint8_t *)buf, (uint16_t)len);
    }*/
}
#endif

int jm_stm32_uart_send(const uint8_t *data, uint16_t len) {
    if (!g_ctx.initialized || !g_ctx.config->uart_send) {
        return JM_ERR_NOT_READY;
    }
    g_ctx.config->uart_send(data, len);
    return JM_SUCCESS;
}

/**
  * @brief  微秒级延时
  * @param  xus 延时时长，范围：0~233015
  * @retval 无
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
  * @brief  毫秒级延时
  * @param  xms 延时时长，范围：0~4294967295
  * @retval 无
  */
void jm_delay_ms(uint32_t xms)
{
	while(xms--)
	{
		jm_delay_us(1000);
	}
}

#if JM_STM32_EVENT_ENABLE

#define JM_STM32_EVENT_QUEUE_SIZE 10
#define JM_MAX_EVENT_LISTENERS 8

typedef void (*jm_event_listener_fn)(jm_event_t *event);

static jm_event_t eventQueue[JM_STM32_EVENT_QUEUE_SIZE];
static uint8_t eventQueueHead = 0;
static uint8_t eventQueueTail = 0;
static bool eventQueueEmpty = true;

typedef struct {
    uint8_t eventType;
    jm_event_listener_fn callback;
    bool active;
} jm_event_listener_entry_t;

static jm_event_listener_entry_t eventListeners[JM_MAX_EVENT_LISTENERS];

//上行事件给给网卡
bool jm_stm32_transEventToCard(jm_event_t *e) {

    //e->type, e->subType, e->data, g_ctx.config->user_data
    uint8_t eventType =e->type; 
    uint16_t subType = e->subType; 
    jm_emap_t *ps =e->data; 
    uint8_t flag = e->flag;

    jm_buf_t *hbuf = jm_buf_create(128);
 	if(hbuf == NULL) {
 		JM_LOG_E("hbuf N");
 		return NULL;
 	}

 	//两个0总长度表示不拆包，也就是本地命令包长度不能大于JM_MAX_SERIAL_BLOCK_SIZE
    jm_buf_put_u8(hbuf, flag);
 	jm_buf_put_u8(hbuf, eventType);
 	jm_buf_put_u16(hbuf, subType);

	if(ps && !jm_emap_encode(ps,hbuf)) {
		JM_LOG_E("transEvent encode MO");
		jm_buf_release(hbuf);
		return false;
	}

	jm_send_serial_packet(JM_TASK_APP_PROXY_NETCARD_EVENT, 0, (uint8_t*) hbuf->data, jm_buf_readable_len(hbuf));

	jm_buf_release(hbuf);

	JM_LOG_D("transEvent E");
	return true;	
}

bool jm_stm32_postEvent(uint8_t eventType, uint16_t subType, void *data, uint8_t flag)
{
    if (eventQueueHead == eventQueueTail && !eventQueueEmpty) {
        return false;
    }

    jm_event_t *e = &eventQueue[eventQueueTail];
    e->type = eventType;
    e->subType = subType;
    e->data = data;
    e->flag = flag;
    eventQueueTail = (eventQueueTail + 1) % JM_STM32_EVENT_QUEUE_SIZE;
    if (eventQueueHead == eventQueueTail) {
        eventQueueEmpty = false;
    }
    return true;
}

bool jm_stm32_regEventListener(uint8_t eventType, jm_event_listener_fn callback)
{
    for (int i = 0; i < JM_MAX_EVENT_LISTENERS; i++) {
        if (eventListeners[i].active &&
            eventListeners[i].eventType == eventType &&
            eventListeners[i].callback == callback) {
            return true;
        }
    }

    for (int i = 0; i < JM_MAX_EVENT_LISTENERS; i++) {
        if (!eventListeners[i].active) {
            eventListeners[i].eventType = eventType;
            eventListeners[i].callback = callback;
            eventListeners[i].active = true;
            return true;
        }
    }
    return false;
}

bool jm_stm32_unregEventListener(uint8_t eventType, jm_event_listener_fn callback)
{
    for (int i = 0; i < JM_MAX_EVENT_LISTENERS; i++) {
        if (eventListeners[i].active &&
            eventListeners[i].eventType == eventType &&
            eventListeners[i].callback == callback) {
            eventListeners[i].active = false;
            return true;
        }
    }
    return true;
}

static void _jm_invokeEventListener(jm_event_t *event)
{
    for (int i = 0; i < JM_MAX_EVENT_LISTENERS; i++) {
        if (eventListeners[i].active && eventListeners[i].eventType == event->type) {
            eventListeners[i].callback(event);
        }
    }
}

static void jm_stm32_runEvent(void)
{
    if (eventQueueHead == eventQueueTail && eventQueueEmpty) {
        return;
    }
    jm_event_t *e = &eventQueue[eventQueueHead];
   
    eventQueueHead = (eventQueueHead + 1) % JM_STM32_EVENT_QUEUE_SIZE;
    if (eventQueueHead == eventQueueTail) {
        eventQueueEmpty = true;
    }
    
    if(!(e->flag & JM_EVENT_FLAG_FROM_NETCARD)) {
		//非网卡所连设备上行事件，转发到网卡所连接设备
		//jm_cli_serial_transEventToHost(jevent);
        jm_stm32_transEventToCard(e);
	}

    _jm_invokeEventListener(e);
}

#endif
