/**
 * @file tuya_audio_player.c
 * @brief Implements audio player functionality for handling MP3 audio streams
 *
 * This source file provides the implementation of an audio player that handles
 * MP3 audio streams. It includes functionality for audio stream management,
 * MP3 decoding, and audio output. The implementation supports audio stream
 * writing, reading, and playback control, as well as volume management.
 * This file is essential for developers working on IoT applications that require
 * audio playback capabilities.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include <stdio.h>
#include "tal_api.h"
#include "tkl_audio.h"
#include "tuya_ringbuf.h"
#include "tkl_thread.h"
#include "ai_audio_media.h"
#include "tuya_audio_player.h"
#include <driver/aud_dac_types.h>
#include <driver/aud_dac.h>
#include <driver/dma.h>
#include <driver/audio_ring_buff.h>
#include <modules/mp3dec.h>

#define TY_MP3_DECODER_MAVOICE_STATE_IN_BUFF_SIZE (MAINBUF_SIZE)
#define PCM_SIZE_MAX                              (MAX_NSAMP * MAX_NCHAN * MAX_NGRAN * 2)
#define MP3_STREAM_BUFF_MAX_LEN                   (1024 * 64 * 2)

typedef struct {
    BOOL_T is_playing;
    BOOL_T is_task_stop;
    BOOL_T first_frame;
    TUYA_PLAYER_STAT stat;
    TUYA_RINGBUFF_T stream_ringbuf;
    MUTEX_HANDLE mutex;
    MUTEX_HANDLE ringbuf_mutex;
    QUEUE_HANDLE msg_queue;
    int bytes_left;
    int bytes_read;
    HMP3Decoder decoder;
    BOOL_T is_eof;
    uint8_t read_buf[MAINBUF_SIZE];
    uint8_t pcm_buf[PCM_SIZE_MAX];
    uint8_t *read_ptr;
} TUYA_PLAYER_CONTEXT;

static TUYA_PLAYER_CONTEXT *s_ctx = NULL;
static MUTEX_HANDLE s_mutex;
static BOOL_T s_is_stop = TRUE;
static THREAD_HANDLE s_t5_player_hander = NULL;

static OPERATE_RET _play_stat_post(TUYA_PLAYER_STAT stat)
{
    TUYA_PLAYER_CONTEXT *ctx = s_ctx;
    int msg = (int)stat;
    return tal_queue_post(ctx->msg_queue, &msg, SEM_WAIT_FOREVER);
}

static OPERATE_RET _play_stat_fetch(TUYA_PLAYER_STAT *stat, uint32_t timeout)
{
    TUYA_PLAYER_CONTEXT *ctx = s_ctx;
    int msg;
    OPERATE_RET ret = tal_queue_fetch(ctx->msg_queue, &msg, timeout);
    if (ret != OPRT_OK) {
        return ret;
    }
    *stat = (TUYA_PLAYER_STAT)msg;
    return OPRT_OK;
}

static int _mp3_find_id3(uint8_t *buf)
{
    char tag_header[10];
    int tag_size = 0;

    memcpy(tag_header, buf, sizeof(tag_header));

    if (tag_header[0] == 'I' && tag_header[1] == 'D' && tag_header[2] == '3') {
        tag_size = ((tag_header[6] & 0x7F) << 21) | ((tag_header[7] & 0x7F) << 14) | ((tag_header[8] & 0x7F) << 7) |
                   (tag_header[9] & 0x7F);
        PR_DEBUG("ID3 tag_size = %d", tag_size);
        return tag_size + sizeof(tag_header);
    } else {
        // tag_header ignored
        return 0;
    }
}

static void _t5_mp3_play_task(void *arg)
{
    OPERATE_RET ret = OPRT_OK;
    MP3FrameInfo frame_info;
    int read_size = 0;
    TUYA_PLAYER_CONTEXT *ctx = (TUYA_PLAYER_CONTEXT *)arg;
    TUYA_PLAYER_STAT stat;
    int offset;
    while (ctx->stat != TUYA_PLAYER_STAT_DESTROY) {
        // fetch play stat
        ret = _play_stat_fetch(&stat, ctx->is_playing ? 5 : 500);
        if (ret == OPRT_OK) {
            PR_DEBUG("tuya audio player task recv stat=%d", stat);
            switch (stat) {
            case TUYA_PLAYER_STAT_IDLE:
                break;
            case TUYA_PLAYER_STAT_PLAY:
                // TODO: post event
                break;

            case TUYA_PLAYER_STAT_STOP:
                if (ctx->bytes_left > 0) {
                    // clear ring buffer
                    tkl_ao_clear_buffer(TKL_AUDIO_TYPE_BOARD, 0);
                }
                break;

            case TUYA_PLAYER_STAT_PAUSE:
                break;

            case TUYA_PLAYER_STAT_RESUME:
                break;

            default:
                break;
            }
        }

        tal_mutex_lock(ctx->mutex);
        if (!ctx->is_playing) {
            goto next_loop;
        }
        if (ctx->bytes_left < MAINBUF_SIZE) {
            if (ctx->bytes_left > 0 && ctx->read_ptr != ctx->read_buf) {
                memmove(ctx->read_buf, ctx->read_ptr, ctx->bytes_left);
            }

            if (ctx->first_frame) {
                int read_size = tuya_audio_player_stream_get_size();
                if (read_size > 0)
                    PR_DEBUG("tuya_audio_player_stream_get_size=%d, bytes_left=%d", read_size, ctx->bytes_left);
            }

            // read ring buffer from audio stream and decoce mp3 stream
            ret =
                tuya_audio_player_stream_read((char *)ctx->read_buf + ctx->bytes_left, MAINBUF_SIZE - ctx->bytes_left);
            if (ret < 0) {
                PR_ERR("tuya_audio_player_stream_read failed, ret=%d", ret);
                goto next_loop;
            } else if (ret == 0 && ctx->bytes_left == 0) {
                // play is eof, change state
                if (ctx->is_eof) {
                    ctx->is_eof = FALSE;
                    tuya_audio_player_stop();
                    PR_DEBUG("tuya audio player play end");
                }
                goto next_loop;
            }

            read_size = ret;
            ctx->bytes_left += read_size;
            ctx->read_ptr = ctx->read_buf;
            if (ctx->bytes_left < MAINBUF_SIZE && !ctx->is_eof) {
                // 修复mp3decode读取数据不足丢弃数据的问题
                goto next_loop;
            }
        }

        if (ctx->first_frame && ctx->bytes_left > 10) {
            int id3_size = _mp3_find_id3(ctx->read_ptr);
            if (id3_size > 0) {
                ctx->read_ptr += id3_size;
                ctx->bytes_left -= id3_size;
            }
            ctx->first_frame = FALSE;
        }
        // decode mp3 stream
        offset = MP3FindSyncWord(ctx->read_ptr, ctx->bytes_left);
        if (offset < 0) {
            PR_ERR("MP3FindSyncWord not find!");
            ctx->bytes_left = 0;
            goto next_loop;
        }
        ctx->read_ptr += offset;
        ctx->bytes_left -= offset;
        int bytes_left = ctx->bytes_left;
        ret = MP3Decode(ctx->decoder, &ctx->read_ptr, &ctx->bytes_left, (short *)ctx->pcm_buf, 0);
        if (ret != ERR_MP3_NONE) {
            PR_ERR("MP3Decode failed, ret=%d, offset=%d, bytes_left=%d, %d, read_size=%d", ret, offset, ctx->bytes_left,
                   bytes_left, read_size);
            // reset bytes_left if no data is decoded
            if (bytes_left == ctx->bytes_left) {
                ctx->bytes_left = 0;
            }
            goto next_loop;
        }
        if (ctx->bytes_left == bytes_left) {
            PR_ERR("MP3Decode alert, bytes_left=%d, %d", ctx->bytes_left, bytes_left);
        }

        MP3GetLastFrameInfo(ctx->decoder, &frame_info);
        tal_mutex_unlock(ctx->mutex);
        PR_TRACE("MP3 frame info: bitrate=%d, nChans=%d, samprate=%d, outputSamps=%d", frame_info.bitrate,
                 frame_info.nChans, frame_info.samprate, frame_info.outputSamps);

        TKL_AUDIO_FRAME_INFO_T frame;
        frame.pbuf = (char *)ctx->pcm_buf;
        frame.used_size = frame_info.outputSamps * frame_info.bitsPerSample / 8;
        ret = tkl_ao_put_frame(0, 0, NULL, &frame);
        if (ret != OPRT_OK) {
            PR_DEBUG("tkl_ao_put_frame failed, ret=%d", ret);
            continue;
        }
        continue;
    next_loop:
        tal_mutex_unlock(ctx->mutex);
    }

    PR_NOTICE("tuya audio player task exit");
    ctx->is_task_stop = TRUE;
    tkl_thread_release(s_t5_player_hander);
}

static void *mp3_private_alloc_psram(size_t size)
{
    return tkl_system_psram_malloc(size);
}

static void mp3_private_free_psram(void *buff)
{
    tkl_system_psram_free(buff);
}

static void *mp3_private_memset_psram(void *s, unsigned char c, size_t n)
{
    return tkl_system_memset(s, c, n);
}

static OPERATE_RET _tuya_audio_player_destroy(void)
{
    TUYA_PLAYER_CONTEXT *ctx = s_ctx;

    PR_NOTICE("tuya audio player destroy...");
    tal_mutex_lock(s_mutex);
    if (s_t5_player_hander) {
        // tal_mutex_lock(ctx->mutex);
        ctx->stat = TUYA_PLAYER_STAT_DESTROY;
        // wait for task exit
        while (!ctx->is_task_stop) {
            tal_system_sleep(10);
        }
        s_t5_player_hander = NULL;
    }

    if (ctx->mutex) {
        tal_mutex_release(ctx->mutex);
        ctx->mutex = NULL;
    }
    if (ctx->ringbuf_mutex) {
        tal_mutex_release(ctx->ringbuf_mutex);
        ctx->ringbuf_mutex = NULL;
    }
    if (ctx->msg_queue) {
        tal_queue_free(ctx->msg_queue);
    }
    if (ctx->stream_ringbuf) {
        tuya_ring_buff_free(ctx->stream_ringbuf);
    }
    if (ctx->decoder) {
        MP3FreeDecoder(ctx->decoder);
    }
    if (ctx) {
        tkl_system_psram_free(ctx);
    }
    tkl_ao_clear_buffer(TKL_AUDIO_TYPE_BOARD, 0);
    s_ctx = NULL;
    s_is_stop = TRUE;
    tal_mutex_unlock(s_mutex);
    PR_NOTICE("tuya audio player destroy success");
    return OPRT_OK;
}

static OPERATE_RET _tuya_audio_player_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_PLAYER_CONTEXT *ctx;

    PR_DEBUG("tuya audio player start...");
    tal_mutex_lock(s_mutex);
    ctx = tkl_system_psram_malloc(sizeof(TUYA_PLAYER_CONTEXT));
    if (ctx == NULL) {
        PR_ERR("tuya player ctx malloc failed");
        goto error;
    }
    memset(ctx, 0, sizeof(TUYA_PLAYER_CONTEXT));
    ctx->read_ptr = ctx->read_buf;
    ctx->stat = TUYA_PLAYER_STAT_STOP;

    TUYA_CALL_ERR_GOTO(tal_queue_create_init(&ctx->msg_queue, sizeof(int), 8), error);
    TUYA_CALL_ERR_GOTO(tuya_ring_buff_create(MP3_STREAM_BUFF_MAX_LEN, OVERFLOW_PSRAM_STOP_TYPE, &ctx->stream_ringbuf),
                       error);
    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&ctx->mutex), error);
    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&ctx->ringbuf_mutex), error);

    ctx->decoder = MP3InitDecoder();
    if (ctx->decoder == NULL) {
        PR_ERR("MP3Decoder init failed");
        goto error;
    }

    s_ctx = ctx;
    TUYA_CALL_ERR_GOTO(tkl_thread_create_in_psram(&s_t5_player_hander, "tuya_audio_player", 1024 * 8, THREAD_PRIO_2,
                                                  _t5_mp3_play_task, ctx),
                       error);
    s_is_stop = FALSE;
    tal_mutex_unlock(s_mutex);
    PR_DEBUG("tuya_audio_player task create success");
    return OPRT_OK;
error:
    _tuya_audio_player_destroy();
    tal_mutex_unlock(s_mutex);
    return rt;
}

/**
 * @brief Initializes the Tuya Audio Player.
 *
 * This function initializes the audio player by creating a mutex, initializing the internal context,
 * and setting up the necessary components.
 *
 * @return OPERATE_RET - The return code of the initialization function.
 *         OPRT_OK - Success.
 *         OPRT_ERROR - An error occurred during initialization.
 */
