/**
 * @file jm_stm32_buf.h
 * @brief 环形缓冲区 / 字节缓冲区操作库
 *
 * 提供动态分配的缓冲区结构 @ref jm_buf_t，支持字节序读写、字符串读写等操作。
 * 用于串口协议数据的封包与解包。
 */

#ifndef JM_STM32_BUF_H_
#define JM_STM32_BUF_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 动态字节缓冲区结构
 */
typedef struct {
    uint8_t *data;       /**< 数据缓冲区指针 */
    uint16_t capacity;   /**< 缓冲区总容量 */
    uint16_t rpos;       /**< 读取位置 */
    uint16_t wpos;       /**< 写入位置 */
    bool need_free;      /**< 是否需要释放 data 指针 */
} jm_buf_t;

/**
 * @brief 创建一个指定容量的动态缓冲区
 * @param capacity 缓冲区容量
 * @return 缓冲区指针（need_free=true），失败返回 NULL
 */
jm_buf_t *jm_buf_create(uint16_t capacity);

/**
 * @brief 用已有数组创建缓冲区（不复制数据）
 * @param data 数据指针
 * @param len  数据长度
 * @return 缓冲区指针（need_free=false），失败返回 NULL
 */
jm_buf_t *jm_buf_wrap_array(const uint8_t *data, uint16_t len);

/**
 * @brief 释放缓冲区
 * @param buf 缓冲区指针
 */
void jm_buf_release(jm_buf_t *buf);

/**
 * @brief 清空缓冲区（重置读写位置）
 * @param buf 缓冲区指针
 */
void jm_buf_clear(jm_buf_t *buf);

/**
 * @brief 获取可读数据长度
 * @param buf 缓冲区指针
 * @return 可读字节数
 */
uint16_t jm_buf_readable_len(const jm_buf_t *buf);

/**
 * @brief 获取可写空间长度
 * @param buf 缓冲区指针
 * @return 可写字节数
 */
uint16_t jm_buf_writeable_len(const jm_buf_t *buf);

/**
 * @brief 重置缓冲区读写位置为 0
 * @param buf 缓冲区指针
 * @return true 成功
 */
bool jm_buf_reset(jm_buf_t *buf);

/**
 * @brief 设置读取位置
 * @param buf  缓冲区指针
 * @param rpos 新读取位置
 * @return true 成功，false 参数非法
 */
bool jm_buf_set_rpos(jm_buf_t *buf, uint16_t rpos);

/**
 * @brief 设置写入位置
 * @param buf  缓冲区指针
 * @param wpos 新写入位置
 * @return true 成功，false 参数非法
 */
bool jm_buf_set_wpos(jm_buf_t *buf, uint16_t wpos);

/**
 * @brief 向前移动读取位置
 * @param buf   缓冲区指针
 * @param count 移动的字节数
 * @return true 成功，false 参数非法
 */
bool jm_buf_move_forward(jm_buf_t *buf, uint16_t count);

/* ---- 读操作（小端序） ---- */

/**
 * @brief 读取一个 uint8
 * @param buf  缓冲区指针
 * @param val  输出值
 * @return true 成功
 */
bool jm_buf_get_u8(jm_buf_t *buf, uint8_t *val);
/**
 * @brief 读取一个 int8
 * @param buf  缓冲区指针
 * @param val  输出值
 * @return true 成功
 */
bool jm_buf_get_s8(jm_buf_t *buf, int8_t *val);
/**
 * @brief 读取一个 bool
 * @param buf  缓冲区指针
 * @param val  输出值
 * @return true 成功
 */
bool jm_buf_get_bool(jm_buf_t *buf, bool *val);
/**
 * @brief 读取一个 uint16（大端序）
 * @param buf  缓冲区指针
 * @param val  输出值
 * @return true 成功
 */
bool jm_buf_get_u16(jm_buf_t *buf, uint16_t *val);
/**
 * @brief 读取一个 int16（大端序）
 * @param buf  缓冲区指针
 * @param val  输出值
 * @return true 成功
 */
bool jm_buf_get_s16(jm_buf_t *buf, int16_t *val);
/**
 * @brief 读取一个 int32（大端序）
 * @param buf  缓冲区指针
 * @param val  输出值
 * @return true 成功
 */
