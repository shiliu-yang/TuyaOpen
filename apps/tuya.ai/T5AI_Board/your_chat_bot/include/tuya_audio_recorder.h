/**
 * @file tuya_audio_recorder.c
 * @brief Implements audio recorder functionality for handling audio streams
 *
 * This source file provides the implementation of an audio recorder that handles
 * audio stream recording, processing, and uploading. It includes functionality for
 * audio stream management, voice state handling, and integration with audio player
 * and voice protocol modules. The implementation supports audio stream writing,
 * reading, and uploading, as well as session management for multi-round conversations.
 * This file is essential for developers working on IoT applications that require
 * audio recording and processing capabilities.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TUYA_AUDIO_RECORDER_H__
#define __TUYA_AUDIO_RECORDER_H__

#include "tkl_audio.h"

typedef void *TUYA_AUDIO_RECORDER_HANDLE;

typedef struct {
    TKL_AUDIO_SAMPLE_E sample_rate;   // Audio sample rate
    TKL_AUDIO_DATABITS_E sample_bits; // Audio sample bits
    TKL_AUDIO_CHANNEL_E channel;      // Audio channel
    uint16_t upload_slice_duration;   // Duration of each uploaded audio data slice, in ms
    uint16_t record_duration;         // Maximum cacheable recording duration, in ms
} TUYA_AUDIO_RECORDER_CONFIG_T;

typedef struct {
    uint32_t silence_threshold;        // Silence wait threshold, in ms
    uint32_t active_threshold;         // Voice activity trigger threshold, in ms
    uint32_t wait_stop_play_threshold; // Threshold for waiting for play to stop, in ms
    uint32_t frame_duration_ms;        // Audio frame duration, in ms
} TUYA_AUDIO_RECORDER_THRESHOLD_T;

typedef enum {
    IN_IDLE = 0,
    IN_SILENCE,
    IN_START,
    IN_VOICE,
    IN_STOP,
    IN_RESUME,
} TY_AI_VoiceState;

/**
 * @brief Initializes the Tuya Audio Recorder.
 *
 * This function initializes the audio recorder by creating a mutex, initializing the player,
 * registering the audio encoders, and setting up the internal context.
 *
 * @return OPERATE_RET - The return code of the initialization function.
 *         OPRT_OK - Success.
 *         OPRT_ERROR - An error occurred during initialization.
 */
OPERATE_RET tuya_audio_recorder_init(void);

/**
 * @brief Starts the Tuya Audio Recorder with the specified configuration.
 *
 * This function starts the audio recorder, allocating necessary buffers and starting a task
 * to process the audio stream. It returns a handle to the recorder context.
 *
 * @param handle - A pointer to the Tuya Audio Recorder handle.
 * @param cfg - A pointer to the Tuya Audio Recorder configuration structure.
 * @return OPERATE_RET - The return code of the start function.
 *         OPRT_OK - Success.
 *         OPRT_INVALID_PARM - Invalid parameter.
 *         OPRT_ERROR - An error occurred during start.
 */
OPERATE_RET tuya_audio_recorder_start(TUYA_AUDIO_RECORDER_HANDLE *handle, const TUYA_AUDIO_RECORDER_CONFIG_T *cfg);

/**
 * @brief Stops the Tuya Audio Recorder.
 *
 * This function stops the audio recorder, freeing allocated resources and stopping the processing
 * task.
 *
 * @param handle - A pointer to the Tuya Audio Recorder handle.
 */
void tuya_audio_recorder_stop(TUYA_AUDIO_RECORDER_HANDLE handle);

/**
 * @brief Writes audio data to the audio stream.
 *
 * This function writes audio data to the internal buffer of the audio recorder.
 *
 * @param handle - A pointer to the Tuya Audio Recorder handle.
 * @param buf - A pointer to the buffer containing the audio data.
 * @param len - The length of the audio data in bytes.
 * @return int - The number of bytes written to the audio stream.
 *         OPRT_COM_ERROR - An error occurred during writing.
 */
int tuya_audio_recorder_stream_write(TUYA_AUDIO_RECORDER_HANDLE handle, const char *buf, uint32_t len);

/**
 * @brief Reads audio data from the audio stream.
 *
 * This function reads audio data from the internal buffer of the audio recorder.
 *
 * @param handle - A pointer to the Tuya Audio Recorder handle.
 * @param buf - A pointer to the buffer to store the audio data.
 * @param len - The length of the audio data buffer.
 * @return int - The number of bytes read from the audio stream.
 *         OPRT_COM_ERROR - An error occurred during reading.
 */
int tuya_audio_recorder_stream_read(TUYA_AUDIO_RECORDER_HANDLE handle, char *buf, uint32_t len);

/**
 * @brief Clears the audio stream buffer.
 *
 * This function resets the internal buffer of the audio recorder.
 *
 * @param handle - A pointer to the Tuya Audio Recorder handle.
 * @return OPERATE_RET - The return code of the clear function.
 *         OPRT_OK - Success.
 */
OPERATE_RET tuya_audio_recorder_stream_clear(TUYA_AUDIO_RECORDER_HANDLE handle);

/**
 * @brief Gets the current size of the audio stream buffer.
 *
 * This function returns the number of bytes used in the internal buffer of the audio recorder.
 *
 * @param handle - A pointer to the Tuya Audio Recorder handle.
 * @return int - The size of the audio stream buffer in bytes.
 */
int tuya_audio_recorder_stream_get_size(TUYA_AUDIO_RECORDER_HANDLE handle);

/**
 * @brief Posts a voice status message.
 *
 * This function posts a voice status message to the internal message queue.
 *
 * @param handle - A pointer to the Tuya Audio Recorder handle.
 * @param stat - The voice status to be posted.
 * @return OPERATE_RET - The return code of the post function.
 *         OPRT_OK - Success.
 *         OPRT_COM_ERROR - An error occurred during posting.
 */
OPERATE_RET ty_ai_voice_stat_post(TUYA_AUDIO_RECORDER_HANDLE handle, TY_AI_VoiceState stat);

#endif /* __TUYA_AUDIO_RECORDER_H__ */