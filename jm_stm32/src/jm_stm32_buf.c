/**
 * @file jm_stm32_buf.c
 * @brief 字节缓冲区操作实现
 */

#include "jm_stm32_buf.h"

jm_buf_t *jm_buf_create(uint16_t capacity)
{
    jm_buf_t *buf = (jm_buf_t *)malloc(sizeof(jm_buf_t));
    if (!buf) return NULL;
    buf->data = (uint8_t *)malloc(capacity);
    if (!buf->data) {
        free(buf);
        return NULL;
    }
    buf->capacity = capacity;
    buf->rpos = 0;
    buf->wpos = 0;
    buf->need_free = true;
    return buf;
}

jm_buf_t *jm_buf_wrap_array(const uint8_t *data, uint16_t len)
{
    jm_buf_t *buf = (jm_buf_t *)malloc(sizeof(jm_buf_t));
    if (!buf) return NULL;
    buf->data = (uint8_t *)data;
    buf->capacity = len;
    buf->rpos = 0;
    buf->wpos = len;
    buf->need_free = false;
    return buf;
}

void jm_buf_release(jm_buf_t *buf)
{
    if (!buf) return;
    if (buf->need_free && buf->data) {
        free(buf->data);
    }
    free(buf);
}

void jm_buf_clear(jm_buf_t *buf)
{
    if (!buf) return;
    buf->rpos = 0;
    buf->wpos = 0;
}

uint16_t jm_buf_readable_len(const jm_buf_t *buf)
{
    if (!buf) return 0;
    return buf->wpos - buf->rpos;
}

uint16_t jm_buf_writeable_len(const jm_buf_t *buf)
{
    if (!buf) return 0;
    return buf->capacity - buf->wpos;
}

bool jm_buf_reset(jm_buf_t *buf)
{
    if (!buf) return false;
    buf->rpos = 0;
    buf->wpos = 0;
    return true;
}

bool jm_buf_set_rpos(jm_buf_t *buf, uint16_t rpos)
{
    if (!buf || rpos > buf->wpos) return false;
    buf->rpos = rpos;
    return true;
}

bool jm_buf_set_wpos(jm_buf_t *buf, uint16_t wpos)
{
    if (!buf || wpos > buf->capacity) return false;
    buf->wpos = wpos;
    return true;
}

bool jm_buf_move_forward(jm_buf_t *buf, uint16_t count)
{
    if (!buf || buf->rpos + count > buf->wpos) return false;
    buf->rpos += count;
    return true;
}

static bool check_read_len(jm_buf_t *buf, uint16_t len)
{
    if (!buf || buf->rpos + len > buf->wpos) return false;
    return true;
}

static bool check_write_len(jm_buf_t *buf, uint16_t len)
{
    if (!buf || buf->wpos + len > buf->capacity) return false;
    return true;
}

bool jm_buf_get_u8(jm_buf_t *buf, uint8_t *val)
{
    if (!check_read_len(buf, 1)) return false;
    *val = buf->data[buf->rpos++];
    return true;
}

bool jm_buf_get_s8(jm_buf_t *buf, int8_t *val)
{
    if (!check_read_len(buf, 1)) return false;
    *val = (int8_t)buf->data[buf->rpos++];
    return true;
}

bool jm_buf_get_char(jm_buf_t *buf, char *val)
{
    int8_t v;
    if (!jm_buf_get_s8(buf, &v)) return false;
    *val = (char)v;
    return true;
}

bool jm_buf_get_bool(jm_buf_t *buf, bool *val)
{
    if (!check_read_len(buf, 1)) return false;
    *val = buf->data[buf->rpos++] != 0;
    return true;
}

bool jm_buf_get_u16(jm_buf_t *buf, uint16_t *val)
{
    if (!check_read_len(buf, 2)) return false;
    *val = ((uint16_t)buf->data[buf->rpos] << 8) | buf->data[buf->rpos + 1];
    buf->rpos += 2;
    return true;
}

