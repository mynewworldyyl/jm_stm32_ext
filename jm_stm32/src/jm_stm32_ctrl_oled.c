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
#include "oled/ssd1306.h"
#include "oled/fonts.h"

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

static FontDef_t* get_font(uint8_t id) {
    switch (id) {
        case 1: return &Font_11x18;
        case 2: return &Font_16x26;
        default: return &Font_7x10;
    }
}

static uint8_t hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

static int parse_hex_bitmap(const char *hex, uint8_t *out, int max_len) {
    int hex_len = strlen(hex);
    int out_len = 0;
    int i;
    for (i = 0; i < hex_len && out_len < max_len; i += 2) {
        if (i + 1 >= hex_len) break;
        out[out_len++] = (hex_val(hex[i]) << 4) | hex_val(hex[i + 1]);
    }
    return out_len;
}

/**
 * @brief OLED 控制处理函数
 *
 * 处理从 ESP8266 下发的 OLED 操作命令。
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
            uint8_t ok = fm_api_oled_init();
            jm_emap_putInt(h, "status", ok ? 1 : 0, false);
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
        case 9: {
            SSD1306_UpdateScreen();
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 10: {
            SSD1306_ToggleInvert();
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 11: {
            uint8_t color = (uint8_t)jm_emap_getInt(ps, "color", 0);
            SSD1306_Fill((SSD1306_COLOR_t)color);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 12: {
            uint16_t x = (uint16_t)jm_emap_getInt(ps, "x", 0);
            uint16_t y = (uint16_t)jm_emap_getInt(ps, "y", 0);
            uint8_t color = (uint8_t)jm_emap_getInt(ps, "color", 1);
            SSD1306_DrawPixel(x, y, (SSD1306_COLOR_t)color);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 13: {
            uint16_t x = (uint16_t)jm_emap_getInt(ps, "x", 0);
            uint16_t y = (uint16_t)jm_emap_getInt(ps, "y", 0);
            SSD1306_GotoXY(x, y);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 14: {
            uint16_t x = (uint16_t)jm_emap_getInt(ps, "x", 0);
            uint16_t y = (uint16_t)jm_emap_getInt(ps, "y", 0);
            char ch = (char)jm_emap_getInt(ps, "ch", 0);
            uint8_t font = (uint8_t)jm_emap_getInt(ps, "font", 0);
            uint8_t color = (uint8_t)jm_emap_getInt(ps, "color", 1);
            SSD1306_GotoXY(x, y);
            SSD1306_Putc(ch, get_font(font), (SSD1306_COLOR_t)color);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 15: {
            uint16_t x0 = (uint16_t)jm_emap_getInt(ps, "x0", 0);
            uint16_t y0 = (uint16_t)jm_emap_getInt(ps, "y0", 0);
            uint16_t x1 = (uint16_t)jm_emap_getInt(ps, "x1", 0);
            uint16_t y1 = (uint16_t)jm_emap_getInt(ps, "y1", 0);
            uint8_t color = (uint8_t)jm_emap_getInt(ps, "color", 1);
            SSD1306_DrawLine(x0, y0, x1, y1, (SSD1306_COLOR_t)color);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 16: {
            uint16_t x = (uint16_t)jm_emap_getInt(ps, "x", 0);
            uint16_t y = (uint16_t)jm_emap_getInt(ps, "y", 0);
            uint16_t w = (uint16_t)jm_emap_getInt(ps, "w", 10);
            uint16_t ht = (uint16_t)jm_emap_getInt(ps, "h", 10);
            uint8_t color = (uint8_t)jm_emap_getInt(ps, "color", 1);
            SSD1306_DrawRectangle(x, y, w, ht, (SSD1306_COLOR_t)color);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 17: {
            uint16_t x = (uint16_t)jm_emap_getInt(ps, "x", 0);
            uint16_t y = (uint16_t)jm_emap_getInt(ps, "y", 0);
            uint16_t w = (uint16_t)jm_emap_getInt(ps, "w", 10);
            uint16_t ht = (uint16_t)jm_emap_getInt(ps, "h", 10);
            uint8_t color = (uint8_t)jm_emap_getInt(ps, "color", 1);
            SSD1306_DrawFilledRectangle(x, y, w, ht, (SSD1306_COLOR_t)color);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 18: {
            uint16_t x1 = (uint16_t)jm_emap_getInt(ps, "x1", 0);
            uint16_t y1 = (uint16_t)jm_emap_getInt(ps, "y1", 0);
            uint16_t x2 = (uint16_t)jm_emap_getInt(ps, "x2", 20);
            uint16_t y2 = (uint16_t)jm_emap_getInt(ps, "y2", 20);
            uint16_t x3 = (uint16_t)jm_emap_getInt(ps, "x3", 40);
            uint16_t y3 = (uint16_t)jm_emap_getInt(ps, "y3", 0);
            uint8_t color = (uint8_t)jm_emap_getInt(ps, "color", 1);
            SSD1306_DrawTriangle(x1, y1, x2, y2, x3, y3, (SSD1306_COLOR_t)color);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 19: {
            uint16_t x1 = (uint16_t)jm_emap_getInt(ps, "x1", 0);
            uint16_t y1 = (uint16_t)jm_emap_getInt(ps, "y1", 0);
            uint16_t x2 = (uint16_t)jm_emap_getInt(ps, "x2", 20);
            uint16_t y2 = (uint16_t)jm_emap_getInt(ps, "y2", 20);
            uint16_t x3 = (uint16_t)jm_emap_getInt(ps, "x3", 40);
            uint16_t y3 = (uint16_t)jm_emap_getInt(ps, "y3", 0);
            uint8_t color = (uint8_t)jm_emap_getInt(ps, "color", 1);
            SSD1306_DrawFilledTriangle(x1, y1, x2, y2, x3, y3, (SSD1306_COLOR_t)color);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 20: {
            int16_t x0 = (int16_t)jm_emap_getInt(ps, "x", 64);
            int16_t y0 = (int16_t)jm_emap_getInt(ps, "y", 32);
            int16_t r = (int16_t)jm_emap_getInt(ps, "r", 10);
            uint8_t color = (uint8_t)jm_emap_getInt(ps, "color", 1);
            SSD1306_DrawCircle(x0, y0, r, (SSD1306_COLOR_t)color);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 21: {
            int16_t x0 = (int16_t)jm_emap_getInt(ps, "x", 64);
            int16_t y0 = (int16_t)jm_emap_getInt(ps, "y", 32);
            int16_t r = (int16_t)jm_emap_getInt(ps, "r", 10);
            uint8_t color = (uint8_t)jm_emap_getInt(ps, "color", 1);
            SSD1306_DrawFilledCircle(x0, y0, r, (SSD1306_COLOR_t)color);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 22: {
            int16_t x = (int16_t)jm_emap_getInt(ps, "x", 0);
            int16_t y = (int16_t)jm_emap_getInt(ps, "y", 0);
            int16_t w = (int16_t)jm_emap_getInt(ps, "w", 64);
            int16_t ht = (int16_t)jm_emap_getInt(ps, "h", 64);
            uint8_t color = (uint8_t)jm_emap_getInt(ps, "color", 1);
            char *hex = jm_emap_getStr(ps, "s");
            if (hex && strlen(hex) >= w * ht / 2) {
                static uint8_t bmp[1024];
                int blen = parse_hex_bitmap(hex, bmp, sizeof(bmp));
                if (blen >= (w * ht / 8)) {
                    SSD1306_DrawBitmap(x, y, bmp, w, ht, color);
                    jm_emap_putInt(h, "status", 1, false);
                } else {
                    jm_emap_putInt(h, "code", 2, false);
                    jm_emap_putStr(h, "msg", "bitmap too short", false, false);
                }
            } else {
                jm_emap_putInt(h, "code", 3, false);
                jm_emap_putStr(h, "msg", "Missing bitmap hex", false, false);
            }
            break;
        }
        case 23: {
            uint8_t start_row = (uint8_t)jm_emap_getInt(ps, "start", 0);
            uint8_t end_row = (uint8_t)jm_emap_getInt(ps, "end", 7);
            SSD1306_ScrollRight(start_row, end_row);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 24: {
            uint8_t start_row = (uint8_t)jm_emap_getInt(ps, "start", 0);
            uint8_t end_row = (uint8_t)jm_emap_getInt(ps, "end", 7);
            SSD1306_ScrollLeft(start_row, end_row);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 25: {
            uint8_t start_row = (uint8_t)jm_emap_getInt(ps, "start", 0);
            uint8_t end_row = (uint8_t)jm_emap_getInt(ps, "end", 7);
            SSD1306_Scrolldiagright(start_row, end_row);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 26: {
            uint8_t start_row = (uint8_t)jm_emap_getInt(ps, "start", 0);
            uint8_t end_row = (uint8_t)jm_emap_getInt(ps, "end", 7);
            SSD1306_Scrolldiagleft(start_row, end_row);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 27: {
            SSD1306_Stopscroll();
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 28: {
            uint8_t i = (uint8_t)jm_emap_getInt(ps, "i", 1);
            SSD1306_InvertDisplay(i);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 29: {
            SSD1306_ON();
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 30: {
            SSD1306_OFF();
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
