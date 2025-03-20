/**
 * @file tuya_voice_protocol_ws.c
 * @brief Defines the WebSocket protocol implementation for Tuya's voice service.
 *
 * This source file provides the implementation of WebSocket-based voice protocol
 * functionality within the Tuya voice service framework. It includes the
 * initialization, configuration, and processing functions for voice data
 * transmission over WebSocket, which handles real-time bidirectional
 * communication between IoT devices and the cloud platform. The implementation
 * supports various voice features including ASR, TTS, and NLP processing through
 * WebSocket protocol. This file is essential for developers working on IoT
 * applications that require low-latency voice interaction and streaming
 * capabilities.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include <stdio.h>
#include <string.h>

#include "tal_api.h"
#include "cJSON.h"
#include "tuya_speaker_voice_gw.h"
#include "protobuf_utils.h"
#include "aispeech.pb-c.h"
#include "tuya_voice_json_parse.h"
#include "tuya_voice_protocol_ws.h"
#include "tuya_voice_protocol.h"
#include "tuya_iot.h"

#define TUYA_WS_REQUEST_ID_MAX_LEN (64)
#define ENABLE_VOICE_DEBUG

typedef enum {
    TY_VOICE_RSP_ASR_MID,
    TY_VOICE_RSP_ASR_FINISH,
    TY_VOICE_RSP_NLP_FINISH,
    TY_VOICE_RSP_SKILL_FINISH,
    TY_VOICE_RSP_SPEECH_FINISH,
    TY_VOICE_RSP_TTS_START,
    TY_VOICE_RSP_TTS_MID,
    TY_VOICE_RSP_TTS_FINISH,
    TY_VOICE_RSP_TTS_INTERRUPTED,
    TY_VOICE_RSP_TEXT_START,
    TY_VOICE_RSP_TEXT_MID,
    TY_VOICE_RSP_TEXT_FINISH,
    TY_VOICE_RSP_TYPE_MAX
} TY_VOICE_RSP_TYPE_E;

typedef struct {
    uint32_t data_len;
    char request_id[TUYA_WS_REQUEST_ID_MAX_LEN];
} TY_VOICE_WS_UPLOAD_CTX_S;

typedef struct {
    char current_id[TUYA_WS_REQUEST_ID_MAX_LEN];
    MUTEX_HANDLE id_mutex;
} TY_VOICE_PROTOCOL_WS_S;

static TUYA_VOICE_CBS_S g_voice_ws_cbs = {0};
static TY_VOICE_PROTOCOL_WS_S g_protocol_ws = {0};

static void speaker_ws_recv_bin_cb(uint8_t *data, size_t len);
static void speaker_ws_recv_text_cb(uint8_t *data, size_t len);
static void __voice_ws_generate_request_id(char *req_id, int id_len);
static OPERATE_RET __format_upload_speex(Speech__Request *req, TUYA_VOICE_WS_START_PARAMS_S *head);
static OPERATE_RET __format_upload_wav(Speech__Request *req, TUYA_VOICE_WS_START_PARAMS_S *head);
static OPERATE_RET __format_upload_ulaw(Speech__Request *req, TUYA_VOICE_WS_START_PARAMS_S *head);

static OPERATE_RET __save_current_request_id(char *request_id)
{
    tal_mutex_lock(g_protocol_ws.id_mutex);
    strncpy(g_protocol_ws.current_id, request_id, TUYA_WS_REQUEST_ID_MAX_LEN);
    tal_mutex_unlock(g_protocol_ws.id_mutex);
    return OPRT_OK;
}
static OPERATE_RET __get_current_request_id(char *request_id)
{
    tal_mutex_lock(g_protocol_ws.id_mutex);
    strncpy(request_id, g_protocol_ws.current_id, TUYA_WS_REQUEST_ID_MAX_LEN);
    tal_mutex_unlock(g_protocol_ws.id_mutex);
    return OPRT_OK;
}

/**
 * @brief Get the current voice request ID
 *
 * This function retrieves the ID of the currently active voice request.
 *
 * @param[out] request_id Buffer to store the current request ID
 *
 * @return OPERATE_RET
 * @retval OPRT_OK Success
 * @retval Other Error codes from internal implementation
 */
OPERATE_RET tuya_voice_get_current_request_id(char *request_id)
{
    return __get_current_request_id(request_id);
}

/**
 * @brief Initialize the WebSocket voice protocol
 *
 * This function initializes the WebSocket voice protocol by setting up callbacks,
 * initializing the WebSocket client, and creating necessary synchronization primitives.
 *
 * @param[in] cbs Pointer to structure containing callback functions
 *
 * @return OPERATE_RET
 * @retval OPRT_OK Success
 * @retval OPRT_INVALID_PARM Invalid parameters (cbs is NULL)
 *
 * @note This function must be called before using any other WebSocket voice protocol functions
 */
OPERATE_RET tuya_voice_proto_ws_init(TUYA_VOICE_CBS_S *cbs)
{
    if (NULL == cbs) {
        PR_ERR("invalid parm");
        return OPRT_INVALID_PARM;
    }

    memcpy(&g_voice_ws_cbs, cbs, sizeof(TUYA_VOICE_CBS_S));

    tuya_speaker_ws_client_init(speaker_ws_recv_bin_cb, speaker_ws_recv_text_cb);

    memset(&g_protocol_ws, 0x00, sizeof(TY_VOICE_PROTOCOL_WS_S));
    tal_mutex_create_init(&g_protocol_ws.id_mutex);
    return OPRT_OK;
}

/**
 * @brief Deinitialize the WebSocket voice protocol
 *
 * This function cleans up resources used by the WebSocket voice protocol,
 * including releasing mutex and clearing protocol state.
 *
 * @return OPERATE_RET
 * @retval OPRT_OK Success
 *
 * @note This function should be called when the WebSocket voice protocol is no longer needed
 */
OPERATE_RET tuya_voice_proto_ws_deinit(void)
{
    tal_mutex_release(g_protocol_ws.id_mutex);
    memset(&g_protocol_ws, 0x00, sizeof(TY_VOICE_PROTOCOL_WS_S));
    return 0;
}

/**
 * @brief Start the WebSocket client
 *
 * This function starts the WebSocket client and prepares it for communication.
 *
 * @return OPERATE_RET
 * @retval OPRT_OK Success
 * @retval Other Error codes from WebSocket client implementation
 */
OPERATE_RET tuya_voice_proto_ws_client_start(void)
{
    return tuya_speaker_ws_client_start();
}

/**
 * @brief Stop the WebSocket client
 *
 * This function stops the WebSocket client and terminates any ongoing communication.
 *
 * @return OPERATE_RET
 * @retval OPRT_OK Success
 * @retval Other Error codes from WebSocket client implementation
 */
OPERATE_RET tuya_voice_proto_ws_client_stop(void)
{
    return tuya_speaker_ws_client_stop();
}

/**
 * @brief Delete the domain name configuration
 *
 * This function removes the configured domain name from the WebSocket client.
 *
 * @return OPERATE_RET
 * @retval OPRT_OK Success
 * @retval Other Error codes from WebSocket client implementation
 *
 * @note This operation may affect the ability to establish new connections
 */