bool jm_buf_get_s16(jm_buf_t *buf, int16_t *val)
{
    if (!check_read_len(buf, 2)) return false;
    *val = (int16_t)(((uint16_t)buf->data[buf->rpos] << 8) | buf->data[buf->rpos + 1]);
    buf->rpos += 2;
    return true;
}

bool jm_buf_get_s32(jm_buf_t *buf, int32_t *val)
{
    if (!check_read_len(buf, 4)) return false;
    *val = ((int32_t)buf->data[buf->rpos] << 24) |
           ((int32_t)buf->data[buf->rpos + 1] << 16) |
           ((int32_t)buf->data[buf->rpos + 2] << 8) |
           buf->data[buf->rpos + 3];
    buf->rpos += 4;
    return true;
}

bool jm_buf_get_s64(jm_buf_t *buf, int64_t *val)
{
    if (!check_read_len(buf, 8)) return false;
    *val = ((int64_t)buf->data[buf->rpos] << 56) |
           ((int64_t)buf->data[buf->rpos + 1] << 48) |
           ((int64_t)buf->data[buf->rpos + 2] << 40) |
           ((int64_t)buf->data[buf->rpos + 3] << 32) |
           ((int64_t)buf->data[buf->rpos + 4] << 24) |
           ((int64_t)buf->data[buf->rpos + 5] << 16) |
           ((int64_t)buf->data[buf->rpos + 6] << 8) |
           buf->data[buf->rpos + 7];
    buf->rpos += 8;
    return true;
}

bool jm_buf_get_u32(jm_buf_t *buf, uint32_t *val)
{
    if (!check_read_len(buf, 4)) return false;
    *val = ((uint32_t)buf->data[buf->rpos] << 24) |
           ((uint32_t)buf->data[buf->rpos + 1] << 16) |
           ((uint32_t)buf->data[buf->rpos + 2] << 8) |
           buf->data[buf->rpos + 3];
    buf->rpos += 4;
    return true;
}

bool jm_buf_get_bytes(jm_buf_t *buf, uint8_t *dst, uint16_t len)
{
    if (!check_read_len(buf, len)) return false;
    memcpy(dst, &buf->data[buf->rpos], len);
    buf->rpos += len;
    return true;
}

bool jm_buf_get_chars(jm_buf_t *buf, char *dst, uint16_t len)
{
    if (!check_read_len(buf, len)) return false;
    memcpy(dst, &buf->data[buf->rpos], len);
    buf->rpos += len;
    return true;
}

char *jm_buf_read_string(jm_buf_t *buf, int8_t *flag)
{
    uint16_t len = 0;
    uint8_t b = 0;

    if (!check_read_len(buf, 1)) {
        if (flag) *flag = -1;
        return NULL;
    }

    b = buf->data[buf->rpos++];

    if (b == 0) {
        if (flag) *flag = 0;
        return NULL;
    }

    if (b < 127) {
        len = b;
    } else {
        if (!check_read_len(buf, 2)) {
            buf->rpos--;
            if (flag) *flag = -1;
            return NULL;
        }
        len = ((uint16_t)buf->data[buf->rpos] << 8) | buf->data[buf->rpos + 1];
        buf->rpos += 2;
    }

    if (!check_read_len(buf, len)) {
        if (flag) *flag = -1;
        return NULL;
    }

    char *str = (char *)malloc(len + 1);
    if (!str) {
        if (flag) *flag = -1;
        return NULL;
    }
    memcpy(str, &buf->data[buf->rpos], len);
    str[len] = '\0';
    buf->rpos += len;

    if (flag) *flag = 1;
    return str;
}

bool jm_buf_put_u8(jm_buf_t *buf, uint8_t val)
{
    if (!check_write_len(buf, 1)) return false;
    buf->data[buf->wpos++] = val;
    return true;
}

bool jm_buf_put_s8(jm_buf_t *buf, int8_t val)
{
    if (!check_write_len(buf, 1)) return false;
    buf->data[buf->wpos++] = (uint8_t)val;
    return true;
}

