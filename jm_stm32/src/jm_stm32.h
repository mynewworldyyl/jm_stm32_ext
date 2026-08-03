#ifndef JM_STM32_H_
#define JM_STM32_H_

#include "jm_pcfg.h"
#include "jm_sensor.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JM_STM32_VERSION "1.0.0"

#define JM_SDADA_CHECK_NUM       0x55
#define JM_SDATA_HEADER_SIZE     3
#define JM_MAX_SERIAL_BLOCK_SIZE 1024
#define JM_SERIAL_MAX_PCK_SIZE   4096

#define JM_SERIALNET_TYPE_UDP      1
#define JM_SERIALNET_TYPE_TCP      2
#define JM_SERIALNET_TYPE_SERIAL   3
#define JM_SERIALNET_TYPE_UDP_COM  4
#define JM_SERIALNET_TYPE_SYS      5

#define PREFIX_TYPE_NULL            -128
#define PREFIX_TYPE_MAP             -122
#define PREFIX_TYPE_BYTE            -121
#define PREFIX_TYPE_SHORTT_TYPE     -120
#define PREFIX_TYPE_INT             -119
#define PREFIX_TYPE_LONG            -118
#define PREFIX_TYPE_STRINGG         -113

#define JM_TASK_APP_PROXY_TCP_CONNECTED    1
#define JM_TASK_APP_PROXY_TCP_DISCONNECTED 2
#define JM_TASK_APP_PROXY_TCP_SEND         3
#define JM_TASK_APP_PROXY_TCP_ERR          5
#define JM_TASK_APP_PROXY_WIFI_CFG         6
#define JM_TASK_APP_PROXY_WIFI_CONNECTED   7
#define JM_TASK_APP_PROXY_INTERNET_ENABLE  10
#define JM_TASK_APP_PROXY_WIFI_IS_ENABLE   12
#define JM_TASK_APP_PROXY_HB               13
#define JM_TASK_APP_PROXY_TCP_CLOSE        14
#define JM_TASK_APP_PROXY_UID              15
#define JM_TASK_APP_PROXY_LOGIN_RESULT     17
#define JM_TASK_APP_PROXY_LOGIN            19
#define JM_TASK_APP_PROXY_SYS_CFG          20
#define JM_TASK_APP_PROXY_TRANS_CMD        21
#define JM_TASK_APP_PROXY_AUDIO_PLAY       22
#define JM_TASK_APP_PROXY_CTRL_CMD         29
#define JM_TASK_APP_PROXY_CTRL_RST         30
#define JM_TASK_APP_PROXY_CTRL_EVENT       27

#define JM_SUCCESS                  0
#define JM_ERR_MEMORY              -1
#define JM_ERR_INVALID_PACKET      -2
#define JM_ERR_CHECKSUM            -3
#define JM_ERR_TIMEOUT             -4
#define JM_ERR_NOT_READY           -5

#define JM_EVENT_WIFI_STATUS       1
#define JM_EVENT_INTERNET_STATUS   2
#define JM_EVENT_LOGIN_RESULT      3
#define JM_EVENT_TCP_CONNECTED     4
#define JM_EVENT_TCP_DISCONNECTED  5
#define JM_EVENT_TCP_SEND_RESULT   6
#define JM_EVENT_TCP_ERROR         7
#define JM_EVENT_UID_RESPONSE      8
#define JM_EVENT_SYS_CFG           9
#define JM_EVENT_TRANS_CMD         10
#define JM_EVENT_CTRL_EVENT        11
#define JM_EVENT_CTRL_CMD          14
#define JM_EVENT_TCP_DATA          12
#define JM_EVENT_UDP_DATA          13

void jm_log_char(char ch);
void jm_log_print(const char *format, ...);


#include <stdarg.h>

#define JM_LOG_DEBUG_ENABLE 1
#define JM_LOG_ERROR_ENABLE 1