OPERATE_RET tuya_voice_proto_ws_del_domain_name(void)
{
    return tuya_speaker_del_domain_name();
}

/**
 * @brief Send text content to TTS service through websocket protocol
 *
 * @details This function sends text content to the Text-to-Speech service for processing.
 *          The function performs the following operations:
 *          - Validates the input text parameter
 *          - Checks websocket connection status
 *          - Generates a unique request ID
 *          - Encodes the request using protobuf format
 *          - Sends the encoded data through websocket connection
 *
 * @param[in] p_tts_text  The text content to be processed by TTS service.
 *                        Must be a null-terminated string.
 *
 * @return OPERATE_RET @n
 * - OPRT_OK: Text sent successfully
 * - OPRT_INVALID_PARM: Invalid parameter (NULL text pointer)
 * - OPRT_COM_ERROR: Communication error, including:
 *                   - Websocket disconnection
 *                   - Protobuf encoding failure
 *                   - Websocket send failure
 * - OPRT_MALLOC_FAILED: Memory allocation failed for encoding buffer
 *
 * @note This function requires an active websocket connection before sending the text.
 *       The maximum text length is limited by the available memory for protobuf encoding.
 */
OPERATE_RET tuya_voice_proto_ws_get_tts_text(char *p_tts_text)
{
    OPERATE_RET rt = OPRT_OK;
    size_t enc_len = 0;
    uint8_t *enc_buf = NULL;
    char *p_tts_content = (char *)p_tts_text;

    TUYA_CHECK_NULL_RETURN(p_tts_content, OPRT_INVALID_PARM);

    if (!tuya_speaker_ws_is_online()) {
        PR_ERR("Communication has been disconnected, can't process voice, get tts failed");
        return OPRT_COM_ERROR;
    }

    char request_id[TUYA_WS_REQUEST_ID_MAX_LEN] = {0};
    __voice_ws_generate_request_id(request_id, sizeof(request_id));

    Speech__Request device_req;
    speech__request__init(&device_req);
    device_req.type = "TEXT";
    device_req.requestid = request_id;
    PR_DEBUG("text upload, requestid: %s, content: %s", device_req.requestid, p_tts_content);
    device_req.block.data = (uint8_t *)p_tts_content;
    device_req.block.len = strlen(p_tts_content);
    do {
        if ((enc_len = speech__request__get_packed_size(&device_req)) <= 0) {
            PR_ERR("protobuf get encoded data len is invalid, enc_lan[%d]", enc_len);
            rt = OPRT_COM_ERROR;
            break;
        }
        if ((enc_buf = (uint8_t *)Malloc(enc_len)) == NULL) {
            PR_ERR("Malloc failed");
            rt = OPRT_MALLOC_FAILED;
            break;
        }
        if ((enc_len = speech__request__pack(&device_req, enc_buf)) <= 0) {
            PR_ERR("protobuf encode data len is invalid, enc_lan[%d]", enc_len);
            rt = OPRT_COM_ERROR;
            Free(enc_buf);
            break;
        }
        if ((rt = tuya_speaker_ws_send_bin(enc_buf, enc_len)) != OPRT_OK) {
            PR_ERR("tuya_speaker_ws_send_bin failed %d", rt);
            rt = OPRT_COM_ERROR;
            Free(enc_buf);
            break;
        }
        Free(enc_buf);
    } while (0);

    return rt;
}

/**
 * @brief Request text-to-speech audio conversion with specific voice settings
 *
 * @details This function sends a text string to the TTS service for conversion to audio,
 *          allowing specification of a session ID and declaimer (voice type).
 *          The function uses protobuf encoding and websocket protocol for communication.
 *
 * @param[in] p_session_id  Session ID for the TTS request
 * @param[in] p_tts_text    The text string to be converted to speech
 * @param[in] p_declaimer   The voice type/declaimer to use for the conversion
 *
 * @return OPERATE_RET @n
 * - OPRT_OK: Success
 * - OPRT_INVALID_PARM: Invalid parameters (NULL pointers)
 * - OPRT_COM_ERROR: Communication error or encoding failed
 * - OPRT_MALLOC_FAILED: Memory allocation failed
 */