OPERATE_RET tuya_audio_player_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    static BOOL_T is_init = FALSE;

    PR_DEBUG("tuya audio player init...");

    if (!is_init) {
        MP3SetBuffMethodAlwaysFourAlignedAccess(mp3_private_alloc_psram, mp3_private_free_psram,
                                                mp3_private_memset_psram);

        TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&s_mutex), error);

        PR_DEBUG("tuya_audio_player task create success");
        is_init = TRUE;
    }

    rt = _tuya_audio_player_init();
    if (rt != OPRT_OK) {
        PR_ERR("tuya audio player init failed");
        _tuya_audio_player_destroy();
        return rt;
    }

    return OPRT_OK;
error:
    if (s_mutex)
        tal_mutex_release(s_mutex);

    return rt;
}

/**
 * @brief Destroys the Tuya Audio Player.
 *
 * This function cleans up the audio player by freeing allocated resources and stopping any ongoing operations.
 *
 * @return OPERATE_RET - The return code of the destruction function.
 *         OPRT_OK - Success.
 */
OPERATE_RET tuya_audio_player_destroy(void)
{
    return _tuya_audio_player_destroy();
}

/**
 * @brief Writes audio data to the audio stream.
 *
 * This function writes audio data to the internal buffer of the audio player.
 *
 * @param buf - A pointer to the buffer containing the audio data.
 * @param len - The length of the audio data in bytes.
 * @return int - The number of bytes written to the audio stream.
 *         OPRT_COM_ERROR - An error occurred during writing.
 */
