/**
 * @file app_vad.c
 * @brief app_vad module is used to
 * @version 0.1
 * @date 2025-03-25
 */

#include "app_vad.h"

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

/***********************************************************
***********************function define**********************
***********************************************************/

OPERATE_RET app_vad_frame_put(uint8_t *pbuf, uint16_t len)
{
    return ty_vad_frame_put(pbuf, len);
}

ty_vad_flag_e app_vad_get_flag(void)
{
    return ty_get_vad_flag();
}

OPERATE_RET app_vad_init(uint16_t sample_rate, uint16_t channel)
{
    OPERATE_RET rt = OPRT_OK;

    ty_vad_config_t vad_config;
    memset(&vad_config, 0, sizeof(ty_vad_config_t));
    vad_config.start_threshold_ms = 300;
    vad_config.end_threshold_ms = 500;
    vad_config.silence_threshold_ms = 0;
    vad_config.sample_rate = sample_rate;
    vad_config.channel = channel;
    vad_config.vad_frame_duration = 10;
    vad_config.scale = 1.0;

    TUYA_CALL_ERR_RETURN(ty_vad_app_init(&vad_config));

    TUYA_CALL_ERR_RETURN(ty_vad_app_start());

    return rt;
}
