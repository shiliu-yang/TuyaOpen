/**
 * @file tuya_audio_player.H
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

#ifndef __TUYA_AUDIO_PLAYER_H__
#define __TUYA_AUDIO_PLAYER_H__

#include "tuya_cloud_types.h"

typedef enum {
    TUYA_PLAYER_STAT_IDLE = 0,
    TUYA_PLAYER_STAT_PLAY,
    TUYA_PLAYER_STAT_STOP,
    TUYA_PLAYER_STAT_PAUSE,
    TUYA_PLAYER_STAT_RESUME,
    TUYA_PLAYER_STAT_DESTROY,
    TUYA_PLAYER_STAT_ERROR,
    TUYA_PLAYER_STAT_MAX,
} TUYA_PLAYER_STAT;

typedef enum {
    AUDIO_ALART_TYPE_NORMAL = 0,
    AUDIO_ALART_TYPE_POWER_ON,
    AUDIO_ALART_TYPE_NOT_ACTIVE,
    AUDIO_ALART_TYPE_NETWORK_CFG,
    AUDIO_ALART_TYPE_NETWORK_CONNECTED,
    AUDIO_ALART_TYPE_NETWORK_FAIL,
    AUDIO_ALART_TYPE_NETWORK_DISCONNECT,
    AUDIO_ALART_TYPE_BATTERY_LOW,
    AUDIO_ALART_TYPE_PLEASE_AGAIN,
    AUDIO_ALART_TYPE_MAX,
} AUDIO_ALART_TYPE;

#define EVENT_TUYA_PLAYER "tuya_player_evt"

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
OPERATE_RET tuya_audio_player_init(void);

/**
 * @brief Destroys the Tuya Audio Player.
 *
 * This function cleans up the audio player by freeing allocated resources and stopping any ongoing operations.
 *
 * @return OPERATE_RET - The return code of the destruction function.
 *         OPRT_OK - Success.
 */
OPERATE_RET tuya_audio_player_destroy(void);

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
int tuya_audio_player_stream_write(char *buf, uint32_t len);

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
int tuya_audio_player_stream_read(char *buf, uint32_t len);

/**
 * @brief Gets the current size of the audio stream buffer.
 *
 * This function returns the number of bytes used in the internal buffer of the audio player.
 *
 * @return int - The size of the audio stream buffer in bytes.
 */
int tuya_audio_player_stream_get_size(void);

/**
 * @brief Gets the available size of the audio stream buffer.
 *
 * This function returns the number of bytes available in the internal buffer of the audio player.
 *
 * @return int - The available size of the audio stream buffer in bytes.
 */
int tuya_audio_player_stream_avail_size(void);

/**
 * @brief Clears the audio stream buffer.
 *
 * This function resets the internal buffer of the audio player.
 *
 * @return OPERATE_RET - The return code of the clear function.
 *         OPRT_OK - Success.
 */
OPERATE_RET tuya_audio_player_stream_clear(void);

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
OPERATE_RET tuya_audio_player_play_raw(char *data, uint32_t len);

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
OPERATE_RET tuya_audio_player_play_alert(AUDIO_ALART_TYPE type, BOOL_T send_eof);

/**
 * @brief Start the audio player.
 *
 * This function initializes the audio player and starts playing audio.
 *
 * @return OPERATE_RET - The return code of the operation.
 *         OPRT_OK on success, OPRT_COM_ERROR on communication error.
 */
OPERATE_RET tuya_audio_player_start(void);

/**
 * @brief Stop the audio player.
 *
 * This function stops the audio player and clears any ongoing playback.
 *
 * @return OPERATE_RET - The return code of the operation.
 *         OPRT_OK on success, OPRT_COM_ERROR on communication error.
 */
OPERATE_RET tuya_audio_player_stop(void);

/**
 * @brief Checks if the audio player is playing.
 *
 * This function checks if the audio player is currently playing audio.
 *
 * @return BOOL_T - TRUE if the audio player is playing, FALSE otherwise.
 */
BOOL_T tuya_audio_player_is_playing(void);

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
OPERATE_RET tuya_audio_player_set_volume(int vol);

/**
 * @brief Gets the current volume of the audio player.
 *
 * This function retrieves the current volume level of the audio player.
 *
 * @return int - The current volume level of the audio player.
 */
int tuya_audio_player_get_volume(void);

#endif /* __TUYA_AUDIO_PLAYER_H__ */