int tuya_audio_player_stream_write(char *buf, uint32_t len)
{
    TUYA_PLAYER_CONTEXT *ctx = s_ctx;
    tal_mutex_lock(s_mutex);
    if (s_is_stop) {
        tal_mutex_unlock(s_mutex);
        return OPRT_COM_ERROR;
    }
    if (ctx == NULL) {
        PR_ERR("tuya audio ctx is NULL");
        return OPRT_COM_ERROR;
    }
    tal_mutex_lock(ctx->mutex);
    ctx->is_eof = (buf == NULL); // set eof flag
    tal_mutex_unlock(ctx->mutex);
    if (buf) {
        tal_mutex_lock(ctx->ringbuf_mutex);
        int ret = tuya_ring_buff_write(ctx->stream_ringbuf, buf, len);
        tal_mutex_unlock(ctx->ringbuf_mutex);
        tal_mutex_unlock(s_mutex);
        return ret;
    } else {
        tal_mutex_unlock(s_mutex);
        return 0;
    }
}

/**
 * @brief Reads audio data from the audio stream.
 *
 * This function reads audio data from the internal buffer of the audio player.
 *
 * @param buf - A pointer to the buffer to store the audio data.
 * @param len - The length of the audio data buffer.
 * @return int - The number of bytes read from the audio stream.
 *         OPRT_COM_ERROR - An error occurred during reading.
 */
