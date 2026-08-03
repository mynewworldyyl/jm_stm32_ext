#ifndef JM_STM32_BUF_H_
#define JM_STM32_BUF_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *data;
    uint16_t capacity;
    uint16_t rpos;
    uint16_t wpos;
    bool need_free;
} jm_buf_t;

jm_buf_t *jm_buf_create(uint16_t capacity);
jm_buf_t *jm_buf_wrap_array(const uint8_t *data, uint16_t len);
void jm_buf_release(jm_buf_t *buf);
void jm_buf_clear(jm_buf_t *buf);
uint16_t jm_buf_readable_len(const jm_buf_t *buf);
uint16_t jm_buf_writeable_len(const jm_buf_t *buf);
bool jm_buf_reset(jm_buf_t *buf);
bool jm_buf_set_rpos(jm_buf_t *buf, uint16_t rpos);
bool jm_buf_set_wpos(jm_buf_t *buf, uint16_t wpos);
bool jm_buf_move_forward(jm_buf_t *buf, uint16_t count);

bool jm_buf_get_u8(jm_buf_t *buf, uint8_t *val);
bool jm_buf_get_s8(jm_buf_t *buf, int8_t *val);
bool jm_buf_get_bool(jm_buf_t *buf, bool *val);
bool jm_buf_get_u16(jm_buf_t *buf, uint16_t *val);
bool jm_buf_get_s16(jm_buf_t *buf, int16_t *val);
bool jm_buf_get_s32(jm_buf_t *buf, int32_t *val);
bool jm_buf_get_u32(jm_buf_t *buf, uint32_t *val);
bool jm_buf_get_bytes(jm_buf_t *buf, uint8_t *dst, uint16_t len);
bool jm_buf_get_chars(jm_buf_t *buf, char *dst, uint16_t len);
char *jm_buf_read_string(jm_buf_t *buf, int8_t *flag);
const uint8_t *jm_buf_read_buf(const jm_buf_t *buf);

bool jm_buf_put_u8(jm_buf_t *buf, uint8_t val);
bool jm_buf_put_s8(jm_buf_t *buf, int8_t val);
bool jm_buf_put_bool(jm_buf_t *buf, bool val);
bool jm_buf_put_u16(jm_buf_t *buf, uint16_t val);
bool jm_buf_put_s16(jm_buf_t *buf, int16_t val);
bool jm_buf_put_s32(jm_buf_t *buf, int32_t val);
bool jm_buf_put_u32(jm_buf_t *buf, uint32_t val);
bool jm_buf_put_bytes(jm_buf_t *buf, const uint8_t *src, int16_t len);
bool jm_buf_put_chars(jm_buf_t *buf, const char *src, int16_t len);
bool jm_buf_write_string(jm_buf_t *buf, const char *str, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif
