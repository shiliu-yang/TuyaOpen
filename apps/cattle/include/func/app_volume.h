/**
 * @file app_volume.h
 * @brief app_volume module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __APP_VOLUME_H__
#define __APP_VOLUME_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Initializes the volume module.
 * @param None
 * @return void
 */
void app_volume_init(void);

/**
 * @brief Sets the volume for the audio module.
 * @param volume The volume level to set.
 * @return void
 */
void app_volume_set(uint8_t volume);

/**
 * @brief Retrieves the current volume setting for the audio module.
 * @param volume Pointer to the variable to store the current volume level.
 * @return void
 */
void app_volume_get(uint8_t *volume);

#ifdef __cplusplus
}
#endif

#endif /* __APP_VOLUME_H__ */