OPERATE_RET tuya_voice_proto_ws_get_tts_audio(char *p_session_id, char *p_tts_text, char *p_declaimer)
{
    OPERATE_RET rt = OPRT_OK;
    size_t enc_len = 0;
    uint8_t *enc_buf = NULL;

    TUYA_CHECK_NULL_RETURN(p_session_id, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(p_tts_text, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(p_declaimer, OPRT_INVALID_PARM);

    if (!tuya_speaker_ws_is_online()) {
        PR_ERR("Communication has been disconnected, can't process voice, get tts audio failed");
        return OPRT_COM_ERROR;
    }

    char request_id[TUYA_WS_REQUEST_ID_MAX_LEN] = {0};
    __voice_ws_generate_request_id(request_id, sizeof(request_id));

    Speech__Request device_req;
    speech__request__init(&device_req);
    device_req.type = "TTS";
    device_req.requestid = request_id;
    PR_DEBUG("text upload, requestid: %s", device_req.requestid);
    device_req.block.data = (uint8_t *)p_tts_text;
    device_req.block.len = strlen(p_tts_text);

    PB_ENC_OPT_ENTRY_S entry;
    pb_enc_opt_entry_init(&entry, (PB_ENC_OPT_ENTRY_INIT_CB)speech__request__options_entry__init);
    pb_enc_opt_entry_set_kv_string(&entry, "declaimer", p_declaimer);
    TUYA_CALL_ERR_RETURN(pb_enc_opt_entry_create_arr(&entry));
    device_req.n_options = entry.node_num;
    device_req.options = (Speech__Request__OptionsEntry **)entry.data_arr;
#ifdef ENABLE_VOICE_DEBUG
    size_t i = 0;
    for (i = 0; i < device_req.n_options; i++) {
        PR_DEBUG("options[%d], %s, %s", i, device_req.options[i]->key, device_req.options[i]->value);
    }
#endif
    do {
        if ((enc_len = speech__request__get_packed_size(&device_req)) <= 0) {
            PR_ERR("protobuf get encoded data len is invalid, enc_lan[%d]", enc_len);
            rt = OPRT_COM_ERROR;
            break;
        }
        if ((enc_buf = (uint8_t *)Malloc(enc_len)) == NULL) {
            PR_ERR("Malloc enc_buf failed");
            rt = OPRT_MALLOC_FAILED;
            break;
        }
        if ((enc_len = speech__request__pack(&device_req, enc_buf)) <= 0) {
            PR_ERR("protobuf encode data len is invalid, enc_lan[%d]", enc_len);
            Free(enc_buf);
            rt = OPRT_COM_ERROR;
            break;
        }
        if ((rt = tuya_speaker_ws_send_bin(enc_buf, enc_len)) != OPRT_OK) {
            PR_ERR("tuya_speaker_ws_send_bin failed %d", rt);
            Free(enc_buf);
            rt = OPRT_COM_ERROR;
            break;
        }
        Free(enc_buf);
    } while (0);

    TUYA_CALL_ERR_RETURN(pb_enc_opt_entry_destory(&entry));

    return rt;
}

/**
 * @brief Initialize and start a voice upload session through websocket protocol
 *
 * @details This function initializes a new voice upload session with the specified parameters.
 *          It performs the following operations:
 *          - Validates input parameters
 *          - Checks websocket connection status
 *          - Generates a unique request ID for the session
 *          - Configures the upload context based on the audio format
 *          - Supports multiple audio formats (SPEEX, WAV, ULAW)
 *          - Handles multi-round dialogue through session ID
 *
 * @param[out] uploader      Pointer to store the created upload context
 * @param[in]  format        Audio format of the voice data to be uploaded:
 *                          - TUYA_VOICE_AUDIO_FORMAT_SPEEX: Speex format
 *                          - TUYA_VOICE_AUDIO_FORMAT_WAV: WAV format
 *                          - TUYA_VOICE_AUDIO_FORMAT_ULAW: μ-law format
 * @param[in]  target        Target type for the voice upload
 * @param[in]  p_session_id  Session ID for multi-round dialogue, can be NULL for single round
 * @param[in]  p_buf         Buffer containing format-specific parameters
 * @param[in]  buf_len       Length of the parameter buffer
 *
 * @return OPERATE_RET @n
 * - OPRT_OK: Upload session started successfully
 * - OPRT_INVALID_PARM: Invalid parameters (NULL uploader or invalid format)
 * - OPRT_COM_ERROR: Communication error or unsupported audio format
 * - OPRT_MALLOC_FAILED: Memory allocation failed for upload context
 *
 * @note The function requires an active websocket connection before starting the upload.
 *       The caller is responsible for managing the memory of p_session_id and p_buf.
 */
OPERATE_RET tuya_voice_proto_ws_upload_start(TUYA_VOICE_UPLOAD_T *uploader, TUYA_VOICE_AUDIO_FORMAT_E format,
                                             TUYA_VOICE_UPLOAD_TARGET_E target, char *p_session_id, uint8_t *p_buf,
                                             uint32_t buf_len)
{
    OPERATE_RET rt = OPRT_OK;
    if (NULL == uploader || format >= TUYA_VOICE_AUDIO_FORMAT_INVALD) {
        PR_ERR("param is invalid, uploader %p, p_buf %p", uploader, p_buf);
        return OPRT_INVALID_PARM;
    }

    if (!tuya_speaker_ws_is_online()) {
        PR_ERR("Communication has been disconnected, can't upload voice, start failed");
        return OPRT_COM_ERROR;
    }

    TY_VOICE_WS_UPLOAD_CTX_S *p_upload_ctx = NULL;
    SAFE_MALLOC_ERR_RET(p_upload_ctx, sizeof(TY_VOICE_WS_UPLOAD_CTX_S));
    __voice_ws_generate_request_id(p_upload_ctx->request_id, sizeof(p_upload_ctx->request_id));
    __save_current_request_id(p_upload_ctx->request_id);

    Speech__Request device_req;
    speech__request__init(&device_req);
    device_req.type = "ASR_START";
    device_req.requestid = p_upload_ctx->request_id;
    PR_INFO("voice upload start, requestid: %s", device_req.requestid);
    if (NULL != p_session_id && 0 != strlen(p_session_id)) {
        /** Multi-round dialogue */
        device_req.sessionid = (char *)p_session_id;
        PR_INFO("sessionid: %s", device_req.sessionid);
    }
    TUYA_VOICE_WS_START_PARAMS_S *p_head = (TUYA_VOICE_WS_START_PARAMS_S *)p_buf;
    switch (format) {
    case TUYA_VOICE_AUDIO_FORMAT_SPEEX:
        TUYA_CALL_ERR_RETURN(__format_upload_speex(&device_req, p_head));
        break;
    case TUYA_VOICE_AUDIO_FORMAT_WAV:
        TUYA_CALL_ERR_RETURN(__format_upload_wav(&device_req, p_head));
        break;
    case TUYA_VOICE_AUDIO_FORMAT_ULAW:
        TUYA_CALL_ERR_RETURN(__format_upload_ulaw(&device_req, p_head));
        break;
    default:
        PR_ERR("this encode type is not currently supported, %d", format);
        return OPRT_COM_ERROR;
    }

    *uploader = p_upload_ctx;

    return OPRT_OK;
}

/**
 * @brief Send voice data in an active upload session
 *
 * @details This function sends voice data chunks through the websocket connection.
 *          It encodes the data using protobuf and sends it as binary data.
 *          The function maintains an internal counter for the total amount of data sent.
 *
 * @param[in] uploader  The upload context created by tuya_voice_proto_ws_upload_start
 * @param[in] buf       Buffer containing the voice data to send
 * @param[in] len       Length of the voice data buffer
 *
 * @return OPERATE_RET @n
 * - OPRT_OK: Success
 * - OPRT_INVALID_PARM: Invalid parameter
 * - OPRT_COM_ERROR: Communication error
 * - OPRT_MALLOC_FAILED: Memory allocation failed
 */
OPERATE_RET tuya_voice_proto_ws_upload_send(TUYA_VOICE_UPLOAD_T uploader, uint8_t *buf, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;
    size_t enc_len = 0;
    uint8_t *enc_buf = NULL;

    if (NULL == uploader || (len && !buf)) {
        PR_ERR("param is invalid");
        return OPRT_INVALID_PARM;
    }

    TY_VOICE_WS_UPLOAD_CTX_S *p_upload_ctx = (TY_VOICE_WS_UPLOAD_CTX_S *)uploader;

    if (!tuya_speaker_ws_is_online()) {
        PR_ERR("Communication has been disconnected, can't upload voice, send failed");
        return OPRT_COM_ERROR;
    }

    Speech__Request device_req;
    speech__request__init(&device_req);
    device_req.requestid = p_upload_ctx->request_id;
    device_req.type = "ASR_MID";
    device_req.block.len = len;
    device_req.block.data = (uint8_t *)buf;

    do {
        if ((enc_len = speech__request__get_packed_size(&device_req)) <= 0) {
            PR_ERR("protobuf get encoded data len is invalid, enc_lan[%d]", enc_len);
            rt = OPRT_COM_ERROR;
            break;
        }
        if ((enc_buf = (uint8_t *)Malloc(enc_len)) == NULL) {
            PR_ERR("Malloc enc_buf failed");
            rt = OPRT_MALLOC_FAILED;
            break;
        }
        if ((enc_len = speech__request__pack(&device_req, enc_buf)) <= 0) {
            PR_ERR("protobuf encode data len is invalid, enc_lan[%d]", enc_len);
            Free(enc_buf);
            rt = OPRT_COM_ERROR;
            break;
        }
        if ((rt = tuya_speaker_ws_send_bin(enc_buf, enc_len)) != OPRT_OK) {
            PR_ERR("tuya_speaker_ws_send_bin failed");
            Free(enc_buf);
            rt = OPRT_COM_ERROR;
            break;
        }
        Free(enc_buf);
    } while (0);

    p_upload_ctx->data_len += enc_len;

    return rt;
}

/**
 * @brief Stop an active voice upload session
 *
 * @details This function terminates a voice upload session. It can either perform
 *          a graceful shutdown by sending an ASR_END message, or force stop the
 *          session immediately. After stopping, it frees the upload context.
 *
 * @param[in] uploader    The upload context to be stopped
 * @param[in] force_stop  If TRUE, forcefully stop without sending ASR_END message
 *                        If FALSE, perform graceful shutdown
 *
 * @return OPERATE_RET @n
 * - OPRT_OK: Success
 * - OPRT_INVALID_PARM: Invalid parameter
 * - OPRT_COM_ERROR: Communication error
 * - OPRT_MALLOC_FAILED: Memory allocation failed during graceful shutdown
 */
OPERATE_RET tuya_voice_proto_ws_upload_stop(TUYA_VOICE_UPLOAD_T uploader, BOOL_T force_stop)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_CHECK_NULL_RETURN(uploader, OPRT_INVALID_PARM);

    TY_VOICE_WS_UPLOAD_CTX_S *p_upload_ctx = (TY_VOICE_WS_UPLOAD_CTX_S *)uploader;

    if (!force_stop) {
        size_t enc_len = 0;
        uint8_t *enc_buf = NULL;

        if (!tuya_speaker_ws_is_online()) {
            PR_ERR("Communication has been disconnected, can't upload voice, stop failed");
            return OPRT_COM_ERROR;
        }

        Speech__Request device_req;
        speech__request__init(&device_req);
        device_req.requestid = p_upload_ctx->request_id;
        device_req.type = "ASR_END";
        PR_INFO("voice upload stop");
        do {
            if ((enc_len = speech__request__get_packed_size(&device_req)) <= 0) {
                PR_ERR("protobuf get encoded data len is invalid, enc_lan[%d]", enc_len);
                rt = OPRT_COM_ERROR;
                break;
            }
            if ((enc_buf = (uint8_t *)Malloc(enc_len)) == NULL) {
                PR_ERR("Malloc enc_buf failed");
                rt = OPRT_MALLOC_FAILED;
                break;
            }
            if ((enc_len = speech__request__pack(&device_req, enc_buf)) <= 0) {
                PR_ERR("protobuf encode data len is invalid, enc_lan[%d]", enc_len);
                Free(enc_buf);
                rt = OPRT_COM_ERROR;
                break;
            }
            if ((rt = tuya_speaker_ws_send_bin(enc_buf, enc_len)) != OPRT_OK) {
                PR_ERR("tuya_speaker_ws_send_bin failed");
                Free(enc_buf);
                rt = OPRT_COM_ERROR;
                break;
            }
            Free(enc_buf);
        } while (0);
        p_upload_ctx->data_len += enc_len;
    } else {
        rt = tuya_voice_proto_ws_interrupt();
    }

    PR_DEBUG("total upload data len:%d force_stop:%d --<<", p_upload_ctx->data_len, force_stop);
    SAFE_FREE(p_upload_ctx);

    return rt;
}

/**
 * @brief Get the message ID (request ID) of an active upload session
 *
 * @details This function retrieves the request ID associated with the current
 *          upload session and copies it to the provided buffer.
 *
 * @param[in]  uploader  The upload context to get the message ID from
 * @param[out] buffer    Buffer to store the message ID string
 * @param[in]  len       Length of the provided buffer
 *
 * @return OPERATE_RET @n
 * - OPRT_OK: Success
 * - OPRT_INVALID_PARM: Invalid uploader parameter
 */
OPERATE_RET tuya_voice_proto_ws_upload_get_message_id(TUYA_VOICE_UPLOAD_T uploader, char *buffer, int len)
{
    if (NULL == uploader) {
        return OPRT_INVALID_PARM;
    }

    TY_VOICE_WS_UPLOAD_CTX_S *p_upload_ctx = (TY_VOICE_WS_UPLOAD_CTX_S *)uploader;

    snprintf(buffer, len, "%s", p_upload_ctx->request_id);

    return OPRT_OK;
}

/**
 * @brief Send a control command through websocket protocol
 *
 * @details This function sends a control command with the specified request ID.
 *          It encodes the command using protobuf and sends it through the websocket
 *          connection. The command is sent as a key-value pair in the options field.
 *
 * @param[in] request_id  The request ID associated with the control command
 * @param[in] command     The control command string to send
 *
 * @return OPERATE_RET @n
 * - OPRT_OK: Success
 * - OPRT_INVALID_PARM: Invalid parameters
 * - OPRT_COM_ERROR: Communication error or encoding failed
 * - OPRT_MALLOC_FAILED: Memory allocation failed
 */
OPERATE_RET tuya_voice_proto_ws_control(char *request_id, char *command)
{
    OPERATE_RET rt = OPRT_OK;
    size_t enc_len = 0;
    uint8_t *enc_buf = NULL;

    if (NULL == request_id || NULL == command) {
        PR_ERR("param is invalid");
        return OPRT_INVALID_PARM;
    }

    if (!tuya_speaker_ws_is_online()) {
        PR_ERR("Communication has been disconnected, can't upload voice, send failed");
        return OPRT_COM_ERROR;
    }

    Speech__Request device_req, *req;
    speech__request__init(&device_req);
    device_req.requestid = request_id;
    device_req.type = "CONTROL";

    PB_ENC_OPT_ENTRY_S entry;
    pb_enc_opt_entry_init(&entry, (PB_ENC_OPT_ENTRY_INIT_CB)speech__request__options_entry__init);
    pb_enc_opt_entry_set_kv_string(&entry, "command", command);

    TUYA_CALL_ERR_RETURN(pb_enc_opt_entry_create_arr(&entry));
    device_req.n_options = entry.node_num;
    device_req.options = (Speech__Request__OptionsEntry **)entry.data_arr;

#ifdef ENABLE_VOICE_DEBUG
    size_t i = 0;
    req = &device_req;
    PR_DEBUG("type: %s, request_id: %s", device_req.type, device_req.requestid);
    for (i = 0; i < req->n_options; i++) {
        PR_DEBUG("options[%02d] %s:%s", i, req->options[i]->key, req->options[i]->value);
    }
#endif

    do {
        if ((enc_len = speech__request__get_packed_size(&device_req)) <= 0) {
            PR_ERR("protobuf get encoded data len is invalid, enc_lan[%d]", enc_len);
            rt = OPRT_COM_ERROR;
            break;
        }
        if ((enc_buf = (uint8_t *)Malloc(enc_len)) == NULL) {
            PR_ERR("Malloc enc_buf failed");
            rt = OPRT_MALLOC_FAILED;
            break;
        }
        if ((enc_len = speech__request__pack(&device_req, enc_buf)) <= 0) {
            PR_ERR("protobuf encode data len is invalid, enc_lan[%d]", enc_len);
            Free(enc_buf);
            rt = OPRT_COM_ERROR;
            break;
        }
        if ((rt = tuya_speaker_ws_send_bin(enc_buf, enc_len)) != OPRT_OK) {
            PR_ERR("tuya_speaker_ws_send_bin failed");
            Free(enc_buf);
            rt = OPRT_COM_ERROR;
            break;
        }
        Free(enc_buf);
    } while (0);

    TUYA_CALL_ERR_RETURN(pb_enc_opt_entry_destory(&entry));
    return rt;
}

/**
 * @brief Send a voice skill request through WebSocket protocol
 *
 * This function sends a voice skill request with specified domain, intent and optional
 * parameters through WebSocket connection. The request is encoded using protobuf format.
 *
 * @param[in] domain The domain of the skill request
 * @param[in] intent The intent of the skill request
 * @param[in] slots Optional JSON string containing slot information, can be NULL
 * @param[in] raw Optional raw data string, can be NULL
 *
 * @return OPERATE_RET
 * @retval OPRT_OK Success
 * @retval OPRT_INVALID_PARM Invalid parameters (domain or intent is NULL)
 * @retval OPRT_COM_ERROR Communication error or encoding error
 * @retval OPRT_MALLOC_FAILED Memory allocation failed
 *
 * @note The function will generate a unique request ID for each call
 * @note The WebSocket connection must be established before calling this function
 */
OPERATE_RET tuya_voice_proto_ws_skill_request(char *domain, char *intent, char *slots, char *raw)
{
    OPERATE_RET rt = OPRT_OK;
    size_t enc_len = 0;
    uint8_t *enc_buf = NULL;

    if (NULL == domain || NULL == intent) {
        PR_ERR("param is invalid");
        return OPRT_INVALID_PARM;
    }

    if (!tuya_speaker_ws_is_online()) {
        PR_ERR("Communication has been disconnected, can't upload voice, send failed");
        return OPRT_COM_ERROR;
    }

    char request_id[TUYA_WS_REQUEST_ID_MAX_LEN] = {0};
    __voice_ws_generate_request_id(request_id, sizeof(request_id));
    __save_current_request_id(request_id);

    Speech__Request device_req, *req;
    speech__request__init(&device_req);
    device_req.requestid = request_id;
    device_req.type = "SKILL";

    PB_ENC_OPT_ENTRY_S entry;
    pb_enc_opt_entry_init(&entry, (PB_ENC_OPT_ENTRY_INIT_CB)speech__request__options_entry__init);
    pb_enc_opt_entry_set_kv_string(&entry, "domain", domain);
    pb_enc_opt_entry_set_kv_string(&entry, "intent", intent);
    if (slots) {
        pb_enc_opt_entry_set_kv_string(&entry, "slots", slots);
    }
    if (raw) {
        pb_enc_opt_entry_set_kv_string(&entry, "raw", raw);
    }
#ifdef ENABLE_VOICE_TTS_STREAM
    pb_enc_opt_entry_set_kv_string(&entry, "tts.stream", "true");
#endif

    TUYA_CALL_ERR_RETURN(pb_enc_opt_entry_create_arr(&entry));
    device_req.n_options = entry.node_num;
    device_req.options = (Speech__Request__OptionsEntry **)entry.data_arr;

#ifdef ENABLE_VOICE_DEBUG
    size_t i = 0;
    req = &device_req;
    PR_DEBUG("type: %s, request_id: %s", device_req.type, device_req.requestid);
    for (i = 0; i < req->n_options; i++) {
        PR_DEBUG("options[%02d] %s:%s", i, req->options[i]->key, req->options[i]->value);
    }
#endif

    do {
        if ((enc_len = speech__request__get_packed_size(&device_req)) <= 0) {
            PR_ERR("protobuf get encoded data len is invalid, enc_lan[%d]", enc_len);
            rt = OPRT_COM_ERROR;
            break;
        }
        if ((enc_buf = (uint8_t *)Malloc(enc_len)) == NULL) {
            PR_ERR("Malloc enc_buf failed");
            rt = OPRT_MALLOC_FAILED;
            break;
        }
        if ((enc_len = speech__request__pack(&device_req, enc_buf)) <= 0) {
            PR_ERR("protobuf encode data len is invalid, enc_lan[%d]", enc_len);
            Free(enc_buf);
            rt = OPRT_COM_ERROR;
            break;
        }
        if ((rt = tuya_speaker_ws_send_bin(enc_buf, enc_len)) != OPRT_OK) {
            PR_ERR("tuya_speaker_ws_send_bin failed");
            Free(enc_buf);
            rt = OPRT_COM_ERROR;
            break;
        }
        Free(enc_buf);
    } while (0);

    TUYA_CALL_ERR_RETURN(pb_enc_opt_entry_destory(&entry));

    return rt;
}

/**
 * @brief Request TTS stream conversion for given text
 *
 * @details This function creates a skill request for text-to-speech streaming.
 *          It formats the text into a slot parameter and sends it to the
 *          general voice playback skill.
 *
 * @param[in] tts_text  The text to be converted to speech (must not be NULL)
 *
 * @return OPERATE_RET @n
 * - OPRT_OK: TTS stream request sent successfully
 * - OPRT_INVALID_PARM: Invalid parameter (NULL text)
 * - OPRT_COM_ERROR: Communication error
 * - OPRT_MALLOC_FAILED: Memory allocation failed
 *
 * @note This is a wrapper function that calls tuya_voice_proto_ws_skill_request
 *       with predefined domain and intent values.
 */
OPERATE_RET tuya_voice_proto_ws_get_tts_stream(char *tts_text)
{
    char slots[2048];
    snprintf(slots, sizeof(slots), "[{\"name\":\"文本\",\"value\":\"%s\"}]", tts_text);
    return tuya_voice_proto_ws_skill_request("通用", "语音播放", slots, NULL);
}

/**
 * @brief Interrupt current voice WebSocket request
 *
 * This function interrupts the currently active voice request by sending an interrupt
 * control command through WebSocket connection. If there is no active request,
 * the function will return successfully without performing any operation.
 *
 * @return OPERATE_RET
 * @retval OPRT_OK Success, either interrupted active request or no request to interrupt
 * @retval Other Error codes from tuya_voice_proto_ws_control function
 *
 * @note This function checks if there is an active request before attempting to interrupt
 */
OPERATE_RET tuya_voice_proto_ws_interrupt(void)
{
    char request_id[TUYA_WS_REQUEST_ID_MAX_LEN] = {0};
    __get_current_request_id(request_id);
    if (request_id[0] == '\0') {
        PR_WARN("ws not need inerrupt!");
        return OPRT_OK;
    }
    return tuya_voice_proto_ws_control(request_id, "interrupt");
}

static void __voice_ws_generate_request_id(char *req_id, int id_len)
{
    uint8_t i = 0, random[4] = {0};

    // 6c6f189feabb27bf1dcrii_1fd27277  20 + 1 + 4 bytes
    for (i = 0; i < sizeof(random); i++) {
        random[i] = (uint8_t)tal_system_get_random(0xFF);
    }
    tuya_iot_client_t *iot_client = tuya_iot_client_get();
    snprintf(req_id, id_len, "%s_%02x%02x%02x%02x", iot_client->activate.devid, random[0], random[1], random[2],
             random[3]);
}

static OPERATE_RET __format_upload_speex(Speech__Request *req, TUYA_VOICE_WS_START_PARAMS_S *head)
{
    OPERATE_RET rt = OPRT_OK;
    size_t enc_len = 0;
    uint8_t *enc_buf = NULL;
    PB_ENC_OPT_ENTRY_S entry;

    pb_enc_opt_entry_init(&entry, (PB_ENC_OPT_ENTRY_INIT_CB)speech__request__options_entry__init);
    pb_enc_opt_entry_set_kv_string(&entry, "format", "spx");
    pb_enc_opt_entry_set_kv_integer(&entry, "channel", head->channels);
    pb_enc_opt_entry_set_kv_integer(&entry, "sampleRate", head->rate);
    pb_enc_opt_entry_set_kv_string(&entry, "sampleBytes", "16");
    pb_enc_opt_entry_set_kv_string(&entry, "spx.versionString", head->ver_string);
    pb_enc_opt_entry_set_kv_integer(&entry, "spx.versionId", head->ver_id);
    pb_enc_opt_entry_set_kv_integer(&entry, "spx.mode", head->mode);
    pb_enc_opt_entry_set_kv_integer(&entry, "spx.vbr", head->vbr);
    pb_enc_opt_entry_set_kv_integer(&entry, "spx.frameSize", head->frame_size);
    pb_enc_opt_entry_set_kv_integer(&entry, "spx.modeBitstreamVersion", head->mode_bit_stream_ver);
    pb_enc_opt_entry_set_kv_integer(&entry, "spx.encodeFrameSize", head->encode_frame_size);
    pb_enc_opt_entry_set_kv_integer(&entry, "spx.bitRate", head->bit_rate);
#ifdef ENABLE_VOICE_TTS_STREAM
    if (g_voice_ws_cbs.tuya_voice_tts_stream)
        pb_enc_opt_entry_set_kv_string(&entry, "tts.stream", "true");
#endif
    if (g_voice_ws_cbs.tuya_voice_text_stream)
        pb_enc_opt_entry_set_kv_string(&entry, "text.stream", "true");

    TUYA_CALL_ERR_RETURN(pb_enc_opt_entry_create_arr(&entry));
    req->n_options = entry.node_num;
    req->options = (Speech__Request__OptionsEntry **)entry.data_arr;

#ifdef ENABLE_VOICE_DEBUG
    size_t i = 0;
    for (i = 0; i < req->n_options; i++) {
        PR_DEBUG("options[%02d] %s:%s", i, req->options[i]->key, req->options[i]->value);
    }
#endif

    do {
        if ((enc_len = speech__request__get_packed_size(req)) <= 0) {
            PR_ERR("protobuf get encoded data len is invalid, enc_lan[%d]", enc_len);
            rt = OPRT_COM_ERROR;
            break;
        }
        if ((enc_buf = (uint8_t *)Malloc(enc_len)) == NULL) {
            PR_ERR("Malloc enc_buf failed");
            rt = OPRT_MALLOC_FAILED;
            break;
        }
        if ((enc_len = speech__request__pack(req, enc_buf)) <= 0) {
            PR_ERR("protobuf encode data len is invalid, enc_lan[%d]", enc_len);
            Free(enc_buf);
            rt = OPRT_COM_ERROR;
            break;
        }
        if ((rt = tuya_speaker_ws_send_bin(enc_buf, enc_len)) != OPRT_OK) {
            PR_ERR("tuya_speaker_ws_send_bin failed");
            Free(enc_buf);
            rt = OPRT_COM_ERROR;
            break;
        }
        Free(enc_buf);
    } while (0);

    TUYA_CALL_ERR_RETURN(pb_enc_opt_entry_destory(&entry));

    return rt;
}

static OPERATE_RET __format_upload_wav(Speech__Request *req, TUYA_VOICE_WS_START_PARAMS_S *head)
{
    OPERATE_RET rt = OPRT_OK;
    size_t enc_len = 0;
    uint8_t *enc_buf = NULL;
    PB_ENC_OPT_ENTRY_S entry;

    pb_enc_opt_entry_init(&entry, (PB_ENC_OPT_ENTRY_INIT_CB)speech__request__options_entry__init);
    pb_enc_opt_entry_set_kv_string(&entry, "format", "wav");
    // pb_enc_opt_entry_set_kv_integer(&entry, "channel", 1);
    // pb_enc_opt_entry_set_kv_integer(&entry, "sampleRate", 16000);
    pb_enc_opt_entry_set_kv_integer(&entry, "channel", head ? head->channels : 1);
    pb_enc_opt_entry_set_kv_integer(&entry, "sampleRate", head ? head->rate : 16000);
    pb_enc_opt_entry_set_kv_string(&entry, "sampleBytes", "16");
#ifdef ENABLE_VOICE_TTS_STREAM
    pb_enc_opt_entry_set_kv_string(&entry, "tts.stream", "true");
#endif

    TUYA_CALL_ERR_RETURN(pb_enc_opt_entry_create_arr(&entry));
    req->n_options = entry.node_num;
    req->options = (Speech__Request__OptionsEntry **)entry.data_arr;

#ifdef ENABLE_VOICE_DEBUG
    size_t i = 0;
    for (i = 0; i < req->n_options; i++) {
        PR_DEBUG("options[%02d] %s:%s", i, req->options[i]->key, req->options[i]->value);
    }
#endif

    do {
        if ((enc_len = speech__request__get_packed_size(req)) <= 0) {
            PR_ERR("protobuf get encoded data len is invalid, enc_lan[%d]", enc_len);
            rt = OPRT_COM_ERROR;
            break;
        }
        if ((enc_buf = (uint8_t *)Malloc(enc_len)) == NULL) {
            PR_ERR("Malloc enc_buf failed");
            rt = OPRT_MALLOC_FAILED;
            break;
        }
        if ((enc_len = speech__request__pack(req, enc_buf)) <= 0) {
            PR_ERR("protobuf encode data len is invalid, enc_lan[%d]", enc_len);
            Free(enc_buf);
            rt = OPRT_COM_ERROR;
            break;
        }
        if ((rt = tuya_speaker_ws_send_bin(enc_buf, enc_len)) != OPRT_OK) {
            PR_ERR("tuya_speaker_ws_send_bin failed");
            Free(enc_buf);
            rt = OPRT_COM_ERROR;
            break;
        }
        Free(enc_buf);
    } while (0);

    TUYA_CALL_ERR_RETURN(pb_enc_opt_entry_destory(&entry));

    return rt;
}

static OPERATE_RET __format_upload_ulaw(Speech__Request *req, TUYA_VOICE_WS_START_PARAMS_S *head)
{
    OPERATE_RET rt = OPRT_OK;
    size_t enc_len = 0;
    uint8_t *enc_buf = NULL;
    PB_ENC_OPT_ENTRY_S entry;

    pb_enc_opt_entry_init(&entry, (PB_ENC_OPT_ENTRY_INIT_CB)speech__request__options_entry__init);
    pb_enc_opt_entry_set_kv_string(&entry, "format", "ulaw");
    pb_enc_opt_entry_set_kv_integer(&entry, "channel", head ? head->channels : 1);
    pb_enc_opt_entry_set_kv_integer(&entry, "sampleRate", head ? head->rate : 16000);
    pb_enc_opt_entry_set_kv_string(&entry, "sampleBytes", "16");
#ifdef ENABLE_VOICE_TTS_STREAM
    pb_enc_opt_entry_set_kv_string(&entry, "tts.stream", "true");
#endif

    TUYA_CALL_ERR_RETURN(pb_enc_opt_entry_create_arr(&entry));
    req->n_options = entry.node_num;
    req->options = (Speech__Request__OptionsEntry **)entry.data_arr;

#ifdef ENABLE_VOICE_DEBUG
    size_t i = 0;
    for (i = 0; i < req->n_options; i++) {
        PR_DEBUG("options[%02d] %s:%s", i, req->options[i]->key, req->options[i]->value);
    }
#endif

    do {
        if ((enc_len = speech__request__get_packed_size(req)) <= 0) {
            PR_ERR("protobuf get encoded data len is invalid, enc_lan[%d]", enc_len);
            rt = OPRT_COM_ERROR;
            break;
        }
        if ((enc_buf = (uint8_t *)Malloc(enc_len)) == NULL) {
            PR_ERR("Malloc enc_buf failed");
            rt = OPRT_MALLOC_FAILED;
            break;
        }
        if ((enc_len = speech__request__pack(req, enc_buf)) <= 0) {
            PR_ERR("protobuf encode data len is invalid, enc_lan[%d]", enc_len);
            Free(enc_buf);
            rt = OPRT_COM_ERROR;
            break;
        }
        if ((rt = tuya_speaker_ws_send_bin(enc_buf, enc_len)) != OPRT_OK) {
            PR_ERR("tuya_speaker_ws_send_bin failed");
            Free(enc_buf);
            rt = OPRT_COM_ERROR;
            break;
        }
        Free(enc_buf);
    } while (0);

    TUYA_CALL_ERR_RETURN(pb_enc_opt_entry_destory(&entry));

    return rt;
}

static OPERATE_RET __parse_cloud_rsp_asr(char *asr)
{
    TUYA_CHECK_NULL_RETURN(asr, OPRT_INVALID_PARM);

    if (g_voice_ws_cbs.tuya_voice_custom == NULL) {
        return OPRT_OK;
    }

    PR_DEBUG("start custom cb");
    char *type = "syncDialogText";
    cJSON *json = cJSON_CreateObject();
    if (json) {
        cJSON_AddStringToObject(json, "speaker", "human");
        cJSON_AddStringToObject(json, "text", asr);
        // cJSON_AddNumberToObject(json, "id", 260);
        g_voice_ws_cbs.tuya_voice_custom(type, json);

        cJSON_Delete(json);
        json = NULL;
    }

    return OPRT_OK;
}

static OPERATE_RET __parse_cloud_rsp_nlu(Speech__Nlu *nlu)
{
    size_t i = 0;

    TUYA_CHECK_NULL_RETURN(nlu, OPRT_INVALID_PARM);
    if (nlu->domain != NULL) {
        PR_DEBUG("nlu->domain: %s", nlu->domain);
    }

    if (nlu->intent != NULL) {
        PR_DEBUG("nlu->intent: %s", nlu->intent);
    }

    for (i = 0; i < nlu->n_slot; i++) {
        if (nlu->slot[i]->name != NULL) {
            PR_DEBUG("nlu->slot[%d]->name: %s", i, nlu->slot[i]->name);
        }
        if (nlu->slot[i]->type != NULL) {
            PR_DEBUG("nlu->slot[%d]->type: %s", i, nlu->slot[i]->type);
        }
        if (nlu->slot[i]->value != NULL) {
            PR_DEBUG("nlu->slot[%d]->value: %s", i, nlu->slot[i]->value);
        }
    }

    return OPRT_OK;
}

static OPERATE_RET __parse_cloud_rsp_skill(Speech__Skill *skill)
{
    cJSON *json = NULL;
    TUYA_CHECK_NULL_RETURN(skill, OPRT_INVALID_PARM);

    if (skill->name != NULL) {
        PR_DEBUG("name: %s", skill->name);
    }

    if (skill->type != NULL) {
        PR_DEBUG("type: %s", skill->type);
    }

    if (skill->data != NULL) {
        PR_DEBUG("data: %s\n", skill->data);
        json = cJSON_Parse(skill->data);
        if (json == NULL) {
            PR_WARN("skill->data is not json foramt");
        } else {
            if (g_voice_ws_cbs.tuya_voice_custom) {
                PR_DEBUG("start custom cb");
                g_voice_ws_cbs.tuya_voice_custom(skill->type, json);
            }
        }
    }

    if ((!strcmp(skill->type, "playTts") || !strcmp(skill->type, "playUrl")) &&
        g_voice_ws_cbs.tuya_voice_play_tts != NULL) {
        TUYA_VOICE_TTS_S *tts = NULL;
        if (tuya_voice_json_parse_tts(json, &tts) != OPRT_OK) {
            PR_ERR("parse tts error");
            return OPRT_COM_ERROR;
        }
        g_voice_ws_cbs.tuya_voice_play_tts(tts);
        tuya_voice_json_parse_free_tts(tts);
    } else if (!strcmp(skill->type, "playAudio") && g_voice_ws_cbs.tuya_voice_play_audio != NULL) {
        TUYA_VOICE_MEDIA_S *media = NULL;
        if (tuya_voice_json_parse_media(json, &media) != OPRT_OK) {
            PR_ERR("parse audio error");
            return OPRT_COM_ERROR;
        }
        g_voice_ws_cbs.tuya_voice_play_audio(media);
        tuya_voice_json_parse_free_media(media);
    }

    if (json != NULL) {
        cJSON_Delete(json);
        json = NULL;
    }

    return OPRT_OK;
}

static OPERATE_RET __handle_cloud_rsp(TY_VOICE_RSP_TYPE_E type, Speech__Response *cloud_rsp)
{
    static char tts_request_id[TUYA_WS_REQUEST_ID_MAX_LEN] = {0};
    switch (type) {
    case TY_VOICE_RSP_ASR_MID:
        if (cloud_rsp->data->asr != NULL) {
            PR_DEBUG("asr mid: %s", cloud_rsp->data->asr);
        }
        break;
    case TY_VOICE_RSP_ASR_FINISH:
        if (cloud_rsp->data->asr != NULL) {
            PR_DEBUG("asr finish: %s", cloud_rsp->data->asr);
            __parse_cloud_rsp_asr(cloud_rsp->data->asr);
        }
        break;
    case TY_VOICE_RSP_NLP_FINISH:
        if (cloud_rsp->data->nlu != NULL) {
            __parse_cloud_rsp_nlu(cloud_rsp->data->nlu);
        }
        break;
    case TY_VOICE_RSP_SKILL_FINISH:
        PR_DEBUG("keepsession: %d", cloud_rsp->data->keepsession);
        if (cloud_rsp->data->sessionid != NULL) {
            PR_DEBUG("sessionid: %s", cloud_rsp->data->sessionid);
        }
        if (cloud_rsp->data->nlg != NULL) {
            PR_DEBUG("nlg: %s", cloud_rsp->data->nlg);
        }
        if (cloud_rsp->data->skill != NULL) {
            __parse_cloud_rsp_skill(cloud_rsp->data->skill);
        }
        break;
    case TY_VOICE_RSP_SPEECH_FINISH:
        PR_DEBUG("keepsession: %d", cloud_rsp->data->keepsession);
        if (cloud_rsp->data->sessionid != NULL) {
            PR_DEBUG("sessionid: %s", cloud_rsp->data->sessionid);
        }
        break;
    case TY_VOICE_RSP_TTS_START:
        snprintf(tts_request_id, TUYA_WS_REQUEST_ID_MAX_LEN, "%s", cloud_rsp->requestid);
        if (g_voice_ws_cbs.tuya_voice_tts_stream) {
            g_voice_ws_cbs.tuya_voice_tts_stream(TUYA_VOICE_STREAM_START, (uint8_t *)cloud_rsp->requestid,
                                                 strlen(cloud_rsp->requestid) + 1);
        }
        break;
    case TY_VOICE_RSP_TTS_MID:
        if (g_voice_ws_cbs.tuya_voice_tts_stream) {
            if (0 == strncmp(tts_request_id, cloud_rsp->requestid, TUYA_WS_REQUEST_ID_MAX_LEN)) {
                g_voice_ws_cbs.tuya_voice_tts_stream(TUYA_VOICE_STREAM_DATA, cloud_rsp->data->block.data,
                                                     cloud_rsp->data->block.len);
            } else {
                // PR_WARN("TTS MID current id: %s, response id: %s", tts_request_id, cloud_rsp->requestid);
            }
        }
        break;
    case TY_VOICE_RSP_TTS_FINISH:
        if (g_voice_ws_cbs.tuya_voice_tts_stream) {
            if (0 == strncmp(tts_request_id, cloud_rsp->requestid, TUYA_WS_REQUEST_ID_MAX_LEN)) {
                g_voice_ws_cbs.tuya_voice_tts_stream(TUYA_VOICE_STREAM_STOP, NULL, 0);
            } else {
                PR_WARN("TTS FINISH current id: %s, response id: %s", tts_request_id, cloud_rsp->requestid);
            }
        }
        break;
    case TY_VOICE_RSP_TTS_INTERRUPTED:
        if (g_voice_ws_cbs.tuya_voice_tts_stream) {
            if (0 == strncmp(tts_request_id, cloud_rsp->requestid, TUYA_WS_REQUEST_ID_MAX_LEN)) {
                g_voice_ws_cbs.tuya_voice_tts_stream(TUYA_VOICE_STREAM_ABORT, NULL, 0);
            } else {
                PR_WARN("TSS INTERRUPTED current id: %s, response id: %s", tts_request_id, cloud_rsp->requestid);
            }
        }
        break;
    case TY_VOICE_RSP_TEXT_START:
        if (g_voice_ws_cbs.tuya_voice_text_stream) {
            g_voice_ws_cbs.tuya_voice_text_stream(TUYA_VOICE_STREAM_START, (uint8_t *)cloud_rsp->requestid,
                                                  strlen(cloud_rsp->requestid) + 1);
        }
        break;
    case TY_VOICE_RSP_TEXT_MID:
        if (g_voice_ws_cbs.tuya_voice_text_stream) {
            g_voice_ws_cbs.tuya_voice_text_stream(TUYA_VOICE_STREAM_DATA, (uint8_t *)cloud_rsp->data->nlg,
                                                  strlen(cloud_rsp->data->nlg) + 1);
        }
        break;
    case TY_VOICE_RSP_TEXT_FINISH:
        if (g_voice_ws_cbs.tuya_voice_text_stream) {
            g_voice_ws_cbs.tuya_voice_text_stream(TUYA_VOICE_STREAM_STOP, NULL, 0);
        }
        break;
    default:
        PR_ERR("shouldn't enter here");
        break;
    }

    return OPRT_OK;
}

// callback used by stream_gw
static void speaker_ws_recv_bin_cb(uint8_t *data, size_t len)
{
    size_t i = 0;
    char *rsp_type[TY_VOICE_RSP_TYPE_MAX] = {"ASR_MID",         "ASR_FINISH", "NLP_FINISH", "SKILL_FINISH",
                                             "SPEECH_FINISH",   "TTS_START",  "TTS_MID",    "TTS_FINISH",
                                             "TTS_INTERRUPTED", "TEXT_START", "TEXT_MID",   "TEXT_FINISH"};

    Speech__Response *cloud_rsp = speech__response__unpack(NULL, len, data);

    TY_GW_CHECK_NULL_RETURN_VOID(cloud_rsp->code);
    TY_GW_CHECK_NULL_RETURN_VOID(cloud_rsp->message);
    TY_GW_CHECK_NULL_RETURN_VOID(cloud_rsp->requestid);
    TY_GW_CHECK_NULL_RETURN_VOID(cloud_rsp->data);
    TY_GW_CHECK_NULL_RETURN_VOID(cloud_rsp->data->type);
    if (strcmp(cloud_rsp->data->type, "TTS_MID")) {
        PR_INFO("cloud rsp, code:%s, message:%s, requestid:%s, type:%s", cloud_rsp->code, cloud_rsp->message,
                cloud_rsp->requestid, cloud_rsp->data->type);
    }

    for (i = 0; i < CNTSOF(rsp_type); i++) {
        if (0 == strcmp(cloud_rsp->data->type, rsp_type[i])) {
            __handle_cloud_rsp((TY_VOICE_RSP_TYPE_E)i, cloud_rsp);
            speech__response__free_unpacked(cloud_rsp, NULL);
            return;
        }
    }

    PR_ERR("invalid rsp type: %s", cloud_rsp->data->type);
    speech__response__free_unpacked(cloud_rsp, NULL);
}

static void speaker_ws_recv_text_cb(uint8_t *data, size_t len)
{
    /** do something */
}

/**
 * @brief Check if WebSocket connection is online
 *
 * This function checks the current status of the WebSocket connection
 * to determine if it is online and available for communication.
 *
 * @return BOOL_T
 * @retval TRUE WebSocket connection is online
 * @retval FALSE WebSocket connection is offline
 */
BOOL_T tuya_voice_proto_ws_is_online(void)
{
    return tuya_speaker_ws_is_online();
}

/**
 * @brief Disconnect the WebSocket connection
 *
 * This function actively disconnects the current WebSocket connection.
 * Any ongoing communication will be terminated.
 *
 * @note After disconnection, the connection needs to be re-established
 *       before any further communication can occur
 */
void tuya_voice_proto_ws_disconnect(void)
{
    tuya_speaker_ws_disconnect();
}

/**
 * @brief Set the keepalive interval for WebSocket connection
 *
 * This function configures the keepalive interval for the WebSocket connection
 * to maintain the connection alive and detect disconnections.
 *
 * @param[in] sec Keepalive interval in seconds
 *
 * @note Setting a proper keepalive interval is important for maintaining
 *       a stable connection and detecting network issues early
 */
void tuya_voice_proto_ws_set_keepalive(uint32_t sec)
{
    tuya_speaker_ws_set_keepalive(sec);
}