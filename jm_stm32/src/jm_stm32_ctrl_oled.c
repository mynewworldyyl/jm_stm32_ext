/**
 * @file jm_stm32_ctrl_oled.c
 * @brief OLED 控制实现
 *
 * 实现通过串口控制命令对 OLED 显示屏进行操作，
 * 支持初始化、清屏、显示字符、字符串、数字等操作。
 */

#include "jm_stm32.h"
#include "jm_stm32_com.h"
#include "oled/fm_api_oled.h"

#if JM_OLED_ENABLE == 1

static void dec_to_str(uint32_t n, char *buf, int len) {
    char tmp[12];
    int i = 0;
    if (n == 0) {
        tmp[i++] = '0';
    } else {
        uint32_t t = n;
        while (t > 0) {
            tmp[i++] = '0' + (t % 10);
            t /= 10;
        }
    }
    int j;
    int pad = len - i;
    if (pad < 0) pad = 0;
    for (j = 0; j < pad; j++) buf[j] = ' ';
    for (j = 0; j < i; j++) buf[pad + j] = tmp[i - 1 - j];
    buf[len] = '\0';
}

static void signed_dec_to_str(int32_t n, char *buf, int len) {
    if (n < 0) {
        buf[0] = '-';
        n = -n;
        dec_to_str((uint32_t)n, buf + 1, len - 1);
    } else {
        dec_to_str((uint32_t)n, buf, len);
    }
}

static void hex_to_str(uint32_t n, char *buf, int len) {
    const char *hex = "0123456789ABCDEF";
    int i;
    for (i = len - 1; i >= 0; i--) {
        buf[i] = hex[n & 0xF];
        n >>= 4;
    }
    buf[len] = '\0';
}

static void bin_to_str(uint32_t n, char *buf, int len) {
    int i;
    for (i = len - 1; i >= 0; i--) {
        buf[i] = (n & 1) ? '1' : '0';
        n >>= 1;
    }
    buf[len] = '\0';
}

/**
 * @brief OLED 控制处理函数
 *
 * 处理从 ESP8266 下发的 OLED 操作命令，支持初始化、清屏、
 * 显示字符、字符串、数字等操作。
 *
 * @param ps 包含命令参数的 emap (op, l, c, ch, s, n, len 等)
 * @return 包含响应数据的 emap，调用者需释放
 */
jm_emap_t *ctrl_remote_ctrlOled(jm_emap_t *ps) {
    jm_emap_t *h = jm_emap_create(0);
    if (!h) return NULL;
    
    jm_emap_putInt(h, "code", 0, false);

    int8_t op = jm_emap_getInt(ps, "op", 0);
    JM_LOG_D("ctrlOled op=%d",op);

    switch (op) {
        case 1: {
            fm_api_oled_init();
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 2: {
            fm_api_oled_clear();
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 3: {
            uint8_t line = (uint8_t)jm_emap_getInt(ps, "l", 1);
            uint8_t col = (uint8_t)jm_emap_getInt(ps, "c", 1);
            char ch = (char)jm_emap_getInt(ps, "ch", 0);
            char str[2] = {ch, '\0'};
            fm_api_oled_write(str, 2, col, line, FONT_7_X_10_PIXELS);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 4: {
            uint8_t line = (uint8_t)jm_emap_getInt(ps, "l", 1);
            uint8_t col = (uint8_t)jm_emap_getInt(ps, "c", 1);
            char *s = jm_emap_getStr(ps, "s");
            JM_LOG_D("l=%d c=%d s=%s",line, col, s);
            if (s) {
                int slen = strlen(s);
                fm_api_oled_write(s, slen + 1, col, line, FONT_7_X_10_PIXELS);
                jm_emap_putInt(h, "status", 1, false);
            } else {
                jm_emap_putInt(h, "code", 1, false);
                jm_emap_putStr(h, "msg", "Missing string param", false, false);
            }
            break;
        }
        case 5: {
            uint8_t line = (uint8_t)jm_emap_getInt(ps, "l", 1);
            uint8_t col = (uint8_t)jm_emap_getInt(ps, "c", 1);
            uint32_t n = (uint32_t)jm_emap_getInt(ps, "n", 0);
            uint8_t len = (uint8_t)jm_emap_getInt(ps, "len", 1);
            char buf[12];
            dec_to_str(n, buf, len);
            fm_api_oled_write(buf, len + 1, col, line, FONT_7_X_10_PIXELS);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 6: {
            uint8_t line = (uint8_t)jm_emap_getInt(ps, "l", 1);
            uint8_t col = (uint8_t)jm_emap_getInt(ps, "c", 1);
            int32_t n = (int32_t)jm_emap_getInt(ps, "n", 0);
            uint8_t len = (uint8_t)jm_emap_getInt(ps, "len", 1);
            char buf[12];
            signed_dec_to_str(n, buf, len);
            fm_api_oled_write(buf, len + 1, col, line, FONT_7_X_10_PIXELS);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 7: {
            uint8_t line = (uint8_t)jm_emap_getInt(ps, "l", 1);
            uint8_t col = (uint8_t)jm_emap_getInt(ps, "c", 1);
            uint32_t n = (uint32_t)jm_emap_getInt(ps, "n", 0);
            uint8_t len = (uint8_t)jm_emap_getInt(ps, "len", 1);
            char buf[12];
            hex_to_str(n, buf, len);
            fm_api_oled_write(buf, len + 1, col, line, FONT_7_X_10_PIXELS);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 8: {
            uint8_t line = (uint8_t)jm_emap_getInt(ps, "l", 1);
            uint8_t col = (uint8_t)jm_emap_getInt(ps, "c", 1);
            uint32_t n = (uint32_t)jm_emap_getInt(ps, "n", 0);
            uint8_t len = (uint8_t)jm_emap_getInt(ps, "len", 1);
            char buf[12];
            bin_to_str(n, buf, len);
            fm_api_oled_write(buf, len + 1, col, line, FONT_7_X_10_PIXELS);
            jm_emap_putInt(h, "status", 1, false);
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
 * @brief OLED 控制模块初始化
 * 注册 OLED 控制命令处理函数（defId=54）。
 */
void jm_oled_ctrl_init(void) {
    jm_ctrl_registFun(ctrl_remote_ctrlOled, 101);
}

#endif // JM_OLED_ENABLE == 1
