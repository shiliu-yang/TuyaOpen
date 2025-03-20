/**
 * @file tuya_voice_protocol.h
 * @brief Defines the core voice protocol interface for Tuya's audio service.
 *
 * This source file provides the implementation of core voice protocol
 * functionality within the Tuya audio service framework. It includes the
 * initialization, configuration, and processing functions for voice protocols,
 * which handles the integration and management of multiple transport protocols
 * including MQTT and WebSocket. The implementation supports various voice
 * features such as TTS, audio streaming, device status reporting, and protocol
 * switching capabilities. This file is essential for developers working on IoT
 * applications that require flexible voice communication protocols and
 * comprehensive audio service management.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TUYA_VOICE_PROTOCOL_H__
#define __TUYA_VOICE_PROTOCOL_H__

#include "tuya_cloud_types.h"
#include "tuya_cloud_com_defs.h"
#include "tal_log.h"
#ifdef __cplusplus
extern "C" {
#endif

#ifndef PACKED
#define PACKED __attribute__((packed))
#endif

#ifndef SAFE_MALLOC_ERR_GOTO

#define SAFE_MALLOC_ERR_GOTO(_ptr, _size, _label)                                                                      \
    do {                                                                                                               \
        if (NULL == ((_ptr) = Malloc((_size)))) {                                                                      \
            PR_ERR("Malloc err.");                                                                                     \
            rt = OPRT_MALLOC_FAILED;                                                                                   \
            goto _label;                                                                                               \
        }                                                                                                              \
        memset((_ptr), 0, (_size));                                                                                    \
    } while (0)
#endif

#ifndef SAFE_MALLOC_ERR_RET

#define SAFE_MALLOC_ERR_RET(_ptr, _size)                                                                               \
    do {                                                                                                               \
        if (NULL == ((_ptr) = Malloc((_size)))) {                                                                      \
            PR_ERR("Malloc err.");                                                                                     \
            return OPRT_MALLOC_FAILED;                                                                                 \
        }                                                                                                              \
        memset((_ptr), 0, (_size));                                                                                    \
    } while (0)
#endif

#ifndef SAFE_MALLOC

#define SAFE_MALLOC(_ptr, _size)                                                                                       \
    do {                                                                                                               \
        if (NULL == ((_ptr) = Malloc((_size)))) {                                                                      \
            PR_ERR("Malloc err.");                                                                                     \
        } else {                                                                                                       \
            memset((_ptr), 0, (_size));                                                                                \
        }                                                                                                              \
    } while (0)
#endif

#ifndef SAFE_FREE

#define SAFE_FREE(_ptr)                                                                                                \
    do {                                                                                                               \
        if (NULL != (_ptr)) {                                                                                          \
            Free((_ptr));                                                                                              \
            (_ptr) = NULL;                                                                                             \
        } else {                                                                                                       \
            PR_TRACE("Pointer is null,do not free again.");                                                            \
        }                                                                                                              \
    } while (0)
#endif

#ifndef TY_GW_CHECK_NULL_RETURN_VOID
#define TY_GW_CHECK_NULL_RETURN_VOID(x)                                                                                \
    do {                                                                                                               \
        if (NULL == (x)) {                                                                                             \
            PR_ERR("%s is null!", #x);                                                                                 \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)
#endif

typedef enum {
    TUYA_VOICE_HTTP_GET,
    TUYA_VOICE_HTTP_POST,
    TUYA_VOICE_HTTP_PUT,
    TUYA_VOICE_HTTP_INVALD
} TUYA_VOICE_HTTP_METHOD_E;

typedef enum {
    TUYA_VOICE_TASK_NORMAL = 0, // music story ...
    TUYA_VOICE_TASK_CLOCK = 1,
    TUYA_VOICE_TASK_ALERT = 2,
    TUYA_VOICE_TASK_RING_TONE = 3,
    TUYA_VOICE_TASK_CALL = 4,
    TUYA_VOICE_TASK_CALL_TTS = 5,
    TUYA_VOICE_TASK_INVALD
} TUYA_VOICE_TASK_TYPE_E;

typedef enum {
    TUYA_VOICE_AUDIO_FORMAT_WAV = 0,
    TUYA_VOICE_AUDIO_FORMAT_MP3 = 1,
    TUYA_VOICE_AUDIO_FORMAT_SPEEX = 2,
    TUYA_VOICE_AUDIO_FORMAT_AAC = 3,
    TUYA_VOICE_AUDIO_FORMAT_AMR = 4,
    TUYA_VOICE_AUDIO_FORMAT_M4A = 5,
    TUYA_VOICE_AUDIO_FORMAT_PCM = 6, // for speaker stream data play
    TUYA_VOICE_AUDIO_FORMAT_OPUS = 7,
    TUYA_VOICE_AUDIO_FORMAT_FLAC = 8,
    TUYA_VOICE_AUDIO_FORMAT_M3U8 = 9,
    TUYA_VOICE_AUDIO_FORMAT_M4B = 10,
    TUYA_VOICE_AUDIO_FORMAT_ULAW = 11,
    TUYA_VOICE_AUDIO_FORMAT_INVALD
} TUYA_VOICE_AUDIO_FORMAT_E;

typedef enum {
    TUYA_VOICE_CODE_CLOUD_SENT_COMMAND,     // Cloud sent command correctly, controlled device returned status normally
    TUYA_VOICE_CODE_CLOUD_NOT_SENT_COMMAND, // Cloud recognized command but did not send it
    TUYA_VOICE_CODE_CLOUD_NOT_IDENTIFY,     // Voice not recognized
    TUYA_VOICE_CODE_CLOUD_DEVICE_NOT_RESPONSE, // Cloud sent command but controlled device did not return status
} TUYA_VOICE_PROCESS_CODE_E;

typedef enum {
    TUYA_VOICE_THING_CONFIG_START = 0,
    TUYA_VOICE_THING_CONFIG_STOP = 1,
    TUYA_VOICE_THING_CONFIG_INVALD
} TUYA_VOICE_THING_CONFIG_MODE_E;

typedef enum {
    TUYA_VOICE_DEV_STATUS_NORMAL = 101,
    TUYA_VOICE_DEV_STATUS_CALL = 102,
    TUYA_VOICE_DEV_STATUS_BT_PLAYING_MEDIA = 103,
    TUYA_VOICE_DEV_STATUS_RESTART = 105,
} TUYA_VOICE_DEV_STATUS_E;

typedef enum {
    TUYA_VOICE_NICK_NAME_OPRT_SET = 0,
    TUYA_VOICE_NICK_NAME_OPRT_DEL = 1,
    TUYA_VOICE_NICK_NAME_OPRT_INVALD
} TUYA_VOICE_NICK_NAME_OPRT_E;

typedef enum {
    TUYA_VOICE_TEL_MODE_ANSWER = 0,
    TUYA_VOICE_TEL_MODE_REFUSE = 1,
    TUYA_VOICE_TEL_MODE_HANGUP = 2,
    TUYA_VOICE_TEL_MODE_CALL = 3,
    TUYA_VOICE_TEL_MODE_BIND = 4,
    TUYA_VOICE_TEL_MODE_UNBIND = 5,
    TUYA_VOICE_TEL_MODE_INVALD
} TUYA_VOICE_TEL_MODE_T;

#define TUYA_VOICE_SESSION_ID_MAX_LEN   (64)
#define TUYA_VOICE_CALLBACK_VAL_MAX_LEN (32)
#define TUYA_VOICE_MESSAGE_ID_MAX_LEN   (64)
#define TUYA_VOICE_SONGNAME_MAX_LEN     (128)
#define TUYA_VOICE_ARTIST_MAX_LEN       (64)

typedef struct {
    char *url;
    char *req_body;
    TUYA_VOICE_AUDIO_FORMAT_E format;
    BOOL_T keep_session;
    TUYA_VOICE_HTTP_METHOD_E http_method;
    TUYA_VOICE_TASK_TYPE_E task_type;
    char session_id[TUYA_VOICE_SESSION_ID_MAX_LEN + 1];
    char message_id[TUYA_VOICE_MESSAGE_ID_MAX_LEN + 1];
    char callback_val[TUYA_VOICE_CALLBACK_VAL_MAX_LEN + 1];
} TUYA_VOICE_TTS_S;

typedef struct {
    uint32_t id;
    char *url;
    char *req_body;
    uint32_t length;
    uint32_t duration;
    TUYA_VOICE_AUDIO_FORMAT_E format;
    TUYA_VOICE_HTTP_METHOD_E http_method;
    char artist[TUYA_VOICE_ARTIST_MAX_LEN];
    char song_name[TUYA_VOICE_SONGNAME_MAX_LEN];
} TUYA_VOICE_MEDIA_SRC_S;

typedef struct {
    TUYA_VOICE_TTS_S *pre_tts;
    int src_cnt;
    TUYA_VOICE_MEDIA_SRC_S *src_array;
} TUYA_VOICE_MEDIA_S;

typedef struct {
    TUYA_VOICE_TTS_S *pre_tts;
    char target_id[64];
    char target_name[128];
    int type;
} TUYA_VOICE_CALL_PHONE_INFO_S;

typedef enum {
    TUYA_VOICE_STREAM_START,
    TUYA_VOICE_STREAM_DATA,
    TUYA_VOICE_STREAM_STOP,
    TUYA_VOICE_STREAM_ABORT,
} TUYA_VOICE_STREAM_E;

typedef struct {
    char *url;
    char *req_body;
    TUYA_VOICE_AUDIO_FORMAT_E format;
    TUYA_VOICE_HTTP_METHOD_E http_method;
    int duration_time;
    char callback_val[TUYA_VOICE_CALLBACK_VAL_MAX_LEN + 1];
} TUYA_VOICE_BGM_S;

typedef struct {
    void (*tuya_voice_audio_sync)(void);
    void (*tuya_voice_play_tts)(TUYA_VOICE_TTS_S *tts);
    void (*tuya_voice_play_audio)(TUYA_VOICE_MEDIA_S *media);
    void (*tuya_voice_custom)(char *type, cJSON *json);
    void (*tuya_voice_cloud_code_process)(TUYA_VOICE_PROCESS_CODE_E code);

    void (*tuya_voice_thing_config)(TUYA_VOICE_THING_CONFIG_MODE_E mode, char *token, uint32_t timeout);
    void (*tuya_voice_nick_name)(TUYA_VOICE_NICK_NAME_OPRT_E oprt, char *nickname, char *pinyin);
    void (*tuya_voice_dnd_mode)(BOOL_T enable, char *start_time, char *end_time, int stamp);
    void (*tuya_voice_subdev_access)(int count);

    void (*tuya_voice_call_phone_v2)(TUYA_VOICE_CALL_PHONE_INFO_S *call_info);

    void (*tuya_voice_tel_operate)(TUYA_VOICE_TEL_MODE_T mode);
    void (*tuya_voice_call_second_dial)(char *dial);
    void (*tuya_voice_tts_stream)(TUYA_VOICE_STREAM_E type, uint8_t *data, int len);
    void (*tuya_voice_play_bgm_audio)(TUYA_VOICE_BGM_S *bgm);

    void (*tuya_voice_text_stream)(TUYA_VOICE_STREAM_E type, uint8_t *data, int len);
} TUYA_VOICE_CBS_S;

OPERATE_RET tuya_voice_proto_init(TUYA_VOICE_CBS_S *cbs);

OPERATE_RET tuya_voice_proto_deinit(void);

OPERATE_RET tuya_voice_proto_audio_report_progress(uint32_t id, uint32_t offset, uint32_t total);

OPERATE_RET tuya_voice_proto_audio_request_next(uint32_t id, BOOL_T need_tts);

OPERATE_RET tuya_voice_proto_audio_request_prev(uint32_t id, BOOL_T need_tts);

OPERATE_RET tuya_voice_proto_audio_request_current(void);

OPERATE_RET tuya_voice_proto_audio_request_playmusic(void);

OPERATE_RET tuya_voice_proto_audio_collect(uint32_t id);

OPERATE_RET tuya_voice_proto_bell_request(char *bell_data_json);

OPERATE_RET tuya_voice_proto_tts_complete_report(char *callback_val);

OPERATE_RET tuya_voice_proto_tts_get(char *tts_content);

OPERATE_RET tuya_voice_proto_devinfo_report(char *devinfo_json);

OPERATE_RET tuya_voice_proto_common_report(char *p_data);

OPERATE_RET tuya_voice_proto_thing_config_stop_report(void);

OPERATE_RET tuya_voice_proto_thing_config_request_report(void);

OPERATE_RET tuya_voice_proto_thing_config_reject_report(void);

OPERATE_RET tuya_voice_proto_thing_config_access_count_report(int count);

OPERATE_RET tuya_voice_proto_nick_name_report(TUYA_VOICE_NICK_NAME_OPRT_E mode, char *nickname, char *pinyin,
                                              BOOL_T set_result);

OPERATE_RET tuya_voice_proto_dndmode_report(BOOL_T set_result, int stamp);

OPERATE_RET tuya_voice_proto_dev_status_report(TUYA_VOICE_DEV_STATUS_E status);

OPERATE_RET tuya_voice_proto_online_local_asr_sync(void);
// for websocket use
OPERATE_RET tuya_voice_proto_start(void);
OPERATE_RET tuya_voice_proto_stop(void);
OPERATE_RET tuya_voice_proto_del_domain_name(void);
OPERATE_RET tuya_voice_proto_get_tts_text(char *p_tts_text);
OPERATE_RET tuya_voice_proto_get_tts_audio(char *p_session_id, char *p_tts_text, char *p_declaimer);
BOOL_T tuya_voice_proto_is_online(void);
void tuya_voice_proto_disconnect(void);
OPERATE_RET tuya_voice_proto_interrupt(void);

void tuya_voice_proto_set_keepalve(uint32_t sec);

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_VOICE_PROTOCOL_H__ */
