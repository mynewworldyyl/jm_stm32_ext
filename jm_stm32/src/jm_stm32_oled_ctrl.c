/**
 * @file jm_stm32_oled_ctrl.c
 * @brief OLED 显示控制接口封装
 *
 * 将 ssd1306.h 中的所有 API 封装为控制命令处理函数，
 * 供 jm_stm32 串口协议框架调用。通过 defId=101 注册后，
 * ESP8266 可下发控制命令来操作 OLED 显示屏。
 *
 * @section 集成步骤
 * 1. 在 `jm_pcfg.h` 中启用 OLED 和控制命令：
 *    @code
 *    #define JM_OLED_ENABLE 1
 *    @endcode
 *
 * 2. 将本文件添加到工程源文件列表。
 *
 * 3. 在 `jm_gpio_init()` 中注册控制函数：
 *    @code
 *    jm_ctrl_registFun(ctrl_oled_display, 101);
 *    @endcode
 *
 * @section 控制命令协议
 * 命令通过 emap 参数传递，`op` 字段指定操作：
 *
 * | op   | 操作                    | 参数                                  |
 * | ---- | ----------------------- | ------------------------------------- |
 * | 0    | 初始化 OLED             | -                                     |
 * | 1    | 显示字符串              | text, x, y, font(0=7x10,1=11x18,2=16x26) |
 * | 2    | 显示单个字符            | ch, x, y, font                        |
 * | 3    | 清屏                    | -                                     |
 * | 4    | 填充屏幕                | v(0=黑, 1=白)                         |
 * | 5    | 更新屏幕缓冲区到LCD     | -                                     |
 * | 6    | 反色显示                | i(0=正常, 1=反色)                     |
 * | 7    | 切换反色                | -                                     |
 * | 8    | 画点                    | x, y, color(0=黑, 1=白)               |
 * | 9    | 画线                    | x0, y0, x1, y1, color                 |
 * | 10   | 画矩形                  | x, y, w, h, color                     |
 * | 11   | 画填充矩形              | x, y, w, h, color                     |
 * | 12   | 画三角形                | x1, y1, x2, y2, x3, y3, color         |
 * | 13   | 画填充三角形            | x1, y1, x2, y2, x3, y3, color         |
 * | 14   | 画圆                    | x, y, r, color                        |
 * | 15   | 画填充圆                | x, y, r, color                        |
 * | 16   | 屏幕右滚动              | start_row, end_row                    |
 * | 17   | 屏幕左滚动              | start_row, end_row                    |
 * | 18   | 对角右滚                | start_row, end_row                    |
 * | 19   | 对角左滚                | start_row, end_row                    |
 * | 20   | 停止滚动                | -                                     |
 * | 21   | 电源控制                | v(1=开启, 0=关闭)                     |
 */

#include "jm_stm32.h"
#include "jm_stm32_com.h"

#if JM_OLED_ENABLE==1

#include "ssd1306.h"
#include "fonts.h"

/**
 * @brief OLED 控制模块初始化
 * 注册 OLED 显示控制命令处理函数（defId=101）。
 */
void jm_oled_ctrl_init(void) {
    jm_ctrl_registFun(ctrl_oled_display, 101);
}

/**
 * @brief 获取字体指针
 * @param font_type 字体编号: 0=Font_7x10, 1=Font_11x18, 2=Font_16x26
 * @return 字体结构体指针，默认返回 Font_7x10
 */
static FontDef_t *oled_get_font(int font_type) {
    switch (font_type) {
        case 1: return &Font_11x18;
        case 2: return &Font_16x26;
        default: return &Font_7x10;
    }
}

/**
 * @brief OLED 显示控制处理函数
 *
 * 处理来自 ESP8266 下发的 OLED 显示控制命令。
 *
 * @param ps 包含命令参数的 emap (op + 操作参数)
 * @return 包含响应数据的 emap，调用者需释放
 */