#define JM_LOG_LINE(format, ...) \
    do { jm_log_print((const char*)format, ## __VA_ARGS__); jm_log_char('\n'); } while(0)

#if JM_LOG_DEBUG_ENABLE
#define JM_LOG_D(format, ...) JM_LOG_LINE(format, ## __VA_ARGS__);
#else
#define JM_LOG_D(format, ...)
#endif

#if JM_LOG_ERROR_ENABLE
#define JM_LOG_E(format, ...) JM_LOG_LINE(format, ## __VA_ARGS__);
#else
#define JM_LOG_E(format, ...)
#endif


#include "jm_stm32_buf.h"

typedef struct {
    int8_t sock;
    char host[64];
    uint16_t port;
    int8_t err_code;
    char err_msg[64];
} jm_tcp_conn_info_t;

typedef struct {
    uint32_t devId; //设备ID
    bool wifi_enabled; //Wifi是否可用
    bool isLogin; //是否已经登录JM平台
    //char sta_ip[16];
    //char sta_mac[18];
} jm_wifi_status_t;

typedef struct {
    int32_t login_code;
    uint32_t dev_uid;
    int32_t act_id;
    int32_t client_id;
    int8_t grp_id;
    char login_key[64];
} jm_login_result_t;

typedef struct {
    int8_t result_code;
} jm_uid_response_t;

typedef struct {
    uint8_t data[128];
    uint16_t len;
} jm_sys_cfg_t;

typedef void (*jm_event_callback_t)(uint8_t event_type,
                                    uint16_t sub_type,
                                    void *data,
                                    void *user_data);

typedef struct {
    uint32_t (*get_sys_time_ms)(void);
    void (*uart_send)(const uint8_t *data, uint16_t len);
    void (*uart_send_log)(const uint8_t *data, uint16_t len);
    jm_event_callback_t event_cb;
    void *user_data;
} jm_config_t;

// Minimal map types for control commands on STM32
typedef struct jm_emap_node {
    char *key;
    int32_t ival;
    char *sval;
    bool is_int;
    bool copy_key;
    bool copy_val;
    struct jm_emap_node *next;
} jm_emap_node_t;

typedef struct {
    jm_emap_node_t *head;
    uint8_t type;
} jm_emap_t;

typedef jm_emap_t* (*jm_ctrl_fn_t)(jm_emap_t *ps);

typedef struct {
    int32_t defId;
    jm_ctrl_fn_t fn;
} jm_ctrl_item_t;

jm_emap_t *jm_emap_create(uint8_t type);
void jm_emap_release(jm_emap_t *map);
bool jm_emap_putInt(jm_emap_t *map, const char *key, int32_t val, bool copyKey);
bool jm_emap_putStr(jm_emap_t *map, const char *key, const char *val, bool needFreeMem, bool copyKey);
bool jm_emap_putByte(jm_emap_t *map, const char *key, int8_t val, bool copyKey);
int32_t jm_emap_getInt(jm_emap_t *map, const char *key, int32_t def);
int8_t jm_emap_getByte(jm_emap_t *map, const char *key, int8_t def);
char *jm_emap_getStr(jm_emap_t *map, const char *key);
bool jm_emap_exist(jm_emap_t *map, const char *key);
bool jm_emap_encode(const jm_emap_t *map, jm_buf_t *buf);
jm_emap_t *jm_emap_decode(const uint8_t *data, uint16_t len);

bool jm_ctrl_registFun(jm_ctrl_fn_t fn, int32_t defId);
jm_emap_t *jm_ctrl_invokeFunc(jm_emap_t *ps);

int jm_stm32_send_ctrl_rst(uint16_t req_id, jm_emap_t *rst);

int jm_stm32_init(const jm_config_t *config);
void jm_stm32_loop(void);
bool jm_stm32_uart_push_byte(uint8_t byte);
void jm_stm32_uart_rx_byte(uint8_t byte);

int jm_serial_read(void *huart);
int jm_serial_write(const uint8_t *data, uint16_t len);

int jm_stm32_send_uid();
int jm_stm32_send_wifi_cfg(const char *ssid, const char *pwd);
int jm_stm32_send_wifi_status_req(void);
int jm_stm32_send_internet_status_req(void);
int jm_stm32_send_login(void);
int jm_stm32_send_tcp_connect(const char *host, uint16_t port);
int jm_stm32_send_tcp_close(int8_t sock);
int jm_stm32_send_tcp_data(int8_t sock, const uint8_t *data, uint16_t len);
int jm_stm32_send_audio_play(const char *text);
int jm_stm32_send_ctrl_event(const uint8_t *data, uint16_t len);
int jm_stm32_send_trans_cmd_response(uint16_t req_id, const uint8_t *data, uint16_t len);
int jm_stm32_send_ctrl_rst(uint16_t req_id, jm_emap_t *rst);

void jm_comp_init(void);
void jm_comp_loop(void);

/**
  * @brief  微秒级延时
  * @param  xus 延时时长，范围：0~233015
  * @retval 无
  */
void jm_delay_us(uint32_t xus);

/**
  * @brief  毫秒级延时
  * @param  xms 延时时长，范围：0~4294967295
  * @retval 无
  */
void jm_delay_ms(uint32_t xms);

#ifdef __cplusplus
}
#endif

#endif