int tuya_audio_player_stream_read(char *buf, uint32_t len)
{
    int ret = 0, read_size = 0;
    TUYA_PLAYER_CONTEXT *ctx = s_ctx;
    tal_mutex_lock(ctx->ringbuf_mutex);
    ret = tuya_ring_buff_read(ctx->stream_ringbuf, buf, len);
    tal_mutex_unlock(ctx->ringbuf_mutex);
    if (ret < 0) {
        PR_ERR("tuya_ring_buff_read failed, ret=%d", ret);
        return OPRT_COM_ERROR;
    }

    read_size = ret;
    return read_size;
}

/**
 * @brief Gets the current size of the audio stream buffer.
 *
 * This function returns the number of bytes used in the internal buffer of the audio player.
 *
 * @return int - The size of the audio stream buffer in bytes.
 */
int tuya_audio_player_stream_get_size(void)
{
    int size = 0;
    TUYA_PLAYER_CONTEXT *ctx = s_ctx;
    tal_mutex_lock(ctx->ringbuf_mutex);
    size = tuya_ring_buff_used_size_get(ctx->stream_ringbuf);
    tal_mutex_unlock(ctx->ringbuf_mutex);
    return size;
}

/**
 * @brief Gets the available size of the audio stream buffer.
 *
 * This function returns the number of bytes available in the internal buffer of the audio player.
 *
 * @return int - The available size of the audio stream buffer in bytes.
 */