bool jm_buf_get_s32(jm_buf_t *buf, int32_t *val);
/**
 * @brief 读取一个 uint32（大端序）
 * @param buf  缓冲区指针
 * @param val  输出值
 * @return true 成功
 */
bool jm_buf_get_u32(jm_buf_t *buf, uint32_t *val);
/**
 * @brief 读取指定长度的字节数据
 * @param buf  缓冲区指针
 * @param dst  输出缓冲区
 * @param len  读取字节数
 * @return true 成功
 */
bool jm_buf_get_bytes(jm_buf_t *buf, uint8_t *dst, uint16_t len);
/**
 * @brief 读取指定长度的字符数据
 * @param buf  缓冲区指针
 * @param dst  输出缓冲区
 * @param len  读取字符数
 * @return true 成功
 */
bool jm_buf_get_chars(jm_buf_t *buf, char *dst, uint16_t len);
/**
 * @brief 读取一个字符串（支持长度前缀）
 *
 * 字符串格式：[len_byte] 或 [0x00][len_hi len_lo]，后跟字符串内容。
 *
 * @param buf  缓冲区指针
 * @param flag 输出标志：0=空串，1=成功，-1=错误
 * @return 字符串指针（已分配内存，需 free），失败返回 NULL
 */
char *jm_buf_read_string(jm_buf_t *buf, int8_t *flag);

/**
 * @brief 获取当前可读数据的指针（不移动 rpos）
 * @param buf 缓冲区指针
 * @return 数据指针
 */
const uint8_t *jm_buf_read_buf(const jm_buf_t *buf);

/* ---- 写操作（大端序） ---- */

/**
 * @brief 写入一个 uint8
 * @param buf  缓冲区指针
 * @param val  值
 * @return true 成功
 */
bool jm_buf_put_u8(jm_buf_t *buf, uint8_t val);
/**
 * @brief 写入一个 int8
 * @param buf  缓冲区指针
 * @param val  值
 * @return true 成功
 */
bool jm_buf_put_s8(jm_buf_t *buf, int8_t val);
/**
 * @brief 写入一个 bool
 * @param buf  缓冲区指针
 * @param val  值
 * @return true 成功
 */
bool jm_buf_put_bool(jm_buf_t *buf, bool val);
/**
 * @brief 写入一个 uint16（大端序）
 * @param buf  缓冲区指针
 * @param val  值
 * @return true 成功
 */
bool jm_buf_put_u16(jm_buf_t *buf, uint16_t val);
/**
 * @brief 写入一个 int16（大端序）
 * @param buf  缓冲区指针
 * @param val  值
 * @return true 成功
 */
bool jm_buf_put_s16(jm_buf_t *buf, int16_t val);
/**
 * @brief 写入一个 int32（大端序）
 * @param buf  缓冲区指针
 * @param val  值
 * @return true 成功
 */
bool jm_buf_put_s32(jm_buf_t *buf, int32_t val);
/**
 * @brief 写入一个 uint32（大端序）
 * @param buf  缓冲区指针
 * @param val  值
 * @return true 成功
 */
bool jm_buf_put_u32(jm_buf_t *buf, uint32_t val);
/**
 * @brief 写入指定长度的字节数据
 * @param buf  缓冲区指针
 * @param src  源数据
 * @param len  数据长度
 * @return true 成功
 */
bool jm_buf_put_bytes(jm_buf_t *buf, const uint8_t *src, int16_t len);
/**
 * @brief 写入指定长度的字符数据
 * @param buf  缓冲区指针
 * @param src  源字符串
 * @param len  字符长度
 * @return true 成功
 */
bool jm_buf_put_chars(jm_buf_t *buf, const char *src, int16_t len);

/**
 * @brief 写入字符串（带长度前缀）
 *
 * 若长度 < 127：写入 1 字节长度 + 字符串。
 * 若长度 >= 127：写入 0x00 + 2 字节长度（大端序） + 字符串。
 *
 * @param buf  缓冲区指针
 * @param str  字符串
 * @param len  字符串长度
 * @return true 成功
 */
bool jm_buf_write_string(jm_buf_t *buf, const char *str, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif
