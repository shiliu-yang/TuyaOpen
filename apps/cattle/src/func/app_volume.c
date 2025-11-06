/**
 * @file app_volume.c
 * @brief app_volume module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "app_volume.h"

#include "ai_audio.h"
#include "app_dp.h"

#include "tal_api.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static uint8_t s_volume_pending = 0;

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Internal workqueue callback to set volume
 * @param data Unused (volume value is in s_volume_pending)
 */
static void __volume_set_workq_cb(void *data)
{
    (void)data;  /* Unused parameter */
    
    uint8_t volume = s_volume_pending;
    PR_DEBUG("Setting volume to: %d", volume);
    
    OPERATE_RET rt = OPRT_OK;

    if (volume > 100) {
        PR_WARN("volume param err: %d", volume);
        volume = 100;
    }

    TUYA_CALL_ERR_LOG(ai_audio_set_volume(volume));
    // upload volume to cloud
    TUYA_CALL_ERR_LOG(app_dp_volume_upload(volume));
}

void app_volume_set(uint8_t volume)
{
    /* Store volume value in global variable */
    s_volume_pending = volume;
    
    /* Schedule volume setting in workqueue to avoid blocking caller thread */
    tal_workq_schedule(WORKQ_SYSTEM, __volume_set_workq_cb, NULL);
}

void app_volume_get(uint8_t *volume)
{
    if (NULL == volume) {
        PR_WARN("volume param err: %p", volume);
        return;
    }

    *volume = ai_audio_get_volume();
    return;
}

void app_volume_init(void)
{
    uint8_t volume = 0;
    volume = ai_audio_get_volume();
    PR_DEBUG("app_volume_init: volume: %d", volume);
    ai_audio_set_volume(volume);
}