int tuya_audio_player_stream_avail_size(void)
{
    int size = 0;
    TUYA_PLAYER_CONTEXT *ctx = s_ctx;
    tal_mutex_lock(ctx->ringbuf_mutex);
    size = tuya_ring_buff_free_size_get(ctx->stream_ringbuf);
    tal_mutex_unlock(ctx->ringbuf_mutex);
    return size;
}

/**
 * @brief Clears the audio stream buffer.
 *
 * This function resets the internal buffer of the audio player.
 *
 * @return OPERATE_RET - The return code of the clear function.
 *         OPRT_OK - Success.
 */
OPERATE_RET tuya_audio_player_stream_clear(void)
{
    OPERATE_RET ret;
    TUYA_PLAYER_CONTEXT *ctx = s_ctx;
    tal_mutex_lock(ctx->ringbuf_mutex);
    ret = tuya_ring_buff_reset(ctx->stream_ringbuf);
    tal_mutex_unlock(ctx->ringbuf_mutex);
    return ret;
}

/**
 * @brief Play raw audio data.
 *
 * This function plays raw audio data provided in the 'data' buffer.
 *
 * @param data Pointer to the audio data buffer.
 * @param len Length of the audio data in bytes.
 * @return OPERATE_RET - The return code of the operation.
 *         OPRT_OK on success, OPRT_COM_ERROR on communication error.
 */
OPERATE_RET tuya_audio_player_play_raw(char *data, uint32_t len)
{
    OPERATE_RET ret = OPRT_OK;
    TUYA_PLAYER_CONTEXT *ctx = s_ctx;
    if (ctx == NULL) {
        PR_ERR("tuya audio ctx is NULL");
        return OPRT_COM_ERROR;
    }
    if (tuya_audio_player_is_playing()) {
        PR_DEBUG("tuya audio is playing, stop it first");
        // stop current playing
        tuya_audio_player_stop();
        tuya_audio_player_start();
    } else {
        PR_DEBUG("tuya audio is not playing, start it");
        tuya_audio_player_start();
    }
    // start play
    while (len > 0) {
        ret = tuya_audio_player_stream_write((char *)data, len);
        if (ret < 0) {
            PR_ERR("tuya_audio_player_stream_write failed, ret=%d", ret);
            return OPRT_COM_ERROR;
        }
        if (ret == 0) {
            tal_system_sleep(10);
        } else {
            len -= ret;
            data += ret;
        }
    }
    return OPRT_OK;
}