bool jm_buf_put_char(jm_buf_t *buf, char val)
{
    return jm_buf_put_s8(buf, (int8_t)val);
}

bool jm_buf_put_bool(jm_buf_t *buf, bool val)
{
    if (!check_write_len(buf, 1)) return false;
    buf->data[buf->wpos++] = val ? 1 : 0;
    return true;
}

bool jm_buf_put_u16(jm_buf_t *buf, uint16_t val)
{
    if (!check_write_len(buf, 2)) return false;
    buf->data[buf->wpos++] = (uint8_t)(val >> 8);
    buf->data[buf->wpos++] = (uint8_t)(val & 0xFF);
    return true;
}

bool jm_buf_put_s16(jm_buf_t *buf, int16_t val)
{
    if (!check_write_len(buf, 2)) return false;
    buf->data[buf->wpos++] = (uint8_t)(val >> 8);
    buf->data[buf->wpos++] = (uint8_t)(val & 0xFF);
    return true;
}

bool jm_buf_put_s32(jm_buf_t *buf, int32_t val)
{
    if (!check_write_len(buf, 4)) return false;
    buf->data[buf->wpos++] = (uint8_t)(val >> 24);
    buf->data[buf->wpos++] = (uint8_t)(val >> 16);
    buf->data[buf->wpos++] = (uint8_t)(val >> 8);
    buf->data[buf->wpos++] = (uint8_t)(val & 0xFF);
    return true;
}

bool jm_buf_put_s64(jm_buf_t *buf, int64_t val)
{
    if (!check_write_len(buf, 8)) return false;
    buf->data[buf->wpos++] = (uint8_t)(val >> 56);
    buf->data[buf->wpos++] = (uint8_t)(val >> 48);
    buf->data[buf->wpos++] = (uint8_t)(val >> 40);
    buf->data[buf->wpos++] = (uint8_t)(val >> 32);
    buf->data[buf->wpos++] = (uint8_t)(val >> 24);
    buf->data[buf->wpos++] = (uint8_t)(val >> 16);
    buf->data[buf->wpos++] = (uint8_t)(val >> 8);
    buf->data[buf->wpos++] = (uint8_t)(val & 0xFF);
    return true;
}

bool jm_buf_put_u32(jm_buf_t *buf, uint32_t val)
{
    if (!check_write_len(buf, 4)) return false;
    buf->data[buf->wpos++] = (uint8_t)(val >> 24);
    buf->data[buf->wpos++] = (uint8_t)(val >> 16);
    buf->data[buf->wpos++] = (uint8_t)(val >> 8);
    buf->data[buf->wpos++] = (uint8_t)(val & 0xFF);
    return true;
}

bool jm_buf_put_bytes(jm_buf_t *buf, const uint8_t *src, int16_t len)
{
    if (!buf || !src || len <= 0) return false;
    if (!check_write_len(buf, (uint16_t)len)) return false;
    memcpy(&buf->data[buf->wpos], src, len);
    buf->wpos += (uint16_t)len;
    return true;
}

bool jm_buf_put_chars(jm_buf_t *buf, const char *src, int16_t len)
{
    if (!buf || !src || len <= 0) return false;
    if (!check_write_len(buf, (uint16_t)len)) return false;
    memcpy(&buf->data[buf->wpos], src, len);
    buf->wpos += (uint16_t)len;
    return true;
}

bool jm_buf_write_string(jm_buf_t *buf, const char *str, uint16_t len)
{
    if (!buf || !str) return false;
    if (len == 0) {
        return jm_buf_put_u8(buf, 0);
    }
    if (len < 127) {
        if (!jm_buf_put_u8(buf, (uint8_t)len)) return false;
    } else {
        if (!jm_buf_put_u8(buf, 0)) return false;
        if (!jm_buf_put_u16(buf, len)) return false;
    }
    return jm_buf_put_chars(buf, str, len);
}

const uint8_t *jm_buf_read_buf(const jm_buf_t *buf)
{
    if (!buf) return NULL;
    return &buf->data[buf->rpos];
}