jm_emap_t *ctrl_oled_display(jm_emap_t *ps) {
    jm_emap_t *h = jm_emap_create(0);
    if (!h) return NULL;

    jm_emap_putInt(h, "code", 0, false);

    int8_t op = (int8_t)jm_emap_getInt(ps, "op", 0);
    JM_LOG_D("op=%d",op);
    
    switch (op) {
        case 0: {
            /* 初始化 OLED */
            uint8_t ret = SSD1306_Init();
            jm_emap_putInt(h, "status", ret, false);
            break;
        }
        case 1: {
            /* 显示字符串 */
            const char *text = jm_emap_getStr(ps, "text");
            uint16_t x = (uint16_t)jm_emap_getInt(ps, "x", 0);
            uint16_t y = (uint16_t)jm_emap_getInt(ps, "y", 0);
            int font_type = (int)jm_emap_getInt(ps, "font", 0);
            FontDef_t *font = oled_get_font(font_type);

            if (text) {
                
                SSD1306_GotoXY(x, y);
                SSD1306_Puts((char *)text, font, SSD1306_COLOR_WHITE);
                SSD1306_UpdateScreen();
                jm_emap_putInt(h, "status", 1, false);
            } else {
                jm_emap_putInt(h, "code", 2, false);
                jm_emap_putStr(h, "msg", "no text", false, false);
            }
            break;
        }
        case 2: {
            /* 显示单个字符 */
            char ch = (char)jm_emap_getInt(ps, "ch", ' ');
            uint16_t x = (uint16_t)jm_emap_getInt(ps, "x", 0);
            uint16_t y = (uint16_t)jm_emap_getInt(ps, "y", 0);
            int font_type = (int)jm_emap_getInt(ps, "font", 0);
            FontDef_t *font = oled_get_font(font_type);

            char str[2] = {ch, '\0'};
            SSD1306_GotoXY(x, y);
            SSD1306_Puts(str, font, SSD1306_COLOR_WHITE);
            SSD1306_UpdateScreen();
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 3: {
            /* 清屏 */
            SSD1306_Clear();
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 4: {
            /* 填充屏幕 */
            int v = (int)jm_emap_getInt(ps, "v", 0);
            SSD1306_Fill(v ? SSD1306_COLOR_WHITE : SSD1306_COLOR_BLACK);
            SSD1306_UpdateScreen();
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 5: {
            /* 更新屏幕缓冲区 */
            SSD1306_UpdateScreen();
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 6: {
            /* 反色显示 */
            int i = (int)jm_emap_getInt(ps, "i", 0);
            SSD1306_InvertDisplay(i);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 7: {
            /* 切换反色 */
            SSD1306_ToggleInvert();
            SSD1306_UpdateScreen();
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 8: {
            /* 画点 */
            uint16_t x = (uint16_t)jm_emap_getInt(ps, "x", 0);
            uint16_t y = (uint16_t)jm_emap_getInt(ps, "y", 0);
            int color = (int)jm_emap_getInt(ps, "color", 1);
            SSD1306_DrawPixel(x, y, color ? SSD1306_COLOR_WHITE : SSD1306_COLOR_BLACK);
            SSD1306_UpdateScreen();
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 9: {
            /* 画线 */
            uint16_t x0 = (uint16_t)jm_emap_getInt(ps, "x0", 0);
            uint16_t y0 = (uint16_t)jm_emap_getInt(ps, "y0", 0);
            uint16_t x1 = (uint16_t)jm_emap_getInt(ps, "x1", 0);
            uint16_t y1 = (uint16_t)jm_emap_getInt(ps, "y1", 0);
            int color = (int)jm_emap_getInt(ps, "color", 1);
            SSD1306_DrawLine(x0, y0, x1, y1, color ? SSD1306_COLOR_WHITE : SSD1306_COLOR_BLACK);
            SSD1306_UpdateScreen();
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 10: {
            /* 画矩形 */
            uint16_t x = (uint16_t)jm_emap_getInt(ps, "x", 0);
            uint16_t y = (uint16_t)jm_emap_getInt(ps, "y", 0);
            uint16_t w = (uint16_t)jm_emap_getInt(ps, "w", 0);
            uint16_t ht = (uint16_t)jm_emap_getInt(ps, "h", 0);
            int color = (int)jm_emap_getInt(ps, "color", 1);
            SSD1306_DrawRectangle(x, y, w, ht, color ? SSD1306_COLOR_WHITE : SSD1306_COLOR_BLACK);
            SSD1306_UpdateScreen();
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 11: {
            /* 画填充矩形 */
            uint16_t x = (uint16_t)jm_emap_getInt(ps, "x", 0);
            uint16_t y = (uint16_t)jm_emap_getInt(ps, "y", 0);
            uint16_t w = (uint16_t)jm_emap_getInt(ps, "w", 0);
            uint16_t ht = (uint16_t)jm_emap_getInt(ps, "h", 0);
            int color = (int)jm_emap_getInt(ps, "color", 1);
            SSD1306_DrawFilledRectangle(x, y, w, ht, color ? SSD1306_COLOR_WHITE : SSD1306_COLOR_BLACK);
            SSD1306_UpdateScreen();
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 12: {
            /* 画三角形 */
            uint16_t x1 = (uint16_t)jm_emap_getInt(ps, "x1", 0);
            uint16_t y1 = (uint16_t)jm_emap_getInt(ps, "y1", 0);
            uint16_t x2 = (uint16_t)jm_emap_getInt(ps, "x2", 0);
            uint16_t y2 = (uint16_t)jm_emap_getInt(ps, "y2", 0);
            uint16_t x3 = (uint16_t)jm_emap_getInt(ps, "x3", 0);
            uint16_t y3 = (uint16_t)jm_emap_getInt(ps, "y3", 0);
            int color = (int)jm_emap_getInt(ps, "color", 1);
            SSD1306_DrawTriangle(x1, y1, x2, y2, x3, y3, color ? SSD1306_COLOR_WHITE : SSD1306_COLOR_BLACK);
            SSD1306_UpdateScreen();
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 13: {
            /* 画填充三角形 */
            uint16_t x1 = (uint16_t)jm_emap_getInt(ps, "x1", 0);
            uint16_t y1 = (uint16_t)jm_emap_getInt(ps, "y1", 0);
            uint16_t x2 = (uint16_t)jm_emap_getInt(ps, "x2", 0);
            uint16_t y2 = (uint16_t)jm_emap_getInt(ps, "y2", 0);
            uint16_t x3 = (uint16_t)jm_emap_getInt(ps, "x3", 0);
            uint16_t y3 = (uint16_t)jm_emap_getInt(ps, "y3", 0);
            int color = (int)jm_emap_getInt(ps, "color", 1);
            SSD1306_DrawFilledTriangle(x1, y1, x2, y2, x3, y3, color ? SSD1306_COLOR_WHITE : SSD1306_COLOR_BLACK);
            SSD1306_UpdateScreen();
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 14: {
            /* 画圆 */
            int16_t x = (int16_t)jm_emap_getInt(ps, "x", 0);
            int16_t y = (int16_t)jm_emap_getInt(ps, "y", 0);
            int16_t r = (int16_t)jm_emap_getInt(ps, "r", 10);
            int color = (int)jm_emap_getInt(ps, "color", 1);
            SSD1306_DrawCircle(x, y, r, color ? SSD1306_COLOR_WHITE : SSD1306_COLOR_BLACK);
            SSD1306_UpdateScreen();
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 15: {
            /* 画填充圆 */
            int16_t x = (int16_t)jm_emap_getInt(ps, "x", 0);
            int16_t y = (int16_t)jm_emap_getInt(ps, "y", 0);
            int16_t r = (int16_t)jm_emap_getInt(ps, "r", 10);
            int color = (int)jm_emap_getInt(ps, "color", 1);
            SSD1306_DrawFilledCircle(x, y, r, color ? SSD1306_COLOR_WHITE : SSD1306_COLOR_BLACK);
            SSD1306_UpdateScreen();
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 16: {
            /* 屏幕右滚动 */
            uint8_t start_row = (uint8_t)jm_emap_getInt(ps, "start_row", 0);
            uint8_t end_row = (uint8_t)jm_emap_getInt(ps, "end_row", 7);
            SSD1306_ScrollRight(start_row, end_row);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 17: {
            /* 屏幕左滚动 */
            uint8_t start_row = (uint8_t)jm_emap_getInt(ps, "start_row", 0);
            uint8_t end_row = (uint8_t)jm_emap_getInt(ps, "end_row", 7);
            SSD1306_ScrollLeft(start_row, end_row);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 18: {
            /* 对角右滚 */
            uint8_t start_row = (uint8_t)jm_emap_getInt(ps, "start_row", 0);
            uint8_t end_row = (uint8_t)jm_emap_getInt(ps, "end_row", 7);
            SSD1306_Scrolldiagright(start_row, end_row);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 19: {
            /* 对角左滚 */
            uint8_t start_row = (uint8_t)jm_emap_getInt(ps, "start_row", 0);
            uint8_t end_row = (uint8_t)jm_emap_getInt(ps, "end_row", 7);
            SSD1306_Scrolldiagleft(start_row, end_row);
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 20: {
            /* 停止滚动 */
            SSD1306_Stopscroll();
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        case 21: {
            /* 电源控制 */
            int v = (int)jm_emap_getInt(ps, "v", 1);
            if (v) SSD1306_ON();
            else SSD1306_OFF();
            jm_emap_putInt(h, "status", 1, false);
            break;
        }
        default: {
            jm_emap_putInt(h, "code", 1, false);
            jm_emap_putStr(h, "msg", "Invalid op", false, false);
            break;
        }
    }

    return h;
}

#endif // JM_OLED_ENABLE==1