/**
 * @brief Play alert audio for different events.
 *
 * This function plays different alert audio types based on the provided event type.
 *
 * @param type The type of alert audio to play.
 * @param send_eof Indicates whether to send an EOF signal after playing the alert.
 * @return OPERATE_RET - The return code of the operation.
 *         OPRT_OK on success, OPRT_COM_ERROR on communication error.
 */
OPERATE_RET tuya_audio_player_play_alert(AUDIO_ALART_TYPE type, BOOL_T send_eof)
{
    OPERATE_RET ret = OPRT_OK;

    PR_DEBUG("tuya audio play alert type=%d", type);
    tal_mutex_lock(s_mutex);
    if (s_is_stop) {
        tal_mutex_unlock(s_mutex);
        return OPRT_COM_ERROR;
    }
    if (s_ctx == NULL) {
        PR_ERR("tuya audio ctx is NULL");
        return OPRT_COM_ERROR;
    }
    switch (type) {
    case AUDIO_ALART_TYPE_POWER_ON:
        ret = tuya_audio_player_play_raw((char *)media_src_power_on, sizeof(media_src_power_on));
        break;
    case AUDIO_ALART_TYPE_NOT_ACTIVE:
        ret = tuya_audio_player_play_raw((char *)media_src_not_active, sizeof(media_src_not_active));
        break;
    case AUDIO_ALART_TYPE_NETWORK_CFG:
        ret = tuya_audio_player_play_raw((char *)media_src_netcfg_mode, sizeof(media_src_netcfg_mode));
        break;
    case AUDIO_ALART_TYPE_NETWORK_CONNECTED:
        ret = tuya_audio_player_play_raw((char *)media_src_network_conencted, sizeof(media_src_network_conencted));
        break;
    case AUDIO_ALART_TYPE_NETWORK_FAIL:
        ret = tuya_audio_player_play_raw((char *)media_src_network_fail, sizeof(media_src_network_fail));
        break;
    case AUDIO_ALART_TYPE_NETWORK_DISCONNECT:
        ret = tuya_audio_player_play_raw((char *)media_src_network_disconnect, sizeof(media_src_network_disconnect));
        break;
    case AUDIO_ALART_TYPE_BATTERY_LOW:
        ret = tuya_audio_player_play_raw((char *)media_src_battery_low, sizeof(media_src_battery_low));
        break;
    case AUDIO_ALART_TYPE_PLEASE_AGAIN:
        ret = tuya_audio_player_play_raw((char *)media_src_please_again, sizeof(media_src_please_again));
        break;
    default:
        tal_mutex_unlock(s_mutex);
        return OPRT_INVALID_PARM;
    }
    if (send_eof) {
        tuya_audio_player_stream_write(NULL, 0);
    }

    tal_mutex_unlock(s_mutex);
    return ret;
}

/**
 * @brief Start the audio player.
 *
 * This function initializes the audio player and starts playing audio.
 *
 * @return OPERATE_RET - The return code of the operation.
 *         OPRT_OK on success, OPRT_COM_ERROR on communication error.
 */
OPERATE_RET tuya_audio_player_start(void)
{
    TUYA_PLAYER_CONTEXT *ctx = s_ctx;
    if (ctx == NULL) {
        PR_ERR("tuya audio ctx is NULL");
        return OPRT_COM_ERROR;
    }
    if (ctx->is_playing) {
        PR_DEBUG("tuya audio is playing...........");
    }
    tal_mutex_lock(ctx->mutex);
    // FIXME: clear mp3 decoder
    if (ctx->bytes_left > 0 || ctx->decoder == NULL) {
        if (ctx->decoder) {
            MP3FreeDecoder(ctx->decoder);
        }
        ctx->decoder = MP3InitDecoder();
        if (ctx->decoder == NULL) {
            ctx->stat = TUYA_PLAYER_STAT_ERROR;
            ctx->is_playing = FALSE;
            PR_ERR("MP3Decoder init failed");
            tal_mutex_unlock(ctx->mutex);
            return OPRT_COM_ERROR;
        }
        PR_DEBUG("MP3Decoder init success");
    }
    ctx->stat = TUYA_PLAYER_STAT_PLAY;
    ctx->is_playing = TRUE;
    ctx->first_frame = TRUE;
    ctx->bytes_left = 0;
    ctx->read_ptr = ctx->read_buf;
    ctx->is_eof = FALSE;
    tuya_audio_player_stream_clear();
    _play_stat_post(TUYA_PLAYER_STAT_PLAY);
    tal_event_publish(EVENT_TUYA_PLAYER, &ctx->stat);
    tal_mutex_unlock(ctx->mutex);
    PR_NOTICE("tuya audio start");
    return OPRT_OK;
}

/**
 * @brief Stop the audio player.
 *
 * This function stops the audio player and clears any ongoing playback.
 *
 * @return OPERATE_RET - The return code of the operation.
 *         OPRT_OK on success, OPRT_COM_ERROR on communication error.
 */
OPERATE_RET tuya_audio_player_stop(void)
{
    TUYA_PLAYER_CONTEXT *ctx = s_ctx;
    if (ctx == NULL) {
        PR_ERR("tuya audio ctx is NULL");
        return OPRT_COM_ERROR;
    }
    tal_mutex_lock(ctx->mutex);
    ctx->stat = TUYA_PLAYER_STAT_STOP;
    ctx->is_playing = FALSE;
    // ctx->bytes_left = 0;
    // ctx->read_ptr = ctx->read_buf;
    // ctx->is_eof = FALSE;
    tuya_audio_player_stream_clear();
    _play_stat_post(TUYA_PLAYER_STAT_STOP);
    tal_event_publish(EVENT_TUYA_PLAYER, &ctx->stat);
    tal_mutex_unlock(ctx->mutex);
    PR_NOTICE("tuya audio stop");
    return OPRT_OK;
}

/**
 * @brief Checks if the audio player is playing.
 *
 * This function checks if the audio player is currently playing audio.
 *
 * @return BOOL_T - TRUE if the audio player is playing, FALSE otherwise.
 */
BOOL_T tuya_audio_player_is_playing(void)
{
    TUYA_PLAYER_CONTEXT *ctx = s_ctx;
    if (ctx == NULL) {
        PR_ERR("tuya audio ctx is NULL");
        return FALSE;
    }
    return ctx->is_playing;
}

/**
 * @brief Sets the volume of the audio player.
 *
 * This function sets the volume level of the audio player.
 *
 * @param vol - The volume level to set (0-100).
 * @return OPERATE_RET - The return code of the volume setting function.
 *         OPRT_OK - Success.
 *         OPRT_INVALID_PARM - Invalid volume value.
 */
OPERATE_RET tuya_audio_player_set_volume(int vol)
{
    if (vol < 0 || vol > 100) {
        PR_ERR("invalid volume value");
        return OPRT_INVALID_PARM;
    }

    tkl_ao_set_vol(TKL_AUDIO_TYPE_BOARD, 0, NULL, vol);
    return OPRT_OK;
}

/**
 * @brief Gets the current volume of the audio player.
 *
 * This function retrieves the current volume level of the audio player.
 *
 * @return int - The current volume level of the audio player.
 */
int tuya_audio_player_get_volume(void)
{
    int vol = 0;
    OPERATE_RET ret = tkl_ao_get_vol(TKL_AUDIO_TYPE_BOARD, 0, NULL, &vol);
    if (ret != OPRT_OK) {
        PR_ERR("tkl_ao_get_vol failed, ret=%d", ret);
        return -1;
    }
    return vol;
}
